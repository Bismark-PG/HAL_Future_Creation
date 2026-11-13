#include "HALBattleTopComponent.h"

#include "ArcadeVehicleMovementComponent.h"
#include "BallControlComponent.h"
#include "BasicBallActor.h"
#include "CombatResolver.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HALAbilitySystemComponent.h"
#include "HALAbilityTypes.h"
#include "HALBattleTopAbility.h"
#include "HALElectricWallGimmick.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "HALHealthAttributeSet.h"
#include "HALImpactReceiverComponent.h"
#include "HALVehiclePresentationComponent.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/Archive.h"
#include "TestVehiclePawn.h"
#include "VehicleDefinition.h"
#include "VehicleHealthComponent.h"
#include "VehicleKnockbackSettings.h"
#include "VehicleRecoveryComponent.h"
#include "VehicleStabilityRules.h"

namespace
{
	double BattleTopServerTime(const UWorld& World)
	{
		const auto* State = World.GetGameState();
		return State ? State->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
	}
}

bool FHALBattleTopVisualState::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	Ar << StartServerTime << SpinRate;
	bOutSuccess = !Ar.IsError();
	return true;
}

UHALBattleTopComponent::UHALBattleTopComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

void UHALBattleTopComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHALBattleTopComponent, VisualState);
}

bool UHALBattleTopComponent::CanStart() const
{
	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	if (!Pawn || IsActive() || !Pawn->GetController() || Pawn->GetController()->GetPawn() != Pawn
		|| !Pawn->IsConfigurationValid() || !HALGameFlow::Permissions(GetWorld(), Pawn).bAbilities) { return false; }
	const auto* ASC = Cast<UHALAbilitySystemComponent>(Pawn->GetAbilitySystemComponent());
	const auto* Health = Pawn->FindComponentByClass<UVehicleHealthComponent>();
	const auto* Recovery = Pawn->FindComponentByClass<UVehicleRecoveryComponent>();
	const auto* Movement = Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
	const auto* Receiver = Pawn->FindComponentByClass<UHALImpactReceiverComponent>();
	FHALCollisionImpactRules Rules;
	if (!ASC || ASC->GetAvatarActor() != Pawn || !ASC->IsHealthInitialized() || !ASC->IsImpactInitialized()
		|| !ASC->IsCombatInitialized() || ASC->IsStunned() || ASC->HasMatchingGameplayTag(HALGameplayTags::State_BattleTop)
		|| !Health || !Health->IsHealthReady() || Health->GetCurrentHP() <= 0
		|| !Movement || Movement->IsBounceBackActive() || (Recovery && Recovery->IsExecuting())
		|| !Receiver || Receiver->GetConfiguration().Version != EHALImpactVersion::TieredV1
		|| !GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules)
		|| Rules.Version != EHALImpactVersion::TieredV1) { return false; }
	const auto Pose = Movement->GetStabilityObservation();
	return Pose.bValid && Pose.bGrounded && Pose.bDrivingAllowed && !Pose.Recovery.bActive
		&& FVector::DotProduct(Pose.Up, Pose.GroundNormal) > 0.f;
}

