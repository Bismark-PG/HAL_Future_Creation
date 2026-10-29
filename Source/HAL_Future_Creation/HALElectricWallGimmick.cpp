#include "HALElectricWallGimmick.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "HALImpactSourceComponent.h"
#include "HALMatchGameState.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "ArcadeVehicleMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarHALElectricWallLog(TEXT("hal.ElectricWallLog"),0,
	TEXT("Paired electric wall accepted contacts, bounce back, Stun and shocked marker. 0=off,1=on."));

bool FHALElectricWallConfig::Validate(FString& Error) const
{
	const auto Positive = [](float V) { return FMath::IsFinite(V) && V > 0; };
	if (!Positive(PillarSpacing) || !Positive(PillarRadius) || !Positive(Height) || Height < 2*PillarRadius
		|| PillarSpacing <= 2*PillarRadius || !Positive(WallHalfThickness) || !FMath::IsFinite(ContactMargin) || ContactMargin < 0
		|| !Positive(ScanInterval) || ScanInterval < .01f || !Positive(StunSeconds)
		|| !Positive(BounceBackDistance) || BounceBackDistance > 1000
		|| !Positive(BounceBackSeconds) || BounceBackSeconds < .05f || BounceBackSeconds > 2)
	{ Error = TEXT("Require separated pillars, Height >= diameter, positive finite dimensions/times, scan >= .01s, bounce distance (0,1000]cm and duration [.05,2]s."); return false; }
	return true;
}

AHALElectricWallGimmick::AHALElectricWallGimmick()
{
	PrimaryActorTick.bCanEverTick = false; bReplicates = true; SetReplicateMovement(true);
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
	PillarA = CreateDefaultSubobject<UCapsuleComponent>(TEXT("PillarA")); PillarA->SetupAttachment(SceneRoot);
	PillarB = CreateDefaultSubobject<UCapsuleComponent>(TEXT("PillarB")); PillarB->SetupAttachment(SceneRoot);
	for (auto* Pillar : {PillarA.Get(),PillarB.Get()})
	{
		Pillar->SetCollisionProfileName(TEXT("BlockAll")); Pillar->SetCollisionObjectType(ECC_WorldStatic);
		Pillar->SetGenerateOverlapEvents(false); Pillar->SetSimulatePhysics(false);
	}
	ElectricField = CreateDefaultSubobject<UBoxComponent>(TEXT("ElectricField")); ElectricField->SetupAttachment(SceneRoot);
	ElectricField->SetCollisionEnabled(ECollisionEnabled::QueryOnly); ElectricField->SetCollisionObjectType(ECC_WorldDynamic);
	ElectricField->SetCollisionResponseToAllChannels(ECR_Ignore); ElectricField->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Overlap);
	ElectricField->SetGenerateOverlapEvents(false);
	PillarVisualA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarVisualA")); PillarVisualA->SetupAttachment(PillarA);
	PillarVisualB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarVisualB")); PillarVisualB->SetupAttachment(PillarB);
	WallVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallVisual")); WallVisual->SetupAttachment(ElectricField);
	for (auto* Visual : {PillarVisualA.Get(),PillarVisualB.Get(),WallVisual.Get()})
	{ Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetGenerateOverlapEvents(false); Visual->SetSimulatePhysics(false); }
	// Natural pillar collision remains, but never adds a second authored impact on top of our contact pulse.
	ContactImpactProfile = CreateDefaultSubobject<UHALImpactSourceComponent>(TEXT("ContactImpactProfile"));
	ContactImpactProfile->Mode = EHALCollisionSourceMode::FixedTier; ContactImpactProfile->Tier = EHALImpactTier::None;
	UpdateGeometry();
}
void AHALElectricWallGimmick::UpdateGeometry()
{
	FString Error;
	if (!Config.Validate(Error))
	{
		for (UPrimitiveComponent* Surface : TArray<UPrimitiveComponent*>{PillarA.Get(),PillarB.Get(),ElectricField.Get()})
		{ Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		return;
	}
	PillarA->SetCapsuleSize(Config.PillarRadius,Config.Height*.5f); PillarB->SetCapsuleSize(Config.PillarRadius,Config.Height*.5f);
	PillarA->SetRelativeLocation(FVector(-Config.PillarSpacing*.5f,0,Config.Height*.5f));
	PillarB->SetRelativeLocation(FVector(Config.PillarSpacing*.5f,0,Config.Height*.5f));
	PillarA->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); PillarB->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ElectricField->SetRelativeLocation(FVector(0,0,Config.Height*.5f));
	ElectricField->SetBoxExtent(FVector(Config.PillarSpacing*.5f-Config.PillarRadius,Config.WallHalfThickness,Config.Height*.5f));
	ElectricField->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}
