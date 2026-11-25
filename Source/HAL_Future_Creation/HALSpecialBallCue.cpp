#include "HALSpecialBallCue.h"
#include "HALBombActor.h"
#include "HALBlackHoleBall.h"
#include "HALMeteorBall.h"
#include "HALTimedBombBall.h"
#include "HALMatchBallLifecycle.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
enum class ECagePiece : uint8 { Rib, Lock, Collar };

struct FCagePieceFlight
{
	ECagePiece Piece;
	float ShellYaw, ExitYaw, Reach, Lift, SpinDegrees, End;
	FVector SpinAxis;
};

// Angles on the intact shell match the two meridians and pole shoes. Ejection
// diverges after the break. Each peer samples the same arc at the cue's age.
const FCagePieceFlight CageFlights[] = {
	{ECagePiece::Rib,     24,   8, 1.24f, .76f,  205, .94f, FVector(.30,.82,.41)},
	{ECagePiece::Rib,    204, 218, 1.17f, .94f, -164, .89f, FVector(.76,.21,.57)},
	{ECagePiece::Lock,    90,  79, 1.31f, .59f, -193, .83f, FVector(.62,.68,.20)},
	{ECagePiece::Lock,   270, 286, 1.12f, .74f,  237, .91f, FVector(.28,.79,.55)},
	{ECagePiece::Collar, 148, 149, 1.04f, 1.08f, 276, .80f, FVector(.71,.35,.60)}
};

double BallCueTime(const UWorld& World)
{
	const auto* State = World.GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
}

struct FCoronaMesh
{
	FDynamicMesh3 Mesh;
	FCoronaMesh() { Mesh.EnableAttributes(); Mesh.Attributes()->EnablePrimaryColors(); }
	void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, FLinearColor Color)
	{
		auto* Colors = Mesh.Attributes()->PrimaryColors();
		auto* Normals = Mesh.Attributes()->PrimaryNormals();
		auto* UVs = Mesh.Attributes()->PrimaryUV();
		UE::Geometry::FIndex4i V;
		const FVector Corners[] = {A, B, C, D};
		const FVector3f Normal(FVector::CrossProduct(B-A, D-A).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector));
		for (int32 I = 0; I < 4; ++I)
		{
			V[I] = Mesh.AppendVertex(Corners[I]);
			Colors->AppendElement(FVector4f(Color.R, Color.G, Color.B, Color.A));
			Normals->AppendElement(Normal);
			UVs->AppendElement(FVector2f(I == 1 || I == 2, I >= 2));
		}
		for (const UE::Geometry::FIndex3i& Face : {UE::Geometry::FIndex3i(V[0],V[1],V[2]), UE::Geometry::FIndex3i(V[0],V[2],V[3])})
		{
			const int32 Triangle = Mesh.AppendTriangle(Face);
			Colors->SetTriangle(Triangle, Face);
			Normals->SetTriangle(Triangle, Face);
			UVs->SetTriangle(Triangle, Face);
		}
	}
};

