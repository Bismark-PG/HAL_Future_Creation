#include "HALBatarang.h"
#include "HALBatarangAbility.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	using namespace UE::Geometry;
	constexpr int32 WakeSegments = 12;
	struct FPuff { FVector Offset; double Radius; float Birth, End; };
	// Offsets are in the knockback frame. Leave most of the struck vehicle visible from above.
	const FPuff Puffs[] = {
		{FVector(-.34,-.05,.21), .37, 0.f, .94f},
		{FVector( .05, .31,.16), .28, .045f, .86f},
		{FVector(-.04,-.31,.23), .30, .08f, 1.f},
	};

	// One strip also serves as the stamp's UV quad. The head of the trail and the
	// forward end of the shock both lie at U=1, independent of camera or mirror.
	FDynamicMesh3 MakeStrip(int32 Segments)
	{
		FDynamicMesh3 Mesh;
		Mesh.EnableAttributes();
		auto* UV = Mesh.Attributes()->PrimaryUV();
		auto* Normals = Mesh.Attributes()->PrimaryNormals();
		for (int32 Row = 0; Row <= Segments; ++Row)
		{
			const float U = float(Row) / Segments;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				Mesh.AppendVertex(FVector(2*U-1,2*Side-1,0));
				UV->AppendElement(FVector2f(U,float(Side)));
				Normals->AppendElement(FVector3f::UnitZ());
			}
		}
		for (int32 Row = 0; Row < Segments; ++Row)
		{
			const int32 Left = Row*2;
			for (const FIndex3i Face : { FIndex3i(Left,Left+1,Left+2), FIndex3i(Left+2,Left+1,Left+3) })
			{
				const int32 ID = Mesh.AppendTriangle(Face);
				UV->SetTriangle(ID,Face);
				Normals->SetTriangle(ID,Face);
			}
		}
		return Mesh;
	}

	void AttachVisual(UMeshComponent& Mesh, USceneComponent* Root)
	{
		Mesh.SetupAttachment(Root);
		Mesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh.SetGenerateOverlapEvents(false);
		Mesh.SetCanEverAffectNavigation(false);
		Mesh.SetCastShadow(false);
		Mesh.bReceivesDecals = false;
		Mesh.SetVisibility(false);
		Mesh.RegisterComponent();
	}
}

void AHALBatarang::PrepareArt()
{
	Blade = NewObject<UStaticMeshComponent>(this,TEXT("ResonanceBlade"));
	Blade->SetStaticMesh(State.Config->BladeMesh);
	AttachVisual(*Blade,GetRootComponent());
	const double AuthoredSpan = FMath::Max(1.,2. * State.Config->BladeMesh->GetBounds().BoxExtent.Y);
	Blade->SetWorldScale3D(FVector(State.Config->WingSpan/AuthoredSpan));
	BladeInk = Blade->CreateDynamicMaterialInstance(0);
	if (State.Config->WakeMaterial && State.Config->WakeSeconds > 0)
	{
		Wake = NewObject<UDynamicMeshComponent>(this,TEXT("FlightWake"));
		AttachVisual(*Wake,GetRootComponent());
		Wake->SetMesh(MakeStrip(WakeSegments));
		WakeInk = Wake->CreateDynamicMaterialInstance(0,State.Config->WakeMaterial);
	}
	if (State.Config->ResonanceMaterial)
	{
		Shape->SetMesh(MakeStrip(1));
		CueInk = Shape->CreateDynamicMaterialInstance(0,State.Config->ResonanceMaterial);
	}
	bArtReady = true;
}

void AHALBatarang::UpdateWake(float Age)
{
	const auto& Config = *State.Config;
	const float Head = FMath::Clamp(Age/Config.FlightSeconds,0.f,1.f);
	const float Tail = FMath::Max(0.f, Head - Config.WakeSeconds/Config.FlightSeconds);
	const FVector HeadPoint = FlightPoint(Head);
	Wake->SetWorldLocation(HeadPoint);
	auto* Mesh = Wake->GetMesh();
	FVector Centers[WakeSegments+1];
	for (int32 Row = 0; Row <= WakeSegments; ++Row)
	{
		Centers[Row] = FlightPoint(FMath::Lerp(Tail,Head,float(Row)/WakeSegments)) - HeadPoint;
	}
	for (int32 Row = 0; Row <= WakeSegments; ++Row)
	{
		const FVector Tangent = Centers[FMath::Min(Row+1,WakeSegments)] - Centers[FMath::Max(0,Row-1)];
		const FVector Across = FVector::CrossProduct(FVector::UpVector,Tangent).GetSafeNormal() * (Config.WingSpan*.41);
		const FVector Center = Centers[Row] - FVector(0,0,Config.WingSpan*.035);
		Mesh->SetVertex(2*Row,Center-Across);
		Mesh->SetVertex(2*Row+1,Center+Across);
	}
	Wake->FastNotifyPositionsUpdated();
	Wake->SetVisibility(Head-Tail > .001f);
}

void AHALBatarang::PrepareBurstClouds()
{
	const auto& Config = *State.Config;
	UStaticMesh* CloudMesh = Config.BurstCloudMesh;
	if (!BurstClouds.IsEmpty() || !CloudMesh || !CloudMesh->GetMaterial(0)) { return; }
	for (int32 I=0; I<UE_ARRAY_COUNT(Puffs); ++I)
	{
		auto* Cloud = NewObject<UStaticMeshComponent>(this);
		Cloud->SetStaticMesh(CloudMesh);
		auto* MID = Cloud->CreateDynamicMaterialInstance(0);
		MID->SetVectorParameterValue(TEXT("Tint"),Config.BurstFireTint);
		MID->SetVectorParameterValue(TEXT("CoolColor"),Config.BurstSmokeColor);
		MID->SetScalarParameterValue(TEXT("LobeSwelling"),.65f);
		MID->SetScalarParameterValue(TEXT("Seed"),2.4f*I);
		Cloud->SetBoundsScale(1.15f);
		Cloud->SetTranslucentSortPriority(1);
		AttachVisual(*Cloud,GetRootComponent());
		BurstClouds.Add(Cloud);
	}
}