bool UHALBattleTopComponent::Start(UHALAbilitySystemComponent& ASC, UHALBattleTopConfig& InConfig,
	FActiveGameplayEffectHandle Effect)
{
	auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority() || !GetWorld() || IsActive() || ASC.GetAvatarActor() != Pawn || ASC.IsStunned()
		|| !Effect.IsValid() || !ASC.HasMatchingGameplayTag(HALGameplayTags::State_BattleTop)) { return false; }

	FHALAttackSourceSnapshot Source;
	Source.SourceActor = Pawn;
	Source.InstigatorActor = Pawn;
	Source.InstigatorPlayerState = Pawn->GetPlayerState();
	if (const auto* Player = Pawn->GetPlayerState()) { Source.InstigatorPlayerId = Player->GetPlayerId(); }
	Source.FixedDamage = InConfig.ContactDamage;
	Source.SourceTag = HALGameplayTags::Combat_Source_BattleTop;
	Source.bCanHitInstigator = false;
	auto NewAction = MakeShared<FHALCombatAction>();
	if (!NewAction->Begin(*Pawn, Source)) { return false; }

	BoundASC = &ASC;
	ActiveEffect = Effect;
	TagChangedHandle = ASC.RegisterGameplayTagEvent(HALGameplayTags::State_BattleTop, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::ActiveTagChanged);
	HealthChangedHandle = ASC.GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute())
		.AddUObject(this, &ThisClass::HealthChanged);
	Config = &InConfig;
	Action = MoveTemp(NewAction);
	LastWallContact = -1.0;
	VisualState.StartServerTime = BattleTopServerTime(*GetWorld());
	VisualState.SpinRate = InConfig.ResolveSpinRate();
	OnRep_VisualState();
	Pawn->ForceNetUpdate();
	if (auto* BallControl = Pawn->FindComponentByClass<UBallControlComponent>())
	{
		if (IsValid(BallControl->GetHeldBall()) && !BallControl->LaunchBattleTopHeldBall(Config->HeldBallSpeed))
		{
			// Keep the ball and constraint if launch validation failed. The GA rolls back this activation.
			Stop();
			return false;
		}
	}
	UE_CLOG(HALAbilities::ShouldLog(), LogHALAbility, Log, TEXT("BattleTop started Pawn=%s Duration=%.2f"), *GetNameSafe(Pawn), Config->DurationSeconds);
	return true;
}

void UHALBattleTopComponent::ActiveTagChanged(FGameplayTag, int32 Count)
{
	if (Count > 0) { return; }
	ActiveEffect.Invalidate(); // The effect is already being removed by GAS.
	Stop();
}

void UHALBattleTopComponent::HealthChanged(const FOnAttributeChangeData& Change)
{
	if (Change.NewValue <= 0.f) { Stop(); }
}

void UHALBattleTopComponent::Stop()
{
	const bool bWasActive = IsActive();
	VisualState = {};
	OnRep_VisualState();
	Action.Reset(); // A pair already accepted can still finish both sides with its captured references.
	Config = nullptr;
	if (auto* ASC = BoundASC.Get())
	{
		ASC->RegisterGameplayTagEvent(HALGameplayTags::State_BattleTop, EGameplayTagEventType::NewOrRemoved).Remove(TagChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).Remove(HealthChangedHandle);
		if (ActiveEffect.IsValid()) { ASC->RemoveActiveGameplayEffect(ActiveEffect); }
	}
	TagChangedHandle.Reset();
	HealthChangedHandle.Reset();
	ActiveEffect.Invalidate();
	BoundASC.Reset();
	if (bWasActive && GetOwner() && GetOwner()->HasAuthority())
	{
		UE_CLOG(HALAbilities::ShouldLog(), LogHALAbility, Log, TEXT("BattleTop stopped Pawn=%s"), *GetNameSafe(GetOwner()));
		GetOwner()->ForceNetUpdate();
	}
}

void UHALBattleTopComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Stop();
	Super::EndPlay(Reason);
}

bool UHALBattleTopComponent::IsActiveOn(const AActor* Actor)
{
	const auto* Top = Actor ? Actor->FindComponentByClass<UHALBattleTopComponent>() : nullptr;
	return Top && Top->IsActive();
}

bool UHALBattleTopComponent::CanResolveContacts() const
{
	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	const auto* Recovery = Pawn ? Pawn->FindComponentByClass<UVehicleRecoveryComponent>() : nullptr;
	const auto* ASC = BoundASC.Get();
	return Pawn && Pawn->HasAuthority() && IsActive() && Config && Action && ASC && ASC->GetAvatarActor() == Pawn
		&& HALGameFlow::Permissions(GetWorld(), Pawn).bAbilities && (!Recovery || !Recovery->IsExecuting());
}