// Common magnetic arrival and expiry. Older ball definitions also use this fallback for hits.
void DrawClaspCue(FCoronaMesh& Out, const UHALTimedBombBallDefinition& Type, EHALBallCue Kind, float T, float R)
{
	const bool Blast = Kind == EHALBallCue::Impact || Kind == EHALBallCue::MountedImpact;
	const bool Arrival = Kind == EHALBallCue::Arrival;
	const float Ease = 1.f-FMath::Pow(1.f-T,3.f);
	const float Fade = 1.f-FMath::SmoothStep(.52f,1.f,T);
	auto Color = [&](FLinearColor C, float Alpha)
	{
		C *= Type.Glow;
		C.A = FMath::Clamp(Alpha*Fade,0.f,1.f);
		return C;
	};
	auto Point = [](float Angle, float Radius, float Height)
	{
		return FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,Height);
	};
	auto Arc = [&](float Begin, float End, float Radius, float Width, float Height, const FLinearColor& C, int32 Steps)
	{
		for (int32 I=0; I<Steps; ++I)
		{
			const float A=FMath::Lerp(Begin,End,float(I)/Steps), B=FMath::Lerp(Begin,End,float(I+1)/Steps);
			Out.Quad(Point(A,Radius-Width,Height),Point(B,Radius-Width,Height),Point(B,Radius,Height),Point(A,Radius,Height),C);
		}
	};
	const float Reach = Blast ? R*Ease : R*(Arrival ? FMath::Lerp(Type.CoronaSize,.75f,Ease) : 1.f+.60f*Ease);
	if (Kind == EHALBallCue::Impact)
	{
		Arc(0,UE_TWO_PI,Reach,Reach*.065f*(1.f-.64f*T),0,Color(Type.CurrentColor,.93f),64);
		Arc(0,UE_TWO_PI,Reach*.98f,Reach*.014f,1,Color(Type.PoleTipColor,.9f),64);
	}
	for (int32 I=0; I<Type.DialSegments; ++I)
	{
		const float A=UE_TWO_PI*I/Type.DialSegments + (Blast ? 0.f : (Arrival ? -.5f : .5f)*Ease);
		const float Size = Kind == EHALBallCue::Impact ? .93f : 1.f;
		Arc(A-.036f,A+.036f,Reach*Size,Reach*(I%3==0 ? .15f : .09f),R*.05f,
			Color(I%3==0 ? Type.PoleTipColor : Type.CoronaColor,.92f),2);
	}
	for (int32 I=0; I<4; ++I)
	{
		const float A = I*UE_HALF_PI + (Arrival ? 1.f-Ease : Ease)*.52f;
		const float Radius = Reach*(Blast ? .72f : .96f);
		const float Height = R*(Blast ? .20f*FMath::Sin(UE_PI*T) : .18f*(1.f-T));
		Arc(A-.42f,A+.42f,Radius,Radius*.14f,Height,Color(Type.CurrentColor,1.f),10);
		Arc(A+.27f,A+.42f,Radius,Radius*.14f,Height+1.f,Color(Type.PoleTipColor,1.f),3);
	}
	if (!Blast) { return; }
	// Small faceted puffs swell outward and rise, then lose opacity before the last frame.
	for (int32 I=0; I<7; ++I)
	{
		const float Angle=I*2.4f;
		const FVector Center=Point(Angle,R*(I ? .24f*Ease : 0.f),R*(.07f+.16f*T));
		const float Size=R*(I ? .16f+.025f*(I%3) : .23f)*FMath::SmoothStep(0.f,.35f,T);
		auto Lobe=[&](int32 Slice,int32 Layer)
		{
			const float Lat=-UE_HALF_PI+UE_PI*Layer/5.f;
			return Center+Point(Slice*UE_TWO_PI/10.f+Angle,Size*FMath::Cos(Lat),Size*FMath::Sin(Lat));
		};
		for (int32 J=0; J<5; ++J)
		{
			const FLinearColor Band=J<2 ? Type.CurrentColor*.5f : (J<4 ? Type.CoronaColor : Type.PoleTipColor);
			for (int32 K=0; K<10; ++K)
			{ Out.Quad(Lobe(K,J),Lobe(K+1,J),Lobe(K+1,J+1),Lobe(K,J+1),Color(Band,.98f)); }
		}
	}
}
}

AHALSpecialBallCue::AHALSpecialBallCue()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Ribbons = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("SolarRibbons"));
	SetRootComponent(Ribbons);
	Ribbons->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ribbons->SetGenerateOverlapEvents(false);
	Ribbons->SetCanEverAffectNavigation(false);
	Ribbons->SetCastShadow(false);
}

