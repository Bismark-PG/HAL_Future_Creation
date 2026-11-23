#include "HALMeteorBall.h"

#include "Blueprint/WidgetTree.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "TestVehiclePawn.h"
#include "Widgets/Layout/SBox.h"

namespace
{
void ConfigureMeteorVisual(UPrimitiveComponent& Component)
{
	Component.SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component.SetGenerateOverlapEvents(false);
	Component.SetCanEverAffectNavigation(false);
	Component.SetCastShadow(false);
	Component.SetVisibility(false);
}

UMaterialInstanceDynamic* PrepareMeteorPlume(UInstancedStaticMeshComponent& Group, UStaticMesh* Mesh, UMaterialInterface* Material, FLinearColor Tint, int32 Count)
{
	Group.SetStaticMesh(Mesh);
	auto* MID = Group.CreateDynamicMaterialInstance(0, Material);
	if (MID) { MID->SetVectorParameterValue(TEXT("Tint"), Tint); }
	for (int32 I = 0; I < Count; ++I) { Group.AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::OneVector)); }
	return MID;
}

float FallingCenterHeight(const UHALMeteorBallDefinition& Type, double SecondsSinceLock)
{
	// The rock and each detached puff sample the same flight path at different times.
	const float Progress = FMath::Clamp(SecondsSinceLock / Type.FallSeconds, 0., 1.);
	const float NearHeight = FMath::Min(1300.f, Type.SkyHeight * .72f);
	const float Far = FMath::Max(0.f, 1.f - Progress / .40f);
	return .5f * Type.FallingRockRadius + (Type.SkyHeight - NearHeight) * Far * Far + NearHeight * (1.f - Progress * Progress);
}
}

AHALMeteorStrike::AHALMeteorStrike()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StrikeOrigin")));
	Rock = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Meteor"));
	GroundFX = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowAndRadius"));
	Flames = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FlameTongues"));
	Smoke = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SmokePuffs"));
	DescentSmoke = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DescentSmoke"));
	Airflow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeteorAirflow"));
	Marker = CreateDefaultSubobject<UWidgetComponent>(TEXT("TrackingMarker"));
	for (UPrimitiveComponent* Part : TArray<UPrimitiveComponent*>{Rock, GroundFX, Flames, Smoke, DescentSmoke, Airflow, Marker})
	{
		Part->SetupAttachment(RootComponent);
		ConfigureMeteorVisual(*Part);
	}
	for (const FName Name : {FName(TEXT("ImpactCloudA")), FName(TEXT("ImpactCloudB")), FName(TEXT("ImpactCloudC")),
		FName(TEXT("ImpactVaporA")), FName(TEXT("ImpactVaporB")), FName(TEXT("ImpactVaporC"))})
	{
		auto* Cloud = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Cloud->SetupAttachment(RootComponent);
		ConfigureMeteorVisual(*Cloud);
		ImpactClouds.Add(Cloud);
	}
	Marker->SetWidgetSpace(EWidgetSpace::Screen);
	Marker->SetPivot(FVector2D(.5f,1.f));
}

