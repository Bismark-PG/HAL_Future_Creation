#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "../HALDynamicCameraComponent.h"
#include "../HALCameraConfig.h"
#include "../TestVehiclePawn.h"
#include "../HALPlayerState.h"
#include "../HALMovementEffectBridge.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "Misc/ScopeLock.h"
#include "UObject/UnrealType.h"
#include "GameFramework/PlayerController.h"

struct FHALCameraTestAccess
{
	static const FHALCameraState& State(const UHALDynamicCameraComponent* C) { return C->State; }
	static void ViewController(UHALDynamicCameraComponent* C,APlayerController* PC) { C->PreviousController=PC; }
	static void InputIdentity(ATestVehiclePawn* Pawn,bool Linked)
	{
		auto* PS=Pawn->GetPlayerState<AHALPlayerState>();
		auto* Data=FindFProperty<FStructProperty>(PS->GetClass(),TEXT("RoomData"))->ContainerPtrToValuePtr<FHALRoomPlayerData>(PS);
		Data->Phase=EHALRoomPlayerPhase::Ready; Data->ReadyAvatar=Linked?Pawn:nullptr;
	}
	static void MovementReady(ATestVehiclePawn* Pawn,bool Ready,bool Recovery=false)
	{
		auto* M=Pawn->FindComponentByClass<UArcadeVehicleMovementComponent>();
		FScopeLock Lock(&M->PendingPhysicsDataLock);
		M->PublishedMovementStatus.bReady=Ready; M->PublishedMovementStatus.bRecovery=Recovery;
		M->PublishedMovementStatus.Value.Epoch=Pawn->MovementEffectBridge->GetBindingEpoch();
	}
	static void NewBindingEpoch(ATestVehiclePawn* Pawn) { Pawn->MovementEffectBridge->Epoch=FGuid::NewGuid(); }
	static void DrivingIntent(ATestVehiclePawn* Pawn) { Pawn->CurrentInputCommand.Throttle=1; Pawn->CurrentInputCommand.Steering=1; }
	static bool Compute(UHALDynamicCameraComponent* C, float Dt, FMinimalViewInfo& View, bool bAccept = true,
		FVector2D Size = FVector2D(960,540)) { return C->Compute(Dt,Size,bAccept,View); }
	static UHALCameraConfig* Enable(ATestVehiclePawn* Pawn, float FOV = 90, float Distance = 1200)
	{
		auto* Component = Pawn->FindComponentByClass<UHALDynamicCameraComponent>();
		auto* Local = NewObject<UVehicleLocalConfig>(Component);
		auto* Config = NewObject<UHALCameraConfig>(Local);
		Config->bReadyForUse = true; Config->BaseCamera.FieldOfView = FOV; Config->BaseCamera.TargetArmLength = Distance;
		Config->BaseCamera.BoomTransform = FTransform(FRotator(-75,0,0));
		Local->CameraProfile = Config; Component->Configure(Local); return Config;
	}
};
#endif
