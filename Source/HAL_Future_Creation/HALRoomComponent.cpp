#include "HALRoomComponent.h"
#include "HALRoomConfig.h"
#include "HALVehicleCatalog.h"
#include "HALPlayerController.h"
#include "HALPlayerState.h"
#include "HALAbilitySystemComponent.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UHALRoomComponent::UHALRoomComponent() { PrimaryComponentTick.bCanEverTick = false; SetIsReplicatedByDefault(true); }
void UHALRoomComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UHALRoomComponent,Data); }
void UHALRoomComponent::OnRep_Data() { OnRoomChanged.Broadcast(); }
void UHALRoomComponent::Publish() { ++Data.Revision; OnRep_Data(); GetOwner()->ForceNetUpdate(); }
bool UHALRoomComponent::CanAdmitPlayer() const
{ return Data.Phase == EHALRoomPhase::Open && Data.Members.Num() < Data.Rules.MaximumPlayers(); }
EHALRoomResult UHALRoomComponent::Initialize(UHALRoomConfig* Config)
{
	if (!GetOwner()->HasAuthority()) { return EHALRoomResult::NotAuthority; }
	FString Error;
	if (IsEnabled() || !Config || !Config->Validate(Error) || GetNetMode() == NM_DedicatedServer)
	{ return EHALRoomResult::InvalidConfiguration; }
	ServerConfig = Config; Data.Rules = Config->Rules; Data.Phase = EHALRoomPhase::Open; Publish(); return EHALRoomResult::Accepted;
}
void UHALRoomComponent::EnablePracticeRoom()
{
	if (!GetOwner()->HasAuthority() || !ServerConfig || Data.Phase != EHALRoomPhase::Open) { return; }
	Data.bPracticeRoom = true; Data.Catalog = ServerConfig->Catalog; Publish();
}
void UHALRoomComponent::CloseForReturn()
{
	if (GetOwner()->HasAuthority() && IsEnabled() && Data.Phase != EHALRoomPhase::Returning)
	{ Data.Phase = EHALRoomPhase::Returning; Publish(); }
}
EHALRoomResult UHALRoomComponent::FailAvatar(AHALPlayerController* PC, uint32 Revision, EHALRoomResult Reason)
{
	if (!GetOwner()->HasAuthority()) { return EHALRoomResult::NotAuthority; }
	auto* PS = Member(PC); if (!PS) { return EHALRoomResult::NotMember; }
	auto P = PS->GetRoomData();
	if (Data.Phase != EHALRoomPhase::Open || P.Phase != EHALRoomPlayerPhase::PreparingAvatar) { return EHALRoomResult::WrongPhase; }
	if (P.SelectionRevision != Revision || Revision == MAX_uint32) { return EHALRoomResult::StaleSelection; }
	P.Phase = EHALRoomPlayerPhase::Selecting; P.ReadyAvatar = nullptr; P.AvatarResult = Reason;
	++P.SelectionRevision; PS->SetRoomData(P); Publish(); return Reason;
}
AHALPlayerState* UHALRoomComponent::Member(const AHALPlayerController* PC) const
{
	AHALPlayerState* PS = PC && PC->GetWorld() == GetWorld() ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
	return PS && !PS->IsActorBeingDestroyed() && PS->GetOwner() == PC && Data.Members.Contains(PS) ? PS : nullptr;
}
EHALRoomResult UHALRoomComponent::AddMember(AHALPlayerController* PC)
{
	if (!GetOwner()->HasAuthority()) { return EHALRoomResult::NotAuthority; }
	if (Member(PC)) { return EHALRoomResult::Accepted; }
	if (!CanAdmitPlayer()) { return Data.Phase == EHALRoomPhase::Open ? EHALRoomResult::Full : EHALRoomResult::WrongPhase; }
	auto* PS = PC && PC->GetWorld() == GetWorld() ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
	if (!PS || PS->GetOwner() != PC || PS->IsActorBeingDestroyed()) { return EHALRoomResult::NotMember; }
	if (!Data.Host && !PC->IsLocalController()) { return EHALRoomResult::NotHost; }
	bool Used[4] = {};
	for (const auto& P : Data.Members) { if (P && P->GetRoomData().Seat >= 0 && P->GetRoomData().Seat < 4) { Used[P->GetRoomData().Seat] = true; } }
	int32 Seat = 0; while (Seat < 4 && Used[Seat]) { ++Seat; }
	if (Seat == 4) { return EHALRoomResult::Full; }
	static const FLinearColor Colors[] = { FLinearColor(1,.25f,.08f),FLinearColor(.1f,.9f,.3f),FLinearColor(1,.75f,.05f),FLinearColor(.85f,.1f,.65f) };
	FHALRoomPlayerData Player; Player.Seat = Seat; Player.Number = Seat+1; Player.Color = Colors[Seat];
	Player.Phase = EHALRoomPlayerPhase::Selecting; Player.SelectionRevision = 1;
	PS->SetRoomData(Player); Data.Members.Add(PS); if (!Data.Host) { Data.Host = PS; }
	Publish(); return EHALRoomResult::Accepted;
}
void UHALRoomComponent::RemoveMember(AHALPlayerController* PC)
{
	if (!GetOwner()->HasAuthority()) { return; }
	// Cleanup must still remove the seat when PlayerState has already entered EndPlay.
	auto* PS = PC && PC->GetWorld() == GetWorld() ? PC->GetPlayerState<AHALPlayerState>() : nullptr;
	if (!PS || !Data.Members.Contains(PS)) { return; }
	if (PS == Data.Host || Data.Phase != EHALRoomPhase::Open) { Data.Phase = EHALRoomPhase::Returning; }
	Data.Members.Remove(PS); if (!PS->IsActorBeingDestroyed()) { PS->SetRoomData({}); } Publish();
	// GF-3/4 own actual connection/map recovery. Never silently shrink a locked exhibition round.
}
bool UHALRoomComponent::AvatarReady(const AHALPlayerController* PC) const
{
	const auto* PS = Member(PC); const auto* Pawn = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
	FString Error;
	UClass* Class = PS && ServerConfig && ServerConfig->Catalog ? ServerConfig->Catalog->Resolve(PS->GetRoomData().VehicleId,Error) : nullptr;
	const auto* ASC = PS ? PS->GetHALAbilitySystemComponent() : nullptr;
	const auto* Health = Pawn ? Pawn->FindComponentByClass<UVehicleHealthComponent>() : nullptr;
	return Class && Pawn && Pawn->GetClass() == Class && Pawn->GetController() == PC && Pawn->GetPlayerState() == PS
		&& Pawn->IsConfigurationValid() && ASC && Pawn->GetAbilitySystemComponent() == ASC
		&& ASC->GetOwnerActor() == PS && ASC->GetAvatarActor() == Pawn && ASC->IsHealthInitialized()
		&& ASC->IsCombatInitialized() && ASC->IsImpactInitialized() && Health && Health->IsHealthReady() && Health->GetCurrentHP() > 0;
}
bool UHALRoomComponent::CanInitializeAvatar(const AHALPlayerController* PC, const APawn* Next) const
{
	const auto* PS = Member(PC); FString Error;
	if (!PS || !Next || Next->GetWorld() != GetWorld() || !ServerConfig || !ServerConfig->Catalog) { return false; }
	const auto& P = PS->GetRoomData();
	if (Data.Phase != EHALRoomPhase::Open && Data.Phase != EHALRoomPhase::Loading) { return false; }
	if (Data.Phase == EHALRoomPhase::Open && P.Phase != EHALRoomPlayerPhase::PreparingAvatar) { return false; }
	if (Data.Phase == EHALRoomPhase::Loading && !Data.LockedRoster.ContainsByPredicate([PS,&P](const FHALRoomRosterEntry& E)
	{ return E.Player == PS && E.VehicleId == P.VehicleId && E.SelectionRevision == P.SelectionRevision; })) { return false; }
	return Next->GetClass() == ServerConfig->Catalog->Resolve(P.VehicleId,Error);
}
void UHALRoomComponent::InvalidateAvatarForLifecycle(AHALPlayerController* PC)
{
	if (!GetOwner()->HasAuthority() || (Data.Phase != EHALRoomPhase::Open && Data.Phase != EHALRoomPhase::Loading)) { return; }
	auto* PS = Member(PC); if (!PS) { return; }
	auto Player = PS->GetRoomData();
	if (Player.Phase == EHALRoomPlayerPhase::PreparingAvatar && !Player.ReadyAvatar) { return; }
	Player.Phase = EHALRoomPlayerPhase::PreparingAvatar; Player.ReadyAvatar = nullptr; PS->SetRoomData(Player); Publish();
}
EHALRoomResult UHALRoomComponent::CompleteAvatar(AHALPlayerController* PC, uint32 Revision, uint32 ExpectedRound)
{
	if (!GetOwner()->HasAuthority()) { return EHALRoomResult::NotAuthority; }
	auto* PS = Member(PC); if (!PS) { return EHALRoomResult::NotMember; }
	auto Player = PS->GetRoomData();
	if ((Data.Phase != EHALRoomPhase::Open && Data.Phase != EHALRoomPhase::Loading) || Player.Phase != EHALRoomPlayerPhase::PreparingAvatar) { return EHALRoomResult::WrongPhase; }
	if (Data.Phase == EHALRoomPhase::Loading && (!ExpectedRound || ExpectedRound != Data.RoundId)) { return EHALRoomResult::StaleSelection; }
	if (Data.Phase == EHALRoomPhase::Loading && (PS->GetVehicleLifecycle().RoundId != ExpectedRound || PS->GetVehicleLifecycle().Avatar != PC->GetPawn())) { return EHALRoomResult::NotReady; }
	if (Player.SelectionRevision != Revision) { return EHALRoomResult::StaleSelection; }
	if (!AvatarReady(PC)) { return EHALRoomResult::NotReady; }
	Player.Phase = EHALRoomPlayerPhase::Ready; Player.ReadyAvatar = PC->GetPawn(); PS->SetRoomData(Player); Publish(); return EHALRoomResult::Accepted;
}
EHALRoomResult UHALRoomComponent::CheckStart(const AHALPlayerController* PC) const
{
	if (!IsEnabled()) { return EHALRoomResult::Inactive; }
	if (Data.Phase != EHALRoomPhase::Open) { return EHALRoomResult::WrongPhase; }
	if (!Member(PC) || Member(PC) != Data.Host || !PC->IsLocalController()) { return EHALRoomResult::NotHost; }
	if (Data.Members.Num() < Data.Rules.MinimumPlayers()) { return EHALRoomResult::NotReady; }
	for (const auto& PS : Data.Members)
	{
		const auto* P = PS ? Cast<AHALPlayerController>(PS->GetOwner()) : nullptr;
		if (!PS || PS->GetRoomData().Phase != EHALRoomPlayerPhase::Ready || !P
			|| PS->GetRoomData().ReadyAvatar != P->GetPawn() || !AvatarReady(P)) { return EHALRoomResult::NotReady; }
	}
	return Data.RoundId == MAX_uint32 ? EHALRoomResult::InvalidRequest : EHALRoomResult::Accepted;
}
EHALRoomResult UHALRoomComponent::Request(AHALPlayerController* PC, EHALRoomAction Action, FName Id, uint32 Revision)
{
	if (!GetOwner()->HasAuthority()) { return EHALRoomResult::NotAuthority; }
	if (!IsEnabled()) { return EHALRoomResult::Inactive; }
	auto* PS = Member(PC); if (!PS) { return EHALRoomResult::NotMember; }
	if (Data.Phase != EHALRoomPhase::Open) { return EHALRoomResult::WrongPhase; }
	auto Player = PS->GetRoomData();
	if (Player.SelectionRevision != Revision) { return EHALRoomResult::StaleSelection; }
	if (Action != EHALRoomAction::SelectVehicle && !Id.IsNone()) { return EHALRoomResult::InvalidRequest; }
	FString Error;
	if (Action == EHALRoomAction::Start)
	{
		const auto Result = CheckStart(PC); if (Result != EHALRoomResult::Accepted) { return Result; }
		Data.LockedRoster.Reset();
		for (const auto& MemberPS : Data.Members)
		{
			const auto& M = MemberPS->GetRoomData(); FHALRoomRosterEntry E;
			E.Player = MemberPS; E.Seat = M.Seat; E.VehicleId = M.VehicleId; E.SelectionRevision = M.SelectionRevision; Data.LockedRoster.Add(E);
		}
		++Data.RoundId; Data.Phase = EHALRoomPhase::Locked; Publish(); return EHALRoomResult::Accepted;
	}
	if (Player.SelectionRevision == MAX_uint32) { return EHALRoomResult::InvalidRequest; }
	switch (Action)
	{
	case EHALRoomAction::SelectVehicle:
		if (Player.Phase != EHALRoomPlayerPhase::Selecting) { return EHALRoomResult::WrongPhase; }
		if (!ServerConfig || !ServerConfig->Catalog || !ServerConfig->Catalog->Resolve(Id,Error)) { return EHALRoomResult::InvalidVehicle; }
		if (Player.VehicleId == Id) { return EHALRoomResult::Accepted; }
		Player.VehicleId = Id; break;
	case EHALRoomAction::ConfirmVehicle:
		if (Player.Phase != EHALRoomPlayerPhase::Selecting) { return EHALRoomResult::WrongPhase; }
		if (!ServerConfig || !ServerConfig->Catalog || !ServerConfig->Catalog->Resolve(Player.VehicleId,Error)) { return EHALRoomResult::InvalidVehicle; }
		Player.Phase = EHALRoomPlayerPhase::PreparingAvatar; Player.ReadyAvatar = nullptr; Player.AvatarResult = EHALRoomResult::Accepted; break;
	case EHALRoomAction::CancelConfirmation:
		if (Player.Phase == EHALRoomPlayerPhase::Selecting) { return EHALRoomResult::Accepted; }
		Player.Phase = EHALRoomPlayerPhase::Selecting; Player.ReadyAvatar = nullptr; Player.AvatarResult = EHALRoomResult::Accepted; break;
	default: return EHALRoomResult::InvalidRequest;
	}
	++Player.SelectionRevision; PS->SetRoomData(Player); Publish(); return EHALRoomResult::Accepted;
}
EHALRoomResult UHALRoomComponent::AdvanceRound(uint32 ExpectedRound, EHALRoomPhase Next)
{
	if (!GetOwner()->HasAuthority()) { return EHALRoomResult::NotAuthority; }
	if (!ExpectedRound || ExpectedRound != Data.RoundId) { return EHALRoomResult::StaleSelection; }
	const bool bValid = (Data.Phase == EHALRoomPhase::Locked && Next == EHALRoomPhase::Loading)
		|| (Data.Phase == EHALRoomPhase::Loading && Next == EHALRoomPhase::Countdown)
		|| (Data.Phase == EHALRoomPhase::Countdown && Next == EHALRoomPhase::Playing)
		|| (Data.Phase != EHALRoomPhase::Inactive && Next == EHALRoomPhase::Returning);
	if (!bValid) { return EHALRoomResult::WrongPhase; }
	Data.Phase = Next; Publish(); return EHALRoomResult::Accepted;
}
bool UHALRoomComponent::AllowsPlayerActions(const APawn* Pawn) const
{
	const auto* PS = Pawn ? Pawn->GetPlayerState<AHALPlayerState>() : nullptr;
	if (!PS || !Data.Members.Contains(PS) || !Pawn->GetController() || Pawn->GetController()->GetPawn() != Pawn
		|| PS->GetRoomData().Phase != EHALRoomPlayerPhase::Ready || PS->GetRoomData().ReadyAvatar != Pawn) { return false; }
	if (Data.Phase == EHALRoomPhase::Open) { return true; }
	return Data.Phase == EHALRoomPhase::Playing && Data.LockedRoster.ContainsByPredicate([PS](const FHALRoomRosterEntry& E)
	{ return E.Player == PS && E.VehicleId == PS->GetRoomData().VehicleId && E.SelectionRevision == PS->GetRoomData().SelectionRevision; });
}