FHALPreparedCombatHit UHALBattleTopComponent::PrepareVehicleHit(UPrimitiveComponent& Source, UPrimitiveComponent& Target,
	const FVector& Impulse, const FHitResult& Hit)
{
	FHALPreparedCombatHit Pending;
	const auto* Top = Source.GetOwner()->FindComponentByClass<UHALBattleTopComponent>();
	if (!Top || !Top->CanResolveContacts()) { return Pending; }
	Pending.Action = Top->Action;
	auto& Request = Pending.Request;
	Request.AttackId = Pending.Action->GetSnapshot().AttackId;
	Request.PulseSequence = Pending.Action->GetPulseSequence();
	Request.bApplyDamage = true;
	Request.bApplyImpact = true;
	Request.Impact.Mode = EHALImpactMode::ExplicitTier;
	Request.Impact.Tier = Top->Config->ContactTier;
	Request.Impact.PushPreset = Top->Config->ContactPush;
	Request.Hit.TargetActor = Target.GetOwner();
	Request.Hit.TargetPhysicsBody = &Target;
	Request.Hit.SourcePhysicsBody = &Source;
	Request.Hit.ImpactPoint = Hit.ImpactPoint;
	Request.Hit.NormalImpulse = Impulse;
	const FVector Direction = Impulse.GetSafeNormal2D();
	Request.Hit.ImpactNormal = Direction.IsNearlyZero()
		? (Source.GetComponentLocation() - Target.GetComponentLocation()).GetSafeNormal2D() : -Direction;
	return Pending;
}

bool UHALBattleTopComponent::HandleVehiclePair(UPrimitiveComponent& Body, UPrimitiveComponent& OtherBody,
	const FVector& NormalImpulse, const FHitResult& Hit)
{
	auto* Owner = Body.GetOwner();
	auto* Other = OtherBody.GetOwner();
	if (!Owner || !Other || &OtherBody != Other->GetRootComponent()
		|| !Other->FindComponentByClass<UHALImpactReceiverComponent>()) { return false; }
	auto Incoming = PrepareVehicleHit(OtherBody, Body, NormalImpulse, Hit);
	auto Outgoing = PrepareVehicleHit(Body, OtherBody, -NormalImpulse, Hit);
	if (Incoming.Action) { FCombatResolver::ResolveCombatHit(*Incoming.Action, Incoming.Request); }
	if (Outgoing.Action) { FCombatResolver::ResolveCombatHit(*Outgoing.Action, Outgoing.Request); }
	return Incoming.Action.IsValid() || Outgoing.Action.IsValid();
}
bool UHALBattleTopComponent::HandleFreeBall(ABasicBallActor& Ball)
{
	if (!CanResolveContacts() || Ball.GetBallState() != EBasicBallState::Free) { return false; }
	FVector Direction = (Ball.GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero()) { Direction = GetOwner()->GetActorForwardVector().GetSafeNormal2D(); }
	return !Direction.IsNearlyZero() && Ball.LaunchFreeFromBattleTop(GetOwner(), Direction * Config->FreeBallSpeed);
}

FVector UHALBattleTopComponent::CalculateWallDelta(const UHALBattleTopConfig& InConfig, const FVector& Normal,
	const FVector& AfterVelocity, const FVector& NaturalDelta)
{
	const FVector HorizontalNormal = Normal.GetSafeNormal2D();
	const double After = FVector::DotProduct(AfterVelocity, HorizontalNormal);
	const double Incoming = FVector::DotProduct(NaturalDelta, HorizontalNormal) - After;
	if (Incoming < InConfig.WallMinimumIncomingSpeed) { return FVector::ZeroVector; }
	const double Rebound = FMath::Min(double(InConfig.WallMaximumReboundSpeed), Incoming * InConfig.WallReboundScale);
	return HorizontalNormal * FMath::Max(0.0, Rebound - After);
}

