#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "TestVehiclePawn.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

namespace
{
	const UHALRoomComponent* SpectatorRoom(const AHALPlayerController* PC)
	{ const auto* S=PC->GetWorld()?PC->GetWorld()->GetGameState<AHALMatchGameState>():nullptr; return S?S->GetRoom():nullptr; }
	TArray<AHALPlayerState*> Candidates(const UHALRoomComponent* R)
	{
		TArray<AHALPlayerState*> Result;
		if (!R) { return Result; }
		for (const auto& P:R->GetData().Match.Players)
		{
			if (P.Life!=EHALRoundLife::Alive) { continue; }
			for (const auto& PS:R->GetData().Members)
			{ if (PS && PS->GetRoomData().PlayerKey==P.PlayerKey && IsValid(PS->GetPawn())) { Result.Add(PS); break; } }
		}
		Result.Sort([](const auto& A,const auto& B){ return A.GetRoomData().Number<B.GetRoomData().Number; });
		return Result;
	}
}
bool AHALPlayerController::IsRoundSpectating() const
{
	const auto* R=SpectatorRoom(this); const auto* PS=GetPlayerState<AHALPlayerState>();
	if (!IsLocalController() || !R || !PS || !R->GetData().Match.bActive
		|| (R->GetData().Phase!=EHALRoomPhase::Playing && R->GetData().Phase!=EHALRoomPhase::Results)) { return false; }
	const auto* P=R->GetData().Match.Find(PS->GetRoomData().PlayerKey);
	return P && (P->Life==EHALRoundLife::Eliminated || P->Life==EHALRoundLife::Forfeited);
}
void AHALPlayerController::CycleSpectatedPlayer(int32 Direction)
{
	if (!IsRoundSpectating() || !Direction) { return; }
	const auto* R=SpectatorRoom(this); if (R->GetData().Phase!=EHALRoomPhase::Playing) { return; }
	const auto List=Candidates(R); if (List.IsEmpty()) { return; }
	const int32 I=List.IndexOfByPredicate([this](const auto* P){ return P->GetRoomData().PlayerKey==SpectatedKey; });
	const auto* Next=List[I==INDEX_NONE?0:(I+(Direction<0?-1:1)+List.Num())%List.Num()];
	SpectatedKey=Next->GetRoomData().PlayerKey; SpectatedNumber=Next->GetRoomData().Number;
}
void AHALPlayerController::UpdateRoundSpectator(float Dt)
{
	if (!IsLocalController() || !GetLocalPlayer() || GetWorld()->bIsTearingDown) { return; }
	if (!IsRoundSpectating())
	{
		if (auto* Vehicle=Cast<ATestVehiclePawn>(GetPawn())) { LastSpectatorFocus=Vehicle->GetActorLocation(); bHasSpectatorFocus=true; }
		if (RoundSpectatorCamera) { ClearRoundSpectator(); if (GetPawn()) { SetViewTarget(GetPawn()); } }
		return;
	}
	const auto* R=SpectatorRoom(this); const auto List=Candidates(R);
	const auto* Selected=List.FindByPredicate([this](const auto* P){ return P->GetRoomData().PlayerKey==SpectatedKey; });
	if (!Selected && !List.IsEmpty()) { Selected=&List[0]; }
	if (Selected)
	{ LastSpectatorFocus=(*Selected)->GetPawn()->GetActorLocation(); bHasSpectatorFocus=true;
		SpectatedKey=(*Selected)->GetRoomData().PlayerKey; SpectatedNumber=(*Selected)->GetRoomData().Number; }
	else { SpectatedKey.Invalidate(); SpectatedNumber=0; }
	const auto& Rules=R->GetData().Match.Rules;
	const FRotator Rotation(Rules.SpectatorPitch,0,0);
	const FVector Location=(bHasSpectatorFocus?LastSpectatorFocus:GetFocalLocation())-Rotation.Vector()*Rules.SpectatorDistance;
	if (!RoundSpectatorCamera)
	{
		FActorSpawnParameters P; P.ObjectFlags|=RF_Transient; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		RoundSpectatorCamera=GetWorld()->SpawnActor<ACameraActor>(Location,Rotation,P);
		if (!RoundSpectatorCamera) { return; }
		RoundSpectatorCamera->SetReplicates(false); bAutoManageActiveCameraTarget=false; SetViewTarget(RoundSpectatorCamera);
	}
	RoundSpectatorCamera->SetActorLocationAndRotation(FMath::VInterpTo(RoundSpectatorCamera->GetActorLocation(),Location,Dt,8),Rotation);
	RoundSpectatorCamera->GetCameraComponent()->SetFieldOfView(Rules.SpectatorFOV);
}
void AHALPlayerController::ClearRoundSpectator()
{
	if (RoundSpectatorCamera) { RoundSpectatorCamera->Destroy(); RoundSpectatorCamera=nullptr; }
	bAutoManageActiveCameraTarget=true; SpectatedKey.Invalidate(); SpectatedNumber=0;
}
