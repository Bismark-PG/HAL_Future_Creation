#include "HALArenaEffectGenerator.h"

#include "HALBlackHole.h"
#include "HALElectricSphereGimmick.h"
#include "HALExpandingSlipperyArea.h"
#include "HALGameFlowPermissions.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* EffectName(EHALArenaEffect Effect)
	{
		switch (Effect)
		{
		case EHALArenaEffect::SlipperyArea: return TEXT("SLIPPERY");
		case EHALArenaEffect::BlackHole: return TEXT("BLACK HOLE");
		case EHALArenaEffect::ElectricField: return TEXT("ELECTRIC");
		default: return TEXT("---");
		}
	}
	FColor EffectColor(EHALArenaEffect Effect)
	{
		switch (Effect)
		{
		case EHALArenaEffect::SlipperyArea: return FColor(40,220,255);
		case EHALArenaEffect::BlackHole: return FColor(200,80,255);
		case EHALArenaEffect::ElectricField: return FColor(255,220,40);
		default: return FColor::White;
		}
	}
	void FinishNormals(UE::Geometry::FDynamicMesh3& Mesh)
	{
		UE::Geometry::FMeshNormals::QuickComputeVertexNormals(Mesh);
		Mesh.EnableAttributes();
		UE::Geometry::FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals());
	}
}

FHALArenaGeneratorConfig::FHALArenaGeneratorConfig()
{
	SlipperyAreaClass = AHALExpandingSlipperyArea::StaticClass();
	BlackHoleClass = AHALBlackHole::StaticClass();
	ElectricFieldClass = AHALElectricSphereGimmick::StaticClass();
}
UClass* FHALArenaGeneratorConfig::EffectClass(EHALArenaEffect Effect) const
{
	switch (Effect)
	{
	case EHALArenaEffect::SlipperyArea: return SlipperyAreaClass.Get();
	case EHALArenaEffect::BlackHole: return BlackHoleClass.Get();
	case EHALArenaEffect::ElectricField: return ElectricFieldClass.Get();
	default: return nullptr;
	}
}
float FHALArenaGeneratorConfig::PreviewRadius(EHALArenaEffect Effect) const
{
	if (!EffectClass(Effect)) { return 0; }
	switch (Effect)
	{
	case EHALArenaEffect::SlipperyArea: return SlipperyAreaClass->GetDefaultObject<AHALExpandingSlipperyArea>()->GetAreaConfig().MaximumRadius;
	case EHALArenaEffect::BlackHole: return BlackHoleClass->GetDefaultObject<AHALBlackHole>()->GetBlackHoleConfig().MaximumRadius;
	case EHALArenaEffect::ElectricField: return ElectricFieldClass->GetDefaultObject<AHALElectricSphereGimmick>()->GetSphereConfig().Radius;
	default: return 0;
	}
}
FVector FHALArenaGeneratorConfig::PlacementOffset(EHALArenaEffect Effect) const
{
	switch (Effect)
	{
	case EHALArenaEffect::SlipperyArea: return EffectOffset+SlipperyAreaOffset;
	case EHALArenaEffect::BlackHole: return EffectOffset+BlackHoleOffset;
	case EHALArenaEffect::ElectricField: return EffectOffset+ElectricFieldOffset;
	default: return EffectOffset;
	}
}
bool FHALArenaGeneratorConfig::Validate(FString& Error) const
{
	const auto Time = [](float V) { return FMath::IsFinite(V) && V >= 0 && V <= 60; };
	const auto Offset = [](const FVector& V) { return !V.ContainsNaN() && V.GetAbsMax() <= 10000; };
	if (!Time(RollSeconds) || !Time(RevealSeconds) || !Time(HintSeconds) || !Time(SpawnDelaySeconds) || !Time(FinishSeconds)
		|| !Offset(EffectOffset) || !Offset(SlipperyAreaOffset) || !Offset(BlackHoleOffset) || !Offset(ElectricFieldOffset) || PillarHalfExtent.ContainsNaN()
		|| PillarHalfExtent.GetMin() < 1 || PillarHalfExtent.GetMax() > 2000)
	{ Error = TEXT("Invalid timing, effect offset or pillar extent."); return false; }
	if (!FMath::IsFinite(MeshSinkDepth) || MeshSinkDepth < 0 || MeshSinkDepth > 10000
		|| !FMath::IsFinite(MeshRotationDegrees) || FMath::Abs(MeshRotationDegrees) > 7200)
	{ Error = TEXT("Invalid mesh sink depth or rotation."); return false; }
	for (const auto Effect : {EHALArenaEffect::SlipperyArea,EHALArenaEffect::BlackHole,EHALArenaEffect::ElectricField})
	{
		if (!Offset(PlacementOffset(Effect))) { Error = TEXT("Combined effect offset exceeds 10000 cm on an axis."); return false; }
		const auto* Class = EffectClass(Effect);
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
			|| !Class->GetDefaultObject<AActor>()->GetIsReplicated())
		{ Error = FString::Printf(TEXT("%s needs a valid replicated effect class."),EffectName(Effect)); return false; }
	}
	if (!SlipperyAreaClass->GetDefaultObject<AHALExpandingSlipperyArea>()->GetAreaConfig().IsValid()
		|| !BlackHoleClass->GetDefaultObject<AHALBlackHole>()->GetBlackHoleConfig().IsValid()
		|| !ElectricFieldClass->GetDefaultObject<AHALElectricSphereGimmick>()->GetSphereConfig().Validate())
	{ Error = TEXT("An effect class has invalid default settings."); return false; }
	Error.Reset(); return true;
}
EHALArenaEffect FHALArenaGeneratorConfig::Draw(FRandomStream& Random)
{
	return static_cast<EHALArenaEffect>(1 + Random.RandRange(0,2));
}

