#include "HALDamageFeedbackSubsystem.h"
#include "HALSmokeScreenTypes.h"
#include "HALVehicleStatusFXSettings.h"
#include "HALVehiclePresentationComponent.h"
#include "HALVehicleFXTargets.h"
#include "TestVehiclePawn.h"
#include "Components/MeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

double UHALDamageFeedbackSubsystem::ServerTime(const UWorld* W)
{ if(!W) { return 0; } const auto* S=W->GetGameState(); return S?S->GetServerWorldTimeSeconds():W->GetTimeSeconds(); }
bool UHALDamageFeedbackSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const auto* W=Cast<UWorld>(Outer);
	return W&&W->IsGameWorld()&&FApp::CanEverRender()&&!IsRunningDedicatedServer()&&Super::ShouldCreateSubsystem(Outer);
}
void UHALDamageFeedbackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection); bStopped=false;
	ConfigPath=GetDefault<UHALVehicleStatusFXSettings>()->DamageFeedbackConfig.ToSoftObjectPath(); const auto Path=ConfigPath;
	if(Path.IsNull()) { bPrepared=true; return; }
	if(Path.ResolveObject()) { CompleteConfig(); return; }
	auto Loading=UAssetManager::GetStreamableManager().RequestAsyncLoad(Path,FStreamableDelegate::CreateWeakLambda(this,[this]() { if(!bStopped) { CompleteConfig(); } }));
	if(!bPrepared) { Handle=Loading; } else if(Loading) { Loading->ReleaseHandle(); }
	if(!Loading&&!bPrepared) { CompleteConfig(); }
}
void UHALDamageFeedbackSubsystem::CompleteConfig()
{
	bPrepared=true; FString Error;
	Config=Cast<UHALDamageFeedbackConfig>(ConfigPath.ResolveObject());
	if(!Config||!Config->Validate(Error))
	{ UE_LOG(LogTemp,Warning,TEXT("DamageFeedback configuration unavailable: %s. Gameplay continues."),Config?*Error:TEXT("missing package or wrong class")); Config=nullptr; Pending.Reset(); }
	if(Handle) { Handle->ReleaseHandle(); Handle.Reset(); }
	auto Queued=MoveTemp(Pending); Pending.Reset();
	for(const auto& P:Queued) { Receive(P.Key.Get(),P.Value); }
}
void UHALDamageFeedbackSubsystem::Receive(AActor* Target,const FHALDamageFeedbackPulse& Pulse)
{
	if(bStopped||!IsValid(Target)||Target->IsActorBeingDestroyed()||Target->IsHidden()||!Pulse.Sequence||!Pulse.AttackId.IsValid()
		||!FMath::IsFinite(Pulse.ServerTime)||!FMath::IsFinite(Pulse.ActualDamage)||Pulse.ActualDamage<=0) { return; }
	if(!bPrepared)
	{
		if(auto* P=Pending.FindByPredicate([&](const auto& E) { return E.Key.Get()==Target; })) { P->Value=Pulse; }
		else if(Pending.Num()<8) { Pending.Emplace(Target,Pulse); }
		return;
	}
	if(!Config||!Config->Style.bEnabled) { return; }
	const double Now=ServerTime(GetWorld());
	if(Now-Pulse.ServerTime>Config->Style.MaximumEventAge) { return; }
	int32 Index=Slots.IndexOfByPredicate([&](const auto& S) { return S.Target.Get()==Target; });
	if(Index==INDEX_NONE)
	{
		// Allocate around pre-existing users of the configured range; sharing an ID would tint their geometry too.
		TSet<int32> Used;
		for(TObjectIterator<UMeshComponent> It;It;++It) if(It->GetWorld()==GetWorld()&&It->bRenderCustomDepth) { Used.Add(It->CustomDepthStencilValue); }
		for(int32 I=Config->Style.MaximumTargets;I<Slots.Num();++I) { RestoreMeshes(I); }
		Slots.SetNum(Config->Style.MaximumTargets);
		for(int32 I=0;I<Slots.Num();++I) if(!Slots[I].Target.IsValid()&&!Used.Contains(Config->Style.FirstStencil+I)) { Index=I; break; }
		if(Index==INDEX_NONE) { return; }
		RestoreMeshes(Index); Slots[Index]={}; Slots[Index].Target=Target;
	}
	// Cosmetic receipt, not prediction: every observer gets the short white/RGB accent even under normal network lag.
	Slots[Index].Playback.Receive(Config->Style,Now); UpdateRendering();
}
void UHALDamageFeedbackSubsystem::RestoreMeshes(int32 Index)
{
	if(!Slots.IsValidIndex(Index)) { return; }
	for(const auto& Lease:Slots[Index].Meshes) if(auto* M=Lease.Mesh.Get())
	{
		if(M->bRenderCustomDepth&&M->CustomDepthStencilValue==Lease.AssignedStencil&&M->CustomDepthStencilWriteMask==ERendererStencilMask::ERSM_Default)
		{ M->SetRenderCustomDepth(Lease.bPreviousCustomDepth); M->SetCustomDepthStencilValue(Lease.PreviousStencil); M->SetCustomDepthStencilWriteMask(Lease.PreviousMask); }
	}
	Slots[Index].Meshes.Reset();
}
void UHALDamageFeedbackSubsystem::RefreshTargets(int32 Index)
{
	auto& Slot=Slots[Index]; auto* A=Slot.Target.Get(); TArray<UMeshComponent*> Current;
	if(auto* Provider=A->FindComponentByClass<UHALVehiclePresentationComponent>())
	{
		FHALVehicleFXTargetSnapshot Snapshot; Provider->CaptureStatusFXTargets(Snapshot);
		for(const auto& T:Snapshot.Targets) if(auto* M=T.Mesh.Get()) { Current.Add(M); }
	}
	else if(auto* M=Cast<UMeshComponent>(A->GetRootComponent());M&&M->IsVisible()&&!M->bHiddenInGame) { Current.Add(M); }
	const bool Changed=Slot.Meshes.ContainsByPredicate([&](const auto& L)
	{ const auto* M=L.Mesh.Get(); return !M||!Current.Contains(M)||!M->bRenderCustomDepth||M->CustomDepthStencilValue!=Config->Style.FirstStencil+Index||M->CustomDepthStencilWriteMask!=ERendererStencilMask::ERSM_Default; });
	if(Changed) { RestoreMeshes(Index); }
	for(auto* M:Current)
	{
		if(Slot.Meshes.ContainsByPredicate([&](const auto& L) { return L.Mesh.Get()==M; })) { continue; }
		// A pre-existing CustomDepth owner is respected even if it happens to use our reserved range.
		if(M->bRenderCustomDepth) { continue; }
		auto& Lease=Slot.Meshes.AddDefaulted_GetRef(); Lease.Mesh=M; Lease.PreviousStencil=M->CustomDepthStencilValue;
		Lease.AssignedStencil=Config->Style.FirstStencil+Index;
		Lease.PreviousMask=M->CustomDepthStencilWriteMask; Lease.bPreviousCustomDepth=M->bRenderCustomDepth;
		M->SetCustomDepthStencilValue(Lease.AssignedStencil); M->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default); M->SetRenderCustomDepth(true);
	}
}
void UHALDamageFeedbackSubsystem::UpdateRendering()
{
	if(!Config||bStopped) { return; } const double Now=ServerTime(GetWorld()); bool Active=false;
	for(int32 I=0;I<Slots.Num();++I)
	{
		auto& S=Slots[I]; auto* A=S.Target.Get(); const auto* V=Cast<ATestVehiclePawn>(A);
		if(!A||A->IsActorBeingDestroyed()||A->IsHidden()||(V&&!V->IsConfigurationValid())||Now>=S.Playback.RetireAt(Config->Style))
		{ RestoreMeshes(I); S={}; continue; }
		const auto W=S.Playback.Weights(Config->Style,Now);
		if(W.GetMax()>KINDA_SMALL_NUMBER&&!HALSmoke::IsConcealed(A)) { RefreshTargets(I); Active|=!S.Meshes.IsEmpty(); }
		else { RestoreMeshes(I); }
	}
	if(!Active) { if(Volume) { Volume->bEnabled=false; } return; }
	if(!Material) { Material=UMaterialInstanceDynamic::Create(Config->Material,this); }
	if(!Volume)
	{
		Volume=GetWorld()->SpawnActor<APostProcessVolume>(); Volume->bUnbound=true; Volume->Priority=100; Volume->BlendWeight=1;
		Volume->Settings.AddBlendable(Material,1);
	}
	Volume->bEnabled=true; const auto& C=Config->Style;
	Material->SetVectorParameterValue(TEXT("BodyColor"),C.BodyColor); Material->SetVectorParameterValue(TEXT("EdgeColor"),C.EdgeColor);
	Material->SetVectorParameterValue(TEXT("EdgeSettings"),FLinearColor(C.OutlinePixels,C.OutlineSoftness,C.DepthTolerance,0));
	const FVector2D D=C.SeparationDirection.GetSafeNormal(); Material->SetVectorParameterValue(TEXT("SplitSettings"),FLinearColor(D.X,D.Y,C.SeparationPixels,0));
	for(int32 I=0;I<8;++I)
	{
		FLinearColor Value(0,0,0,0);
		if(Slots.IsValidIndex(I)&&!Slots[I].Meshes.IsEmpty())
		{ const auto W=Slots[I].Playback.Weights(C,Now); Value=FLinearColor(C.FirstStencil+I,W.X*C.BodyOpacity,W.Y*C.WhiteOpacity,W.Z*C.RGBOpacity); }
		Material->SetVectorParameterValue(FName(*FString::Printf(TEXT("Target%d"),I)),Value);
	}
}
void UHALDamageFeedbackSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const double Now=ServerTime(GetWorld()); Pending.RemoveAll([&](const auto& P) { return !P.Key.IsValid()||Now-P.Value.ServerTime>1; });
	UpdateRendering();
	if(Slots.ContainsByPredicate([](const auto& S) { return S.Target.IsValid(); })) { return; }
	Slots.Reset();
}
void UHALDamageFeedbackSubsystem::ClearTarget(AActor* Target)
{
	Pending.RemoveAll([&](const auto& P) { return P.Key.Get()==Target; });
	for(int32 I=0;I<Slots.Num();++I) if(Slots[I].Target.Get()==Target) { RestoreMeshes(I); Slots[I]={}; }
	UpdateRendering();
}
int32 UHALDamageFeedbackSubsystem::GetActiveTargetCount() const
{ int32 N=0; for(const auto& S:Slots) { N+=!S.Meshes.IsEmpty(); } return N; }
int32 UHALDamageFeedbackSubsystem::GetLeasedMeshCount() const
{ int32 N=0; for(const auto& S:Slots) { N+=S.Meshes.Num(); } return N; }
void UHALDamageFeedbackSubsystem::Deinitialize()
{
	bStopped=true; if(Handle) { Handle->CancelHandle(); Handle.Reset(); }
	for(int32 I=0;I<Slots.Num();++I) { RestoreMeshes(I); } Slots.Reset(); Pending.Reset();
	if(Volume) { Volume->bEnabled=false; Volume->Destroy(); Volume=nullptr; } Material=nullptr; Config=nullptr;
	Super::Deinitialize();
}
void UHALDamageFeedbackSubsystem::LogStatus() const
{
	UE_LOG(LogTemp,Display,TEXT("DamageFeedback Config=%s Prepared=%d Enabled=%d Targets=%d Meshes=%d Records=%d Pending=%d Pass=%d"),
		*GetNameSafe(Config),bPrepared,Config&&Config->Style.bEnabled,GetActiveTargetCount(),GetLeasedMeshCount(),Slots.Num(),Pending.Num(),Volume&&Volume->bEnabled);
}
#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs GDamageFeedbackPreview(TEXT("hal.DamageFeedbackPreview"),TEXT("hal.DamageFeedbackPreview [local|all]: local cosmetic preview, no damage/RPC."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* W)
		{
			if(!W||Args.Num()>1) { return; } auto* FX=W->GetSubsystem<UHALDamageFeedbackSubsystem>(); if(!FX) { return; }
			const bool All=Args.Num()==1&&Args[0]==TEXT("all");
			for(TActorIterator<ATestVehiclePawn> It(W);It;++It) if(All||It->IsLocallyControlled())
			{ FHALDamageFeedbackPulse P; P.Sequence=1; P.AttackId=FGuid::NewGuid(); P.AttackPulse=1; P.ActualDamage=1; P.ServerTime=FX->ServerTime(W); FX->Receive(*It,P); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GDamageFeedbackStatus(TEXT("hal.DamageFeedbackStatus"),TEXT("Print local shared damage-feedback readiness and rendering resources."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* W)
		{ if(W) if(auto* FX=W->GetSubsystem<UHALDamageFeedbackSubsystem>()) { FX->LogStatus(); } }));
}
#endif
