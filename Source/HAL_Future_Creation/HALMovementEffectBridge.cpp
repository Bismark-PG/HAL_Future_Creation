#include "HALMovementEffectBridge.h"
#include "HALAbilitySystemComponent.h"
#include "HALMovementAttributeSet.h"
#include "ArcadeVehicleMovementComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UHALMovementEffectBridge::UHALMovementEffectBridge()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
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
		AccelerationHandle = ASC->GetGameplayAttributeValueChangeDelegate(UHALMovementAttributeSet::GetAccelerationMultiplierDeltaAttribute()).AddUObject(this, &ThisClass::AttributeChanged);
		SpeedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UHALMovementAttributeSet::GetSpeedLimitMultiplierDeltaAttribute()).AddUObject(this, &ThisClass::AttributeChanged);
		bDirty = true;
		GetOwner()->ForceNetUpdate();
	}
	WorldHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ThisClass::Flush);
	ConfigurePhysics();
}
void UHALMovementEffectBridge::Unbind()
{
	if (ASC)
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UHALMovementAttributeSet::GetAccelerationMultiplierDeltaAttribute()).Remove(AccelerationHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UHALMovementAttributeSet::GetSpeedLimitMultiplierDeltaAttribute()).Remove(SpeedHandle);
	}
	FWorldDelegates::OnWorldPostActorTick.Remove(WorldHandle);
	WorldHandle.Reset(); AccelerationHandle.Reset(); SpeedHandle.Reset();
	ASC = nullptr;
	bDirty = false;
	LastPublished = {};
	if (GetOwner()->HasAuthority() && Epoch.IsValid()) { Epoch.Invalidate(); GetOwner()->ForceNetUpdate(); }
	if (Movement) { Movement->ConfigureMovementEffects({}, History, GetOwner()->HasAuthority(), BoundPlayerId); }
}
void UHALMovementEffectBridge::EndPlay(const EEndPlayReason::Type Reason) { Unbind(); Super::EndPlay(Reason); }
void UHALMovementEffectBridge::AttributeChanged(const FOnAttributeChangeData& Data) { bDirty = true; }
void UHALMovementEffectBridge::Flush(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld() || !ASC || ASC->GetAvatarActor() != GetOwner() || !Movement) { return; }
	// Tick-offset readiness belongs to the GT API; the physics thread only reads this atomic flag.
	Movement->SetMovementTickOffsetReady(GetOwner()->HasAuthority() || !History || History->IsNetworkPhysicsTickOffsetAssigned());
	if (!GetOwner()->HasAuthority() || !bDirty) { return; }
	const auto* Values = ASC->GetSet<UHALMovementAttributeSet>();
	FHALMovementSnapshot Snapshot;
	Snapshot.Epoch = Epoch;
	if (!Values || !FHALMovementSnapshot::CalculateScales(Values->GetAccelerationMultiplierDelta(), Values->GetSpeedLimitMultiplierDelta(), Snapshot.AccelerationScale, Snapshot.SpeedScale)) { return; }
	bDirty = false;
	if (LastPublished.IsValidValues() && LastPublished.AccelerationScale == Snapshot.AccelerationScale && LastPublished.SpeedScale == Snapshot.SpeedScale) { return; }
	if (LastPublished.Revision == MAX_uint32)
	{
		Epoch = FGuid::NewGuid(); Snapshot.Epoch = Epoch; LastPublished = {};
		ConfigurePhysics(); GetOwner()->ForceNetUpdate();
	}
	Snapshot.Revision = LastPublished.Revision + 1;
	if (Movement->QueueMovementParameters(Snapshot)) { LastPublished = Snapshot; }
	else { bDirty = true; }
}
