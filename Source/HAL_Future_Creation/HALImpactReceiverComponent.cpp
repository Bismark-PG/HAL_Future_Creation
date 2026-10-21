#include "HALImpactReceiverComponent.h"

#include "HALImpactAttributeSet.h"
#include "HALImpactSourceComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "CombatResolver.h"
#include "VehicleKnockbackSettings.h"
#include "ArcadeVehicleMovementComponent.h"
#include "BallControlComponent.h"
#include "BasicBallActor.h"
#include "AbilitySystemGlobals.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UHALImpactReceiverComponent::UHALImpactReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UHALImpactReceiverComponent::ApplyConfiguration(const FHALVehicleImpactConfig& InConfig)
{
	FString Error;
	if (HasBegunPlay() || !InConfig.Validate(Error)) { return false; }
	Config = InConfig;
	return true;
}

bool UHALImpactReceiverComponent::InitializeStabilityRules()
{
	if (bStabilityInitialized) { return bStabilityValid; }
	bStabilityInitialized = true;
	auto* Shared = GetWorld()->GetSubsystem<UVehicleStabilitySubsystem>();
	if (!Shared || !Shared->InitializeRules()) { return false; }
	StabilityRules = Shared->GetContactRules();
	LocalStabilitySignature = Shared->GetSignature();
	if (auto* Root = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		if (auto* Material = Shared->GetVehicleMaterial()) { Root->SetPhysMaterialOverride(Material); }
	}
	bStabilityValid = true;
	if (GetOwner()->HasAuthority()) { StabilitySignature = LocalStabilitySignature; }
	else { OnRep_StabilitySignature(); }
	return bStabilityValid;
}

void UHALImpactReceiverComponent::OnRep_StabilitySignature()
{
	if (!bStabilityInitialized || !StabilitySignature || StabilitySignature == LocalStabilitySignature) { return; }
	bStabilityValid = false;
	UE_LOG(LogTemp, Error, TEXT("%s: VehicleStabilityRules mismatch local=%u authority=%u. Client vehicle physics blocked; restart with matching configuration."),
		*GetNameSafe(GetOwner()), LocalStabilitySignature, StabilitySignature);
	if (auto* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>()) { Movement->Deactivate(); }
	if (auto* Root = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		Root->SetSimulatePhysics(false); Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool UHALImpactReceiverComponent::CheckConfiguredRules()
{
	if (!InitializeStabilityRules()) { return false; }
	FHALCollisionImpactRules Rules;
	if (!GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules)) { return false; }
	if (Rules.Version == EHALImpactVersion::TieredV1 && Config.Version != Rules.Version)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: TieredV1 rules require migrated VehicleDefinition.Impact. No default fallback."), *GetNameSafe(GetOwner()));
		return false;
	}
	return true;
}

void UHALImpactReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!InitializeStabilityRules()) { return; }
	if (!GetOwner()->HasAuthority()) { return; }
	FHALCollisionImpactRules Rules;
	if (!GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules) || Rules.Version != EHALImpactVersion::TieredV1) { return; }
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		Root->SetNotifyRigidBodyCollision(true);
		Root->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleContact);
	}
	GetWorld()->GetTimerManager().SetTimer(ContactTimer, this, &ThisClass::ExpireContacts, Rules.ContactRearmSeconds, true);
}

void UHALImpactReceiverComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(ContactTimer);
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		Root->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleContact);
	}
	Contacts.Empty();
	Super::EndPlay(Reason);
}

void UHALImpactReceiverComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHALImpactReceiverComponent, LastImpact);
	DOREPLIFETIME(UHALImpactReceiverComponent, StabilitySignature);
}

void UHALImpactReceiverComponent::ExpireContacts()
{
	FHALCollisionImpactRules Rules;
	if (!GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules)) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = Contacts.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now - It.Value().LastSeen > Rules.ContactRearmSeconds) { It.RemoveCurrent(); }
	}
}

