#include "HALPursuitTargetComponent.h"
#include "HALPolicePursuit.h"
#include "HALPolicePursuitAbility.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "TestVehiclePawn.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
UHALPursuitTargetComponent::UHALPursuitTargetComponent()
{ SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false; PrimaryComponentTick.TickGroup=TG_PostPhysics; }
double UHALPursuitTargetComponent::PresentationTime() const
{ const auto* S=GetWorld()->GetGameState(); return S?S->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds(); }
bool UHALPursuitTargetComponent::AcceptArrest(AHALPolicePursuit& Source)
{
	auto* V=Cast<ATestVehiclePawn>(GetOwner()); auto* A=V?Cast<UHALAbilitySystemComponent>(V->GetAbilitySystemComponent()):nullptr;
	if(!GetOwner()->HasAuthority()||!Source.AuthorizesArrest(V)||!A||A->GetAvatarActor()!=V||A->IsStunned()
		||A->HasMatchingGameplayTag(HALGameplayTags::State_PursuitProtected)) { return false; }
	if(ASC.Get()!=A) { Clear(); ASC=A; StunHandle=A->RegisterGameplayTagEvent(HALGameplayTags::State_Stun,EGameplayTagEventType::NewOrRemoved).AddUObject(this,&ThisClass::StunChanged); }
	const auto Apply=[&](TSubclassOf<UGameplayEffect> Class)
	{
		auto Context=A->MakeEffectContext(); Context.AddSourceObject(&Source); auto Spec=A->MakeOutgoingSpec(Class,1,Context);
		return Spec.IsValid()?A->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()):FActiveGameplayEffectHandle();
	};
	Protection=Apply(UHALPursuitProtectionEffect::StaticClass()); if(!Protection.IsValid()) { return false; }
	if(!A->ApplyStun(Source.GetState().Rules.StunSeconds,&Source)) { A->RemoveActiveGameplayEffect(Protection); Protection.Invalidate(); return false; }
	Arrest=Apply(UHALPursuitArrestEffect::StaticClass());
	ensureMsgf(Arrest.IsValid(),TEXT("Native arrest effect must apply to a ready ASC"));
	State.FX=Source.GetState().FX; State.bArrested=true; State.bProtected=true; State.ProtectionEnd=-1;
	State.ArrestStart=PresentationTime(); State.ReleaseStart=-1;
	ProtectionSeconds=Source.GetState().Rules.ProtectionSeconds;
	GetWorld()->GetTimerManager().SetTimer(Timer,this,&ThisClass::UpdateProtection,.05f,true);
	GetOwner()->ForceNetUpdate(); OnRep_State(); return true;
}
void UHALPursuitTargetComponent::StunChanged(FGameplayTag,int32 Count)
{
	if(!GetOwner()->HasAuthority()||Count>0||!State.bArrested) { return; }
	State.bArrested=false; State.ProtectionEnd=GetWorld()->GetTimeSeconds()+ProtectionSeconds;
	State.ReleaseStart=PresentationTime();
	if(auto* A=ASC.Get()) { A->RemoveActiveGameplayEffect(Arrest); } Arrest.Invalidate();
	GetOwner()->ForceNetUpdate(); OnRep_State();
}
void UHALPursuitTargetComponent::UpdateProtection()
{
	if(!GetOwner()->HasAuthority()) { return; }
	const auto* V=Cast<ATestVehiclePawn>(GetOwner()); auto* A=ASC.Get();
	if(!V||!A||A->GetAvatarActor()!=V||!V->CanReceiveSkillEffects()) { Clear(); return; }
	if(State.bArrested&&!A->IsStunned()) { StunChanged(HALGameplayTags::State_Stun,0); }
	if(!State.bArrested&&State.bProtected&&GetWorld()->GetTimeSeconds()>=State.ProtectionEnd)
	{ A->RemoveActiveGameplayEffect(Protection); Protection.Invalidate(); State.bProtected=false; GetWorld()->GetTimerManager().ClearTimer(Timer); GetOwner()->ForceNetUpdate(); }
}
void UHALPursuitTargetComponent::Clear()
{
	if(GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(Timer); }
	if(auto* A=ASC.Get())
	{
		A->RegisterGameplayTagEvent(HALGameplayTags::State_Stun,EGameplayTagEventType::NewOrRemoved).Remove(StunHandle);
		if(GetOwner()->HasAuthority()) { A->RemoveActiveGameplayEffect(Arrest); A->RemoveActiveGameplayEffect(Protection); }
	}
	ASC.Reset(); StunHandle.Reset(); Arrest.Invalidate(); Protection.Invalidate(); State=FHALPursuitTargetState();
	OnRep_State(); if(GetOwner()->HasAuthority()) { GetOwner()->ForceNetUpdate(); }
}
void UHALPursuitTargetComponent::OnRep_State()
{
	const bool Show=State.bArrested||(State.ReleaseStart>=0&&PresentationTime()<State.ReleaseStart+State.FX.Chains.ReleaseSeconds);
	SetComponentTickEnabled(Show&&FApp::CanEverRender()&&GetNetMode()!=NM_DedicatedServer);
	UpdateMarker();
}
void UHALPursuitTargetComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* F)
{ Super::TickComponent(Dt,Type,F); UpdateMarker(); }
void UHALPursuitTargetComponent::EndPlay(EEndPlayReason::Type R) { Clear(); Super::EndPlay(R); }
void UHALPursuitTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UHALPursuitTargetComponent,State); }
