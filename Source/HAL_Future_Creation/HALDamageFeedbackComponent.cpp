#include "HALDamageFeedbackComponent.h"
#include "HALDamageFeedbackSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> GHALDamageFeedbackLog(TEXT("hal.DamageFeedbackLog"),0,TEXT("Log authoritative damage feedback pulses and local receipt."));
UHALDamageFeedbackComponent::UHALDamageFeedbackComponent()
{ SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=false; }
void UHALDamageFeedbackComponent::NotifyResolvedDamage(const FGuid& Attack,uint32 Pulse,float Damage)
{
	if(!GetWorld()||!GetOwner()||!GetOwner()->HasAuthority()||!Attack.IsValid()||!Pulse||!FMath::IsFinite(Damage)||Damage<=0) { return; }
	FHALDamageFeedbackPulse P; if(++NextSequence==0) { ++NextSequence; }
	P.Sequence=NextSequence; P.AttackId=Attack; P.AttackPulse=Pulse; P.ActualDamage=Damage;
	P.ServerTime=UHALDamageFeedbackSubsystem::ServerTime(GetWorld()); MulticastPulse(P);
}
void UHALDamageFeedbackComponent::MulticastPulse_Implementation(FHALDamageFeedbackPulse P)
{
	if(!GetWorld()||!IsValid(GetOwner())||GetOwner()->IsActorBeingDestroyed()) { return; }
	if(!P.Sequence||!P.AttackId.IsValid()||!P.AttackPulse||!FMath::IsFinite(P.ServerTime)||!FMath::IsFinite(P.ActualDamage)||P.ActualDamage<=0
		||(bReceived&&int32(P.Sequence-LastSequence)<=0)) { return; }
	bReceived=true; LastSequence=P.Sequence; ++ReceivedCount;
	if(auto* FX=GetWorld()->GetSubsystem<UHALDamageFeedbackSubsystem>()) { FX->Receive(GetOwner(),P); }
	if(GHALDamageFeedbackLog.GetValueOnGameThread())
	{ UE_LOG(LogTemp,Display,TEXT("DamageFeedback Target=%s Sequence=%u Attack=%s Pulse=%u Damage=%.2f NetMode=%d"),*GetNameSafe(GetOwner()),P.Sequence,*P.AttackId.ToString(),P.AttackPulse,P.ActualDamage,int32(GetNetMode())); }
}
void UHALDamageFeedbackComponent::ClearPresentation()
{ if(GetWorld()) if(auto* FX=GetWorld()->GetSubsystem<UHALDamageFeedbackSubsystem>()) { FX->ClearTarget(GetOwner()); } }
void UHALDamageFeedbackComponent::EndPlay(EEndPlayReason::Type Reason)
{ ClearPresentation(); Super::EndPlay(Reason); }