void UHALImpactReceiverComponent::SuppressContact(UPrimitiveComponent* OtherBody)
{
	if (!IsValid(OtherBody)) { return; }
	if (!Contacts.Contains(OtherBody) && Contacts.Num() >= 128) { ExpireContacts(); }
	if (!Contacts.Contains(OtherBody) && Contacts.Num() >= 128) { return; }
	FContact& Contact = Contacts.FindOrAdd(OtherBody);
	Contact.LastSeen = GetWorld()->GetTimeSeconds();
	Contact.bResolved = true;
}

void UHALImpactReceiverComponent::HandleContact(UPrimitiveComponent* Body, AActor* Other,
	UPrimitiveComponent* OtherBody, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bStabilityValid || !GetOwner()->HasAuthority() || !IsValid(Other) || Other == GetOwner() || !IsValid(Body)
		|| !IsValid(OtherBody) || OtherBody->GetOwner() != Other || Body != GetOwner()->GetRootComponent() || !Body->IsSimulatingPhysics()
		|| NormalImpulse.ContainsNaN()) { return; }
	FHALCollisionImpactRules Rules;
	if (!GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules)
		|| Rules.Version != EHALImpactVersion::TieredV1 || Config.Version != Rules.Version) { return; }
	// Active attack owns its one response. Controlled balls cannot attack through their constraints.
	if (const ABasicBallActor* Ball = Cast<ABasicBallActor>(Other))
	{
		if (Ball->GetBallState() != EBasicBallState::Free) { return; }
	}
	if (!Contacts.Contains(OtherBody) && Contacts.Num() >= 128) { ExpireContacts(); }
	if (!Contacts.Contains(OtherBody) && Contacts.Num() >= 128) { return; }
	FContact& Contact = Contacts.FindOrAdd(OtherBody);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - Contact.LastSeen > Rules.ContactRearmSeconds) { Contact.bResolved = false; }
	Contact.LastSeen = Now;
	if (Contact.bResolved) { return; }
	const float Mass = Body->GetMass();
	if (!FMath::IsFinite(Mass) || Mass <= SMALL_NUMBER) { return; }
	const float Delta = NormalImpulse.Size() / Mass;
	if (Rules.SelectTier(Delta, EHALImpactTier::Heavy) == EHALImpactTier::None) { return; }
	EHALImpactTier Tier;
	if (const UHALImpactSourceComponent* Profile = Other->FindComponentByClass<UHALImpactSourceComponent>())
	{
		Tier = Profile->Mode == EHALCollisionSourceMode::FixedTier ? Profile->Tier : Rules.SelectTier(Delta, Profile->Tier);
	}
	else if (const UHALImpactReceiverComponent* Vehicle = Other->FindComponentByClass<UHALImpactReceiverComponent>())
	{
		Tier = Rules.SelectTier(Delta, Vehicle->Config.CollisionTierCap);
	}
	else { Tier = Rules.SelectTier(Delta, Other->IsA<ABasicBallActor>() ? Rules.FreeBallTierCap : Rules.EnvironmentTierCap); }
	Contact.bResolved = true; // Including immune/None outcomes: holding contact is not a new strike.
	FHALAttackSourceSnapshot Source;
	Source.SourceActor = Other;
	Source.FixedDamage = 0;
	Source.SourceTag = HALGameplayTags::Combat_Source_Collision;
	FHALCombatAction Action;
	if (!Action.Begin(*GetOwner(), Source)) { return; }
	FHALCombatHitRequest Request;
	Request.AttackId = Action.GetSnapshot().AttackId;
	Request.PulseSequence = Action.GetPulseSequence();
	Request.bApplyDamage = false;
	Request.bApplyImpact = true;
	Request.Impact.Mode = EHALImpactMode::PhysicalContact;
	Request.Impact.Tier = Tier;
	Request.Hit.SourceActor = Other;
	Request.Hit.TargetActor = GetOwner();
	Request.Hit.SourcePhysicsBody = OtherBody;
	Request.Hit.TargetPhysicsBody = Body;
	Request.Hit.NormalImpulse = NormalImpulse;
	Request.Hit.ImpactPoint = Hit.ImpactPoint;
	Request.Hit.ImpactNormal = Hit.ImpactNormal;
	Request.Hit.SourceMass = OtherBody->IsSimulatingPhysics() ? OtherBody->GetMass() : 0;
	FCombatResolver::ResolveCombatHit(Action, Request);
	Action.Close();
}

