#include "HALMovementEffectBridge.h"
#include "HALAbilitySystemComponent.h"
#include "HALMovementAttributeSet.h"
#include "HALGameplayTags.h"
#include "ArcadeVehicleMovementComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UHALMovementEffectBridge::UHALMovementEffectBridge()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}
void UHALMovementEffectBridge::SetAreaRecoverySeconds(const FVector& Seconds)
{
	// Several sources/families can leave in one GT frame. A later unchanged
	// channel must not erase the recovery requested by an earlier exit.
	for (int32 Index = 0; Index < 3; ++Index) { AreaRecoverySeconds[Index] = FMath::Max(AreaRecoverySeconds[Index], Seconds[Index]); }
}
void UHALMovementEffectBridge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHALMovementEffectBridge, Epoch);
}
void UHALMovementEffectBridge::ConfigurePhysics()
{
	if (!Movement) { Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>(); }
	if (!History) { History = GetOwner()->FindComponentByClass<UNetworkPhysicsComponent>(); }
	if (Movement) { Movement->ConfigureMovementEffects(Epoch, History, GetOwner()->HasAuthority(), BoundPlayerId); }
}
void UHALMovementEffectBridge::OnRep_Epoch() { ConfigurePhysics(); }
void UHALMovementEffectBridge::Bind(UHALAbilitySystemComponent* InASC, int32 PlayerId)
{
	if (!InASC || InASC->GetAvatarActor() != GetOwner() || !InASC->GetSet<UHALMovementAttributeSet>()) { return; }
	if (ASC == InASC && WorldHandle.IsValid()) { ConfigurePhysics(); return; }
	Unbind();
	ASC = InASC;
	BoundPlayerId = PlayerId;
	if (GetOwner()->HasAuthority())
	{
		Epoch = FGuid::NewGuid();
		const FGameplayAttribute Attributes[] = {
			UHALMovementAttributeSet::GetAccelerationMultiplierDeltaAttribute(), UHALMovementAttributeSet::GetSpeedLimitMultiplierDeltaAttribute(),
			UHALMovementAttributeSet::GetLateralGripMultiplierDeltaAttribute(), UHALMovementAttributeSet::GetAreaAccelerationDeltaAttribute(),
			UHALMovementAttributeSet::GetAreaSpeedDeltaAttribute(), UHALMovementAttributeSet::GetAreaGripDeltaAttribute() };
		for (const auto& Attribute : Attributes)
		{
			AttributeHandles.Emplace(Attribute, ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::AttributeChanged));
		}
		bDirty = true;
		StunHandle = ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Stun, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::StunChanged);
		GetOwner()->ForceNetUpdate();
	}
	WorldHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ThisClass::Flush);
	ConfigurePhysics();
}
void UHALMovementEffectBridge::Unbind()
{
	if (ASC)
	{
		ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Stun, EGameplayTagEventType::NewOrRemoved).Remove(StunHandle);
		for (const auto& Pair : AttributeHandles) { ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value); }
	}
	FWorldDelegates::OnWorldPostActorTick.Remove(WorldHandle);
	WorldHandle.Reset(); AttributeHandles.Reset(); StunHandle.Reset();
	ASC = nullptr;
	bDirty = false;
	LastPublished = {};
	AreaRecoverySeconds = FVector::ZeroVector;
	if (GetOwner()->HasAuthority() && Epoch.IsValid()) { Epoch.Invalidate(); GetOwner()->ForceNetUpdate(); }
	if (Movement) { Movement->ConfigureMovementEffects({}, History, GetOwner()->HasAuthority(), BoundPlayerId); }
}
void UHALMovementEffectBridge::EndPlay(const EEndPlayReason::Type Reason) { Unbind(); Super::EndPlay(Reason); }
void UHALMovementEffectBridge::AttributeChanged(const FOnAttributeChangeData& Data) { bDirty = true; }
void UHALMovementEffectBridge::StunChanged(FGameplayTag Tag, int32 Count) { bDirty = true; }
void UHALMovementEffectBridge::Flush(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld() || !ASC || ASC->GetAvatarActor() != GetOwner() || !Movement) { return; }
	// Tick-offset readiness belongs to the GT API; the physics thread only reads this atomic flag.
	Movement->SetMovementTickOffsetReady(GetOwner()->HasAuthority() || !History || History->IsNetworkPhysicsTickOffsetAssigned());
	if (!GetOwner()->HasAuthority() || !bDirty) { return; }
	const auto* Values = ASC->GetSet<UHALMovementAttributeSet>();
	FHALMovementSnapshot Snapshot;
	Snapshot.Epoch = Epoch;
	Snapshot.bControlsBlocked = ASC->IsStunned();
	if (!Values) { return; }
	Snapshot.bHasAreaState = true;
	Snapshot.AreaAcceleration.OrdinaryDelta = Values->GetAccelerationMultiplierDelta();
	Snapshot.AreaSpeed.OrdinaryDelta = Values->GetSpeedLimitMultiplierDelta();
	Snapshot.AreaGrip.OrdinaryDelta = Values->GetLateralGripMultiplierDelta();
	Snapshot.AreaAcceleration.Target = 1 + Values->GetAreaAccelerationDelta();
	Snapshot.AreaSpeed.Target = 1 + Values->GetAreaSpeedDelta();
	Snapshot.AreaGrip.Target = 1 + Values->GetAreaGripDelta();
	Snapshot.AreaAcceleration.RecoverySeconds = AreaRecoverySeconds.X;
	Snapshot.AreaSpeed.RecoverySeconds = AreaRecoverySeconds.Y;
	Snapshot.AreaGrip.RecoverySeconds = AreaRecoverySeconds.Z;
	Snapshot.AccelerationScale = Snapshot.AreaAcceleration.EffectiveValue(MAX_int32);
	Snapshot.SpeedScale = Snapshot.AreaSpeed.EffectiveValue(MAX_int32);
	Snapshot.GripScale = Snapshot.AreaGrip.EffectiveValue(MAX_int32);
	const auto SameTarget = [](const FHALMovementAreaChannel& A, const FHALMovementAreaChannel& B)
	{ return A.OrdinaryDelta == B.OrdinaryDelta && A.Target == B.Target; };
	bDirty = false;
	if (LastPublished.IsValidValues() && LastPublished.bControlsBlocked == Snapshot.bControlsBlocked
		&& SameTarget(LastPublished.AreaAcceleration, Snapshot.AreaAcceleration)
		&& SameTarget(LastPublished.AreaSpeed, Snapshot.AreaSpeed) && SameTarget(LastPublished.AreaGrip, Snapshot.AreaGrip))
	{ AreaRecoverySeconds = FVector::ZeroVector; return; }
	if (LastPublished.Revision == MAX_uint32)
	{
		Epoch = FGuid::NewGuid(); Snapshot.Epoch = Epoch; LastPublished = {};
		ConfigurePhysics(); GetOwner()->ForceNetUpdate();
	}
	Snapshot.Revision = LastPublished.Revision + 1;
	if (Movement->QueueMovementParameters(Snapshot)) { LastPublished = Snapshot; AreaRecoverySeconds = FVector::ZeroVector; }
	else { bDirty = true; }
}
