// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatResolver.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "VehicleHealthComponent.h"
#include "VehicleDefinition.h"
#include "VehicleConfigurationApplication.h"
#include "AbilitySystemGlobals.h"
#include "HALAbilitySystemComponent.h"
#include "HALCombatAttributeSet.h"
#include "HALHealthAttributeSet.h"
#include "HALApplyDamageEffect.h"
#include "HALCombatEffectContext.h"
#include "HALGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarHALCombatLog(TEXT("hal.CombatLog"), 0,
	TEXT("Log accepted authority combat results (attack/pulse/source/raw/resolved/actual/impact). 0=off, 1=on."));

namespace
{
	FVector GetPushDirection(const FVehicleHitContext& Hit)
	{
		FVector PushDirection = FVector::VectorPlaneProject(-Hit.ImpactNormal, FVector::UpVector).GetSafeNormal();
		if (PushDirection.IsNearlyZero() && IsValid(Hit.SourceActor))
		{
			PushDirection = FVector::VectorPlaneProject(
				Hit.TargetActor->GetActorLocation() - Hit.SourceActor->GetActorLocation(),
				FVector::UpVector).GetSafeNormal();
		}
		return PushDirection;
	}

	float CalculateImpactSpeed(
		const FVehicleHitContext& Hit,
		const FVector& PushDirection,
		const float StrengthMultiplier)
	{
		const FVector RelativeVelocity = Hit.SourceVelocity - Hit.TargetVelocityAtImpactPoint;
		const float RelativeNormalSpeed = FMath::Abs(FVector::DotProduct(RelativeVelocity, PushDirection));
		const float ImpulseEquivalentSpeed = Hit.SourceMass > KINDA_SMALL_NUMBER
			? Hit.NormalImpulse.Size() / Hit.SourceMass
			: 0.0f;
		return FMath::Max(RelativeNormalSpeed, ImpulseEquivalentSpeed)
			* FMath::Max(0.0f, StrengthMultiplier);
	}

	bool IsTargetGrounded(
		const UPrimitiveComponent& Body,
		const FVehicleHitContext& Hit,
		const float ExtraDistance)
	{
		const UWorld* World = Body.GetWorld();
		if (!World)
		{
			return false;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleKnockbackGroundProbe), false);
		QueryParams.AddIgnoredActor(Hit.TargetActor);
		if (IsValid(Hit.SourceActor)) { QueryParams.AddIgnoredActor(Hit.SourceActor); }
		const FVector Start = Body.Bounds.Origin;
		const float TraceDistance = Body.Bounds.BoxExtent.Z + FMath::Max(0.0f, ExtraDistance);
		return World->LineTraceTestByChannel(
			Start,
			Start - (FVector::UpVector * TraceDistance),
			ECC_Visibility,
			QueryParams);
	}

	void ApplyTierOutcome(
		UPrimitiveComponent& Body,
		const FVector& PushDirection,
		const FVehicleKnockbackTierDefinition& Tier,
		const float MaxAngularSpeed,
		const bool bTargetGrounded)
	{
		const FVector UpDirection = FVector::UpVector;
		const FVector CurrentVelocity = Body.GetPhysicsLinearVelocity();
		const float CurrentVerticalSpeed = FVector::DotProduct(CurrentVelocity, UpDirection);
		float VerticalDeltaSpeed = FMath::Max(
			0.0f,
			FMath::Max(0.0f, Tier.TargetVerticalSpeed) - CurrentVerticalSpeed);
		if (bTargetGrounded && Tier.TargetVerticalSpeed <= KINDA_SMALL_NUMBER)
		{
			VerticalDeltaSpeed = -FMath::Max(0.0f, CurrentVerticalSpeed);
		}
		const FVector VelocityChange =
			(PushDirection * FMath::Max(0.0f, Tier.HorizontalDeltaSpeed))
			+ (UpDirection * VerticalDeltaSpeed);
		if (!VelocityChange.IsNearlyZero())
		{
			Body.AddImpulse(VelocityChange, NAME_None, true);
		}

		const FVector CurrentAngularVelocity = Body.GetPhysicsAngularVelocityInRadians();
		const float PreservedYawSpeed = FVector::DotProduct(CurrentAngularVelocity, UpDirection);
		FVector DesiredAngularVelocity = UpDirection * PreservedYawSpeed;
		bool bApplyAuthoredRotation = bTargetGrounded && Tier.TargetVerticalSpeed <= KINDA_SMALL_NUMBER;

		if (Tier.AirborneFlipTurns > 0.0f && Tier.TargetVerticalSpeed > KINDA_SMALL_NUMBER)
		{
			bApplyAuthoredRotation = true;
			const float ExpectedAirTime = (2.0f * Tier.TargetVerticalSpeed) / 980.0f;
			const float RequestedFlipSpeed =
				(2.0f * PI * Tier.AirborneFlipTurns) / FMath::Max(0.1f, ExpectedAirTime);
			const float FlipSpeed = FMath::Min(FMath::Max(0.0f, MaxAngularSpeed), RequestedFlipSpeed);
			const FVector FlipAxis = FVector::CrossProduct(UpDirection, PushDirection).GetSafeNormal();
			DesiredAngularVelocity += FlipAxis * FlipSpeed;
		}

		if (bApplyAuthoredRotation)
		{
			Body.AddAngularImpulseInRadians(
				DesiredAngularVelocity - CurrentAngularVelocity,
				NAME_None,
				true);
		}
	}
}

