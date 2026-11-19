#include "HALHookComponent.h"
#include "HALHookThrow.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"

namespace
{
	bool HookInputLogEnabled()
	{ const auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("hal.HookLog")); return C&&C->GetInt()!=0; }
}

UHALHookComponent::UHALHookComponent() { PrimaryComponentTick.bCanEverTick=false; SetIsReplicatedByDefault(true); }
void UHALHookComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UHALHookComponent,OwnedHook); DOREPLIFETIME(UHALHookComponent,CapturingHook); }
bool UHALHookComponent::Claim(AHALHookThrow& Hook)
{ if(!GetOwner()->HasAuthority()||IsValid(CapturingHook)) { return false; } CapturingHook=&Hook; GetOwner()->ForceNetUpdate(); return true; }
void UHALHookComponent::ReleaseClaim(AHALHookThrow& Hook)
{ if(GetOwner()->HasAuthority()&&CapturingHook==&Hook) { CapturingHook=nullptr; GetOwner()->ForceNetUpdate(); } }
void UHALHookComponent::SetOwnedHook(AHALHookThrow* Hook)
{ if(GetOwner()->HasAuthority()) { OwnedHook=Hook; GetOwner()->ForceNetUpdate(); } }
bool UHALHookComponent::BlocksActions() const { return IsValid(OwnedHook)&&OwnedHook->LocksCaster(); }
bool UHALHookComponent::BlocksActionsOn(const AActor* Actor)
{ const auto* C=Actor?Actor->FindComponentByClass<UHALHookComponent>():nullptr; return C&&C->BlocksActions(); }
void UHALHookComponent::ObserveSteering(float Value)
{
	const auto* P=Cast<APawn>(GetOwner());
	if(!P||!P->IsLocallyControlled()||!IsValid(OwnedHook)||OwnedHook->GetPhase()!=EHALHookPhase::Choosing) { LocalChoice.Invalidate(); return; }
	if(LocalChoice!=OwnedHook->GetActivation()) { LocalChoice=OwnedHook->GetActivation(); LastSentDirection=2; }
	if(!FMath::IsFinite(Value)||Value<-1||Value>1||FMath::IsNearlyEqual(Value,LastSentDirection,.01f)) { return; }
	LastSentDirection=Value; ++NextDirectionSequence; if(!NextDirectionSequence) { ++NextDirectionSequence; }
	UE_CLOG(HookInputLogEnabled(),LogTemp,Log,TEXT("Hook input Owner=%s Authority=%d Activation=%s Sequence=%u Value=%.2f"),*GetNameSafe(GetOwner()),GetOwner()->HasAuthority(),*LocalChoice.ToString(),NextDirectionSequence,Value);
	ServerDirection(LocalChoice,NextDirectionSequence,Value);
}
void UHALHookComponent::ServerDirection_Implementation(FGuid Activation,uint32 Sequence,float Value)
{
	const auto* P=Cast<APawn>(GetOwner());
	if(!P||!Cast<APlayerController>(P->GetController())||P->GetController()->GetPawn()!=P||!IsValid(OwnedHook)) { return; }
	UE_CLOG(HookInputLogEnabled(),LogTemp,Log,TEXT("Hook authority direction Owner=%s Activation=%s Sequence=%u Value=%.2f Phase=%d"),*GetNameSafe(GetOwner()),*Activation.ToString(),Sequence,Value,int32(OwnedHook->GetPhase()));
	OwnedHook->AcceptDirection(Activation,Sequence,Value);
}
void UHALHookComponent::NotifyExternalLaunch() { if(IsValid(OwnedHook)) { OwnedHook->Interrupt(false); } }
void UHALHookComponent::ClearForOwnerExit()
{
	if(!GetOwner()->HasAuthority()) { return; }
	if(IsValid(OwnedHook)) { OwnedHook->Interrupt(true); }
	if(IsValid(CapturingHook)) { CapturingHook->TargetLost(); }
	OwnedHook=CapturingHook=nullptr;
}
void UHALHookComponent::EndPlay(EEndPlayReason::Type Reason) { ClearForOwnerExit(); Super::EndPlay(Reason); }
