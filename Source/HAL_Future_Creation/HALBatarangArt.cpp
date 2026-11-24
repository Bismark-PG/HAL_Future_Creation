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

void AHALBatarang::PresentArt(float Age, const FTransform& Pin, const FLinearColor& SeatColor)
{
	const auto& Config = *State.Config;
	const bool bFlying = State.Phase == EHALDartPhase::Flight;
	const bool bPinned = State.Phase == EHALDartPhase::Pinned;
	const float Span = Config.WingSpan;
	Blade->SetVisibility(bFlying || bPinned);
	if (Wake && !bFlying) { Wake->SetVisibility(false); }
	Shape->SetVisibility(CueInk && !bFlying);
	if (BladeInk) { BladeInk->SetVectorParameterValue(TEXT("SeatColor"),SeatColor); }
	if (bFlying)
	{
		const float T = FMath::Clamp(Age/Config.FlightSeconds,0.f,1.f);
		Blade->SetWorldLocationAndRotation(FlightPoint(T), FRotator(0,State.Yaw+State.Mirror*Age*800,10+14*FMath::Sin(Age*12)));
		if (BladeInk) { BladeInk->SetScalarParameterValue(TEXT("Signal"),.7f); }
		if (WakeInk)
		{
			WakeInk->SetVectorParameterValue(TEXT("SeatColor"),SeatColor);
			UpdateWake(Age);
		}
		return;
	}
	if (CueInk) { CueInk->SetVectorParameterValue(TEXT("SeatColor"),SeatColor); }
	if (bPinned)
	{
		const FVector Inward = Pin.GetUnitAxis(EAxis::X);
		const FVector Socket = Pin.GetLocation()-Inward*(Span*.22);
		Blade->SetWorldLocationAndRotation(Socket,Pin.GetRotation());
		const float TimeLeft = FMath::Max(0.f, Config.FuseSeconds-Age);
		const float Urgency = 1-FMath::Clamp(TimeLeft/1.2f,0.f,1.f);
		// Integrating the accelerating frequency keeps the pulse continuous at the warning boundary.
		const float Cycles = 1.7f*Age + 1.4f*Urgency*Urgency;
		const float Beat = FMath::Pow(.5f+.5f*FMath::Cos(UE_TWO_PI*Cycles),3.f);
		if (BladeInk) { BladeInk->SetScalarParameterValue(TEXT("Signal"),.35f+1.8f*Beat); }
		if (CueInk)
		{
			const FVector Exposed = Socket-Inward*(Span*.20)+FVector(0,0,Span*.075);
			Shape->SetWorldLocationAndRotation(Exposed,FRotator(0,(-Inward).Rotation().Yaw,0));
			Shape->SetWorldScale3D(FVector(Span*.69));
			CueInk->SetScalarParameterValue(TEXT("Burst"),0);
			CueInk->SetScalarParameterValue(TEXT("Phase"),FMath::Clamp(Age/.24f,0.f,1.f));
			CueInk->SetScalarParameterValue(TEXT("Signal"),Beat);
		}
		return;
	}
	if (CueInk)
	{
		const float T = FMath::Clamp(Age/float(HALBatarang::BurstSeconds),0.f,1.f);
		const float Reach = FMath::Max(1.f,Config.BurstReach);
		// A low crest rises out of the contact, then opens towards the resolved horizontal impulse.
		const FVector Origin = State.BurstPoint+FVector(0,0,Span*(.16f+.63f*FMath::Sin(UE_PI*T)));
		Shape->SetWorldLocationAndRotation(Origin,State.BurstDirection.Rotation());
		Shape->SetWorldScale3D(FVector(Reach,Reach*.75f,1));
		CueInk->SetScalarParameterValue(TEXT("Burst"),1);
		CueInk->SetScalarParameterValue(TEXT("Phase"),T);
		Shape->SetVisibility(T < 1);
	}
}