void AHALElectricWallGimmick::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); UpdateGeometry(); }
bool AHALElectricWallGimmick::Configure(const FHALElectricWallConfig& InConfig)
{
	FString Error;
	if (!HasAuthority() || HasActorBegunPlay() || !InConfig.Validate(Error)) { return false; }
	Config = InConfig; UpdateGeometry(); return true;
}
void AHALElectricWallGimmick::OnRep_Config() { UpdateGeometry(); }
void AHALElectricWallGimmick::BeginPlay()
{
	Super::BeginPlay(); UpdateGeometry();
	if (!HasAuthority() || GetNetMode()==NM_Client) { return; }
	FString Error;
	bReady = Config.Validate(Error);
	if (!bReady) { UE_LOG(LogTemp,Error,TEXT("Electric wall %s disabled: invalid geometry/timing. %s"),*GetName(),*Error); return; }
	GetWorldTimerManager().SetTimer(ScanTimer,this,&ThisClass::ScanContacts,Config.ScanInterval,true,0.f);
}
void AHALElectricWallGimmick::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(ScanTimer); Contacts.Reset(); bReady = false; Super::EndPlay(Reason);
}
void AHALElectricWallGimmick::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALElectricWallGimmick,Config); DOREPLIFETIME(AHALElectricWallGimmick,AcceptedContactCount);
}
FVector AHALElectricWallGimmick::GetPushDirection(UPrimitiveComponent* Surface,const FVector& Current,const FVector& Previous,const FVector& Velocity) const
{
	// Contact can already have reversed Chaos velocity. Prefer the sampled incoming travel,
	// independent of wall orientation or the vehicle's facing (including reversing/drifting).
	const FVector Travel = Current-Previous;
	if (Travel.SizeSquared2D()>.01) { return -Travel.GetSafeNormal2D(); }
	if (Velocity.SizeSquared2D()>.01) { return -Velocity.GetSafeNormal2D(); }
	// No incoming motion (e.g. a car spawned inside the field): fall back to geometry.
	if (Surface==ElectricField)
	{
		const FVector Normal = ElectricField->GetRightVector().GetSafeNormal2D();
		double Side = FVector::DotProduct(Previous-ElectricField->GetComponentLocation(),Normal);
		if (FMath::Abs(Side)<.1) { Side = FVector::DotProduct(Current-ElectricField->GetComponentLocation(),Normal); }
		if (FMath::Abs(Side)<.1) { Side = -FVector::DotProduct(Velocity,Normal); }
		return Normal*(Side<0 ? -1 : 1);
	}
	FVector Direction = (Current-Surface->GetComponentLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero()) { Direction = -Velocity.GetSafeNormal2D(); }
	return Direction.IsNearlyZero() ? GetActorRightVector().GetSafeNormal2D() : Direction;
}
bool AHALElectricWallGimmick::ApplyContact(ATestVehiclePawn* Target,UPrimitiveComponent* Surface,const FVector& Previous)
{
	if (!bReady || !HasAuthority() || GetNetMode()==NM_Client || !IsValid(Target)
		|| (Surface!=PillarA && Surface!=PillarB && Surface!=ElectricField)) { return false; }
	const auto* Match = GetWorld()->GetGameState<AHALMatchGameState>();
	if (Match && Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
	auto* ASC = Cast<UHALAbilitySystemComponent>(Target->GetAbilitySystemComponent());
	const auto* Health = Target->FindComponentByClass<UVehicleHealthComponent>();
	auto* Body = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	if (!Target->IsConfigurationValid() || !Target->GetController() || Target->GetController()->GetPawn()!=Target
		|| !ASC || ASC->GetAvatarActor()!=Target || !ASC->IsHealthInitialized()
		|| !Health || !Health->IsHealthReady() || Health->GetCurrentHP()<=0 || !Body || !Body->IsSimulatingPhysics()) { return false; }
	auto& Record = Contacts.FindOrAdd(Target);
	if (Record.bTriggered) { return false; }
	const FVector Direction = Record.bInContact && !Record.IncomingPushDirection.IsNearlyZero()
		? Record.IncomingPushDirection : GetPushDirection(Surface,Body->GetComponentLocation(),Previous,Body->GetPhysicsLinearVelocity());
	if (Direction.IsNearlyZero()) { return false; }
	auto* Movement=Target->FindComponentByClass<UArcadeVehicleMovementComponent>();
	if (!Movement || !Movement->QueueBounceBack(Direction,Config.BounceBackDistance,Config.BounceBackSeconds)) { return false; }
	if (!ASC->ApplyStun(Config.StunSeconds,this)) { return false; }
	const bool bShocked = ASC->MarkShockedForCurrentStun(this);
	Record.bTriggered = true; Record.BounceEndsAt=GetWorld()->GetTimeSeconds()+Config.BounceBackSeconds+Config.ScanInterval; ++AcceptedContactCount; ForceNetUpdate();
	if (CVarHALElectricWallLog.GetValueOnGameThread())
	{
		UE_LOG(LogTemp,Log,TEXT("Electric wall: Source=%s Target=%s Surface=%s Contact=%u BounceCm=%.2f BounceSeconds=%.3f Direction=%s Stun=%.3f Shocked=%d"),
			*GetName(),*Target->GetName(),*Surface->GetName(),AcceptedContactCount,Config.BounceBackDistance,Config.BounceBackSeconds,
			*Direction.ToCompactString(),ASC->GetEffectTimeRemaining(HALGameplayTags::State_Stun),bShocked);
	}
	return true;
}
void AHALElectricWallGimmick::ScanContacts()
{
	if (!bReady || !HasAuthority() || GetNetMode()==NM_Client) { return; }
	const auto* Match = GetWorld()->GetGameState<AHALMatchGameState>();
	if (Match && Match->GetMatchPhase()!=EHALMatchPhase::Playing) { Contacts.Reset(); return; }
	TMap<TWeakObjectPtr<ATestVehiclePawn>,UPrimitiveComponent*> Touches;
	const FCollisionObjectQueryParams Objects(ECC_PhysicsBody);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ElectricWallContacts),false,this);
	for (UPrimitiveComponent* Surface : TArray<UPrimitiveComponent*>{PillarA.Get(),PillarB.Get(),ElectricField.Get()})
	{
		TArray<FOverlapResult> Hits;
		const auto* Pillar = Cast<UCapsuleComponent>(Surface);
		const float Scale = Surface->GetComponentScale().GetAbsMax();
		const auto Shape = Pillar ? FCollisionShape::MakeCapsule(Pillar->GetScaledCapsuleRadius()+Config.ContactMargin*Scale,Pillar->GetScaledCapsuleHalfHeight()+Config.ContactMargin*Scale)
			: FCollisionShape::MakeBox(ElectricField->GetScaledBoxExtent());
		GetWorld()->OverlapMultiByObjectType(Hits,Surface->GetComponentLocation(),Surface->GetComponentQuat(),Objects,Shape,Query);
		for (const auto& Hit : Hits)
		{ if (auto* V = Cast<ATestVehiclePawn>(Hit.GetActor()); V && !Touches.Contains(V)) { Touches.Add(V,Surface); } }
		if (bDrawDebug)
		{
			if (Pillar) { DrawDebugCapsule(GetWorld(),Surface->GetComponentLocation(),Shape.GetCapsuleHalfHeight(),Shape.GetCapsuleRadius(),Surface->GetComponentQuat(),FColor::Cyan,false,Config.ScanInterval,0,2); }
			else { DrawDebugBox(GetWorld(),Surface->GetComponentLocation(),ElectricField->GetScaledBoxExtent(),Surface->GetComponentQuat(),FColor::Cyan,false,Config.ScanInterval,0,2); }
		}
	}
	for (TActorIterator<ATestVehiclePawn> It(GetWorld()); It; ++It)
	{
		auto* V = *It; auto* Body = Cast<UPrimitiveComponent>(V->GetRootComponent()); if (!Body) { continue; }
		const FVector Current = Body->GetComponentLocation();
		const auto* Old = Contacts.Find(V); const FVector Previous = Old && Old->SampledAt>=0 ? Old->PreviousLocation : Current;
		UPrimitiveComponent* Surface = Touches.FindRef(V);
		const bool bOverlapping = Surface!=nullptr;
		if (!Surface && !Current.Equals(Previous,.01))
		{
			// Sweep the actual vehicle collision shape. No overlap-event flags or per-frame Tick required.
			TArray<FHitResult> Swept; FComponentQueryParams SweepQuery(SCENE_QUERY_STAT(ElectricWallVehicleSweep),V);
			SweepQuery.bFindInitialOverlaps = true;
			GetWorld()->ComponentSweepMulti(Swept,Body,Previous,Current,Body->GetComponentQuat(),SweepQuery);
			for (const auto& Hit : Swept)
			{ if (Hit.GetActor()==this && (Hit.GetComponent()==PillarA || Hit.GetComponent()==PillarB || Hit.GetComponent()==ElectricField)) { Surface = Hit.GetComponent(); break; } }
		}
		auto& Record = Contacts.FindOrAdd(V);
		if (Surface)
		{
			if (!Record.bInContact) { Record.IncomingPushDirection = GetPushDirection(Surface,Current,Previous,Body->GetPhysicsLinearVelocity()); }
			Record.bInContact = true;
			ApplyContact(V,Surface,Previous);
		}
		// One bounce per continuous contact across all three surfaces. A completed crossing
		// is not an overlap; a later entry can bounce again.
		// A high-speed full crossing can bounce back through the same field. Keep that
		// return in the same interaction; rearm only after the bounce and a clear sample.
		if (!Surface && GetWorld()->GetTimeSeconds()>=Record.BounceEndsAt) { Record.bTriggered=false; }
		Record.bInContact = bOverlapping;
		if (!bOverlapping) { Record.IncomingPushDirection = FVector::ZeroVector; }
		Record.PreviousLocation = Current; Record.SampledAt = GetWorld()->GetTimeSeconds();
	}
	for (auto It=Contacts.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } }
}
