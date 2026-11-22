#include "HALBlackHoleBall.h"

#include "CombatResolver.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VehicleHealthComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHALBlackHoleBall, Log, All);

bool UHALBlackHoleBallDefinition::ResolveField(FHALBlackHoleConfig& Out) const
{
	const auto* Template = FieldClass.GetDefaultObject();
	if (!Template || FieldClass->HasAnyClassFlags(CLASS_Abstract)
		|| !FMath::IsWithinInclusive(RadiusScale, .05f, 1.f) || !FMath::IsWithinInclusive(StrengthScale, .05f, 1.f)
		|| !FMath::IsWithinInclusive(DurationScale, .05f, 1.f)) { return false; }
	Out = Template->GetBlackHoleConfig();
	if (!Out.IsValid()) { return false; }
	Out.MaximumRadius = FMath::Max(1.f, Out.MaximumRadius * RadiusScale);
	Out.CenterSofteningRadius = FMath::Max(1.f, Out.CenterSofteningRadius * RadiusScale);
	Out.ContinuousAcceleration *= StrengthScale;
	Out.SwirlAcceleration *= StrengthScale;
	Out.ContinuousSpeedLimit = FMath::Max(1.f, Out.ContinuousSpeedLimit * StrengthScale);
	Out.SwirlSpeedLimit = FMath::Max(1.f, Out.SwirlSpeedLimit * StrengthScale);
	Out.PulseDeltaSpeed *= StrengthScale;
	Out.EntrySpeedMultiplier = FMath::Lerp(1.f, Out.EntrySpeedMultiplier, StrengthScale);
	Out.DragGripMultiplier = FMath::Lerp(1.f, Out.DragGripMultiplier, StrengthScale);
	Out.DriveBendStrength *= StrengthScale;
	const bool bHasPulses = Out.DurationSeconds > Out.PulseInterval;
	Out.ExpansionSeconds *= DurationScale;
	Out.DurationSeconds = FMath::Max(.01f, Out.DurationSeconds * DurationScale);
	if (bHasPulses) { Out.PulseInterval = FMath::Clamp(Out.DurationSeconds * .5f, .1f, Out.PulseInterval); }
	return Out.IsValid();
}

bool UHALBlackHoleBallDefinition::Validate(FString& Error) const
{
	if (!Super::Validate(Error)) { return false; }
	FHALBlackHoleConfig Tuned;
	if (ResolveField(Tuned)) { return true; }
	Error = TEXT("Select a valid black hole field and set its radius, strength and duration scales within 0.05..1.");
	return false;
}

UHALSpecialBallDefinition* AHALBlackHoleBall::DefaultType() const
{
	return GetDefault<UHALMatchBallSettings>()->BlackHoleBallType.LoadSynchronous();
}

void AHALBlackHoleBall::BeginPlay()
{
	Super::BeginPlay();
	const auto* Type = Cast<UHALBlackHoleBallDefinition>(Definition);
	auto* VortexSurface = Cast<UMaterialInstanceDynamic>(VisualMesh->GetMaterial(0));
	if (VortexSurface && Type && Type->FieldClass)
	{
		const auto Direction = Type->FieldClass.GetDefaultObject()->GetBlackHoleConfig().SwirlDirection;
		VortexSurface->SetScalarParameterValue(TEXT("SwirlSign"), Direction == EHALBlackHoleSwirlDirection::Clockwise ? -1.f : 1.f);
	}
}

FVector AHALBlackHoleBall::GroundPoint(AActor* Struck, const FVector& Contact, const FVector& Normal) const
{
	const float Radius = PhysicsRoot->GetScaledSphereRadius();
	const bool bVehicle = Struck && Struck->FindComponentByClass<UVehicleHealthComponent>();
	const FVector Point = bVehicle ? Contact : Contact + Normal.GetSafeNormal2D() * (Radius * .15f);
	const FString Map = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	const auto* GroundRules = GetDefault<UHALMatchBallSettings>()->Arenas.FindByPredicate([&Map](const FHALMatchBallArena& Entry)
	{ return Entry.MatchMap.ToSoftObjectPath().GetLongPackageName() == Map && Entry.IsValid(); });
	const double FallbackZ = GroundRules ? GroundRules->FloorCenter.Z : Point.Z - Radius;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BlackHoleBallFloor), false, this);
	// A single static mesh may supply both the wall and its adjoining floor.
	if (bVehicle) { Query.AddIgnoredActor(Struck); }
	Query.AddIgnoredActor(GetLaunchedBy());
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Hits;
	const FVector Below(Point.X, Point.Y, FMath::Min(Point.Z - 10000., FallbackZ - Radius));
	GetWorld()->LineTraceMultiByObjectType(Hits, Point + FVector::UpVector * 3., Below, Objects, Query);
	for (const FHitResult& Hit : Hits)
	{
		const auto* Body = Hit.GetComponent();
		if (!Hit.bStartPenetrating && Hit.ImpactNormal.Z >= .55 && Body && Body->IsPhysicsCollisionEnabled()
			&& !Body->IsSimulatingPhysics() && Body->GetCollisionResponseToChannel(PhysicsRoot->GetCollisionObjectType()) == ECR_Block)
		{ return Hit.ImpactPoint + FVector::UpVector * 2.; }
	}
	// Thin perimeter walls may have no walkable surface directly below their contact.
	return FVector(Point.X, Point.Y, FallbackZ + 2.);
}

bool AHALBlackHoleBall::ApplyContact(AActor* Other, const FVector& Origin, const FVector& Normal, const FVector& Travel)
{
	const auto* Type = Cast<UHALBlackHoleBallDefinition>(Definition);
	FHALBlackHoleConfig Field;
	if (!Type || !Type->ResolveField(Field)) { return false; }
	const FTransform Transform(GroundPoint(Other, Origin, Normal));
	float DirectDamage = 0.f;
	if (Other && Other->FindComponentByClass<UVehicleHealthComponent>())
	{
		auto& Action = GetLaunchAction();
		FHALCombatHitRequest Hit;
		Hit.AttackId = Action.GetSnapshot().AttackId;
		Hit.PulseSequence = Action.GetPulseSequence();
		Hit.Hit.SourceActor = this;
		Hit.Hit.TargetActor = Other;
		Hit.Hit.ImpactPoint = Origin;
		Hit.Hit.ImpactNormal = Normal;
		Hit.bApplyImpact = false;
		const FHALCombatHitResult Result = FCombatResolver::ResolveCombatHit(Action, Hit);
		if (!Result.bHitAccepted) { return false; }
		DirectDamage = Result.ActualHPRemoved;
	}
	UE_LOG(LogHALBlackHoleBall, Log, TEXT("Consumed %s Launcher=%s Contact=%s Point=%s Floor=%s DirectHP=%.1f"),
		*GetName(), *GetNameSafe(GetLaunchedBy()), *GetNameSafe(Other), *Origin.ToCompactString(), *Transform.GetLocation().ToCompactString(), DirectDamage);
	// A lethal direct hit can end the round synchronously.
	const auto* Match = GetWorld()->GetGameState<AHALMatchGameState>();
	if (Match && Match->GetMatchPhase() == EHALMatchPhase::Finished) { return true; }
	auto* Vortex = GetWorld()->SpawnActorDeferred<AHALBlackHole>(Type->FieldClass, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (ensure(Vortex && Vortex->ConfigureMatchBallField(Field))) { Vortex->FinishSpawning(Transform); }
	else if (Vortex) { Vortex->Destroy(); }
	return true;
}
