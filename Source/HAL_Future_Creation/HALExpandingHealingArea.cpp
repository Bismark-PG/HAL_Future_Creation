#include "HALExpandingHealingArea.h"
#include "HALHealingAreaReceiver.h"
#include "HALGameFlowPermissions.h"
#include "TestVehiclePawn.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/App.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#if WITH_EDITORONLY_DATA
#include "HALHealingAreaGizmoComponent.h"
#endif

bool FHALExpandingHealingAreaConfig::IsValid() const
{
	const auto Range = [](float V, float Min, float Max) { return FMath::IsFinite(V) && V >= Min && V <= Max; };
	return Range(ExpansionSeconds, 0, 60) && Range(DurationSeconds, .01f, 120)
		&& Range(MaximumRadius, 1, 10000) && Range(HalfHeight, 1, 1000) && Healing.IsValid();
}

AHALExpandingHealingArea::AHALExpandingHealingArea()
{
	bReplicates = true; bAlwaysRelevant = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AreaCenter")));
#if WITH_EDITORONLY_DATA
	EditorGizmo = CreateEditorOnlyDefaultSubobject<UHALHealingAreaGizmoComponent>(TEXT("HealingAreaGizmo"));
	if (EditorGizmo) { EditorGizmo->SetupAttachment(GetRootComponent()); }
#endif
	PrimaryActorTick.bCanEverTick = ENABLE_DRAW_DEBUG && FApp::CanEverRender();
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
}
bool AHALExpandingHealingArea::ConfigureArea(const FHALExpandingHealingAreaConfig& Config)
{
	if (!HasAuthority() || HasActorBegunPlay() || !Config.IsValid()) { return false; }
	Settings = Config; return true;
}
double AHALExpandingHealingArea::Age() const
{
	const auto* State = GetWorld()->GetGameState();
	return (State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) - StartedAt;
}
float AHALExpandingHealingArea::GetCurrentRadius() const
{
	if (StartedAt < 0) { return 0; }
	const double Elapsed = Age();
	return Elapsed < 0 || Settings.IsExpired(Elapsed) ? 0 : Settings.RadiusAt(Elapsed);
}
void AHALExpandingHealingArea::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(PrimaryActorTick.bCanEverTick && !IsNetMode(NM_DedicatedServer));
	if (!HasAuthority()) { return; }
	if (!Settings.IsValid()) { UE_LOG(LogTemp, Error, TEXT("%s: invalid expanding healing area settings."), *GetNameSafe(this)); Destroy(); return; }
	const auto* State = GetWorld()->GetGameState();
	StartedAt = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	UpdateArea();
	SetLifeSpan(Settings.ExpansionSeconds + Settings.DurationSeconds);
	GetWorldTimerManager().SetTimer(UpdateTimer, this, &ThisClass::UpdateArea, .02f, true);
	ForceNetUpdate();
}
void AHALExpandingHealingArea::UpdateArea()
{
	if (!HasAuthority() || StartedAt < 0) { return; }
	const double Elapsed = Age();
	if (Settings.IsExpired(Elapsed)) { Destroy(); return; }
	TSet<TWeakObjectPtr<UHALHealingAreaReceiver>> Inside;
	for (TActorIterator<ATestVehiclePawn> It(GetWorld()); It; ++It)
	{
		if (!It->IsConfigurationValid() || !HALGameFlow::Permissions(GetWorld(), *It).bWorldInteractions
			|| !HALExpandingArea::Contains(GetActorLocation(), It->GetActorLocation(), Settings.RadiusAt(Elapsed), Settings.HalfHeight)) { continue; }
		auto* Receiver = It->FindComponentByClass<UHALHealingAreaReceiver>();
		if (Receiver && Receiver->RegisterArea(this, Settings.Healing)) { Inside.Add(Receiver); }
	}
	for (const auto& Entry : Occupants)
	{
		if (auto* Receiver = Entry.Get(); Receiver && !Inside.Contains(Entry)) { Receiver->UnregisterArea(this); }
	}
	Occupants = MoveTemp(Inside);
}
void AHALExpandingHealingArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if ENABLE_DRAW_DEBUG
	if (!bDebugDraw || StartedAt < 0 || Age() < 0 || Settings.IsExpired(Age())) { return; }
	HALExpandingArea::DrawGroundDebug(*this, GetCurrentRadius(), Settings.HalfHeight,
		Age() < Settings.ExpansionSeconds ? FColor::Cyan : FColor::Green);
#endif
}
void AHALExpandingHealingArea::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(UpdateTimer);
	for (const auto& Entry : Occupants) { if (auto* Receiver = Entry.Get()) { Receiver->UnregisterArea(this); } }
	Occupants.Reset(); Super::EndPlay(Reason);
}
void AHALExpandingHealingArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALExpandingHealingArea, Settings);
	DOREPLIFETIME(AHALExpandingHealingArea, StartedAt);
	DOREPLIFETIME(AHALExpandingHealingArea, bDebugDraw);
}