AHALArenaEffectGenerator::AHALArenaEffectGenerator()
{
	bReplicates = true; bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("GeneratorBase")));
	PillarBody = CreateDefaultSubobject<UBoxComponent>(TEXT("PillarBody")); PillarBody->SetupAttachment(RootComponent);
	PillarBody->SetCollisionProfileName(TEXT("BlockAll")); PillarBody->SetGenerateOverlapEvents(false);
	PillarVisual = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("PillarVisual")); PillarVisual->SetupAttachment(RootComponent);
	PillarVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HintRing = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("HintRing")); HintRing->SetupAttachment(RootComponent);
	HintRing->SetCollisionEnabled(ECollisionEnabled::NoCollision); HintRing->SetVisibility(false); HintRing->SetCastShadow(false);
	StageText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StageText")); StageText->SetupAttachment(RootComponent);
	StageText->SetHorizontalAlignment(EHTA_Center); StageText->SetVerticalAlignment(EVRTA_TextCenter);
	StageText->SetWorldSize(22); StageText->SetRelativeRotation(FRotator(90,0,0));
}
bool AHALArenaEffectGenerator::ConfigureGenerator(const FHALArenaGeneratorConfig& Config)
{
	FString Error;
	if (!HasAuthority() || HasActorBegunPlay() || !Config.Validate(Error)) { return false; }
	Settings = Config; RefreshGeometry(); return true;
}
void AHALArenaEffectGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform); RefreshGeometry(); RefreshPresentation();
}
void AHALArenaEffectGenerator::RefreshGeometry()
{
	const FVector Extent = Settings.PillarHalfExtent.ContainsNaN() ? FVector(40,40,100) : Settings.PillarHalfExtent.ComponentMax(FVector(1));
	PillarBody->SetBoxExtent(Extent); PillarBody->SetRelativeLocation(FVector(0,0,Extent.Z));
	PillarBody->SetCollisionEnabled(Settings.bSolidPillar ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	UE::Geometry::FDynamicMesh3 Mesh;
	for (int32 Index=0;Index<8;++Index) { Mesh.AppendVertex(FVector3d((Index&1)?Extent.X:-Extent.X,(Index&2)?Extent.Y:-Extent.Y,(Index&4)?Extent.Z:-Extent.Z)); }
	for (const auto& Triangle : TArray<UE::Geometry::FIndex3i>{{0,2,1},{1,2,3},{4,5,6},{5,7,6},{0,1,4},{1,5,4},{2,6,3},{3,6,7},{0,4,2},{2,4,6},{1,3,5},{3,7,5}})
	{ Mesh.AppendTriangle(Triangle); }
	FinishNormals(Mesh); PillarVisual->SetMesh(MoveTemp(Mesh));
	PillarVisual->SetRelativeLocation(FVector(0,0,Extent.Z));
	PillarVisual->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
	HintRing->SetMaterial(0,UMaterial::GetDefaultMaterial(MD_Surface));
	StageText->SetRelativeLocation(FVector(0,0,Extent.Z*2+12));
	BuiltRingRadius = -1;
}
void AHALArenaEffectGenerator::BeginPlay()
{
	Super::BeginPlay(); bReady = true; RefreshGeometry();
	if (HasAuthority())
	{
		FString Error;
		if (!Settings.Validate(Error))
		{
			UE_LOG(LogTemp,Error,TEXT("Arena generator %s disabled: %s"),*GetName(),*Error);
			EnterStage(EHALArenaGeneratorStage::Disabled); return;
		}
		Random.Initialize(Settings.RandomSeed != 0 ? Settings.RandomSeed : FMath::Rand());
	}
	InitializeMeshMotion();
	UpdateGenerator();
	if (bEnding || IsActorBeingDestroyed()) { return; }
	FTimerManagerTimerParameters Timer; Timer.bLoop = true; Timer.bMaxOncePerFrame = true;
	GetWorldTimerManager().SetTimer(UpdateTimer,this,&ThisClass::UpdateGenerator,.05f,Timer);
}
double AHALArenaEffectGenerator::ServerNow() const
{
	const auto* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : GetWorld() ? GetWorld()->GetTimeSeconds() : 0;
}
bool AHALArenaEffectGenerator::CanRun() const
{
	if (!HALGameFlow::Permissions(GetWorld()).bWorldInteractions) { return false; }
	const auto* Match = GetWorld()->GetGameState<AHALMatchGameState>();
	if (!Match) { return true; }
	const auto* Room = Match->GetRoom();
	if (Room && Room->IsEnabled()) { return !Settings.bWaitForMatchStart || Room->GetData().Phase == EHALRoomPhase::Playing; }
	// Disabling the startup wait is useful for previewing in legacy maps; it must not disable round-end cleanup.
	return Match->GetMatchPhase() != EHALMatchPhase::Finished
		&& (!Settings.bWaitForMatchStart || Match->GetMatchPhase() == EHALMatchPhase::Playing);
}
void AHALArenaEffectGenerator::EnterStage(EHALArenaGeneratorStage Stage, float Duration)
{
	State.Stage = Stage; State.StartedAt = ServerNow(); State.Duration = Duration;
	ForceNetUpdate(); RefreshPresentation();
}
void AHALArenaEffectGenerator::BeginCycle()
{
	State.Cycle = State.Cycle == MAX_int32 ? 1 : State.Cycle+1;
	State.Effect = EHALArenaEffect::None; State.WarningRadius = 0; State.ActiveEffect = nullptr; PendingClass = nullptr;
	EnterStage(EHALArenaGeneratorStage::Rolling,Settings.RollSeconds);
}
void AHALArenaEffectGenerator::UpdateGenerator()
{
	if (!bReady || bEnding) { return; }
	if (HasAuthority() && State.Stage != EHALArenaGeneratorStage::Disabled)
	{
		if (!CanRun())
		{
			RemoveEffect();
			if (State.Stage != EHALArenaGeneratorStage::Waiting)
			{
				State.Effect = EHALArenaEffect::None; State.WarningRadius = 0; PendingClass = nullptr;
				EnterStage(EHALArenaGeneratorStage::Waiting);
			}
		}
		else if (State.Stage == EHALArenaGeneratorStage::Waiting) { BeginCycle(); }
		else if (State.Stage != EHALArenaGeneratorStage::Active && ServerNow()-State.StartedAt >= State.Duration)
		{
			switch (State.Stage)
			{
			case EHALArenaGeneratorStage::Rolling:
				State.Effect = FHALArenaGeneratorConfig::Draw(Random); PendingClass = Settings.EffectClass(State.Effect);
				State.EffectCenter = GetActorLocation()+Settings.PlacementOffset(State.Effect);
				State.WarningRadius = Settings.PreviewRadius(State.Effect);
				EnterStage(EHALArenaGeneratorStage::Revealing,Settings.RevealSeconds); break;
			case EHALArenaGeneratorStage::Revealing: EnterStage(EHALArenaGeneratorStage::Hint,Settings.HintSeconds); break;
			case EHALArenaGeneratorStage::Hint: EnterStage(EHALArenaGeneratorStage::Delay,Settings.SpawnDelaySeconds); break;
			case EHALArenaGeneratorStage::Delay: SpawnEffect(); break;
			case EHALArenaGeneratorStage::Finished: BeginCycle(); break;
			default: break;
			}
		}
	}
	RefreshPresentation();
}
void AHALArenaEffectGenerator::SpawnEffect()
{
	if (!HasAuthority() || State.ActiveEffect) { return; }
	if (!PendingClass)
	{
		UE_LOG(LogTemp,Error,TEXT("Arena generator %s lost its selected effect class."),*GetName());
		EnterStage(EHALArenaGeneratorStage::Disabled); return;
	}
	const FTransform Pose(FQuat::Identity,State.EffectCenter,FVector::OneVector);
	auto* Effect = GetWorld()->SpawnActorDeferred<AActor>(PendingClass,Pose,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Effect) { Effect->FinishSpawning(Pose); }
	if (bEnding) { if (IsValid(Effect)) { Effect->Destroy(); } return; }
	if (!IsValid(Effect) || Effect->IsActorBeingDestroyed())
	{
		UE_LOG(LogTemp,Error,TEXT("Arena generator %s failed to spawn %s."),*GetName(),*GetNameSafe(PendingClass));
		EnterStage(EHALArenaGeneratorStage::Disabled); return;
	}
	State.ActiveEffect = Effect; Effect->OnEndPlay.AddUniqueDynamic(this,&ThisClass::EffectEnded);
	EnterStage(EHALArenaGeneratorStage::Active);
}
void AHALArenaEffectGenerator::EffectEnded(AActor* Actor, EEndPlayReason::Type Reason)
{
	if (!HasAuthority() || bEnding || State.ActiveEffect != Actor) { return; }
	State.ActiveEffect = nullptr;
	if (Reason == EEndPlayReason::Destroyed) { EnterStage(EHALArenaGeneratorStage::Finished,Settings.FinishSeconds); }
	else { EnterStage(EHALArenaGeneratorStage::Waiting); }
}
void AHALArenaEffectGenerator::RemoveEffect()
{
	if (auto* Effect = State.ActiveEffect.Get())
	{
		State.ActiveEffect = nullptr; Effect->OnEndPlay.RemoveDynamic(this,&ThisClass::EffectEnded);
		if (HasAuthority() && !Effect->IsActorBeingDestroyed()) { Effect->Destroy(); }
	}
}
EHALArenaEffect AHALArenaEffectGenerator::GetDisplayedEffect() const
{
	if (State.Stage != EHALArenaGeneratorStage::Rolling) { return State.Effect; }
	const int32 Index = int32(FMath::FloorToDouble(FMath::Max(0.,ServerNow()-State.StartedAt)/.12))%3;
	return static_cast<EHALArenaEffect>(1+Index);
}
float AHALArenaEffectGenerator::GetStageRemainingSeconds() const
{
	if (State.Stage == EHALArenaGeneratorStage::Waiting || State.Stage == EHALArenaGeneratorStage::Active || State.Stage == EHALArenaGeneratorStage::Disabled) { return -1; }
	return float(FMath::Max(0.,double(State.Duration)-(ServerNow()-State.StartedAt)));
}
void AHALArenaEffectGenerator::RefreshPresentation()
{
	RefreshMeshMotion();
	const bool bWarning = State.Stage == EHALArenaGeneratorStage::Hint || State.Stage == EHALArenaGeneratorStage::Delay;
	HintRing->SetVisibility(bWarning && State.WarningRadius > 0);
	if (bWarning && State.WarningRadius > 0)
	{
		if (BuiltRingRadius != State.WarningRadius)
		{
			UE::Geometry::FDynamicMesh3 Mesh;
			constexpr int32 Segments = 64;
			const double Outer = State.WarningRadius, Inner = FMath::Max(0.,Outer-8.);
			for (int32 I=0;I<Segments;++I)
			{
				const double Angle = I*2.*UE_PI/Segments;
				for (double Radius : {Inner,Outer}) { Mesh.AppendVertex(FVector3d(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0)); }
			}
			for (int32 I=0;I<Segments;++I)
			{
				const int32 A=I*2,B=(I+1)%Segments*2;
				Mesh.AppendTriangle(A,A+1,B); Mesh.AppendTriangle(A+1,B+1,B);
			}
			FinishNormals(Mesh); HintRing->SetMesh(MoveTemp(Mesh)); BuiltRingRadius = State.WarningRadius;
		}
		FVector Ground = State.EffectCenter;
		FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(GeneratorHintGround),false,this);
		if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit,Ground+FVector(0,0,300),Ground-FVector(0,0,1000),FCollisionObjectQueryParams(ECC_WorldStatic),Query))
		{ Ground = Hit.ImpactPoint; }
		Ground.Z += 3;
		HintRing->SetWorldLocationAndRotation(Ground,FQuat::Identity); HintRing->SetWorldScale3D(FVector::OneVector);
	}
	const auto Displayed = GetDisplayedEffect();
	const TCHAR* Label = TEXT("WAITING");
	switch (State.Stage)
	{
	case EHALArenaGeneratorStage::Rolling: Label=TEXT("ROLL"); break;
	case EHALArenaGeneratorStage::Revealing: Label=TEXT("CHOSEN"); break;
	case EHALArenaGeneratorStage::Hint: Label=TEXT("WARNING"); break;
	case EHALArenaGeneratorStage::Delay: Label=TEXT("INCOMING"); break;
	case EHALArenaGeneratorStage::Active: Label=TEXT("ACTIVE"); break;
	case EHALArenaGeneratorStage::Finished: Label=TEXT("FINISHED"); break;
	case EHALArenaGeneratorStage::Disabled: Label=TEXT("DISABLED"); break;
	default: break;
	}
	const float Remaining = GetStageRemainingSeconds();
	StageText->SetText(FText::FromString(Remaining >= 0 ? FString::Printf(TEXT("%s\n%s\n%.1f"),Label,EffectName(Displayed),Remaining)
		: FString::Printf(TEXT("%s\n%s"),Label,EffectName(Displayed))));
	StageText->SetTextRenderColor(EffectColor(Displayed));
	if (bReady && (!bPresented || PresentedStage != State.Stage || PresentedCycle != State.Cycle))
	{
		bPresented=true; PresentedStage=State.Stage; PresentedCycle=State.Cycle;
		if (GetNetMode()!=NM_DedicatedServer) { OnGeneratorStageChanged(State.Stage,State.Effect,State.Cycle); }
	}
}

