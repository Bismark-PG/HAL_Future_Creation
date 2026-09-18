#include "VehicleConfigurationApplication.h"

#include "VehicleDefinition.h"
#include "TestVehiclePawn.h"
#include "PassiveTestVehicle.h"
#include "BasicBallActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

bool VehicleConfiguration::UsesDefinition(const AActor* Actor)
{
	if (const ATestVehiclePawn* Vehicle = Cast<ATestVehiclePawn>(Actor)) { return Vehicle->GetConfigurationSource() == EVehicleConfigurationSource::Definition; }
	if (const APassiveTestVehicle* Vehicle = Cast<APassiveTestVehicle>(Actor)) { return Vehicle->GetConfigurationSource() == EVehicleConfigurationSource::Definition; }
	if (const ABasicBallActor* Ball = Cast<ABasicBallActor>(Actor)) { return Ball->GetConfigurationSource() == EVehicleConfigurationSource::Definition; }
	return false;
}

bool VehicleConfiguration::IsReadyForGameplay(const AActor* Actor)
{
	if (const ATestVehiclePawn* Vehicle = Cast<ATestVehiclePawn>(Actor)) { return Vehicle->IsConfigurationValid(); }
	if (const APassiveTestVehicle* Vehicle = Cast<APassiveTestVehicle>(Actor)) { return Vehicle->IsConfigurationValid(); }
	if (const ABasicBallActor* Ball = Cast<ABasicBallActor>(Actor)) { return Ball->IsConfigurationValid(); }
	return IsValid(Actor);
}

void VehicleConfiguration::ApplyBody(UPrimitiveComponent& Component, const FVehicleBodyConfig& Config)
{
	check(!Component.GetOwner()->HasActorBegunPlay());
	Component.SetRelativeScale3D(Config.Scale);
	// CopyBodyInstancePropertiesFrom asserts that the destination has no live body.
	Component.DestroyPhysicsState();
	Component.BodyInstance.CopyBodyInstancePropertiesFrom(&Config.Body);
	Component.SetGenerateOverlapEvents(Config.bGenerateOverlapEvents);
	Component.SetHiddenInGame(Config.bHiddenInGame);
	Component.RecreatePhysicsState();
}

void VehicleConfiguration::ApplyVisual(UStaticMeshComponent& Component, const FVehicleVisualConfig& Config)
{
	Component.SetStaticMesh(Config.Mesh);
	Component.EmptyOverrideMaterials();
	for (int32 Index = 0; Index < Config.Materials.Num(); ++Index) { Component.SetMaterial(Index, Config.Materials[Index]); }
	Component.SetOverlayMaterial(Config.OverlayMaterial);
	Component.SetRelativeTransform(Config.RelativeTransform);
	Component.SetVisibility(Config.bVisible);
	Component.SetHiddenInGame(Config.bHiddenInGame);
	Component.SetCastShadow(Config.bCastShadow);
	Component.SetSimulatePhysics(false);
	Component.SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void VehicleConfiguration::ApplyCamera(USpringArmComponent& Boom, UCameraComponent& Camera, const FVehicleCameraConfig& Config)
{
	Boom.SetAbsolute(Config.bAbsoluteLocation, Config.bAbsoluteRotation, Config.bAbsoluteScale);
	Boom.SetRelativeTransform(Config.BoomTransform);
	Boom.TargetArmLength = Config.TargetArmLength;
	Boom.SocketOffset = Config.SocketOffset;
	Boom.TargetOffset = Config.TargetOffset;
	Boom.bDoCollisionTest = Config.bDoCollisionTest;
	Boom.ProbeSize = Config.ProbeSize;
	Boom.ProbeChannel = Config.ProbeChannel;
	Boom.bUsePawnControlRotation = Config.bUsePawnControlRotation;
	Boom.bInheritPitch = Config.bInheritPitch;
	Boom.bInheritYaw = Config.bInheritYaw;
	Boom.bInheritRoll = Config.bInheritRoll;
	Boom.bEnableCameraLag = Config.bEnableCameraLag;
	Boom.bEnableCameraRotationLag = Config.bEnableCameraRotationLag;
	Boom.CameraLagSpeed = Config.CameraLagSpeed;
	Boom.CameraRotationLagSpeed = Config.CameraRotationLagSpeed;
	Boom.CameraLagMaxDistance = Config.CameraLagMaxDistance;
	Boom.bUseCameraLagSubstepping = Config.bUseCameraLagSubstepping;
	Boom.CameraLagMaxTimeStep = Config.CameraLagMaxTimeStep;
	Boom.bClampToMaxPhysicsDeltaTime = Config.bClampToMaxPhysicsDeltaTime;
	Boom.bDrawDebugLagMarkers = Config.bDrawDebugLagMarkers;
	Camera.SetRelativeTransform(Config.CameraTransform);
	Camera.SetFieldOfView(Config.FieldOfView);
	Camera.AspectRatio = Config.AspectRatio;
	Camera.bConstrainAspectRatio = Config.bConstrainAspectRatio;
	Camera.PostProcessBlendWeight = Config.PostProcessBlendWeight;
	Camera.PostProcessSettings = Config.PostProcess;
	Camera.bUsePawnControlRotation = false;
}