bool UHALBattleTopComponent::ResolveWallContact(UPrimitiveComponent& Body, UPrimitiveComponent& Wall,
	const FVector& NormalImpulse, const FHitResult& Hit, const FVehicleContactStabilityRules& Rules, FVector& OutDeltaVelocity)
{
	OutDeltaVelocity = FVector::ZeroVector;
	if (!CanResolveContacts() || !IsValid(Wall.GetOwner()) || Wall.GetOwner()->IsA<AHALElectricWallGimmick>()) { return false; }
	const bool bStationaryWorld = HALVehicleStability::ClassifySupportSource(&Wall, Hit.ImpactPoint) == TEXT("StationaryWorld");
	if (!Rules.IsOrdinaryWall(bStationaryWorld, Hit.ImpactNormal)) { return false; }
	const auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	if (!Movement || Movement->IsBounceBackActive()) { return false; }
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bNewContact = LastWallContact < 0 || Now - LastWallContact >= Config->WallContactRearmSeconds;
	LastWallContact = Now;
	const float Mass = Body.GetMass();
	if (bNewContact && FMath::IsFinite(Mass) && Mass > SMALL_NUMBER)
	{
		const FVector After = Body.GetPhysicsLinearVelocity();
		if (!After.ContainsNaN()) { OutDeltaVelocity = CalculateWallDelta(*Config, Hit.ImpactNormal, After, NormalImpulse / Mass); }
	}
	return true;
}

void UHALBattleTopComponent::OnRep_VisualState()
{
	const bool bDraw = IsActive() && GetNetMode() != NM_DedicatedServer && FApp::CanEverRender();
	SetComponentTickEnabled(bDraw);
	if (!bDraw) { RestoreVisual(); }
}

void UHALBattleTopComponent::RestoreVisual()
{
	if (auto* Mesh = RotatingMesh.Get(); Mesh && Mesh->GetRelativeTransform().Equals(LastVisualTransform))
	{ Mesh->SetRelativeTransform(VisualBase); }
	RotatingMesh.Reset();
}

bool UHALBattleTopComponent::GetUnspunMeshTransform(const UMeshComponent& Mesh, FTransform& Out) const
{
	const auto* Parent = Mesh.GetAttachParent();
	if (RotatingMesh.Get() != &Mesh || !Parent || !Mesh.GetRelativeTransform().Equals(LastVisualTransform)) { return false; }
	Out = VisualBase * Parent->GetComponentTransform();
	return true;
}

void UHALBattleTopComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	const auto* Presentation = GetOwner()->FindComponentByClass<UHALVehiclePresentationComponent>();
	if (!IsActive() || !GetWorld() || !Presentation) { RestoreVisual(); return; }
	FHALVehicleFXTargetSnapshot Snapshot;
	Presentation->CaptureStatusFXTargets(Snapshot);
	auto* Mesh = !Snapshot.Targets.IsEmpty() && Snapshot.Targets[0].Id == TEXT("Body") ? Snapshot.Targets[0].Mesh.Get() : nullptr;
	if (Mesh != RotatingMesh.Get())
	{
		RestoreVisual();
		RotatingMesh = Mesh;
		if (Mesh) { VisualBase = LastVisualTransform = Mesh->GetRelativeTransform(); }
	}
	if (!Mesh) { return; }
	if (!Mesh->GetRelativeTransform().Equals(LastVisualTransform)) { VisualBase = Mesh->GetRelativeTransform(); }
	const double Seconds = FMath::Max(0.0, BattleTopServerTime(*GetWorld()) - VisualState.StartServerTime);
	const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(FMath::Fmod(Seconds * VisualState.SpinRate, 360.0)));
	FTransform Animated = VisualBase;
	Animated.SetRotation(Spin * VisualBase.GetRotation());
	Animated.SetLocation(Spin.RotateVector(VisualBase.GetLocation()));
	Mesh->SetRelativeTransform(Animated);
	LastVisualTransform = Mesh->GetRelativeTransform();
}
