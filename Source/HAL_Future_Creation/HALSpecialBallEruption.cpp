#include "HALSpecialBallCue.h"
#include "HALBombActor.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
using namespace UE::Geometry;

// The authored mesh is the largest pose. Each vertex travels along one ray from the
// impact during playback, so its cover limit can be measured once, at creation.
struct FSolarEruption
{
	UWorld& World;
	const FVector Origin;
	const FVector TraceStart;
	const double Radius;
	FDynamicMesh3 Mesh;
	TArray<FBox> CoverBounds;

	FSolarEruption(UWorld& InWorld, const FVector& Location, const FVector& Normal, double InRadius)
		: World(InWorld), Origin(Location), TraceStart(Location + Normal * 1.5), Radius(InRadius)
	{
		Mesh.EnableAttributes();
		Mesh.Attributes()->SetNumUVLayers(2);
		// Most vertices are above the flat deck. Only rays intersecting a nearby solid's
		// bounds need an exact cover query; this work is done once per impact.
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		TArray<FOverlapResult> Nearby;
		World.OverlapMultiByObjectType(Nearby, Origin, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius + 2.),
			FCollisionQueryParams(SCENE_QUERY_STAT(SolarEruptionBounds), false));
		for (const FOverlapResult& Hit : Nearby)
		{
			const auto* Body = Hit.GetComponent();
			if (Body && Body->IsPhysicsCollisionEnabled() && Body->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block)
			{
				CoverBounds.Add(Body->Bounds.GetBox().ExpandBy(1.));
			}
		}
	}

	void Vertex(const FVector& UnitPosition, const FVector& Normal, const FVector2f& UV, float Layer)
	{
		const FVector Offset = UnitPosition.GetClampedToMaxSize(.97) * Radius;
		const FVector End = Origin + Offset;
		const bool bMayHitCover = CoverBounds.ContainsByPredicate([&](const FBox& Box)
		{
			return Box.IsInsideOrOn(TraceStart) || FMath::LineBoxIntersection(Box, TraceStart, End, End-TraceStart);
		});
		float Open = bMayHitCover ? FHALBlastArea::OpenFraction(World, TraceStart, End) : 1.f;
		if (Open < 1.f)
		{
			// The cover ray starts just outside the surface, while animation scales about the impact.
			Open = FMath::Max(0., Open - FVector::Distance(Origin, TraceStart) / FMath::Max(1., Offset.Size()));
		}
		Mesh.AppendVertex(Offset);
		Mesh.Attributes()->PrimaryNormals()->AppendElement(FVector3f(Normal));
		Mesh.Attributes()->GetUVLayer(0)->AppendElement(UV);
		Mesh.Attributes()->GetUVLayer(1)->AppendElement(FVector2f(Open, Layer));
	}

	void Triangle(int32 A, int32 B, int32 C)
	{
		const FIndex3i Indices(A, B, C);
		const int32 ID = Mesh.AppendTriangle(Indices);
		Mesh.Attributes()->PrimaryNormals()->SetTriangle(ID, Indices);
		Mesh.Attributes()->GetUVLayer(0)->SetTriangle(ID, Indices);
		Mesh.Attributes()->GetUVLayer(1)->SetTriangle(ID, Indices);
	}

	void JoinRows(int32 Start, int32 Segments)
	{
		for (int32 I = 0; I < Segments; ++I)
		{
			const int32 A = Start + I;
			const int32 B = A + Segments + 1;
			Triangle(A, A+1, B);
			Triangle(A+1, B+1, B);
		}
	}

	void Footprint(const FVector& Center, const FVector& Out, double Size, float Delay)
	{
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Out);
		constexpr int32 Steps = 14;
		for (int32 Arc = 0; Arc < 2; ++Arc)
		{
			for (int32 I = 0; I <= Steps; ++I)
			{
				const double U = double(I)/Steps;
				const double Angle = 2.9*U + Arc*UE_PI + .40;
				const double Width = (.03 + .42*FMath::Sin(UE_PI*U)) * Size;
				const FVector D = FMath::Cos(Angle)*Out + (.77*FMath::Sin(Angle))*Side;
				const double Outer = (.72 + .26*U)*Size;
				const int32 Row = Mesh.VertexCount();
				Vertex(Center + (Outer-Width)*D, FVector::UpVector, FVector2f(U,0), 2.f+Delay);
				Vertex(Center + Outer*D, FVector::UpVector, FVector2f(U,1), 2.f+Delay);
				if (I) { JoinRows(Row-2,1); }
			}
		}
	}

	void Arch(const FVector& Out, double Reach, double Bend, double Height, double Width, double FloorZ, float Delay)
	{
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Out);
		const FVector Root = .045 * Out + .035 * FVector::UpVector;
		const FVector Lift = .09 * Out + Bend * Side + (Height * 1.40) * FVector::UpVector;
		const FVector Curl = (Reach * 1.32) * Out + (Bend * .83) * Side + (Height * 1.31) * FVector::UpVector;
		const FVector Foot = Reach * Out - .045 * Side + FloorZ * FVector::UpVector;
		const FVector PlaneNormal = (Height * Side - Bend * FVector::UpVector).GetSafeNormal();

		constexpr int32 Steps = 22;
		constexpr int32 Sides = 9;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const double U = double(I) / Steps, V = 1. - U;
			const FVector Center = V*V*V*Root + (3.*V*V*U)*Lift + (3.*V*U*U)*Curl + U*U*U*Foot;
			const FVector Tangent = (V*V*(Lift-Root) + 2.*V*U*(Curl-Lift) + U*U*(Foot-Curl)).GetSafeNormal();
			const FVector Broad = FVector::CrossProduct(PlaneNormal, Tangent).GetSafeNormal()
				.RotateAngleAxis(24.*FMath::Sin(UE_PI*U) - 10., Tangent);
			const FVector Thin = FVector::CrossProduct(Tangent, Broad).GetSafeNormal();
			const double HalfWidth = .5 * Width * (.045 + .955 * FMath::Pow(FMath::Max(0., FMath::Sin(UE_PI*U)), .78));
			const int32 Row = Mesh.VertexCount();
			for (int32 J = 0; J <= Sides; ++J)
			{
				const double Angle = UE_TWO_PI * J / Sides;
				const double C = FMath::Cos(Angle), S = FMath::Sin(Angle);
				const FVector Normal = (Broad*C + Thin*(S/.43)).GetSafeNormal();
				Vertex(Center + HalfWidth * (Broad*C + Thin*(.43*S)), Normal, FVector2f(U, double(J)/Sides), Delay);
			}
			if (I) { JoinRows(Row - (Sides+1), Sides); }
		}
		Footprint(Foot, Out, Width*.57, .25f+Delay);
	}

	void Puff(const FVector& Center, const FVector& Extent, double Seed, float Delay)
	{
		constexpr int32 Belts = 7;
		constexpr int32 Sides = 14;
		for (int32 I = 0; I <= Belts; ++I)
		{
			const int32 Row = Mesh.VertexCount();
			const double Latitude = (double(I)/Belts - .5) * (UE_PI - .015);
			for (int32 J = 0; J <= Sides; ++J)
			{
				const double A = UE_TWO_PI * J / Sides;
				const FVector N(FMath::Cos(Latitude)*FMath::Cos(A), FMath::Cos(Latitude)*FMath::Sin(A), FMath::Sin(Latitude));
				const double Bulge = 1. + .095 * FMath::Sin(3.*A + Seed + 1.4*N.Z) * FMath::Square(FMath::Cos(Latitude));
				const FVector Normal = (N / Extent).GetSafeNormal();
				Vertex(Center + Extent*N*Bulge, Normal, FVector2f(double(J)/Sides, double(I)/Belts), 1.f + Delay);
			}
			if (I) { JoinRows(Row - (Sides+1), Sides); }
		}
	}
};
}