bool FCombatResolver::CaptureVehicleAttack(AActor& Source, AActor& Instigator, float ActionMultiplier,
	float ActionMultiplierDelta, FHALAttackSourceSnapshot& OutSnapshot)
{
	UHALAbilitySystemComponent* ASC = Cast<UHALAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Instigator));
	const UHALCombatAttributeSet* Attributes = ASC ? ASC->GetSet<UHALCombatAttributeSet>() : nullptr;
	if (!Source.HasAuthority() || !Instigator.HasAuthority() || Source.GetWorld() != Instigator.GetWorld()
		|| !VehicleConfiguration::IsReadyForGameplay(&Source) || !VehicleConfiguration::IsReadyForGameplay(&Instigator)
		|| !ASC || !ASC->IsCombatInitialized() || !ASC->IsHealthInitialized()
		|| ASC->GetAvatarActor() != &Instigator || !Attributes
		|| ASC->GetCombatConfigVersion() != EHALCombatConfigVersion::AttackSnapshotV1) { return false; }
	FHALAttackSourceSnapshot Snapshot;
	Snapshot.DamageMode = EHALDamageMode::VehicleAttackScaled;
	Snapshot.SourceActor = &Source;
	Snapshot.InstigatorActor = &Instigator;
	if (const APawn* Pawn = Cast<APawn>(&Instigator))
	{
		Snapshot.InstigatorPlayerState = Pawn->GetPlayerState();
		if (const APlayerState* State = Pawn->GetPlayerState()) { Snapshot.InstigatorPlayerId = State->GetPlayerId(); }
	}
	Snapshot.CapturedAttackPower = Attributes->GetAttackPower();
	Snapshot.ActionMultiplier = ActionMultiplier;
	Snapshot.MultiplierDeltaSum = Attributes->GetOutputMultiplierDelta() + ActionMultiplierDelta;
	float RawDamage;
	if (!Snapshot.CalculateRawDamage(RawDamage)) { return false; }
	OutSnapshot = Snapshot;
	return true;
}