void AHALArenaEffectGenerator::InitializeMeshMotion()
{
	if (!Settings.bAnimateMeshes || IsNetMode(NM_DedicatedServer) || MeshMotionRoot) { return; }
	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);
	// A native placeholder has no authored meshes. Do not guess which decorations to move if more are added.
	if (Meshes.Num() != 2) { return; }
	for (const auto* Mesh : Meshes)
	{
		if (!Mesh->GetStaticMesh() || Mesh->IsSimulatingPhysics()) { return; }
	}
	MeshMotionRoot = NewObject<USceneComponent>(this,TEXT("MeshMotionRoot"),RF_Transient);
	MeshMotionRoot->SetupAttachment(RootComponent);
	MeshMotionRoot->SetMobility(EComponentMobility::Movable);
	MeshMotionRoot->RegisterComponent();
	for (auto* Source : Meshes)
	{
		FMeshMotionSource Saved;
		Saved.Source = Source; Saved.bVisible = Source->IsVisible();
		Saved.bHiddenInGame = Source->bHiddenInGame; Saved.bCastShadow = Source->CastShadow;
		MeshMotionSources.Add(Saved);
		// The component template preserves materials and render settings without editing the BP asset.
		auto* Visual = NewObject<UStaticMeshComponent>(this,Source->GetClass(),NAME_None,RF_Transient,Source);
		// Instance templates skip automatic ownership in UActorComponent::PostInitProperties.
		AddOwnedComponent(Visual);
		Visual->SetIsReplicated(false);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetGenerateOverlapEvents(false);
		Visual->SetSimulatePhysics(false);
		Visual->SetMobility(EComponentMobility::Movable);
		Visual->SetupAttachment(MeshMotionRoot);
		Visual->SetRelativeTransform(Source->GetComponentTransform().GetRelativeTransform(GetActorTransform()));
		Visual->RegisterComponent();
		Source->SetVisibility(false,false); Source->SetHiddenInGame(true,false); Source->SetCastShadow(false);
	}
}