void AHALSpecialBallCue::Spawn(UWorld& World, EHALBallCue Kind, UHALSpecialBallDefinition* Type,
	const FVector& Origin, float Radius, const FVector& Normal)
{
	if (!Type || World.GetNetMode() == NM_Client) { return; }
	const double Started = BallCueTime(World);
	const bool bExplosion = Kind == EHALBallCue::Impact || Kind == EHALBallCue::MountedImpact;
	if (bExplosion && World.GetNetMode() != NM_Standalone && World.GetGameState()
		&& (Type->IsA<UHALExplosiveBallDefinition>() || Type->IsA<UHALTimedBombBallDefinition>()))
	{
		if (auto* MatchBalls = World.GetGameState()->FindComponentByClass<UHALMatchBallLifecycle>())
		{
			MatchBalls->MulticastBallImpact(Type, Kind, Origin, Radius, Normal, Started);
			return;
		}
	}
	CreateCue(World, Kind, Type, Origin, Radius, Normal, Started, true);
}

void AHALSpecialBallCue::CreateCue(UWorld& World, EHALBallCue Kind, UHALSpecialBallDefinition* Type,
	const FVector& Origin, float Radius, const FVector& Normal, double Started, bool bReplicate)
{
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spawn.CustomPreSpawnInitialization = [=](AActor* Actor)
	{
		auto* Cue = CastChecked<AHALSpecialBallCue>(Actor);
		// The constructor has already initialized RemoteRole. Disable both it and replication for local playback.
		Cue->SetReplicates(bReplicate);
		Cue->State.Type = Type;
		Cue->State.Kind = Kind;
		Cue->State.Radius = Radius;
		Cue->State.SurfaceNormal = Normal;
		Cue->State.Started = Started;
	};
	if (auto* Cue = World.SpawnActor<AHALSpecialBallCue>(StaticClass(), FTransform(Origin), Spawn))
	{
		Cue->SetLifeSpan(Type->CueSeconds(Kind) + 1.f);
		if (bReplicate) { Cue->ForceNetUpdate(); }
	}
}

void AHALSpecialBallCue::BeginPlay()
{
	Super::BeginPlay();
	OnRep_State();
}

void AHALSpecialBallCue::OnRep_State()
{
	if (bPrepared || !State.Type || State.Radius <= 0 || !Ribbons->IsRegistered() || GetNetMode() == NM_DedicatedServer) { return; }
	bPrepared = true;
	const float Age = FMath::Max(0., BallCueTime(*GetWorld()) - State.Started);
	if (Age >= State.Type->CueSeconds(State.Kind)) { return; }
	if (State.Kind == EHALBallCue::Expiration)
	{
		const auto* Hole = Cast<UHALBlackHoleBallDefinition>(State.Type);
		if (Hole && Hole->ExpirationVortex && Hole->FieldClass)
		{
			auto* FX = UNiagaraFunctionLibrary::SpawnSystemAttached(Hole->ExpirationVortex, RootComponent, NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, false);
			if (FX)
			{
				const auto Direction = Hole->FieldClass.GetDefaultObject()->GetBlackHoleConfig().SwirlDirection;
				FX->SetVariableFloat(TEXT("User.Radius"), State.Radius * Hole->CoronaSize);
				FX->SetVariableFloat(TEXT("User.SwirlSign"), Direction == EHALBlackHoleSwirlDirection::Clockwise ? -1.f : 1.f);
				FX->Activate(true);
				if (Age >= 1.f/60.f) { FX->AdvanceSimulation(FMath::FloorToInt(Age * 60.f), 1.f/60.f); }
				return;
			}
		}
	}
	Ribbons->SetMaterial(0, State.Type->CueMaterial);
	if (State.Kind == EHALBallCue::Impact || State.Kind == EHALBallCue::MountedImpact)
	{
		const auto* Clasp = Cast<UHALTimedBombBallDefinition>(State.Type.Get());
		if (Clasp && Clasp->ClaspBurstMaterial) { PrepareClaspBurst(*Clasp); }
	}
	if (State.Kind == EHALBallCue::Impact && !State.Type->IsA<UHALTimedBombBallDefinition>())
	{
		PrepareWave();
		const auto* Solar = Cast<UHALExplosiveBallDefinition>(State.Type.Get());
		if (Solar && Solar->EruptionMaterial) { PrepareEruption(*Solar); }
	}
	SetActorTickEnabled(true);
	Tick(0);
}