FHALCombatHitResult FCombatResolver::ResolveCombatHit(FHALCombatAction& Action, const FHALCombatHitRequest& Request)
{
	FHALCombatHitResult Result;
	Result.AttackId = Request.AttackId;
	Result.PulseSequence = Request.PulseSequence;
	const FHALAttackSourceSnapshot& Source = Action.GetSnapshot();
	Result.InstigatorPlayerId = Source.InstigatorPlayerId;
	Result.RejectReason = Action.CheckRequest(Request);
	if (Result.RejectReason != EHALHitRejectReason::None) { return Result; }
	AActor* Target = Request.Hit.TargetActor;
	if (!VehicleConfiguration::IsReadyForGameplay(Target))
	{
		Result.RejectReason = EHALHitRejectReason::TargetNotReady;
		return Result;
	}
	const APawn* TargetPawn = Cast<APawn>(Target);
	const APlayerState* TargetState = TargetPawn ? TargetPawn->GetPlayerState() : nullptr;
	if (!Source.bCanHitInstigator && (Target == Source.InstigatorActor.Get()
		|| (Source.InstigatorPlayerId != INDEX_NONE && TargetState && TargetState->GetPlayerId() == Source.InstigatorPlayerId)))
	{
		Result.RejectReason = EHALHitRejectReason::ExcludedInstigator;
		return Result;
	}

	UHALAbilitySystemComponent* ASC = Cast<UHALAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target));
	const UHALHealthAttributeSet* Health = ASC ? ASC->GetSet<UHALHealthAttributeSet>() : nullptr;
	const UHALCombatAttributeSet* Combat = ASC ? ASC->GetSet<UHALCombatAttributeSet>() : nullptr;
	FGameplayEffectSpecHandle DamageSpec;
	float PreviousHP = 0.0f;
	if (Request.bApplyDamage)
	{
		if (!ASC || !Health || !Combat || !ASC->IsHealthInitialized() || !ASC->IsCombatInitialized()
			|| ASC->GetAvatarActor() != Target || Health->GetMaxHP() < 1.0f)
		{
			Result.RejectReason = EHALHitRejectReason::TargetNotReady;
			return Result;
		}
		if (!Source.CalculateRawDamage(Result.RawDamage) || !FMath::IsFinite(Combat->GetDamageTakenMultiplier())
			|| !FMath::IsFinite(Health->GetCurrentHP()))
		{
			Result.RejectReason = EHALHitRejectReason::InvalidDamage;
			return Result;
		}
		Result.bDamageImmune = ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_DamageImmune);
		const double Resolved = Result.bDamageImmune ? 0.0
			: double(Result.RawDamage) * FMath::Max(0.0f, Combat->GetDamageTakenMultiplier());
		if (!FMath::IsFinite(Resolved) || Resolved > MAX_flt)
		{
			Result.RejectReason = EHALHitRejectReason::InvalidDamage;
			return Result;
		}
		Result.ResolvedDamage = float(Resolved);
		PreviousHP = Health->GetCurrentHP();
		if (Result.ResolvedDamage > 0.0f && PreviousHP > 0.0f)
		{
			auto* StableContext = new FHALCombatEffectContext();
			StableContext->AttackId = Source.AttackId;
			StableContext->PulseSequence = Request.PulseSequence;
			StableContext->InstigatorPlayerId = Source.InstigatorPlayerId;
			StableContext->SourceTag = Source.SourceTag;
			StableContext->EffectTags = Source.EffectTags;
			AActor* Instigator = Source.InstigatorPlayerState.IsValid()
				? static_cast<AActor*>(Source.InstigatorPlayerState.Get()) : Source.InstigatorActor.Get();
			StableContext->AddInstigator(Instigator, Source.SourceActor.Get());
			StableContext->AddSourceObject(Source.SourceActor.Get());
			FGameplayEffectContextHandle Context(StableContext);
			DamageSpec = ASC->MakeOutgoingSpec(UHALApplyDamageEffect::StaticClass(), 1.0f, Context);
			if (!DamageSpec.IsValid())
			{
				Result.RejectReason = EHALHitRejectReason::EffectUnavailable;
				return Result;
			}
			DamageSpec.Data->SetSetByCallerMagnitude(HALGameplayTags::Data_Combat_Damage, -Result.ResolvedDamage);
		}
	}
	// Consume before notifications/physics: reentrant callbacks cannot resolve this target twice.
	Action.ConsumeTarget(Target);
	Result.bHitAccepted = true;
	if (DamageSpec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
		Result.ActualHPRemoved = FMath::Max(0.0f, PreviousHP - Health->GetCurrentHP());
		ASC->GetOwner()->ForceNetUpdate();
	}

	// This branch never reads HP, damage amount or damage immunity.
	FVehicleHitContext Hit = Request.Hit;
	Hit.SourceActor = Source.SourceActor.Get();
	Hit.InstigatorActor = Source.InstigatorActor.Get();
	Hit.InstigatorPlayerId = Source.InstigatorPlayerId;
	UPrimitiveComponent* Body = Hit.TargetPhysicsBody;
	if (!IsValid(Body) || Body->GetOwner() != Hit.TargetActor || !Body->IsSimulatingPhysics())
	{
		Body = Cast<UPrimitiveComponent>(Hit.TargetActor->GetRootComponent());
	}
	if (Request.bApplyImpact)
	{
		Result.ImpactOutcome = EHALImpactOutcome::NoPhysicsBody;
		if (IsValid(Body) && Body->IsSimulatingPhysics())
		{
			Result.ImpactOutcome = EHALImpactOutcome::InvalidMeasurement;
			if (!Hit.ImpactNormal.ContainsNaN() && !Hit.NormalImpulse.ContainsNaN() && !Hit.SourceVelocity.ContainsNaN()
				&& !Hit.TargetVelocityAtImpactPoint.ContainsNaN() && FMath::IsFinite(Hit.SourceMass) && Hit.SourceMass >= 0.0f
				&& FMath::IsFinite(Request.KnockbackStrengthMultiplier) && Request.KnockbackStrengthMultiplier >= 0.0f)
			{
				const FVector PushDirection = GetPushDirection(Hit);
				Result.ImpactOutcome = EHALImpactOutcome::NoDirection;
				if (!PushDirection.IsNearlyZero())
				{
					Result.ImpactSpeed = CalculateImpactSpeed(Hit, PushDirection, Request.KnockbackStrengthMultiplier);
					FVehicleKnockbackConfig Rules;
					const UVehicleKnockbackSettings* Settings = GetDefault<UVehicleKnockbackSettings>();
					Result.ImpactOutcome = EHALImpactOutcome::RulesUnavailable;
					if (FMath::IsFinite(Result.ImpactSpeed) && Settings && Settings->GetRuntimeRules(Rules))
					{
						Result.KnockbackTier = Rules.SelectTier(Result.ImpactSpeed);
						ApplyTierOutcome(*Body, PushDirection, Rules.GetTierDefinition(Result.KnockbackTier),
							Rules.MaxAirborneAngularSpeed, IsTargetGrounded(*Body, Hit, Rules.GroundProbeExtraDistance));
						Result.bImpactApplied = true;
						Result.ImpactOutcome = EHALImpactOutcome::Applied;
					}
				}
			}
		}
	}
	if (CVarHALCombatLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("HAL Combat: Attack=%s Pulse=%u PlayerId=%d Source=%s Mode=%d Target=%s Raw=%.2f Resolved=%.2f Actual=%.2f DamageImmune=%d Impact=%d Tier=%d"),
			*Result.AttackId.ToString(), Result.PulseSequence, Result.InstigatorPlayerId, *Source.SourceTag.ToString(),
			int32(Source.DamageMode), *GetNameSafe(Target), Result.RawDamage, Result.ResolvedDamage, Result.ActualHPRemoved,
			Result.bDamageImmune ? 1 : 0, int32(Result.ImpactOutcome), int32(Result.KnockbackTier));
	}
	return Result;
}