float AHALArenaEffectGenerator::MeshMotionAlpha() const
{
	const double Age = FMath::Max(0.,ServerNow()-State.StartedAt);
	double Progress = 0;
	if (State.Stage == EHALArenaGeneratorStage::Active) { return 1.f; }
	if (State.Stage == EHALArenaGeneratorStage::Finished)
	{ Progress = State.Duration > 0 ? 1.-FMath::Clamp(Age/State.Duration,0.,1.) : 0; }
	else if (State.Stage == EHALArenaGeneratorStage::Revealing || State.Stage == EHALArenaGeneratorStage::Hint || State.Stage == EHALArenaGeneratorStage::Delay)
	{
		const double Duration = double(Settings.RevealSeconds)+Settings.HintSeconds+Settings.SpawnDelaySeconds;
		const double Previous = State.Stage == EHALArenaGeneratorStage::Delay ? double(Settings.RevealSeconds)+Settings.HintSeconds
			: State.Stage == EHALArenaGeneratorStage::Hint ? Settings.RevealSeconds : 0;
		Progress = Duration > 0 ? FMath::Clamp((Previous+Age)/Duration,0.,1.) : 1;
	}
	return float(Progress*Progress*(3.-2.*Progress));
}

void AHALArenaEffectGenerator::UpdateMeshMotion()
{
	if (!MeshMotionRoot) { return; }
	const float Alpha = MeshMotionAlpha();
	const FQuat Rotation(FVector::UpVector,FMath::DegreesToRadians(Settings.MeshRotationDegrees*Alpha));
	MeshMotionRoot->SetWorldLocationAndRotation(GetActorLocation()-FVector(0,0,Settings.MeshSinkDepth*Alpha),Rotation*GetActorQuat());
	MeshMotionRoot->SetWorldScale3D(GetActorScale3D());
}