void AHALSpecialBallCue::PrepareClaspBurst(const UHALTimedBombBallDefinition& Type)
{
	ClaspBurstMID = UMaterialInstanceDynamic::Create(Type.ClaspBurstMaterial, this);
	if (!ClaspBurstMID) { return; }
	ClaspBurstMID->SetScalarParameterValue(TEXT("GroundPulse"), State.Kind == EHALBallCue::Impact ? 1.f : 0.f);
	ClaspBurstMID->SetScalarParameterValue(TEXT("SmokeSize"), Type.ClaspSmokeSize);
	ClaspBurstMID->SetScalarParameterValue(TEXT("Glow"), Type.Glow);
	Ribbons->SetMaterial(0, ClaspBurstMID);

	// Both peers use the actual opening axis at detonation. The center lifts off the
	// surface while the outer pressure front stays flat and at the gameplay radius.
	const FVector Side = FVector(State.SurfaceNormal).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::RightVector);
	const FVector Across = FVector::CrossProduct(FVector::UpVector, Side);
	FDynamicMesh3 Sheet;
	Sheet.EnableAttributes();
	auto* UV = Sheet.Attributes()->PrimaryUV();
	auto* Normals = Sheet.Attributes()->PrimaryNormals();
	constexpr int32 Steps = 10;
	for (int32 J = 0; J <= Steps; ++J)
	{
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float X = 2.f*I/Steps-1.f, Y = 2.f*J/Steps-1.f;
			const float Crown = 1.f - FMath::SmoothStep(0.f, .72f, FMath::Sqrt(X*X+Y*Y));
			Sheet.AppendVertex((X*Side + Y*Across + FVector::UpVector*(.16f*Crown))*State.Radius);
			UV->AppendElement(FVector2f(float(I)/Steps,float(J)/Steps));
			Normals->AppendElement(FVector3f::UnitZ());
		}
	}
	for (int32 J = 0; J < Steps; ++J)
	{
		for (int32 I = 0; I < Steps; ++I)
		{
			const int32 A = J*(Steps+1)+I, B = A+Steps+1;
			for (const UE::Geometry::FIndex3i& Face : {UE::Geometry::FIndex3i(A,B,A+1), UE::Geometry::FIndex3i(A+1,B,B+1)})
			{
				const int32 Index = Sheet.AppendTriangle(Face);
				UV->SetTriangle(Index,Face);
				Normals->SetTriangle(Index,Face);
			}
		}
	}
	Ribbons->SetMesh(MoveTemp(Sheet));

	UStaticMesh* const Shapes[] = { Type.RibFragmentMesh.Get(), Type.LockFragmentMesh.Get(), Type.CollarFragmentMesh.Get() };
	auto* Metal = Type.MakeCageMaterial(this);
	for (const FCagePieceFlight& Flight : CageFlights)
	{
		auto* Shape = Shapes[static_cast<uint8>(Flight.Piece)];
		UStaticMeshComponent* Piece = nullptr;
		if (Shape)
		{
			Piece = NewObject<UStaticMeshComponent>(this);
			Piece->SetMobility(EComponentMobility::Movable);
			Piece->SetupAttachment(Ribbons);
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Piece->SetGenerateOverlapEvents(false);
			Piece->SetCanEverAffectNavigation(false);
			Piece->SetCastShadow(false);
			Piece->SetStaticMesh(Shape);
			if (Metal) { Piece->SetMaterial(0, Metal); }
			Piece->RegisterComponent();
		}
		CageFragments.Add(Piece);
	}
}