FVehicleHitResolution FCombatResolver::ResolveVehicleHit(const FVehicleHitContext& Hit,
	const float Damage, const float KnockbackStrengthMultiplier)
{
	FVehicleHitResolution Result;
	if (!IsValid(Hit.SourceActor) || !VehicleConfiguration::IsReadyForGameplay(Hit.SourceActor)) { return Result; }
	FHALAttackSourceSnapshot Source;
	Source.SourceActor = Hit.SourceActor;
	Source.InstigatorActor = Hit.InstigatorActor;
	Source.InstigatorPlayerId = Hit.InstigatorPlayerId;
	Source.bCanHitInstigator = false;
	Source.FixedDamage = Damage;
	FHALCombatAction Action;
	if (!Action.Begin(*Hit.SourceActor, Source)) { return Result; }
	FHALCombatHitRequest Request;
	Request.AttackId = Action.GetSnapshot().AttackId;
	Request.Hit = Hit;
	Request.bApplyImpact = true;
	Request.KnockbackStrengthMultiplier = KnockbackStrengthMultiplier;
	const FHALCombatHitResult Resolved = ResolveCombatHit(Action, Request);
	Result.bResolved = Resolved.bHitAccepted;
	Result.ImpactSpeed = Resolved.ImpactSpeed;
	Result.KnockbackTier = Resolved.KnockbackTier;
	Action.Close();
	return Result;
}
