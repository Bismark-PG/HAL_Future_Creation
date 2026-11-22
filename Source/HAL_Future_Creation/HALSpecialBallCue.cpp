#include "HALSpecialBallCue.h"
#include "HALBombActor.h"
#include "HALBlackHoleBall.h"
#include "HALMatchBallLifecycle.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
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
	if (Kind == EHALBallCue::Impact && World.GetNetMode() != NM_Standalone && World.GetGameState())
	{
		auto* MatchBalls = World.GetGameState()->FindComponentByClass<UHALMatchBallLifecycle>();
		auto* Solar = Cast<UHALExplosiveBallDefinition>(Type);
		if (MatchBalls && Solar)
		{
			MatchBalls->MulticastExplosiveImpact(Solar, Origin, Radius, Normal, Started);
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
	if (State.Kind == EHALBallCue::Impact)
	{
		PrepareWave();
		const auto* Solar = Cast<UHALExplosiveBallDefinition>(State.Type.Get());
		if (Solar && Solar->EruptionMaterial) { PrepareEruption(*Solar); }
	}
	SetActorTickEnabled(true);
	Tick(0);
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
	Draw(T);
}

void AHALSpecialBallCue::Draw(float T)
{
	FCoronaMesh Corona;
	const auto& Type = *State.Type;
	const float Radius = State.Radius;
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
