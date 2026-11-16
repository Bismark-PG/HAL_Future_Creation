#include "HALPursuitHUDWidget.h"
#include "HALPolicePursuit.h"
#include "HALPursuitTargetComponent.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
TArray<FHALPursuitView> UHALPursuitHUDWidget::GatherViews(UWorld* W)
{
	TArray<FHALPursuitView> Out; if(!W) { return Out; }
	TArray<AHALPolicePursuit*> Sources; for(TActorIterator<AHALPolicePursuit> It(W);It;++It) { if(!It->IsActorBeingDestroyed()) { Sources.Add(*It); } }
	Sources.Sort([](const auto& A,const auto& B){return A.GetName()<B.GetName();});
	for(const auto* P:Sources) for(const auto& R:P->GetState().Records)
	{
		if(!IsValid(R.Target)||R.Value<=0) { continue; } const float F=R.Value/P->GetState().Rules.Threshold;
		auto* V=Out.FindByPredicate([&](const auto& X){return X.Target==R.Target;});
		if(!V) { V=&Out.AddDefaulted_GetRef(); V->Target=R.Target; }
		if(F>V->Fraction) { V->Fraction=F; V->Style=P->GetState().FX; }
	}
	for(TActorIterator<ATestVehiclePawn> It(W);It;++It)
	{
		const auto* C=It->FindComponentByClass<UHALPursuitTargetComponent>(); if(!C) { continue; } const auto& S=C->GetState();
		auto* V=Out.FindByPredicate([&](const auto& X){return X.Target==*It;});
		if(!V&&(S.bArrested||S.bProtected)) { V=&Out.AddDefaulted_GetRef(); V->Target=*It; V->Style=S.FX; }
		if(V) { V->bArrested=S.bArrested; V->bProtected=S.bProtected; }
	}
	Out.RemoveAll([](const FHALPursuitView& V){const auto* H=IsValid(V.Target)?V.Target->FindComponentByClass<UVehicleHealthComponent>():nullptr; return !H||!H->IsHealthReady()||H->GetCurrentHP()<=0;});
	return Out;
}
TArray<FHALPursuitView> UHALPursuitHUDWidget::GetPursuitViews() const { return GatherViews(GetWorld()); }
TSharedRef<SWidget> UHALPursuitHUDWidget::RebuildWidget()
{ bNative=!(WidgetTree&&WidgetTree->RootWidget); return bNative?SNew(SBox):Super::RebuildWidget(); }
void UHALPursuitHUDWidget::NativeConstruct()
{ Super::NativeConstruct(); SetVisibility(ESlateVisibility::HitTestInvisible); GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::Refresh,.1f,true); Refresh(); }
void UHALPursuitHUDWidget::NativeDestruct()
{ if(GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(Timer); } Super::NativeDestruct(); }
void UHALPursuitHUDWidget::Refresh() { ReceivePursuitViews(GetPursuitViews()); }
void UHALPursuitHUDWidget::PaintStars(const FGeometry& G,FSlateWindowElementList& E,int32 Layer,FVector2D Center,float Fraction,const FHALPursuitFX& S)
{
	const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
	const float Width=S.StarCount*S.StarSize+(S.StarCount-1)*S.StarGap;
	for(int32 Star=0;Star<S.StarCount;++Star)
	{
		const FVector2f C(Center.X-Width*.5+S.StarSize*.5+Star*(S.StarSize+S.StarGap),Center.Y);
		TArray<FVector2f> Points;
		for(int32 J=0;J<10;++J) { const float A=-UE_PI*.5+J*UE_PI/5,R=S.StarSize*.5f*(J%2?.44f:1); Points.Add(C+FVector2f(FMath::Cos(A)*R,FMath::Sin(A)*R)); }
		const float Fill=FMath::Clamp(Fraction*S.StarCount-Star,0.f,1.f),Limit=C.X-S.StarSize*.5f+S.StarSize*Fill;
		TArray<FSlateVertex> Vertices; TArray<SlateIndex> Indices;
		if(Fill>0) for(int32 J=0;J<10;++J)
		{
			const TArray<FVector2f> Triangle{C,Points[J],Points[(J+1)%10]}; TArray<FVector2f> Clip;
			for(int32 K=0;K<3;++K)
			{
				const auto A=Triangle[K],B=Triangle[(K+1)%3]; const bool IA=A.X<=Limit,IB=B.X<=Limit;
				if(IA) { Clip.Add(A); } if(IA!=IB) { Clip.Add(FMath::Lerp(A,B,(Limit-A.X)/(B.X-A.X))); }
			}
			for(int32 K=1;K+1<Clip.Num();++K) for(auto P:{Clip[0],Clip[K],Clip[K+1]})
			{ Indices.Add(SlateIndex(Vertices.Num())); Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),P,FVector2f(.5f),S.StarColor.ToFColor(true))); }
		}
		if(Vertices.Num()) { FSlateDrawElement::MakeCustomVerts(E,Layer,Resource,Vertices,Indices,nullptr,0,0); }
		const FVector2f First=Points[0]; Points.Add(First);
		FSlateDrawElement::MakeLines(E,Layer+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,S.StarOutline,true,1.6f);
	}
}
int32 UHALPursuitHUDWidget::NativePaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& C,FSlateWindowElementList& E,int32 L,const FWidgetStyle& S,bool B) const
{
	const int32 Base=Super::NativePaint(A,G,C,E,L,S,B); if(!bNative||!GetOwningPlayer()) { return Base; }
	for(const auto& V:GetPursuitViews())
	{
		FVector Center,Extent; V.Target->GetActorBounds(true,Center,Extent); FVector2D Screen;
		if(!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),Center+FVector(0,0,Extent.Z+30),Screen,true)) { continue; }
		if(V.Fraction>0) { PaintStars(G,E,Base+1,Screen-FVector2D(0,V.Style.StarsAboveHealth),V.Fraction,V.Style); }
		if(V.bArrested)
		{
			FSlateDrawElement::MakeText(E,Base+3,G.ToPaintGeometry(FVector2D(1),FSlateLayoutTransform(Screen-FVector2D(34,V.Style.StarsAboveHealth))),
				TEXT("ARRESTED"),FCoreStyle::GetDefaultFontStyle("Bold",13),ESlateDrawEffect::None,V.Style.Red);
		}
	}
	return Base+3;
}
void UHALPursuitHUDSubsystem::EnsureForWorld(UWorld* W,TSubclassOf<UHALPursuitHUDWidget> Class)
{
	if(!W||!FApp::CanEverRender()) { return; }
	for(auto It=W->GetPlayerControllerIterator();It;++It)
	{
		auto* PC=It->Get(); auto* LP=PC&&PC->IsLocalController()?PC->GetLocalPlayer():nullptr; if(!LP) { continue; }
		auto* S=LP->GetSubsystem<UHALPursuitHUDSubsystem>(); if(!S) { continue; }
		const TSubclassOf<UHALPursuitHUDWidget> Wanted=Class?Class:TSubclassOf<UHALPursuitHUDWidget>(UHALPursuitHUDWidget::StaticClass());
		if(S->Widget&&(S->Widget->GetWorld()!=W||S->Widget->GetClass()!=Wanted)) { S->Widget->RemoveFromParent(); S->Widget=nullptr; }
		if(!S->Widget)
		{
			S->Widget=CreateWidget<UHALPursuitHUDWidget>(PC,Wanted);
			if(S->Widget) { S->Widget->AddToViewport(15); }
			if(!S->Cleanup.IsValid()) { S->Cleanup=FWorldDelegates::OnWorldCleanup.AddUObject(S,&ThisClass::CleanupWorld); }
		}
	}
}
void UHALPursuitHUDSubsystem::CleanupWorld(UWorld* W,bool,bool)
{ if(Widget&&Widget->GetWorld()==W) { Widget->RemoveFromParent(); Widget=nullptr; } }
void UHALPursuitHUDSubsystem::Deinitialize()
{ if(Widget) { Widget->RemoveFromParent(); Widget=nullptr; } FWorldDelegates::OnWorldCleanup.Remove(Cleanup); Super::Deinitialize(); }