void AHALMeteorStrike::RefreshPresentation()
{
	auto* Type = State.Type.Get();
	if (!Type || (!HasActorBegunPlay() && !IsActorBeginningPlay()) || !Rock->IsRegistered()
		|| !FApp::CanEverRender() || GetNetMode() == NM_DedicatedServer) { return; }
	if (!GroundMID)
	{
		GroundFX->SetStaticMesh(Type->GroundPlane);
		GroundMID = GroundFX->CreateDynamicMaterialInstance(0, Type->WarningMaterial);
		if (!GroundMID) { return; }
		GroundMID->SetVectorParameterValue(TEXT("RimColor"), Type->CoronaColor);
		const float PlaneWidth = Type->GroundPlane->GetBounds().BoxExtent.X * 2.f;
		GroundFX->SetWorldScale3D(FVector(2.f * Type->DamageRadius / FMath::Max(1.f,PlaneWidth)));
		GroundFX->SetTranslucentSortPriority(1);
		PrepareImpactFX();
		Rock->SetStaticMesh(Type->Visual.Mesh);
		if (Type->Visual.Materials.Num())
		{
			if (auto* Surface = Rock->CreateDynamicMaterialInstance(0,Type->Visual.Materials[0]))
			{
				Surface->SetVectorParameterValue(TEXT("CoreColor"),Type->CoreColor);
				Surface->SetVectorParameterValue(TEXT("CurrentColor"),Type->CurrentColor);
				Surface->SetVectorParameterValue(TEXT("CoronaColor"),Type->CoronaColor);
				Surface->SetScalarParameterValue(TEXT("Glow"),Type->Glow);
				Surface->SetScalarParameterValue(TEXT("FlowSpeed"),Type->SurfaceFlowSpeed);
			}
		}
		PrepareMeteorPlume(*Flames,Type->FlameMesh,Type->PlumeMaterial,Type->CurrentColor,8);
		PrepareMeteorPlume(*Smoke,Type->Visual.Mesh,Type->PlumeMaterial,Type->SmokeColor,10);
		DescentSmoke->SetHasPerInstancePrevTransforms(true);
		DescentSmokeMID = PrepareMeteorPlume(*DescentSmoke, Type->Visual.Mesh,
			Type->DescentSmokeMaterial ? Type->DescentSmokeMaterial.Get() : Type->PlumeMaterial.Get(), Type->SmokeColor, 24);
		PuffTransforms.Init(FTransform(FQuat::Identity, GroundFX->GetComponentLocation(), FVector::ZeroVector), DescentSmoke->GetInstanceCount());
		PreviousPuffTransforms = PuffTransforms;
		if (Type->AirflowMesh)
		{
			Airflow->SetStaticMesh(Type->AirflowMesh);
			AirflowMID = Airflow->CreateDynamicMaterialInstance(0, Type->AirflowMaterial);
			if (AirflowMID)
			{
				AirflowMID->SetScalarParameterValue(TEXT("UseRuntimeTime"), 1.f);
				AirflowMID->SetScalarParameterValue(TEXT("FlowSpeed"), Type->AirflowSpeed);
				AirflowMID->SetScalarParameterValue(TEXT("Opacity"), Type->AirflowStrength);
				AirflowMID->SetScalarParameterValue(TEXT("ContourWidth"), .10f);
				AirflowMID->SetScalarParameterValue(TEXT("Turbulence"), 5.f);
			}
		}
		Marker->SetWidgetClass(Type->MarkerWidgetClass ? Type->MarkerWidgetClass.Get() : UHALMeteorMarkerWidget::StaticClass());
		Marker->SetDrawSize(FVector2D(Type->MarkerSize,Type->MarkerSize+Type->MarkerLift));
		Marker->InitWidget();
	}
	SetActorTickEnabled(true);
	DrawFlight(FMath::Max(0., ServerNow() - State.Started));
}

void AHALMeteorStrike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State.Type && GroundMID) { DrawFlight(FMath::Max(0., ServerNow() - State.Started)); }
}