void AHALBatarang::PresentBurstClouds(const FVector& Origin, float Phase)
{
	PrepareBurstClouds(); // A late client may first observe the dart during detonation.
	if (BurstClouds.IsEmpty()) { return; }
	const auto& Config = *State.Config;
	const FRotator Heading = State.BurstDirection.Rotation();
	const FVector Extent = Config.BurstCloudMesh->GetBounds().BoxExtent.ComponentMax(FVector(1));
	for (int32 I=0; I<BurstClouds.Num(); ++I)
	{
		const FPuff& Puff = Puffs[I];
		auto* Cloud = BurstClouds[I].Get();
		const float Life = FMath::Clamp((Phase-Puff.Birth)/(Puff.End-Puff.Birth),0.f,1.f);
		Cloud->SetVisibility(Life > 0 && Life < 1);
		if (!Cloud->IsVisible()) { continue; }
		const float Fill = 1.f-FMath::Pow(1.f-FMath::Min(1.f,Life/.52f),2.6f);
		const float Drift = FMath::SmoothStep(.3f,1.f,Life);
		const double R = Config.BurstReach*Puff.Radius*(.08+.92*Fill+.13*Drift);
		const FVector Travel = Heading.RotateVector(Puff.Offset)*Config.BurstReach*(.76*Fill+.24*Life);
		const FVector Center = Origin+Travel+FVector::UpVector*(5.+Config.BurstReach*(.12*Fill+.20*Drift));
		Cloud->SetWorldTransform(FTransform(FRotator(10*Fill,I*117+Heading.Yaw,3*Drift),Center,
			FVector(R,R*.92,R*(.56+.40*Fill))/Extent));
		auto* MID = CastChecked<UMaterialInstanceDynamic>(Cloud->GetMaterial(0));
		MID->SetScalarParameterValue(TEXT("Heat"),1.f-FMath::SmoothStep(.16f,.88f,Life));
		MID->SetScalarParameterValue(TEXT("Phase"),Life);
	}
}

void AHALBatarang::PresentArt(float Age, const FTransform& Pin)
{
	const auto& Config = *State.Config;
	const bool bFlying = State.Phase == EHALDartPhase::Flight;
	const bool bPinned = State.Phase == EHALDartPhase::Pinned;
	const float Span = Config.WingSpan;
	Blade->SetVisibility(bFlying || bPinned);
	if (Wake && !bFlying) { Wake->SetVisibility(false); }
	Shape->SetVisibility(CueInk && !bFlying);
	if (bFlying)
	{
		const float T = FMath::Clamp(Age/Config.FlightSeconds,0.f,1.f);
		Blade->SetWorldLocationAndRotation(FlightPoint(T), FRotator(0,State.Yaw+State.Mirror*Age*800,10+14*FMath::Sin(Age*12)));
		if (BladeInk) { BladeInk->SetScalarParameterValue(TEXT("Signal"),.7f); }
		if (WakeInk)
		{
			UpdateWake(Age);
		}
		return;
	}
	if (bPinned)
	{
		const FVector Inward = Pin.GetUnitAxis(EAxis::X);
		const double Nose = Blade->GetStaticMesh()->GetBoundingBox().Max.X*Blade->GetComponentScale().X;
		const FVector Socket = Pin.GetLocation()+Inward*(Span*Config.PinDepthPercent*.01-Nose);
		Blade->SetWorldLocationAndRotation(Socket,Pin.GetRotation());
		const float Beat = Config.FusePulse(Config.FuseSeconds-Age);
		if (BladeInk) { BladeInk->SetScalarParameterValue(TEXT("Signal"),.35f+1.8f*Beat); }
		Shape->SetVisibility(CueInk && Age < .20f);
		if (Shape->IsVisible())
		{
			const FVector Exposed = Pin.GetLocation()-Inward*(Span*.055)+Pin.GetUnitAxis(EAxis::Z)*(Span*.07);
			Shape->SetWorldLocationAndRotation(Exposed,FRotationMatrix::MakeFromXZ(-Inward,Pin.GetUnitAxis(EAxis::Z)).ToQuat());
			Shape->SetWorldScale3D(FVector(Span*.39));
			CueInk->SetScalarParameterValue(TEXT("Burst"),0);
			CueInk->SetScalarParameterValue(TEXT("Phase"),FMath::Clamp(Age/.24f,0.f,1.f));
			CueInk->SetScalarParameterValue(TEXT("Signal"),Beat);
		}
		return;
	}
	const float T = FMath::Clamp(Age/float(HALBatarang::BurstSeconds),0.f,1.f);
	const FRotator Heading = State.BurstDirection.Rotation();
	const FVector Origin = State.BurstPoint+Heading.RotateVector(BurstContactOffset);
	PresentBurstClouds(Origin,T);
	if (CueInk)
	{
		const float Reach = FMath::Max(1.f,Config.BurstReach);
		Shape->SetWorldLocationAndRotation(Origin+FVector::UpVector*14.,Heading);
		Shape->SetWorldScale3D(FVector(Reach,Reach,1));
		CueInk->SetScalarParameterValue(TEXT("Burst"),1);
		CueInk->SetScalarParameterValue(TEXT("Phase"),T);
		Shape->SetVisibility(T < 1);
	}
}