void AHALSpecialBallCue::PrepareEruption(const UHALExplosiveBallDefinition& Type)
{
	FSolarEruption Shape(*GetWorld(), GetActorLocation(), State.SurfaceNormal, State.Radius);
	const FWaveRing& Equator = Rings[0];
	FVector OpenDirection = FVector::ZeroVector;
	for (int32 I = 0; I < Equator.Open.Num(); ++I)
	{
		OpenDirection += Equator.Points[I].GetSafeNormal() * Equator.Open[I];
	}
	OpenDirection /= Equator.Open.Num();
	const bool bAgainstCover = OpenDirection.SizeSquared2D() > FMath::Square(.16);
	const double Yaw = bAgainstCover ? FMath::RadiansToDegrees(FMath::Atan2(OpenDirection.Y, OpenDirection.X)) : 94.;
	auto Direction = [Yaw](double Angle) { return FVector::ForwardVector.RotateAngleAxis(Yaw+Angle, FVector::UpVector); };
	const auto* Ground = Rings.FindByPredicate([](const FWaveRing& R) { return R.bGround; });
	const double FootZ = Ground ? FMath::Max(-.46, Ground->Points[0].Z / State.Radius) + .012 : .012;
	const double H = Type.ProminenceHeight, W = Type.ProminenceWidth;
	Shape.Arch(Direction(0), .85, bAgainstCover ? -.42 : -.59, H, W, FootZ, 0.f);
	// At a wall both side arches bend into the open half of the arena before returning.
	Shape.Arch(Direction(bAgainstCover ? 69. : 164.), .78, bAgainstCover ? -.35 : .54, H*.88, W*.92, FootZ, .024f);
	Shape.Arch(Direction(bAgainstCover ? -69. : -81.), .65, bAgainstCover ? .38 : -.58, H*.81, W*.71, FootZ, .045f);
	Shape.Footprint(FVector(0,0,FootZ+.004), Direction(0), .26, .045f);

	const double Smoke = Type.CoreSmokeSize;
	if (Smoke > 0.)
	{
		const FVector Bias = bAgainstCover ? Direction(0)*.35 : FVector::ZeroVector;
		auto Puff = [&](FVector Offset, FVector Extent, double Seed, float Delay)
		{
			Shape.Puff(Smoke*(Offset+Bias), Smoke*Extent, Seed, Delay);
		};
		Puff(FVector(-.22, -.42, .30), FVector(.56, .53, .51), .2, .075f);
		Puff(FVector(.22, .33, .37), FVector(.62, .52, .66), 2.1, .06f);
		Puff(FVector(.12, -.06, .77), FVector(.51, .54, .53), 4.7, .10f);
		Puff(FVector(-.46, .28, .22), FVector(.41, .40, .36), 1.3, .13f);
	}
	Eruption = NewObject<UDynamicMeshComponent>(this, TEXT("SolarEruption"));
	AddInstanceComponent(Eruption);
	Eruption->SetupAttachment(Ribbons);
	Eruption->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Eruption->SetGenerateOverlapEvents(false);
	Eruption->SetCanEverAffectNavigation(false);
	Eruption->SetCastShadow(false);
	EruptionMID = UMaterialInstanceDynamic::Create(Type.EruptionMaterial, Eruption);
	EruptionMID->SetVectorParameterValue(TEXT("CoreColor"), Type.CoreColor);
	EruptionMID->SetVectorParameterValue(TEXT("CurrentColor"), Type.CurrentColor);
	EruptionMID->SetVectorParameterValue(TEXT("CoronaColor"), Type.CoronaColor);
	EruptionMID->SetScalarParameterValue(TEXT("Glow"), Type.Glow);
	// Progress is already synchronized for the whole impact. Convert its normalized age to ribbon travel.
	EruptionMID->SetScalarParameterValue(TEXT("ProminenceFlowTravel"), Type.ProminenceFlowSpeed * Type.ImpactSeconds);
	EruptionMID->SetScalarParameterValue(TEXT("ProminenceFlowStrength"), Type.ProminenceFlowStrength);
	EruptionMID->SetScalarParameterValue(TEXT("ProminenceTailRatio"), Type.ProminenceTailSeconds / Type.ImpactSeconds);
	Eruption->SetMaterial(0, EruptionMID);
	Eruption->SetMesh(MoveTemp(Shape.Mesh));
	Eruption->RegisterComponent();
}