void AHALMeteorStrike::DrawDescentSmoke(double SecondsSinceLock)
{
	const auto& Type = *State.Type;
	const float Radius = Type.FallingRockRadius;
	const float MeshRadius = FMath::Max(1.f, float(Type.Visual.Mesh->GetBounds().BoxExtent.GetMax()));
	const int32 Count = DescentSmoke->GetInstanceCount();
	const int32 Pairs = Count / 2;
	const double Lifetime = Type.SmokeLifetime;
	const double EmittedUntil = FMath::Clamp(SecondsSinceLock, 0., double(Type.FallSeconds));
	const float HeadHeight = FallingCenterHeight(Type, EmittedUntil);
	const float Reach = Radius * Type.TrailLength * 2.5f;
	const FVector Ground(State.Ground);
	const FVector Wind(.62, -.78, 0.);
	Swap(PuffTransforms, PreviousPuffTransforms);
	if (DescentSmokeMID) { DescentSmokeMID->SetScalarParameterValue(TEXT("SmokeTime"), SecondsSinceLock); }
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Keep every puff in world space at its emission height, including across impact or a late replication update.
		const int32 Pair = Index / 2;
		const double FirstBirth = (Pair + .5) * Lifetime / Pairs;
		const int32 Cycle = FMath::FloorToInt((EmittedUntil - FirstBirth) / Lifetime);
		const double Birth = FirstBirth + Cycle * Lifetime;
		const float Life = (SecondsSinceLock - Birth) / Lifetime;
		FTransform Puff(FQuat::Identity, Ground, FVector::ZeroVector);
		if (Birth >= 0. && Life >= 0.f && Life < 1.f)
		{
			const float Seed = (Pair + Cycle * Pairs) * 2.39996f;
			const float Side = Index % 2 ? -1.f : 1.f;
			const float Angle = Seed + Side * (1.12f + 1.90f * Life);
			const FVector Roll(FMath::Cos(Angle), FMath::Sin(Angle), 0.);
			const float Height = FallingCenterHeight(Type, Birth);
			const float LengthFade = 1.f - FMath::SmoothStep(.70f * Reach, Reach, Height - HeadHeight);
			const float Envelope = FMath::SmoothStep(0.f,.14f,Life) * (1.f - FMath::SmoothStep(.52f,1.f,Life));
			const float Size = (.28f + .46f * Life) * (1.f + .19f * FMath::Cos(Seed + Side)) * Envelope * LengthFade;
			FVector Center = Ground + FVector::UpVector * (Height + Radius * (.44f + .75f * Life));
			Center += Radius * (Roll * .21f + Type.SmokeSpread * (Roll * (.88f * Life) + Wind * (.97f * Life * Life)));
			const FQuat Rotation(FVector(.54, -.31, .78).GetSafeNormal(), Seed + Side * 3.1f * Life);
			Puff = FTransform(Rotation, Center, FVector(1.16, .88, 1.04) * (Size * Radius / MeshRadius));
		}
		PuffTransforms[Index] = Puff;
		// A recycled puff starts with no screen velocity. Continuing puffs supply motion vectors for temporal AA.
		if (PreviousPuffTransforms[Index].GetScale3D().IsNearlyZero() || Puff.GetScale3D().IsNearlyZero())
		{
			PreviousPuffTransforms[Index] = Puff;
		}
	}
	DescentSmoke->BatchUpdateInstancesTransforms(0, PuffTransforms, PreviousPuffTransforms, true, true, true);
}

