#include "HALPlayerState.h"
#include "HALAbilitySystemComponent.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALPlayerController.h"
#include "TestVehiclePawn.h"
#include "Engine/World.h"

bool AHALPlayerState::CanBindVehicleAvatar(const ATestVehiclePawn* Pawn) const
{ return !Lifecycle.Revision || Lifecycle.Avatar == Pawn; }
void AHALPlayerState::OnRep_Lifecycle()
{
	if (auto* Pawn = Lifecycle.Avatar.Get(); Pawn && Pawn->GetPlayerState() == this)
	{ Pawn->RefreshAbilitySystemBinding(); }
}
bool AHALPlayerState::ChangeVehicleAvatar(ATestVehiclePawn* Next, uint32 ChangeSerial)
{ return TransitionVehicleAvatar(Next,ChangeSerial,0); }
bool AHALPlayerState::BeginNewRoundAvatar(ATestVehiclePawn* Next, uint32 RoundId, uint32 ChangeSerial)
{ return RoundId && TransitionVehicleAvatar(Next,ChangeSerial,RoundId); }
bool AHALPlayerState::TransitionVehicleAvatar(ATestVehiclePawn* Next, uint32 Serial, uint32 Round)
{
	auto* PC = Cast<AHALPlayerController>(GetOwner());
	if (!HasAuthority() || !PC || PC->GetPlayerState<AHALPlayerState>() != this || !Serial || !IsValid(Next) || Next->IsActorBeingDestroyed()
		|| Next->GetWorld() != GetWorld() || !Next->HasAuthority() || bVehicleTransitionInProgress) { return false; }
	if (Serial == Lifecycle.ChangeSerial)
	{
		return bLastVehicleTransitionSucceeded && LastVehicleTransitionKey == FObjectKey(Next)
			&& LastVehicleTransitionRound == Round && PC->GetPawn() == Next && AbilitySystem->GetAvatarActor() == Next;
	}
	FHALVehicleLifecycleSeed Seed;
	if (AbilitySystem->GetOwnerActor() && AbilitySystem->GetOwnerActor() != this) { return false; }
	if (AbilitySystem->GetAvatarActor() && AbilitySystem->GetAvatarActor() != PC->GetPawn()) { return false; }
	if (Serial < Lifecycle.ChangeSerial || Lifecycle.Revision == MAX_uint32 || (Round && Round <= Lifecycle.RoundId)
		|| PC->GetPawn() == Next || Next->GetController() || !Next->GetLifecycleSeed(Seed)) { return false; }
	const auto* State = GetWorld()->GetGameState<AHALMatchGameState>();
	auto* Room = State ? State->GetRoom() : nullptr;
	if (Room && Room->IsEnabled())
	{
		if (!Room->CanInitializeAvatar(PC,Next) || (Round && (Room->GetData().Phase != EHALRoomPhase::Loading || Room->GetData().RoundId != Round))
			|| (!Round && Room->GetData().Phase != EHALRoomPhase::Open)) { return false; }
	}
	TGuardValue<bool> Guard(bVehicleTransitionInProgress,true);
	Lifecycle.ChangeSerial = Serial; ++Lifecycle.Revision;
	LastVehicleTransitionKey = FObjectKey(Next); LastVehicleTransitionRound = Round; bLastVehicleTransitionSucceeded = false;
	if (Room && Room->IsEnabled()) { Room->InvalidateAvatarForLifecycle(PC); }
	if (auto* Old = Cast<ATestVehiclePawn>(PC->GetPawn())) { Old->RetireForVehicleTransition(); }
	PC->UnPossess();
	Lifecycle.Avatar = Next;
	AbilitySystem->InitAbilityActorInfo(this,Next);
	if (!AbilitySystem->ResetVehicleAttributes(Seed,Round != 0))
	{ Lifecycle.Avatar = nullptr; AbilitySystem->ClearAvatarIfCurrent(Next); ForceNetUpdate(); return false; }
	PC->Possess(Next);
	if (PC->GetPawn() != Next || AbilitySystem->GetAvatarActor() != Next)
	{ Lifecycle.Avatar = nullptr; AbilitySystem->ClearAvatarIfCurrent(Next); ForceNetUpdate(); return false; }
	if (Round) { Lifecycle.RoundId = Round; }
	bLastVehicleTransitionSucceeded = true; ForceNetUpdate();
	UE_LOG(LogTemp,Log,TEXT("Vehicle lifecycle: Player=%d Serial=%u Revision=%u Round=%u Avatar=%s MaxHP=%.1f Attack=%.1f Reduction=%d"),
		GetPlayerId(),Serial,Lifecycle.Revision,Lifecycle.RoundId,*GetNameSafe(Next),Seed.MaxHP,Seed.Combat.AttackPower,Seed.Impact.TierReduction);
	return true;
}
