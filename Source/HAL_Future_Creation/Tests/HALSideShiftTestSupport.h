#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../HALSideShiftComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BallControlComponent.h"
#include "../TestVehiclePawn.h"
#include "../BasicBallActor.h"
#include "../HALVehiclePresentationComponent.h"

struct FHALSideShiftTestAccess
{
	static void Pose(ATestVehiclePawn* P,bool Grounded=true)
	{
		auto* M=P->FindComponentByClass<UArcadeVehicleMovementComponent>(); M->bShouldRunPhysics.Store(true);
		M->PublishedStability.bValid=true; M->PublishedStability.bGrounded=M->PublishedStability.bDrivingAllowed=Grounded;
		M->PublishedStability.Up=M->PublishedStability.GroundNormal=FVector::UpVector;
		M->PublishedContact.bValid=true; M->PublishedContact.Rotation=P->GetActorQuat();
		M->PublishedContact.Position=P->GetActorLocation(); M->PublishedContact.Velocity=FVector::ZeroVector;
	}
	static bool Acquire(ATestVehiclePawn* P,ABasicBallActor* Ball) { return P->FindComponentByClass<UBallControlComponent>()->AcquireBall(Ball); }
	static const FHALSideShiftVisualState& Visual(const UHALSideShiftComponent& C) { return C.Visual; }
	static void Visual(UHALSideShiftComponent& C,const FHALSideShiftFX& FX,int8 Side,double Age)
	{
		C.Visual={}; C.Visual.Activation=FGuid::NewGuid(); C.Visual.FX=FX; C.Visual.Side=Side;
		C.Visual.Direction=FVector(0,Side,0); C.Visual.StartServerTime=C.ServerTime()-Age;
		C.Visual.ShiftSeconds=FHALSideShiftMotion{}.DurationSeconds; C.Visual.DriftSeconds=FHALSideShiftMotion{}.DriftSeconds; C.Visual.bDriving=true; C.OnRep_Visual();
	}
	static const auto& Jets(const UHALSideShiftComponent& C) { return C.Jets; }
	static void ClearFX(UHALSideShiftComponent& C) { C.ClearParticles(true); }
	static void EndFX(UHALSideShiftComponent& C) { C.Visual.bDriving=false; C.OnRep_Visual(); }
	static void ReplayRequest(UHALSideShiftComponent& C,int8 Side,uint32 Sequence) { C.ProcessRequest(Side,Sequence); }
	static void SetupMovement(UArcadeVehicleMovementComponent& M) { M.SetAsyncPhysicsTickEnabled(false); M.bShouldRunPhysics.Store(true); M.RecoveryRules.bEnabled=false; }
	static void SetInput(ATestVehiclePawn& P,float Steering,bool Drift)
	{ P.CurrentInputCommand.Steering=Steering; P.CurrentInputCommand.bHandbrake=Drift; P.PushInputCommand(); }
	static void DriveInput(ATestVehiclePawn& P,float Steering,bool Drift,float Throttle)
	{ P.CurrentInputCommand.Throttle=Throttle; SetInput(P,Steering,Drift); }
};
#endif