void AHALMeteorStrike::DrawFlight(double Age)
{
	const auto& Type = *State.Type;
	const bool bTracking = State.Phase == EHALMeteorPhase::Tracking;
	const bool bFalling = State.Phase == EHALMeteorPhase::Falling;
	const bool bImpact = State.Phase == EHALMeteorPhase::Impact;
	const double SecondsSinceLock = Age - Type.TrackingSeconds;
	const float Fall = FMath::Clamp((Age-Type.TrackingSeconds)/Type.FallSeconds,0.,1.);
	const double SecondsAfterImpact = SecondsSinceLock - Type.FallSeconds;
	const float After = FMath::Clamp(SecondsAfterImpact / Type.ImpactSeconds, 0., 1.);
	const float RiseTime = FMath::Min(Type.TrackingSeconds,Type.RiseSeconds);
	const bool bRising = bTracking && Age < RiseTime;
	const bool bMark = bTracking && Age < Type.TrackingSeconds && IsValid(State.Target);
	Marker->SetVisibility(bMark);
	if (bMark)
	{
		FVector Center,Extent;
		State.Target->GetActorBounds(true,Center,Extent);
		Marker->SetWorldLocation(Center + FVector::UpVector * (Extent.Z+30.));
		if (auto* Widget = Cast<UHALMeteorMarkerWidget>(Marker->GetUserWidgetObject()))
		{ Widget->SetLock(State.Type, FMath::Clamp(Age/Type.TrackingSeconds,0.,1.)); }
	}

	const FVector Ground(State.Ground);
	GroundFX->SetVisibility(bFalling || (bImpact && After < 1.f));
	if (!bTracking)
	{
		GroundFX->SetWorldLocation(Ground + FVector::UpVector*3.);
		if (bImpact && ImpactGroundMID)
		{
			if (GroundFX->GetMaterial(0) != ImpactGroundMID) { GroundFX->SetMaterial(0, ImpactGroundMID); }
			ImpactGroundMID->SetScalarParameterValue(TEXT("Progress"), After);
		}
		else
		{
			GroundMID->SetScalarParameterValue(TEXT("FallProgress"), Fall);
			GroundMID->SetScalarParameterValue(TEXT("ImpactProgress"), bImpact ? After : -1.f);
		}
	}
	if (bImpact)
	{
		if (bUsesVolumeClouds) { DrawVolumeClouds(SecondsAfterImpact); }
		else { DrawCrestSheets(After); }
	}
	const bool bFlight = bRising || bFalling;
	Rock->SetVisibility(bFlight);
	const bool bTail = bFlight && Type.TrailLength > 0;
	const bool bImpactPlume = bImpact && After < 1.f && CloudMIDs.IsEmpty();
	Flames->SetVisibility(bTail || bImpactPlume);
	Smoke->SetVisibility((bRising && bTail) || bImpactPlume);
	const bool bWake = (bFalling || bImpact) && Type.TrailLength > 0.f
		&& SecondsSinceLock >= 0. && SecondsSinceLock < Type.FallSeconds + Type.SmokeLifetime;
	DescentSmoke->SetVisibility(bWake);
	if (bWake) { DrawDescentSmoke(SecondsSinceLock); }
	const bool bAir = bFalling && AirflowMID && Type.AirflowStrength > 0.f;
	Airflow->SetVisibility(bAir);
	if (!bFlight && !bImpact) { return; }
	if (bImpact && After >= 1.f)
	{
		if (!bWake && SecondsAfterImpact >= Type.GetCloudDuration()) { SetActorTickEnabled(false); }
		return;
	}

	const float Radius = Type.FallingRockRadius;
	const float MeshRadius = FMath::Max(1.f, float(Type.Visual.Mesh->GetBounds().BoxExtent.GetMax()));
	FVector Head = Ground + FVector::UpVector * Radius * .50f;
	if (bRising)
	{
		const double Rise = Age/RiseTime;
		Head = FVector(State.LaunchPoint) + FVector::UpVector * (Type.SkyHeight * Rise * (.35 + .65*Rise));
	}
	else if (bFalling)
	{
		Head.Z = Ground.Z + FallingCenterHeight(Type, SecondsSinceLock);
	}
	if (bFlight)
	{
		const FQuat Tumble = bFalling
			? FQuat(FVector(.68,.37,.63).GetSafeNormal(), FMath::DegreesToRadians(Type.RockSpinRate * SecondsSinceLock))
			: FRotator(Age*125.,Age*160.,Age*30.).Quaternion();
		const FVector RockShape = bFalling ? FVector(1.09,.88,1.02) : FVector::OneVector;
		Rock->SetWorldTransform(FTransform(Tumble, Head, RockShape * (Radius / MeshRadius)));
	}
	if (bAir)
	{
		const FBox Box = Type.AirflowMesh->GetBoundingBox();
		const FVector Extent = Box.GetSize();
		const double SideScale = 3.8 * Radius / FMath::Max(1., Extent.Y);
		const double LengthScale = 3.7 * Radius / FMath::Max(1., Extent.X);
		const FRotator Down(-90., 25., 0.);
		const FVector Nose = Head - FVector::UpVector * (1.04 * Radius);
		Airflow->SetWorldTransform(FTransform(Down, Nose + FVector::UpVector * (Box.Max.X * LengthScale),
			FVector(LengthScale, SideScale, SideScale)));
		AirflowMID->SetScalarParameterValue(TEXT("MotionTime"), SecondsSinceLock);
		AirflowMID->SetScalarParameterValue(TEXT("Envelope"), FMath::SmoothStep(0.f,.15f,Fall) * (1.f-FMath::SmoothStep(.97f,1.f,Fall)));
	}
	if (!bTail && !bImpactPlume) { return; }
	auto DrawPlume = [&](UInstancedStaticMeshComponent& Group, bool bSmoke)
	{
		const int32 Count = Group.GetInstanceCount();
		for (int32 I = 0; I < Count; ++I)
		{
			const float S = bImpact ? (I+.5f)/Count : FMath::Frac((I + float(Age)*(bSmoke ? 6.f : 9.f))/Count);
			const float Angle = I * 2.39996f;
			const FVector Radial(FMath::Cos(Angle),FMath::Sin(Angle),0);
			FVector P;
			FVector Scale;
			FQuat Rotation = FQuat::Identity;
			if (bImpact)
			{
				const float Travel = After*(2.f-After);
				P = Ground + Radial * Type.DamageRadius * Travel * (bSmoke ? .42f+.32f*S : .62f+.32f*S);
				P.Z += Radius*(.12f+(bSmoke ? .35f+1.5f*S : 2.2f+S)*FMath::Sin(After*UE_PI) + (bSmoke ? .6f*After : 0.f));
				Scale = FVector(bSmoke ? .59f : .18f) * Radius/MeshRadius * (1.f-FMath::SmoothStep(.25f,1.f,After));
				if (!bSmoke)
				{
					Scale.Z *= 2.2f;
					Rotation = FQuat::FindBetweenNormals(FVector::UpVector,(Radial+FVector::UpVector*.5f).GetSafeNormal());
				}
			}
			else
			{
				const float Wave = FMath::Sin(float(Age)*11.f+Angle);
				const float Trail = (bSmoke ? .48f+1.10f*S : .08f+.68f*S)*Type.TrailLength;
				P = Head + FVector::UpVector * Radius * Trail * (bRising ? -1.f : 1.f);
				P += (Radial*(bSmoke ? (.13f+.30f*S) : .35f) + FVector(.30f,-.18f,0)*Trail) * Radius;
				const float Fade = FMath::SmoothStep(0.f,.09f,S)*(1.f-FMath::SmoothStep(.73f,1.f,S));
				Scale = FVector(bSmoke ? (.51f+.10f*Wave) : .64f*(1.f-.52f*S)) * Fade * Radius/MeshRadius;
				Scale.Z *= bSmoke ? 1.12f : 2.75f;
				if (!bSmoke)
				{
					const FVector TailDirection = FVector(.25,-.14,bRising ? -1. : 1.) + Radial*(.17f+.14f*Wave);
					Rotation = FQuat::FindBetweenNormals(FVector::UpVector,TailDirection.GetSafeNormal());
				}
			}
			Group.UpdateInstanceTransform(I,FTransform(Rotation,P,Scale),true,I==Count-1,true);
		}
	};
	DrawPlume(*Flames,false);
	if (!bFalling) { DrawPlume(*Smoke,true); }
}