void AHALArenaEffectGenerator::RefreshMeshMotion()
{
	if (!bReady) { return; }
	// Instance components can arrive after BeginPlay (for example on a late network replica).
	if (!MeshMotionRoot) { InitializeMeshMotion(); }
	if (!MeshMotionRoot) { return; }
	UpdateMeshMotion();
	const bool bMoving = State.Stage == EHALArenaGeneratorStage::Revealing || State.Stage == EHALArenaGeneratorStage::Hint
		|| State.Stage == EHALArenaGeneratorStage::Delay || State.Stage == EHALArenaGeneratorStage::Finished;
	if (bMoving && !GetWorldTimerManager().IsTimerActive(MeshMotionTimer))
	{
		FTimerManagerTimerParameters Timer; Timer.bLoop = true; Timer.bMaxOncePerFrame = true;
		GetWorldTimerManager().SetTimer(MeshMotionTimer,this,&ThisClass::UpdateMeshMotion,1.f/60,Timer);
	}
	else if (!bMoving) { GetWorldTimerManager().ClearTimer(MeshMotionTimer); }
}

void AHALArenaEffectGenerator::ClearMeshMotion()
{
	GetWorldTimerManager().ClearTimer(MeshMotionTimer);
	if (MeshMotionRoot)
	{
		TArray<USceneComponent*> Copies; MeshMotionRoot->GetChildrenComponents(false,Copies);
		for (auto* Copy : Copies) { Copy->DestroyComponent(); }
		MeshMotionRoot->DestroyComponent(); MeshMotionRoot = nullptr;
	}
	for (const auto& Saved : MeshMotionSources)
	{
		if (auto* Source = Saved.Source.Get())
		{ Source->SetVisibility(Saved.bVisible,false); Source->SetHiddenInGame(Saved.bHiddenInGame,false); Source->SetCastShadow(Saved.bCastShadow); }
	}
	MeshMotionSources.Reset();
}

void AHALArenaEffectGenerator::OnRep_Settings() { RefreshGeometry(); RefreshPresentation(); }
void AHALArenaEffectGenerator::OnRep_State() { RefreshPresentation(); }
void AHALArenaEffectGenerator::EndPlay(const EEndPlayReason::Type Reason)
{
	bEnding=true; GetWorldTimerManager().ClearTimer(UpdateTimer); RemoveEffect();
	ClearMeshMotion();
	HintRing->SetVisibility(false); StageText->SetVisibility(false);
	Super::EndPlay(Reason);
}
void AHALArenaEffectGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALArenaEffectGenerator,Settings); DOREPLIFETIME(AHALArenaEffectGenerator,State);
}
