#include "HALBullChargeComponent.h"
#include "HALBullChargeAbility.h"
#include "HALBattleTopComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALHealthAttributeSet.h"
#include "HALMovementEffectBridge.h"
#include "HALGameplayTags.h"
#include "HALGameFlowPermissions.h"
#include "HALAbilityTypes.h"
#include "ArcadeVehicleMovementComponent.h"
#include "BallControlComponent.h"
#include "BasicBallActor.h"
#include "CombatResolver.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "HALImpactReceiverComponent.h"
#include "VehicleRecoveryComponent.h"
#include "VehicleKnockbackSettings.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "EventManager.h"
#include "EventsData.h"

UHALBullChargeComponent::UHALBullChargeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}
void UHALBullChargeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHALBullChargeComponent, bActive);
}
void UHALBullChargeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority() && GetWorld()->GetPhysicsScene())
	{
		GetWorld()->GetPhysicsScene()->GetSolver()->GetEventManager()->RegisterHandler<Chaos::FCollisionEventData>(
			Chaos::EEventType::Collision, this, &ThisClass::CollisionEvents);
		bCollisionRegistered = true;
	}
}
void UHALBullChargeComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Stop();
	if (bCollisionRegistered && GetWorld() && GetWorld()->GetPhysicsScene())
	{
		GetWorld()->GetPhysicsScene()->GetSolver()->GetEventManager()->UnregisterHandler(Chaos::EEventType::Collision, this);
	}
	bCollisionRegistered = false;
	Super::EndPlay(Reason);
}
void UHALBullChargeComponent::CollisionEvents(const Chaos::FCollisionEventData& Event)
{
	if (!bActive || Event.CollisionData.TimeCreated <= LastCollisionBatch) { return; }
	LastCollisionBatch = Event.CollisionData.TimeCreated;
	ContactSamples.Reset();
	const auto* Scene = GetWorld()->GetPhysicsScene();
	// Engine queues OnComponentHit from this same buffer and dispatches after event handlers.
	// Velocity is post-solve; DeltaVelocity is V - PreV in UE EventDefaults.cpp.
	for (const auto& Data : Event.CollisionData.AllCollisionsArray)
	{
		if (Data.bProbe || !Data.Proxy1 || !Data.Proxy2) { continue; }
		auto* First = Scene->GetOwningComponent<UPrimitiveComponent>(Data.Proxy1);
		auto* Second = Scene->GetOwningComponent<UPrimitiveComponent>(Data.Proxy2);
		const bool bFirst = First && First->GetOwner() == GetOwner();
		const bool bSecond = Second && Second->GetOwner() == GetOwner();
		if ((!bFirst && !bSecond) || !First || !Second) { continue; }
		auto* Other = bFirst ? Second : First;
		const auto* Existing = ContactSamples.Find(Other);
		if (Existing && Existing->SolverTime <= Data.SolverTime) { continue; }
		auto& Sample = ContactSamples.FindOrAdd(Other);
		Sample.BeforeVelocity = bFirst ? Data.Velocity1 - Data.DeltaVelocity1 : Data.Velocity2 - Data.DeltaVelocity2;
		Sample.OtherBeforeVelocity = bFirst ? Data.Velocity2 - Data.DeltaVelocity2 : Data.Velocity1 - Data.DeltaVelocity1;
		Sample.SolverTime = Data.SolverTime;
		Sample.GameFrame = GFrameCounter;
	}
}
bool UHALBullChargeComponent::CanStart() const
{
	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	const auto* Movement = Pawn ? Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
	const auto* Health = Pawn ? Pawn->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	const auto* ASC = Health ? Health->GetBoundAbilitySystem() : nullptr;
	const auto* Receiver = Pawn ? Pawn->FindComponentByClass<UHALImpactReceiverComponent>() : nullptr;
	const auto* Recovery = Pawn ? Pawn->FindComponentByClass<UVehicleRecoveryComponent>() : nullptr;
	FHALCollisionImpactRules Rules;
	// Also used by the owner's GAS request preflight. Authority is enforced in Start and contact resolution.
	if (!Pawn || !Pawn->GetController() || Pawn->GetController()->GetPawn() != Pawn || !Pawn->IsConfigurationValid()
		|| !ASC || ASC->GetAvatarActor() != Pawn || !ASC->IsHealthInitialized() || !ASC->IsImpactInitialized()
		|| !ASC->IsCombatInitialized() || ASC->GetCombatConfigVersion() != EHALCombatConfigVersion::AttackSnapshotV1
		|| Health->GetCurrentHP() <= 0 || ASC->IsStunned() || IsActive() || UHALBattleTopComponent::IsActiveOn(Pawn)
		|| !HALGameFlow::Permissions(GetWorld(), Pawn).bAbilities || !Movement || Movement->IsBounceBackActive()
		|| (Recovery && Recovery->IsExecuting()) || !Receiver || Receiver->GetConfiguration().Version != EHALImpactVersion::TieredV1
		|| !GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules) || Rules.Version != EHALImpactVersion::TieredV1) { return false; }
	const auto Pose = Movement->GetStabilityObservation();
	return Pose.bValid && Pose.bGrounded && Pose.bDrivingAllowed && !Pose.Recovery.bActive
		&& FVector::DotProduct(Pose.Up, Pose.GroundNormal) > 0;
}
bool UHALBullChargeComponent::Start(UHALAbilitySystemComponent& ASC, UHALBullChargeConfig& InConfig, FActiveGameplayEffectHandle Effect)
{
	auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	auto* Bridge = Pawn ? Pawn->FindComponentByClass<UHALMovementEffectBridge>() : nullptr;
	FString Error;
	if (!Pawn || !Pawn->HasAuthority() || bActive || !Bridge || ASC.GetAvatarActor() != Pawn || !Effect.IsValid()
		|| !ASC.HasMatchingGameplayTag(HALGameplayTags::State_BullCharge) || !InConfig.Validate(Error)
		|| !FCombatResolver::CaptureVehicleAttack(*Pawn, *Pawn, 1.f, 0.f, AttackSnapshot)) { return false; }
	AttackSnapshot.SourceTag = HALGameplayTags::Combat_Source_BullCharge;
	AttackSnapshot.bCanHitInstigator = false;
	BoundASC = &ASC;
	Config = &InConfig;
	ActiveEffect = Effect;
	bActive = true;
	NextTargetHit.Reset();
	ContactSamples.Reset();
	TagChangedHandle = ASC.RegisterGameplayTagEvent(HALGameplayTags::State_BullCharge, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::ActiveTagChanged);
	HealthChangedHandle = ASC.GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute())
		.AddUObject(this, &ThisClass::HealthChanged);
	FHALOverdriveMotion Motion = Config->Motion;
	Motion.Activation = FGuid::NewGuid();
	Motion.bActive = true;
	if (auto* Balls = Pawn->FindComponentByClass<UBallControlComponent>(); Balls && IsValid(Balls->GetHeldBall()))
	{
		const auto* Root = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
		const float Speed = Root ? FVector::DotProduct(Root->GetPhysicsLinearVelocity(), Root->GetForwardVector().GetSafeNormal2D()) : 0;
		if (!Balls->LaunchSkillHeldBall(Config->BallSpeed(Speed))) { Stop(); return false; }
	}
	// Commit motion only after the held-ball transaction succeeds. A rejected launch
	// must retain the previous motion/recovery rather than publish a cancelled charge.
	Bridge->SetOverdriveMotion(Motion);
	Pawn->ForceNetUpdate();
	UE_CLOG(HALAbilities::ShouldLog(), LogHALAbility, Log, TEXT("BullCharge started Pawn=%s Duration=%.2f Ramp=%.2f TargetCD=%.2f"),
		*GetNameSafe(Pawn), Motion.DurationSeconds, Motion.RampSeconds, Config->TargetCooldownSeconds);
	return true;
}
void UHALBullChargeComponent::ActiveTagChanged(FGameplayTag, int32 Count)
{
	if (Count == 0) { ActiveEffect.Invalidate(); Stop(); }
}
void UHALBullChargeComponent::HealthChanged(const FOnAttributeChangeData& Change)
{
	if (Change.NewValue <= 0) { Stop(); }
}
void UHALBullChargeComponent::Stop()
{
	if (!bActive || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	bActive = false;
	if (auto* Bridge = GetOwner()->FindComponentByClass<UHALMovementEffectBridge>())
	{
		// The bridge retains the activation tuning so physics can stamp early exit and recover.
		Bridge->EndOverdriveMotion();
	}
	if (auto* ASC = BoundASC.Get())
	{
		ASC->RegisterGameplayTagEvent(HALGameplayTags::State_BullCharge, EGameplayTagEventType::NewOrRemoved).Remove(TagChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).Remove(HealthChangedHandle);
		if (ActiveEffect.IsValid()) { ASC->RemoveActiveGameplayEffect(ActiveEffect); }
	}
	TagChangedHandle.Reset(); HealthChangedHandle.Reset(); ActiveEffect.Invalidate(); BoundASC.Reset();
	NextTargetHit.Reset(); ContactSamples.Reset(); Config = nullptr;
	GetOwner()->ForceNetUpdate();
	UE_CLOG(HALAbilities::ShouldLog(), LogHALAbility, Log, TEXT("BullCharge stopped Pawn=%s"), *GetNameSafe(GetOwner()));
}
bool UHALBullChargeComponent::IsActiveOn(const AActor* Actor)
{
	const auto* Charge = Actor ? Actor->FindComponentByClass<UHALBullChargeComponent>() : nullptr;
	return Charge && Charge->IsActive();
}
bool UHALBullChargeComponent::CanResolveContacts() const
{
	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	const auto* ASC = BoundASC.Get();
	const auto* Movement = Pawn ? Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>() : nullptr;
	if (!Pawn || !Pawn->HasAuthority() || !bActive || !Config || !ASC || ASC->GetAvatarActor() != Pawn
		|| ASC->IsStunned() || !Movement || !HALGameFlow::Permissions(GetWorld(), Pawn).bAbilities) { return false; }
	const auto Pose = Movement->GetStabilityObservation();
	const auto Status = Movement->GetMovementEffectStatus();
	return Pose.bValid && Pose.bGrounded && Pose.bDrivingAllowed && !Pose.Recovery.bActive
		&& Status.bReady && Status.Definition.Overdrive.IsDriving(Status.ServerFrame);
}
FHALPreparedCombatHit UHALBullChargeComponent::PrepareVehicleHit(UPrimitiveComponent& Source, UPrimitiveComponent& Target,
	const FVector& NormalImpulse, const FHitResult& Hit)
{
	FHALPreparedCombatHit Pending;
	const auto* Sample = ContactSamples.Find(&Target);
	if (!CanResolveContacts() || !Sample || Sample->GameFrame != GFrameCounter) { return Pending; }
	const auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	const auto Before = Movement->GetContactObservation();
	float Speed;
	if (!Before.bValid || !Config->Qualifies(Before.Rotation.GetForwardVector(), Hit.ImpactPoint - Before.Position,
		-Hit.ImpactNormal, Sample->BeforeVelocity, Sample->OtherBeforeVelocity, Speed)) { return Pending; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (const auto* Next = NextTargetHit.Find(Target.GetOwner()); Next && Now < *Next) { return Pending; }
	for (auto It = NextTargetHit.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } }
	if (NextTargetHit.Num() >= 256 && !NextTargetHit.Contains(Target.GetOwner())) { return Pending; }
	FHALAttackSourceSnapshot Snapshot = AttackSnapshot;
	Snapshot.ActionMultiplier = Config->DamageMultiplier(Speed);
	Pending.Action = MakeShared<FHALCombatAction>();
	if (!Pending.Action->Begin(*GetOwner(), Snapshot)) { Pending.Action.Reset(); return Pending; }
	NextTargetHit.Add(Target.GetOwner(), Now + Config->TargetCooldownSeconds);
	auto& Request = Pending.Request;
	Request.AttackId = Pending.Action->GetSnapshot().AttackId;
	Request.PulseSequence = Pending.Action->GetPulseSequence();
	Request.bApplyDamage = Request.bApplyImpact = true;
	Request.Impact.Mode = EHALImpactMode::ExplicitTier;
	Request.Impact.Tier = Config->ContactTier;
	Request.Impact.PushPreset = Config->ContactPush;
	Request.Hit.TargetActor = Target.GetOwner();
	Request.Hit.SourcePhysicsBody = &Source;
	Request.Hit.TargetPhysicsBody = &Target;
	Request.Hit.SourceVelocity = Sample->BeforeVelocity;
	Request.Hit.TargetVelocityAtImpactPoint = Sample->OtherBeforeVelocity;
	Request.Hit.SourceMass = Source.GetMass();
	Request.Hit.ImpactPoint = Hit.ImpactPoint;
	Request.Hit.ImpactNormal = Hit.ImpactNormal;
	Request.Hit.NormalImpulse = NormalImpulse;
	UE_CLOG(HALAbilities::ShouldLog(), LogHALAbility, Log, TEXT("BullCharge contact Pawn=%s Target=%s PreSpeed=%.2f Multiplier=%.3f TargetCD=%.2f"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Target.GetOwner()), Speed, Snapshot.ActionMultiplier, Config->TargetCooldownSeconds);
	return Pending;
}
bool UHALBullChargeComponent::HandleVehiclePair(UPrimitiveComponent& Body, UPrimitiveComponent& Other,
	const FVector& NormalImpulse, const FHitResult& Hit)
{
	if (!IsActiveOn(Body.GetOwner()) && !IsActiveOn(Other.GetOwner())) { return false; }
	if (&Body != Body.GetOwner()->GetRootComponent() || &Other != Other.GetOwner()->GetRootComponent()
		|| !Other.GetOwner()->FindComponentByClass<UHALImpactReceiverComponent>()) { return false; }
	const auto Prepare = [](UPrimitiveComponent& Source, UPrimitiveComponent& Target, const FVector& Impulse, const FHitResult& Contact)
	{
		auto* Charge = Source.GetOwner()->FindComponentByClass<UHALBullChargeComponent>();
		return Charge && Charge->IsActive() ? Charge->PrepareVehicleHit(Source, Target, Impulse, Contact)
			: UHALBattleTopComponent::PrepareVehicleHit(Source, Target, Impulse, Contact);
	};
	FHitResult Reverse = Hit;
	Reverse.ImpactNormal = -Hit.ImpactNormal;
	auto Incoming = Prepare(Other, Body, NormalImpulse, Reverse);
	auto Outgoing = Prepare(Body, Other, -NormalImpulse, Hit);
	if (Incoming.Action) { FCombatResolver::ResolveCombatHit(*Incoming.Action, Incoming.Request); }
	if (Outgoing.Action) { FCombatResolver::ResolveCombatHit(*Outgoing.Action, Outgoing.Request); }
	return Incoming.Action.IsValid() || Outgoing.Action.IsValid();
}
bool UHALBullChargeComponent::HandleFreeBall(ABasicBallActor& Ball, const FHitResult& Hit)
{
	if (!CanResolveContacts() || Ball.GetBallState() != EBasicBallState::Free) { return false; }
	const auto* Sample = ContactSamples.Find(Cast<UPrimitiveComponent>(Ball.GetRootComponent()));
	const auto Before = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetContactObservation();
	float Speed;
	if (!Sample || Sample->GameFrame != GFrameCounter || !Before.bValid
		|| !Config->Qualifies(Before.Rotation.GetForwardVector(), Hit.ImpactPoint - Before.Position,
			-Hit.ImpactNormal, Sample->BeforeVelocity, Sample->OtherBeforeVelocity, Speed)) { return false; }
	return Ball.LaunchFreeFromVehicleSkill(GetOwner(), Before.Rotation.GetForwardVector().GetSafeNormal2D() * Config->BallSpeed(Speed));
}