TSharedRef<SWidget> UHALMeteorMarkerWidget::RebuildWidget()
{
	bNative = !(WidgetTree && WidgetTree->RootWidget);
	return bNative ? SNew(SBox) : Super::RebuildWidget();
}

void UHALMeteorMarkerWidget::SetLock(UHALMeteorBallDefinition* Type, float Progress)
{
	if (BallType == Type && TrackingProgress == Progress) { return; }
	BallType = Type;
	TrackingProgress = Progress;
	if (!bNative) { ReceiveTrackingProgress(Progress); }
	InvalidateLayoutAndVolatility();
}

int32 UHALMeteorMarkerWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	const int32 Base = Super::NativePaint(Args,G,Culling,Elements,Layer,Style,bEnabled);
	if (!bNative || !BallType) { return Base; }
	const float Size = G.GetLocalSize().X;
	auto Draw = [&](UTexture2D* Texture, float Scale, int32 Z)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		const FVector2D Box(Size*Scale);
		const FVector2D Offset((Size-Box.X)*.5);
		FSlateDrawElement::MakeBox(Elements,Z,G.ToPaintGeometry(Box,FSlateLayoutTransform(Offset)),&Brush);
	};
	const float Closed = BallType->MarkerClosedScale;
	Draw(BallType->MarkerJaws,FMath::Lerp(1.f,Closed,FMath::SmoothStep(0.f,1.f,TrackingProgress)),Base+1);
	Draw(BallType->MarkerCore,Closed,Base+2);
	return Base+2;
}
