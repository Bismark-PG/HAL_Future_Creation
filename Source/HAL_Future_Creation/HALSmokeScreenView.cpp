#include "HALSmokeScreenView.h"
#include "HALSmokeScreen.h"
#include "HALPlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "HAL/IConsoleManager.h"

bool UHALSmokeScreenView::ShouldHideWorldTarget(const AActor* Target) { return HALSmoke::IsConcealed(Target); }
TSharedRef<SWidget> UHALSmokeScreenView::RebuildWidget() { return SNew(SBox); }
void UHALSmokeScreenView::NativeConstruct()
{
	Super::NativeConstruct(); SetVisibility(ESlateVisibility::HitTestInvisible); SetIsFocusable(false);
	ForceVolatile(true); // Camera projection and cloud growth must repaint while this view is active.
}
void UHALSmokeScreenView::NativeTick(const FGeometry& G,float Delta)
{
	Super::NativeTick(G,Delta); auto* PC=GetOwningPlayer(); if(!PC||!PC->IsLocalController()) { return; }
	APawn* Pawn=PC->GetPawn();
	if(const auto* Match=Cast<AHALPlayerController>(PC);Match&&Match->IsRoundSpectating()) { Pawn=nullptr; }
	if(!IsValid(Pawn)||Pawn->IsActorBeingDestroyed()) { Pawn=nullptr; }
	if(LastPawn.Get()!=Pawn) { Obscuration=0; bHasPosition=false; LastPawn=Pawn; }
	TSet<AHALSmokeScreen*> Active; float Desired=0; const FHALSmokeScreenFX* Chosen=nullptr;
	const double Now=HALSmoke::ServerTime(GetWorld());
	for(TActorIterator<AHALSmokeScreen> It(GetWorld());It;++It)
	{
		auto* Source=*It; if(Source->IsActorBeingDestroyed()||Source->GetStartTime()<0) { continue; }
		Active.Add(Source); const auto& FX=Source->GetFX();
		if(FX.CloudMaterial)
		{
			auto& M=CloudMaterials.FindOrAdd(Source);
			if(!M||M->Parent!=FX.CloudMaterial) { M=UMaterialInstanceDynamic::Create(FX.CloudMaterial,this); }
			M->SetScalarParameterValue(TEXT("Clock"),float(Now)); M->SetScalarParameterValue(TEXT("FlowSpeed"),FX.FlowSpeed);
			M->SetVectorParameterValue(TEXT("DarkColor"),FX.DarkColor); M->SetVectorParameterValue(TEXT("LightColor"),FX.LightColor);
		}
		const float Weight=Pawn?Source->Influence(*Pawn,true,Now,bHasPosition?&PreviousPosition:nullptr)*FX.MaximumObscuration:0;
		if(!Chosen||Weight>Desired) { Chosen=&FX; Desired=Weight; }
	}
	for(auto It=CloudMaterials.CreateIterator();It;++It) { if(!Active.Contains(It.Key())) { It.RemoveCurrent(); } }
	if(Pawn) { PreviousPosition=HALSmoke::TargetPosition(*Pawn); bHasPosition=true; }
	else { bHasPosition=false; Obscuration=0; }
	if(Chosen)
	{
		FadeIn=Chosen->FadeInSeconds; FadeOut=Chosen->FadeOutSeconds;
		if(Chosen->MaximumObscuration>0) { FullStrength=Chosen->MaximumObscuration; }
		if(Chosen->ScreenMaterial)
		{
			if(!ScreenMaterial||ScreenMaterial->Parent!=Chosen->ScreenMaterial) { ScreenMaterial=UMaterialInstanceDynamic::Create(Chosen->ScreenMaterial,this); }
			ScreenMaterial->SetScalarParameterValue(TEXT("Clock"),float(Now)); ScreenMaterial->SetScalarParameterValue(TEXT("FlowSpeed"),Chosen->FlowSpeed);
			ScreenMaterial->SetVectorParameterValue(TEXT("DarkColor"),Chosen->DarkColor); ScreenMaterial->SetVectorParameterValue(TEXT("LightColor"),Chosen->LightColor);
		}
	}
	// Preserve the last style while fading after the final source is removed.
	Obscuration=HALSmoke::AdvanceObscuration(Obscuration,Desired,Delta,FadeIn,FadeOut,FullStrength);
	if(ScreenMaterial)
	{ ScreenMaterial->SetScalarParameterValue(TEXT("Strength"),Obscuration); ScreenMaterial->SetScalarParameterValue(TEXT("Clock"),float(Now)); }
	if(Active.IsEmpty()&&Obscuration<=.001f)
	{ SetVisibility(ESlateVisibility::Collapsed); CloudMaterials.Reset(); ScreenMaterial=nullptr; bHasPosition=false; LastPawn.Reset(); }
}
int32 UHALSmokeScreenView::NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& E,int32 L,const FWidgetStyle& S,bool B) const
{
	const int32 Base=Super::NativePaint(A,G,C,E,L,S,B);
	if(!GetOwningPlayer()||!FSlateApplication::IsInitialized()) { return Base; }
	static const auto* Debug=IConsoleManager::Get().FindConsoleVariable(TEXT("hal.SmokeDebug"));
	if(Debug&&Debug->GetInt()>=2) { return Base; } // Explicit local calibration mode; no gameplay bypass.
	const double Now=HALSmoke::ServerTime(GetWorld());
	for(const auto& Pair:CloudMaterials)
	{
		const auto* Source=Pair.Key.Get(); auto* Material=Pair.Value.Get();
		if(!IsValid(Source)||Source->IsActorBeingDestroyed()||!Material) { continue; }
		const auto& R=Source->GetRules(); FSlateBrush Brush; Brush.SetResourceObject(Material);
		const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(Brush);
		for(const auto& P:Source->GetPuffs())
		{
			const float Age=float(Now-Source->GetStartTime())-P.BirthAge;
			const float Alpha=R.Density(Age)*Source->GetFX().CloudOpacity; if(Alpha<=.001f) { continue; }
			const float Radius=R.Radius(Age); const FVector Center=FVector(P.Ground)+FVector(0,0,R.Height*.5f);
			TArray<FSlateVertex> Vertices; TArray<SlateIndex> Indices; bool Valid=true;
			const FColor Color=FLinearColor(1,1,1,Alpha).ToFColor(false);
			const auto Add=[&](FVector World,FVector2f UV)
			{
				FVector2D Screen;
				if(!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),World,Screen,true)) { Valid=false; return; }
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(Screen),FVector4f(UV.X,UV.Y,1,1),UV,Color));
			};
			Add(Center,FVector2f(.5f));
			for(int32 I=0;I<=R.WallRays;++I)
			{
				const float Angle=2*PI*I/R.WallRays,Length=FMath::Min(Radius,P.Boundary(Angle));
				const FVector2f XY(FMath::Cos(Angle)*Length,FMath::Sin(Angle)*Length);
				Add(Center+FVector(XY.X,XY.Y,0),FVector2f(.5f)+FVector2f(FMath::Cos(Angle),FMath::Sin(Angle))*.5f);
				if(I>0) { Indices.Append({SlateIndex(0),SlateIndex(I),SlateIndex(I+1)}); }
			}
			if(Valid) { FSlateDrawElement::MakeCustomVerts(E,Base+1,Resource,Vertices,Indices,nullptr,0,0); }
		}
	}
	if(ScreenMaterial&&Obscuration>.001f)
	{
		FSlateBrush Brush; Brush.SetResourceObject(ScreenMaterial);
		FSlateDrawElement::MakeBox(E,Base+2,G.ToPaintGeometry(),&Brush,ESlateDrawEffect::None,FLinearColor::White);
	}
	return Base+2;
}
void UHALSmokeViewSubsystem::EnsureForWorld(UWorld* W)
{
	if(!W||!W->IsGameWorld()||!FApp::CanEverRender()||W->bIsTearingDown) { return; }
	for(auto It=W->GetPlayerControllerIterator();It;++It)
	{
		auto* PC=It->Get(); auto* LP=PC&&PC->IsLocalController()?PC->GetLocalPlayer():nullptr; if(!LP) { continue; }
		auto* Subsystem=LP->GetSubsystem<UHALSmokeViewSubsystem>(); if(!Subsystem) { continue; }
		if(Subsystem->View&&Subsystem->View->GetWorld()!=W) { Subsystem->View->RemoveFromParent(); Subsystem->View=nullptr; }
		if(!Subsystem->View)
		{
			Subsystem->View=CreateWidget<UHALSmokeScreenView>(PC,UHALSmokeScreenView::StaticClass());
			if(Subsystem->View) { Subsystem->View->AddToPlayerScreen(-1000); }
			if(!Subsystem->Cleanup.IsValid()) { Subsystem->Cleanup=FWorldDelegates::OnWorldCleanup.AddUObject(Subsystem,&ThisClass::CleanupWorld); }
		}
		if(Subsystem->View) { Subsystem->View->SetVisibility(ESlateVisibility::HitTestInvisible); }
	}
}
void UHALSmokeViewSubsystem::CleanupWorld(UWorld* W,bool,bool)
{ if(View&&View->GetWorld()==W) { View->RemoveFromParent(); View=nullptr; } }
void UHALSmokeViewSubsystem::Deinitialize()
{
	if(View) { View->RemoveFromParent(); View=nullptr; }
	FWorldDelegates::OnWorldCleanup.Remove(Cleanup); Super::Deinitialize();
}