void UHALImpactReceiverComponent::Resolve(const FHALCombatHitRequest& Request, FHALCombatHitResult& Result)
{
	Result.ImpactOutcome = EHALImpactOutcome::TargetNotReady;
	UHALAbilitySystemComponent* ASC = Cast<UHALAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	const UHALImpactAttributeSet* Attributes = ASC ? ASC->GetSet<UHALImpactAttributeSet>() : nullptr;
	if (!bStabilityValid || !GetOwner()->HasAuthority() || Config.Version != EHALImpactVersion::TieredV1 || !ASC
		|| !ASC->IsImpactInitialized() || ASC->GetAvatarActor() != GetOwner() || !Attributes) { return; }
	Result.ImpactOutcome = EHALImpactOutcome::InvalidMeasurement;
	if (Request.Impact.Mode == EHALImpactMode::LegacyMeasurement || !HALImpact::IsValidTier(Request.Impact.Tier)) { return; }
	const auto& Hit = Request.Hit;
	if (Hit.ImpactNormal.ContainsNaN() || Hit.NormalImpulse.ContainsNaN()) { return; }
	SuppressContact(Hit.SourcePhysicsBody);
	FHALImpactSummary Summary;
	Summary.EventId = Request.AttackId;
	Summary.PulseSequence = Request.PulseSequence;
	Summary.SourceTier = Request.Impact.Tier;
	Summary.bImmune = ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	Summary.EffectiveTier = HALImpact::Reduce(Summary.SourceTier, FMath::FloorToInt(Attributes->GetTierReduction()), Summary.bImmune);
	UArcadeVehicleMovementComponent* Movement = GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	Summary.ServerPhysicsFrame = Movement ? Movement->GetLastPhysicsFrame() : INDEX_NONE;
	// Keep accepted tier diagnostics even when a later physics-body/direction check fails.
	LastImpact = Summary;
	Result.ImpactSummary = Summary;
	GetOwner()->ForceNetUpdate();
	Result.ImpactOutcome = EHALImpactOutcome::Suppressed;
	if (Summary.EffectiveTier != EHALImpactTier::None)
	{
		UPrimitiveComponent* Body = Hit.TargetPhysicsBody;
		if (!IsValid(Body) || Body->GetOwner() != GetOwner()) { Body = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()); }
		FVehicleKnockbackConfig Table;
		FHALCollisionImpactRules Rules;
		Result.ImpactOutcome = EHALImpactOutcome::NoPhysicsBody;
		if (!Body || !Body->IsSimulatingPhysics()) { return; }
		Result.ImpactOutcome = EHALImpactOutcome::RulesUnavailable;
		if (!GetDefault<UVehicleKnockbackSettings>()->GetRuntimeRules(Table)
			|| !GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules)) { return; }
		const float Mass = Body->GetMass();
		if (!FMath::IsFinite(Mass) || Mass <= SMALL_NUMBER) { return; }
		const bool bPhysical = Request.Impact.Mode == EHALImpactMode::PhysicalContact;
		FVector Direction = bPhysical ? Hit.NormalImpulse.GetSafeNormal()
			: FVector::VectorPlaneProject(-Hit.ImpactNormal, FVector::UpVector).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = IsValid(Hit.SourceActor) ? (GetOwner()->GetActorLocation() - Hit.SourceActor->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		}
		Result.ImpactOutcome = EHALImpactOutcome::NoDirection;
		if (Direction.IsNearlyZero()) { return; }
		Result.KnockbackTier = HALImpact::ToLegacy(Summary.EffectiveTier);
		const auto& Tier = Table.GetTierDefinition(Result.KnockbackTier);
		const float Natural = FMath::Max(0.0, FVector::DotProduct(Hit.NormalImpulse, Direction)) / Mass;
		float Supplement = FMath::Max(0.0f, Tier.HorizontalDeltaSpeed - Natural);
		if (bPhysical)
		{
			const FVector OtherVelocity = IsValid(Hit.SourcePhysicsBody) && Hit.SourcePhysicsBody->IsSimulatingPhysics()
				? Hit.SourcePhysicsBody->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint) : FVector::ZeroVector;
			const float RelativeAfter = FVector::DotProduct(Body->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint) - OtherVelocity, Direction);
			const float Ratio = Hit.SourceMass > SMALL_NUMBER ? Mass / Hit.SourceMass : 0;
			Supplement = HALImpact::PhysicalSupplement(Tier.HorizontalDeltaSpeed, Natural, RelativeAfter, Ratio, Rules.MaximumRuleRestitution);
			const auto* OtherBody = Hit.SourcePhysicsBody;
			const auto* Other = Hit.SourceActor;
			Result.NaturalNormalDeltaSpeed = Natural;
			Result.RelativeNormalSpeedAfter = RelativeAfter;
			Result.SupportSourceReason = !StabilityRules.bSuppressSupportSupplement ? FName(TEXT("Disabled"))
				: IsValid(OtherBody) && IsValid(Other) && OtherBody->GetOwner() == Other
				? HALVehicleStability::ClassifySupportSource(OtherBody, Hit.ImpactPoint) : FName(TEXT("InvalidSource"));
			if (StabilityRules.SuppressesSupplement(Result.SupportSourceReason == TEXT("StationaryWorld"), Hit.ImpactNormal, Direction))
			{
				Result.bSupportSupplementSuppressed = true;
				Supplement = 0; // Keep accepted tier, resistance, contact episode and drop below.
			}
		}
		FVehicleImpactAction PhysicsAction;
		PhysicsAction.EventSequence = ++NextEvent;
		if (NextEvent == 0) { PhysicsAction.EventSequence = ++NextEvent; }
		PhysicsAction.DeltaVelocity = Direction * Supplement;
		// Natural contacts never reuse a projectile's launch/flip profile.
		if (!bPhysical && Tier.TargetVerticalSpeed > KINDA_SMALL_NUMBER)
		{
			PhysicsAction.bMinimumUpwardSpeed = true;
			PhysicsAction.MinimumUpwardSpeed = Tier.TargetVerticalSpeed;
			if (Tier.AirborneFlipTurns > 0)
			{
				PhysicsAction.bSetPlanarAngularVelocity = true;
				const float AirTime = 2 * Tier.TargetVerticalSpeed / 980.0f;
				PhysicsAction.PlanarAngularVelocity = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal()
					* FMath::Min(Table.MaxAirborneAngularSpeed, 2 * PI * Tier.AirborneFlipTurns / FMath::Max(0.1f, AirTime));
			}
		}
		bool bQueued = false;
		if (Movement) { bQueued = Movement->QueueImpact(PhysicsAction); }
		else
		{
			Body->AddImpulse(PhysicsAction.ResolveLinearDelta(Body->GetPhysicsLinearVelocity()), NAME_None, true);
			if (PhysicsAction.bSetPlanarAngularVelocity) { Body->SetPhysicsAngularVelocityInRadians(PhysicsAction.ResolveAngularVelocity(Body->GetPhysicsAngularVelocityInRadians())); }
			bQueued = true;
		}
		if (!bQueued) { return; }
		Result.RuleDeltaVelocity = PhysicsAction.DeltaVelocity;
		Result.RuleMinimumUpwardSpeed = PhysicsAction.bMinimumUpwardSpeed ? PhysicsAction.MinimumUpwardSpeed : 0;
		Result.bImpactApplied = true;
		Result.ImpactOutcome = EHALImpactOutcome::Applied;
		if (Config.bDropOnImpact && uint8(Summary.EffectiveTier) >= uint8(Config.MinimumDropTier))
		{
			if (UBallControlComponent* BallControl = GetOwner()->FindComponentByClass<UBallControlComponent>())
			{
				Summary.bBallDropped = BallControl->ReleaseFromImpact(Direction);
			}
		}
	}
	LastImpact = Summary;
	Result.ImpactSummary = Summary;
}
