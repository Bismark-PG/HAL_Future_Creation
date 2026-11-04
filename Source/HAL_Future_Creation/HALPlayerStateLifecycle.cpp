#include "HALPlayerState.h"
#include "HALAbilitySystemComponent.h"
#include "HALMatchGameState.h"
#include "HALRoomComponent.h"
#include "HALPlayerController.h"
#include "TestVehiclePawn.h"
#include "Engine/World.h"

void AHALPlayerState::PrepareForWorldExit()
{
	if (!HasAuthority()) { return; }
	if (auto* Pawn=Cast<ATestVehiclePawn>(AbilitySystem->GetAvatarActor())) { Pawn->RetireForVehicleTransition(); }
	Lifecycle.Avatar=nullptr; RoomData.ReadyAvatar=nullptr; RoomData.bClientLoaded=false; RoomData.LoadToken.Invalidate();
	AbilitySystem->InitAbilityActorInfo(this,nullptr); ForceNetUpdate();
}
void AHALPlayerState::RestoreTravelCounters(uint32 Serial, uint32 Revision, uint32 Round)
{
	if (!HasAuthority()) { return; }
	Lifecycle.ChangeSerial=FMath::Max(Lifecycle.ChangeSerial,Serial); Lifecycle.Revision=FMath::Max(Lifecycle.Revision,Revision);
	Lifecycle.RoundId=FMath::Max(Lifecycle.RoundId,Round); Lifecycle.Avatar=nullptr;
	bLastVehicleTransitionSucceeded=false; LastVehicleTransitionKey=FObjectKey();
	AbilitySystem->InitAbilityActorInfo(this,nullptr);
}
void AHALPlayerState::CopyProperties(APlayerState* Other)
{
	Super::CopyProperties(Other);
	if (auto* Next=Cast<AHALPlayerState>(Other))
	{
		auto P=RoomData; P.ReadyAvatar=nullptr; P.bClientLoaded=false; P.LoadToken.Invalidate(); Next->SetRoomData(P);
		Next->RestoreTravelCounters(Lifecycle.ChangeSerial,Lifecycle.Revision,Lifecycle.RoundId);
	}
}

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
	// GAS InitializeComponent defaults Owner and Avatar to the PlayerState before its first Pawn exists.
	const bool bInitialOwnerAvatar = !Lifecycle.Revision && !PC->GetPawn()
		&& AbilitySystem->GetOwnerActor() == this && AbilitySystem->GetAvatarActor() == this;
	if (AbilitySystem->GetAvatarActor() && AbilitySystem->GetAvatarActor() != PC->GetPawn() && !bInitialOwnerAvatar) { return false; }
	if (Serial < Lifecycle.ChangeSerial || Lifecycle.Revision == MAX_uint32 || (Round && Round <= Lifecycle.RoundId)
		|| PC->GetPawn() == Next || Next->GetController() || !Next->GetLifecycleSeed(Seed)) { return false; }
	const auto* State = GetWorld()->GetGameState<AHALMatchGameState>();
	auto* Room = State ? State->GetRoom() : nullptr;
	if (Room && Room->IsEnabled())
	{
		if (!Room->CanInitializeAvatar(PC,Next) || (Round && (Room->GetData().Phase != EHALRoomPhase::Loading || Room->GetData().RoundId != Round))
			|| (!Round && Room->GetData().Phase != EHALRoomPhase::Open)) { return false; }
	}
	FString AbilityError;
	if (!ValidateVehicleAbilities(Next,AbilityError,Cast<ATestVehiclePawn>(AbilitySystem->GetAvatarActor())))
	{ UE_LOG(LogHALAbility,Error,TEXT("Vehicle transition abilities rejected: %s"),*AbilityError); return false; }
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
	if (PC->GetPawn() != Next || AbilitySystem->GetAvatarActor() != Next || !AreVehicleAbilitiesReady(Next))
	{ Lifecycle.Avatar = nullptr; AbilitySystem->ClearAvatarIfCurrent(Next); ForceNetUpdate(); return false; }
	if (Round) { Lifecycle.RoundId = Round; }
	bLastVehicleTransitionSucceeded = true; ForceNetUpdate();
	UE_LOG(LogTemp,Log,TEXT("Vehicle lifecycle: Player=%d Serial=%u Revision=%u Round=%u Avatar=%s MaxHP=%.1f Attack=%.1f Reduction=%d"),
		GetPlayerId(),Serial,Lifecycle.Revision,Lifecycle.RoundId,*GetNameSafe(Next),Seed.MaxHP,Seed.Combat.AttackPower,Seed.Impact.TierReduction);
	return true;
}
