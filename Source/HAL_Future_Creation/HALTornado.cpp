#include "HALTornado.h"
#include "HALTornadoAbility.h"
#include "HALSlipperyTrail.h"
#include "HALGameFlowPermissions.h"
#include "HALGameplayTags.h"
#include "HALAbilitySystemComponent.h"
#include "HALImpactReceiverComponent.h"
#include "TestVehiclePawn.h"
#include "BasicBallActor.h"
#include "CombatResolver.h"
#include "VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> LogTornado(TEXT("hal.TornadoLog"), 0, TEXT("Car05 authority phases, wall stops, vehicle hits and ball disarms."));
	bool Range(float V, float Min, float Max) { return FMath::IsFinite(V) && V >= Min && V <= Max; }
	FCollisionObjectQueryParams WorldObjects()
	{ FCollisionObjectQueryParams O; O.AddObjectTypesToQuery(ECC_WorldStatic); O.AddObjectTypesToQuery(ECC_WorldDynamic); return O; }
	bool Solid(const FHitResult& Hit)
	{
		const auto* C = Hit.GetComponent(); const auto* A = Hit.GetActor();
		return C && C->IsPhysicsCollisionEnabled() && !Cast<ATestVehiclePawn>(A) && !Cast<ABasicBallActor>(A)
			&& (C->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block || C->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block);
	}
	bool GroundAt(const UWorld& W, const AActor* Ignore, const FHALTornadoRules& R, const FVector& Probe, FVector& Out)
	{
		TArray<FHitResult> Hits; FCollisionQueryParams Q(SCENE_QUERY_STAT(TornadoGround), false, Ignore);
		W.LineTraceMultiByObjectType(Hits, Probe + FVector::UpVector * R.MaximumGroundStep,
			Probe - FVector::UpVector * R.GroundProbe, WorldObjects(), Q);
		for (const auto& Hit : Hits) if (Solid(Hit))
		{
			if (Hit.bStartPenetrating || Hit.ImpactNormal.Z < R.MinimumGroundNormalZ || Hit.GetComponent()->IsSimulatingPhysics()) { return false; }
			Out = Hit.ImpactPoint; return true;
		}
		return false;
	}
	bool ClearLine(const UWorld& W, const AActor* Ignore, const FVector& From, const FVector& To)
	{
		TArray<FHitResult> Hits; W.LineTraceMultiByObjectType(Hits, From, To, WorldObjects(), FCollisionQueryParams(SCENE_QUERY_STAT(TornadoOcclusion), false, Ignore));
		for (const auto& Hit : Hits) { if (Solid(Hit) && Hit.Time < .999f) { return false; } } return true;
	}
}
bool FHALTornadoRules::IsValid() const
{
	return Range(GatherSeconds,.1,5) && Range(ActiveSeconds,.1,20) && Range(DissipateSeconds,.1,5)
		&& Range(Speed,0,2000) && Range(FrontOffset,0,300) && Range(Radius,30,400) && Range(Height,60,600)
		&& Range(GroundProbe,20,300) && Range(MaximumGroundStep,5,100) && Range(MinimumGroundNormalZ,.2,1)
		&& Range(TargetCooldownSeconds,.1,5) && HALImpact::IsValidPushPreset(PushPreset)
		&& Range(BallSpeed,100,12000) && Range(BallUpwardSpeed,0,2000) && Range(BallCooldownSeconds,.1,2) && Range(DisarmPickupLockSeconds,.1,2);
}
EHALTornadoPhase FHALTornadoRules::Phase(double Age) const
{
	if (Age < 0 || !FMath::IsFinite(Age) || Age >= TotalSeconds()) { return EHALTornadoPhase::Finished; }
	if (Age < GatherSeconds) { return EHALTornadoPhase::Gathering; }
	return Age < GatherSeconds + ActiveSeconds ? EHALTornadoPhase::Active : EHALTornadoPhase::Dissipating;
}
float FHALTornadoRules::PhaseProgress(double Age) const
{
	switch (Phase(Age))
	{
	case EHALTornadoPhase::Gathering: return float(Age / GatherSeconds);
	case EHALTornadoPhase::Active: return float((Age - GatherSeconds) / ActiveSeconds);
	case EHALTornadoPhase::Dissipating: return float((Age - GatherSeconds - ActiveSeconds) / DissipateSeconds);
	default: return 1;
	}
}
bool FHALTornadoFX::IsValid() const
{
	for (auto C : {ShadowColor, MidColor, LightColor, GrayColor}) if (!Range(C.R,0,10) || !Range(C.G,0,10) || !Range(C.B,0,10)) { return false; }
	return Range(ToneSoftness,.02,.4) && Range(Opacity,0,1) && Range(SpinRate,0,3600) && Range(FlowSpeed,0,10)
		&& Range(Turbulence,0,10) && Range(GatherRadiusScale,1,3) && Range(GroundLift,0,30)
		&& Range(DifferentialRotation,0,.8) && Range(RiseSpeed,0,1) && Range(CoreOpacity,0,1)
		&& Range(OuterOpacity,0,1) && Range(OuterRadiusScale,1,1.5) && Range(OuterSpeedScale,.1,2)
		&& Range(GrayMix,0,1) && Range(ColorCycleSeconds,.2,10)
		&& Range(FormationExponent,.4,3) && Range(FormationFeather,.03,.4) && Range(GlowStrength,0,4) && Range(GlowPower,1,8)
		&& Range(ShadowThreshold,0,1) && Range(LightThreshold,0,1) && LightThreshold >= ShadowThreshold+.02f;
}
AHALTornado::AHALTornado()
{
	bReplicates = true; bAlwaysRelevant = true; SetReplicateMovement(false); SetNetUpdateFrequency(25);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("TornadoRoot")));
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}
bool AHALTornado::OwnerReady() const
{
	const auto* V = Cast<ATestVehiclePawn>(GetOwner());
	const auto* H = V ? V->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	return V && V->IsConfigurationValid() && V->GetController() && V->GetController()->GetPawn() == V
		&& H && H->IsHealthReady() && H->GetCurrentHP() > 0 && HALGameFlow::Permissions(GetWorld(), V).bAbilities;
}
bool AHALTornado::FindStart(const ATestVehiclePawn& V, const FHALTornadoRules& R, FVector& Ground)
{
	if (!R.IsValid() || !AHALSlipperyTrail::CanLay(V)) { return false; }
	const auto* Body = Cast<UPrimitiveComponent>(V.GetRootComponent()); if (!Body) { return false; }
	const auto Forward = Body->GetForwardVector().GetSafeNormal2D(); if (Forward.IsNearlyZero()) { return false; }
	const float Extent = FVector::DotProduct(Forward.GetAbs(), Body->Bounds.BoxExtent);
	const FVector Probe = Body->Bounds.Origin + Forward * (Extent + R.Radius + R.FrontOffset);
	if (!GroundAt(*V.GetWorld(), &V, R, Probe, Ground)) { return false; }
	TArray<FHitResult> Hits;
	V.GetWorld()->SweepMultiByObjectType(Hits, Ground + FVector(0,0,R.Radius+2), Ground + FVector(0,0,R.Radius+2),
		FQuat::Identity, WorldObjects(), FCollisionShape::MakeSphere(R.Radius), FCollisionQueryParams(SCENE_QUERY_STAT(TornadoSpawn),false,&V));
	for (const auto& Hit : Hits) if (Solid(Hit) && Hit.ImpactNormal.Z < R.MinimumGroundNormalZ) { return false; }
	return ClearLine(*V.GetWorld(), &V, Body->Bounds.Origin, Ground + FVector(0,0,Body->Bounds.BoxExtent.Z));
}
double AHALTornado::ServerTime() const
{ const auto* S = GetWorld()->GetGameState(); return S ? S->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds(); }
bool AHALTornado::Start(const UHALTornadoConfig& Config, const FVector& Ground)
{
	FString Error;
	if (!HasAuthority() || !OwnerReady() || State.Activation.IsValid() || !Config.Validate(Error) || Ground.ContainsNaN()
		|| !FCombatResolver::CaptureVehicleAttack(*this,*GetOwner(),0,0,Attack)) { return false; }
	State.Rules = Config.Rules; State.FX = Config.FX; State.Ground = Ground;
	State.Direction = GetOwner()->GetActorForwardVector().GetSafeNormal2D(); if (State.Direction.IsNearlyZero()) { return false; }
	State.Activation = FGuid::NewGuid(); State.StartServerTime = State.SampleServerTime = ServerTime();
	Attack.SourceTag = HALGameplayTags::Combat_Source_Tornado; Attack.bCanHitInstigator = false;
	SetActorLocation(Ground); SetActorTickEnabled(true); ForceNetUpdate();
	UE_CLOG(LogTornado.GetValueOnGameThread(), LogTemp, Log, TEXT("Tornado begin %s Activation=%s"), *GetName(), *State.Activation.ToString()); return true;
}
bool AHALTornado::SweptContact(const FVector& A, const FVector& B, float Radius, float Height, float& Time)
{
	if (A.ContainsNaN() || B.ContainsNaN() || !FMath::IsFinite(Radius) || !FMath::IsFinite(Height) || Radius <= 0 || Height <= 0) { return false; }
	double Min = 0, Max = 1; const FVector D = B-A;
	if (FMath::Abs(D.Z) < SMALL_NUMBER) { if (A.Z < 0 || A.Z > Height) { return false; } }
	else
	{
		double L = -A.Z/D.Z, H = (Height-A.Z)/D.Z; if (L > H) { Swap(L,H); }
		Min = FMath::Max(Min,L); Max = FMath::Min(Max,H); if (Min > Max) { return false; }
	}
	const double Q = D.SizeSquared2D(), C = A.SizeSquared2D()-FMath::Square(double(Radius));
	if ((A+D*Min).SizeSquared2D() <= FMath::Square(double(Radius))) { Time = float(Min); return true; }
	if (Q < SMALL_NUMBER) { return false; }
	const double Linear = 2*(A.X*D.X+A.Y*D.Y), Discriminant = Linear*Linear-4*Q*C;
	if (Discriminant < 0) { return false; }
	const double Entry = (-Linear-FMath::Sqrt(Discriminant))/(2*Q);
	if (Entry < Min || Entry > Max) { return false; } Time = float(Entry); return true;
}
void AHALTornado::Contacts(const FVector& From, const FVector& To, float Horizon)
{
	if (!HasAuthority() || bGameplayEnded || State.Rules.Phase(ServerTime()-State.StartServerTime) != EHALTornadoPhase::Active) { return; }
	const double Now = ServerTime(); const auto& R = State.Rules;
	for (auto It = NextHit.CreateIterator(); It; ++It) if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	for (auto It = BallContacts.CreateIterator(); It; ++It)
	{
		const auto* Ball = It->Get(); float Time = 0;
		const float HalfHeight = Ball ? Ball->GetPhysicsRoot()->Bounds.BoxExtent.Z : 0;
		const FVector Relative = Ball ? Ball->GetActorLocation()-To+FVector(0,0,HalfHeight) : FVector::ZeroVector;
		if (!Ball || !SweptContact(Relative,Relative,R.Radius+Ball->GetPhysicsRoot()->Bounds.BoxExtent.X,R.Height+2*HalfHeight,Time)) { It.RemoveCurrent(); }
	}
	const auto Contact = [&](AActor& Target, UPrimitiveComponent& Body, FVector& Center, FVector& Direction) -> bool
	{
		if (Target.IsActorBeingDestroyed() || !Body.IsSimulatingPhysics()) { return false; }
		if (const auto* Next = NextHit.Find(&Target); Next && Now < *Next) { return false; }
		const FVector P = Body.Bounds.Origin, E = Body.Bounds.BoxExtent;
		const FVector End = P + Body.GetPhysicsLinearVelocity()*Horizon;
		const float Extra = FMath::Min(E.X,E.Y); float Time = 0;
		if (!SweptContact(P-From+FVector(0,0,E.Z),End-To+FVector(0,0,E.Z),R.Radius+Extra,R.Height+2*E.Z,Time)) { return false; }
		Center = FMath::Lerp(From,To,Time); const auto TargetPoint = FMath::Lerp(P,End,Time);
		const FVector Origin(Center.X,Center.Y,FMath::Clamp(TargetPoint.Z,Center.Z+5,Center.Z+R.Height));
		if (!ClearLine(*GetWorld(),this,Origin,TargetPoint)) { return false; }
		// Do not pre-empt a real world obstacle that the ball reaches earlier this physics step.
		if (!ClearLine(*GetWorld(),&Target,P,TargetPoint)) { return false; }
		Direction = (TargetPoint-Center).GetSafeNormal2D(); if (Direction.IsNearlyZero()) { Direction = State.Direction; }
		return true;
	};
	for (TActorIterator<ATestVehiclePawn> It(GetWorld()); It; ++It)
	{
		auto* V = *It; if (V == GetOwner()) { continue; }
		auto* Health = V->FindComponentByClass<UVehicleHealthComponent>();
		auto* Body = Cast<UPrimitiveComponent>(V->GetRootComponent());
		FVector Center, Direction;
		if (!Health || !Health->IsHealthReady() || Health->GetCurrentHP() <= 0 || !Body || !Contact(*V,*Body,Center,Direction)) { continue; }
		FHALCombatAction Action; auto Snapshot = Attack; Snapshot.AttackId = FGuid::NewGuid();
		if (!Action.Begin(*this,Snapshot)) { continue; }
		NextHit.Add(V,Now+R.TargetCooldownSeconds);
		FHALCombatHitRequest Request; Request.AttackId = Action.GetSnapshot().AttackId; Request.PulseSequence = Action.GetPulseSequence();
		Request.bApplyDamage = false; Request.bApplyImpact = true; Request.Impact.Mode = EHALImpactMode::ExplicitTier;
		Request.Impact.Tier = EHALImpactTier::Heavy; Request.Impact.PushPreset = R.PushPreset;
		Request.Hit.TargetActor = V; Request.Hit.TargetPhysicsBody = Body;
		Request.Hit.ImpactNormal = -Direction; Request.Hit.ImpactPoint = Body->Bounds.Origin;
		const auto Result = FCombatResolver::ResolveCombatHit(Action,Request);
		if (Result.bHitAccepted) { ++VehicleHitCount; LastAcceptedUpwardSpeed = Result.RuleMinimumUpwardSpeed; }
		UE_CLOG(LogTornado.GetValueOnGameThread(),LogTemp,Log,TEXT("Tornado vehicle Target=%s Accepted=%d Tier=%d Up=%.1f"),*V->GetName(),Result.bHitAccepted,int32(Result.ImpactSummary.EffectiveTier),Result.RuleMinimumUpwardSpeed);
	}
	for (TActorIterator<ABasicBallActor> It(GetWorld()); It; ++It)
	{
		auto* Ball = *It; if (Ball->GetBallState() == EBasicBallState::Controlled || BallContacts.Contains(Ball)) { continue; }
		FVector Center, Direction; auto* Body = Ball->GetPhysicsRoot();
		if (!Body || !Contact(*Ball,*Body,Center,Direction)) { continue; }
		float Entry = 0; const auto E = Body->Bounds.BoxExtent;
		const FVector P = Body->Bounds.Origin, End = P+Body->GetPhysicsLinearVelocity()*Horizon;
		if (!SweptContact(P-From+FVector(0,0,E.Z),End-To+FVector(0,0,E.Z),R.Radius+E.X,R.Height+2*E.Z,Entry)) { continue; }
		if (Entry > SMALL_NUMBER)
		{
			TArray<FHitResult> Earlier;
			GetWorld()->SweepMultiByObjectType(Earlier,P,FMath::Lerp(P,End,Entry),FQuat::Identity,FCollisionObjectQueryParams::AllObjects,
				FCollisionShape::MakeSphere(Body->GetScaledSphereRadius()),FCollisionQueryParams(SCENE_QUERY_STAT(TornadoBallFirstContact),false,Ball));
			bool bBlocked = false;
			for (const auto& Hit : Earlier)
			{
				const auto* C = Hit.GetComponent();
				bBlocked |= C && C->IsPhysicsCollisionEnabled() && Hit.Time < .999f
					&& C->GetCollisionResponseToChannel(Body->GetCollisionObjectType()) == ECR_Block;
			}
			if (bBlocked) { continue; }
		}
		PendingBall = Ball;
		if (Ball->DeflectByTornado(*this, Direction*R.BallSpeed+FVector::UpVector*R.BallUpwardSpeed))
		{ NextHit.Add(Ball,Now+R.BallCooldownSeconds); BallContacts.Add(Ball); ++BallHitCount; }
		PendingBall.Reset();
	}
}
bool AHALTornado::AuthorizesBallContact(const ABasicBallActor& Ball) const
{
	return HasAuthority() && PendingBall.Get() == &Ball && !bGameplayEnded && OwnerReady()
		&& State.Rules.Phase(ServerTime()-State.StartServerTime) == EHALTornadoPhase::Active;
}
void AHALTornado::FinishGameplay()
{
	if (bGameplayEnded) { return; } bGameplayEnded = true; NextHit.Reset(); PendingBall.Reset(); BallContacts.Reset(); OnGameplayEnded.Broadcast(this);
}
void AHALTornado::Simulate(float DeltaSeconds)
{
	if (!OwnerReady()) { Destroy(); return; }
	const double Age = ServerTime()-State.StartServerTime; const auto& R = State.Rules;
	if (R.Phase(Age) == EHALTornadoPhase::Finished) { Destroy(); return; }
	if (R.Phase(Age) == EHALTornadoPhase::Dissipating) { FinishGameplay(); LastAge = Age; return; }
	if (R.Phase(Age) != EHALTornadoPhase::Active) { LastAge = Age; return; }
	const FVector From = State.Ground;
	double Remaining = FMath::Max(0., Age-FMath::Max(LastAge,double(R.GatherSeconds)));
	while (!State.bStopped && Remaining > SMALL_NUMBER)
	{
		const float Step = float(FMath::Min(Remaining,.025)); Remaining -= Step;
		FVector Next = State.Ground+State.Direction*R.Speed*Step, Ground;
		if (!GroundAt(*GetWorld(),this,R,Next,Ground) || FMath::Abs(Ground.Z-State.Ground.Z) > R.MaximumGroundStep)
		{ State.bStopped = true; break; }
		const auto Lift = FVector(0,0,FMath::Min(R.Radius,R.Height*.5f)+2);
		TArray<FHitResult> Hits; GetWorld()->SweepMultiByObjectType(Hits,State.Ground+Lift,Ground+Lift,FQuat::Identity,WorldObjects(),
			FCollisionShape::MakeSphere(R.Radius),FCollisionQueryParams(SCENE_QUERY_STAT(TornadoWall),false,this));
		float Limit = 1;
		for (const auto& Hit : Hits) if (Solid(Hit) && Hit.ImpactNormal.Z < R.MinimumGroundNormalZ) { Limit = FMath::Min(Limit,Hit.Time); }
		State.Ground = FMath::Lerp(State.Ground,Ground,Limit < 1 ? FMath::Max(0.f,Limit-.001f) : 1.f);
		if (Limit < 1) { State.bStopped = true; ForceNetUpdate(); }
	}
	State.SampleServerTime = ServerTime(); SetActorLocation(State.Ground); LastAge = Age;
	Contacts(From,State.Ground,FMath::Min(DeltaSeconds,float(R.GatherSeconds+R.ActiveSeconds-Age)));
}
void AHALTornado::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); if (!State.Activation.IsValid()) { return; }
	if (HasAuthority()) { Simulate(DeltaSeconds); }
	else
	{
		FVector Target = State.Ground;
		if (!State.bStopped && State.Rules.Phase(ServerTime()-State.StartServerTime) == EHALTornadoPhase::Active)
		{ Target += State.Direction*State.Rules.Speed*FMath::Clamp(ServerTime()-State.SampleServerTime,0.,.06); }
		SetActorLocation(FMath::VInterpTo(GetActorLocation(),Target,DeltaSeconds,18));
	}
	if (!IsActorBeingDestroyed()) { UpdatePresentation(DeltaSeconds); }
#if !UE_BUILD_SHIPPING
	if (State.Rules.bDrawBounds) { DrawDebugCylinder(GetWorld(),GetActorLocation(),GetActorLocation()+FVector::UpVector*State.Rules.Height,State.Rules.Radius,24,FColor::Cyan,false,0); }
#endif
}
void AHALTornado::OnRep_State()
{
	SetActorTickEnabled(State.Activation.IsValid());
	if (!bPresented) { SetActorLocation(State.Ground); bPresented = true; }
}
void AHALTornado::EndPlay(EEndPlayReason::Type Reason)
{ FinishGameplay(); Super::EndPlay(Reason); }
void AHALTornado::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHALTornado,State); }