void AHALSpecialBallCue::UpdateCageFragments(float Progress, const UHALTimedBombBallDefinition& Type)
{
	const float BallRadius = State.Kind == EHALBallCue::MountedImpact
		? State.Radius / Type.CoronaSize : Type.GetScaledPhysicsRadius();
	const float MeshScale = BallRadius / 50.f;
	const FVector Side = FVector(State.SurfaceNormal).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::RightVector);
	const float Facing = Side.Rotation().Yaw - 90.f;
	// Meshes are centered on the metal they came from, in the same 50 cm frame as JawMesh.
	static const FVector Mounts[] = { FVector(46.3,0,7.5), FVector(47,0,-4), FVector(30.3,0,34) };
	for (int32 Index = 0; Index < CageFragments.Num(); ++Index)
	{
		auto* Piece = CageFragments[Index].Get();
		if (!Piece || !Piece->IsVisible()) { continue; }
		const FCagePieceFlight& Flight = CageFlights[Index];
		const float T = FMath::Min(Progress / Flight.End, 1.f);
		if (T >= 1.f) { Piece->SetVisibility(false); continue; }
		const FQuat MountRotation(FVector::UpVector, FMath::DegreesToRadians(Facing + Flight.ShellYaw));
		const FVector Start = MountRotation.RotateVector(Mounts[static_cast<uint8>(Flight.Piece)]) * MeshScale;
		const FVector Direction = FRotator(0, Facing + Flight.ExitYaw, 0).Vector();
		const float Reach = FMath::Max(float(Start.Size2D()), State.Radius * Type.ClaspBurstReach * Flight.Reach);
		const float Out = 1.f - FMath::Square(1.f-T);
		FVector Position = FMath::Lerp(Start, Direction * Reach, Out);
		Position.Z = FMath::Lerp(Start.Z, -.68*BallRadius, T) + BallRadius * Flight.Lift * 4.f*T*(1.f-T);
		const FQuat Tumble(Flight.SpinAxis.GetSafeNormal(), FMath::DegreesToRadians(Flight.SpinDegrees * T * (1.18f-.18f*T)));
		const float Scale = MeshScale * (1.f-FMath::SmoothStep(.64f, 1.f, T));
		Piece->SetRelativeTransform(FTransform(MountRotation*Tumble, Position, FVector(Scale)));
	}
}

void AHALSpecialBallCue::PrepareWave()
{
	const FVector Origin = GetActorLocation();
	const FVector Start = Origin + FVector(State.SurfaceNormal) * 1.5;
	const float Radius = State.Radius;
	auto AddRing = [&](float Z, bool bGround)
	{
		auto& Ring = Rings.AddDefaulted_GetRef();
		Ring.bGround = bGround;
		const float Horizontal = FMath::Sqrt(FMath::Max(0.f, Radius * Radius - Z * Z));
		for (int32 I = 0; I < 64; ++I)
		{
			const float Angle = UE_TWO_PI * I / 64;
			FVector Point(Horizontal * FMath::Cos(Angle), Horizontal * FMath::Sin(Angle), Z);
			Ring.Points.Add(Point);
			Ring.Open.Add(FHALBlastArea::OpenFraction(*GetWorld(), Start, Origin + Point));
		}
	};
	AddRing(0, false);
	const auto* Solar = Cast<UHALExplosiveBallDefinition>(State.Type.Get());
	if (!Solar || !Solar->EruptionMaterial)
	{
		AddRing(.42f * Radius, false);
		AddRing(.76f * Radius, false);
	}
	FCollisionObjectQueryParams WorldGeometry(ECC_WorldStatic);
	TArray<FHitResult> GroundHits;
	GetWorld()->LineTraceMultiByObjectType(GroundHits, Origin + FVector::UpVector * 2., Origin - FVector::UpVector * Radius,
		WorldGeometry, FCollisionQueryParams(SCENE_QUERY_STAT(SolarGround), false, this));
	for (const FHitResult& Hit : GroundHits)
	{
		// The flat footprint is accurate on a level deck. On ramps the 3D wave defines the extent.
		if (Hit.GetComponent() && Hit.GetComponent()->IsPhysicsCollisionEnabled() && Hit.ImpactNormal.Z > .98f)
		{
			const float FloorZ = Hit.ImpactPoint.Z - Origin.Z + 1.;
			if (FMath::Abs(FloorZ) < Radius && FMath::Abs(FloorZ) > 2.f) { AddRing(FloorZ, true); }
			break;
		}
	}
}

void AHALSpecialBallCue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!State.Type) { return; }
	const float Elapsed = FMath::Max(0., BallCueTime(*GetWorld()) - State.Started);
	const float Duration = State.Type->CueSeconds(State.Kind);
	if (Elapsed >= Duration) { SetActorTickEnabled(false); Ribbons->SetVisibility(false, true); return; }
	// Arches can outlast the range ring. Their paint still advances in game seconds throughout the tail.
	const float T = Elapsed / (State.Kind == EHALBallCue::Impact ? State.Type->ImpactSeconds : Duration);
	if (EruptionMID) { EruptionMID->SetScalarParameterValue(TEXT("Progress"), T); }
	if (T >= 1.f) { Ribbons->SetVisibility(false, false); return; }
	if (ClaspBurstMID)
	{
		ClaspBurstMID->SetScalarParameterValue(TEXT("Progress"), T);
		UpdateCageFragments(T, *CastChecked<UHALTimedBombBallDefinition>(State.Type.Get()));
		return;
	}
	Draw(T);
}

void AHALSpecialBallCue::Draw(float T)
{
	FCoronaMesh Corona;
	const auto& Type = *State.Type;
	const float Radius = State.Radius;
	if (const auto* Clasp = Cast<UHALTimedBombBallDefinition>(&Type))
	{
		DrawClaspCue(Corona,*Clasp,State.Kind,T,Radius);
		Ribbons->SetMesh(MoveTemp(Corona.Mesh));
		return;
	}
	const float Envelope = FMath::Min(1.f, (1.f - T) * 4.f);
	const auto Tint = [&](const FLinearColor& Color, float Brightness, float Opacity)
	{
		FLinearColor Result = Color * (Type.Glow * Brightness);
		Result.A = FMath::Clamp(Opacity * Envelope, 0.f, 1.f);
		return Result;
	};
	if (State.Kind == EHALBallCue::Impact)
	{
		const float Front = 1.f - FMath::Pow(1.f - T, 2.6f);
		for (int32 L = 0; L < Rings.Num(); ++L)
		{
			const auto& Ring = Rings[L];
			auto Band = [&](float Outer, float Width, FLinearColor Color)
			{
				for (int32 I = 0; I < Ring.Points.Num(); ++I)
				{
					if (EruptionMID && (I + 3) % 17 < 3) { continue; }
					const int32 J = (I + 1) % Ring.Points.Num();
					const float Inner = FMath::Max(0.f, Outer - Width);
					if (FMath::Min(Ring.Open[I], Ring.Open[J]) <= Inner + .003f) { continue; }
					// A ground ring at constant height starts only when the expanding sphere reaches the deck.
					auto Point = [&](int32 Index, float Scale)
					{
						const FVector End = Ring.Points[Index];
						if (!Ring.bGround) { return End * Scale; }
						const float XY = FMath::Sqrt(FMath::Max(0., FMath::Square(Radius * Scale) - End.Z * End.Z));
						return End.GetSafeNormal2D() * XY + FVector::UpVector * End.Z;
					};
					Corona.Quad(Point(I, Inner), Point(J, Inner), Point(J, FMath::Min(Outer, Ring.Open[J])),
						Point(I, FMath::Min(Outer, Ring.Open[I])), Color);
				}
			};
			if (EruptionMID)
			{
				// The first flash marks the real boundary. A single low ripple then follows the arches.
				if (T < .28f) { Band(1.f, .013f, Tint(Type.CoronaColor, 1.55f, (1.f - T/.28f) * .85f)); }
				if (L == Rings.Num()-1) { Band(FMath::Max(.025f, Front), .022f, Tint(Type.CurrentColor, 1.55f, .73f*(1.f-T))); }
				continue;
			}
			if (T < .28f) { Band(1.f, .018f, Tint(Type.CoronaColor, 2.f, (1.f - T / .28f) * (L == 0 ? .9f : .46f))); }
			Band(FMath::Max(.03f, Front), Ring.bGround ? .055f : .032f, Tint(Type.CoronaColor, 1.7f, .85f));
			Band(FMath::Max(.02f, Front * .91f), .045f, Tint(Type.CurrentColor, 1.15f, .76f));
		}
		if (const auto* Solar = Cast<UHALExplosiveBallDefinition>(&Type); Solar && T < .88f)
		{
			const float Fade = 1.f - FMath::SmoothStep(.40f, .88f, T);
			for (int32 I = 0; I < Solar->FlareCount; ++I)
			{
				const auto& Ring = Rings[!EruptionMID && I % 3 == 0 ? 1 : 0];
				const int32 N = Ring.Points.Num();
				const int32 Center = FMath::RoundToInt(float(I * N) / Solar->FlareCount);
				const int32 Width = FMath::Max(1, N / (Solar->FlareCount*2));
				auto Position = [&](int32 Offset, float Length)
				{
					const int32 J = (Center + Offset + N) % N;
					return Ring.Points[J] * FMath::Min(Length, Ring.Open[J]);
				};
				const float Flicker = .5f + .5f * FMath::Cos(I * 2.40f + T * 4.2f);
				if (EruptionMID)
				{
					const float Tip = (.42f + .51f*Front) * (.78f + .2f*Flicker);
					const int32 J = Center % N;
					if (Tip > Ring.Open[J] || T < .10f) { continue; }
					const FVector Head = Position(0, Tip);
					const FVector Tail = Position(-1, Tip - (.045f + .07f * Flicker)*Fade);
					const FVector HalfWidth = Ring.Points[(J+1)%N].GetSafeNormal2D() - Ring.Points[J].GetSafeNormal2D();
					const FVector Middle = (Head + Tail)*.5;
					Corona.Quad(Tail, Middle + HalfWidth*(Radius*.064), Head, Middle - HalfWidth*(Radius*.064),
						Tint(Type.CoronaColor, 1.5f, .9f*Fade));
					continue;
				}
				const float Root = .025f + .46f * Front * Front;
				const float Crest = Root + (.05f + .32f*Front)*Fade;
				const float Tip = FMath::Min(.96f, Crest + (.11f + .18f*Flicker)*Fade);
				const FVector Inner = Position(-Width, Root);
				const FVector Left = Position(-Width, Crest*.92f);
				const FVector Right = Position(Width, Crest*.88f);
				Corona.Quad(Inner, Left, Position(1,Tip), Right, Tint(Type.CurrentColor, 1.8f, Fade));
				Corona.Quad(Inner, Position(-1, Crest*.82f), Position(1, Tip*.86f), Position(1,Crest*.8f),
					Tint(Type.CoronaColor, 1.55f, Fade));
				if (I % 2 == 0)
				{
					const float Spark = FMath::Min(.97f, Tip + .09f);
					Corona.Quad(Position(2,Spark-.035f), Position(1,Spark), Position(2,Spark+.018f), Position(3,Spark),
						Tint(Type.CoronaColor, 1.8f, Fade));
				}
			}
		}
	}
	else if (Type.IsA<UHALMeteorBallDefinition>())
	{
		// Arrival accretes incandescent fragments. Expiry cools and loosens the rock into drifting ash.
		const bool bArrival = State.Kind == EHALBallCue::Arrival;
		const float Smooth = FMath::SmoothStep(0.f,1.f,T);
		const float Fade = bArrival ? 1.f-Smooth*.45f : 1.f-Smooth;
		for (int32 I=0; I<13; ++I)
		{
			const float Seed = (I%5) / 4.f;
			const float Angle = I*2.39996f + (bArrival ? Smooth*.96f : Smooth*.28f);
			const FVector Out(FMath::Cos(Angle),FMath::Sin(Angle),0);
			const FVector Across(-Out.Y,Out.X,0);
			const float Distance = bArrival ? FMath::Lerp(Type.CoronaSize,.68f,Smooth) : .48f + .88f*Smooth;
			const float Height = bArrival ? (.65f+Seed)*(1.f-Smooth) : T*(.35f+1.15f*Seed);
			const FVector P = Radius * (Out*Distance + FVector::UpVector*Height);
			const FVector W = Across * Radius*(.10f+.08f*Seed)*Fade;
			const FVector H = (Out*.56f+FVector::UpVector)*Radius*(.20f+.12f*Seed)*Fade;
			const FVector Peak = P + (Out*.70f+FVector::UpVector)*Radius*(bArrival ? .60f : .30f)*Fade;
			const FLinearColor Char = FMath::Lerp(Type.CurrentColor,Type.CoreColor,bArrival ? .30f : Smooth);
			Corona.Quad(P-W,P-H,P+W,P+H,Tint(Char,1.1f,Fade));
			Corona.Quad(P-W*.75f,P+H*.24f,P+W*.65f,Peak,Tint(Type.CoronaColor,bArrival ? 1.7f : Fade,Fade));
		}
	}
	else
	{
		const bool bArrival = State.Kind == EHALBallCue::Arrival;
		const float Size = Radius * FMath::Lerp(Type.CoronaSize, bArrival ? 1.f : .12f, FMath::SmoothStep(0.f, 1.f, T));
		for (int32 Arm = 0; Arm < 6; ++Arm)
		{
			const FQuat Tilt(FVector::ForwardVector, FMath::DegreesToRadians(16.f + 17.f * (Arm % 3)));
			const FQuat Rotation = FQuat(FVector::UpVector, UE_TWO_PI * Arm / 6.f) * Tilt;
			auto Point = [&](int32 Step, float Thickness)
			{
				const float S = Step / 18.f;
				const float Angle = S * (bArrival ? 2.4f : 1.55f) + T * (bArrival ? 3.f : -1.8f);
				const float Distance = Size * (.60f + .40f * S + Thickness * FMath::Sin(UE_PI * S));
				return Rotation.RotateVector(FVector(FMath::Cos(Angle), FMath::Sin(Angle), .18f * (S - .5f)) * Distance);
			};
			for (int32 Step = 0; Step < 18; ++Step)
			{
				const FLinearColor Ink = Arm % 2 ? Type.CurrentColor : Type.CoronaColor;
				Corona.Quad(Point(Step, 0), Point(Step+1, 0), Point(Step+1, .13f), Point(Step, .13f), Tint(Ink, bArrival ? 1.6f : 1.f-T*.65f, .95f));
			}
		}
		if (!bArrival)
		{
			for (int32 I = 0; I < 12; ++I)
			{
				const float Azimuth = I * 2.4f;
				const FVector Radial(FMath::Cos(Azimuth), FMath::Sin(Azimuth), 0);
				const FVector P = Radial * Radius * (.6f + T * (.35f + (I % 3)*.15f)) + FVector::UpVector * Radius * T * .65f;
				const FVector Along = Radial * Radius * (.18f - .12f*T);
				const FVector Side = FVector::CrossProduct(Radial, FVector::UpVector) * Radius * .06;
				Corona.Quad(P-Along, P-Side, P+Along, P+Side, Tint(Type.CoronaColor, 1.f-T*.7f, 1.f-T));
			}
		}
	}
	Ribbons->SetMesh(MoveTemp(Corona.Mesh));
}

void AHALSpecialBallCue::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHALSpecialBallCue, State);
}
