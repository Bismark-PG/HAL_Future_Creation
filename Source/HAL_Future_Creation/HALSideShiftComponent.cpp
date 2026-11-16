#include "HALSideShiftComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALAbilityTypes.h"
#include "HALGameplayTags.h"
#include "HALHealthAttributeSet.h"
#include "HALGameFlowPermissions.h"
#include "HALMovementEffectBridge.h"
#include "HALBattleTopComponent.h"
#include "HALBullChargeComponent.h"
#include "TestVehiclePawn.h"
#include "VehicleHealthComponent.h"
#include "VehicleRecoveryComponent.h"
#include "ArcadeVehicleMovementComponent.h"
#include "HALVehiclePresentationComponent.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

UHALSideShiftComponent::UHALSideShiftComponent()
{
	PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false;
	PrimaryComponentTick.TickGroup=TG_PostPhysics; SetIsReplicatedByDefault(true);
}
void UHALSideShiftComponent::BeginPlay()
{
	Super::BeginPlay();
	if(auto* P=GetOwner()->FindComponentByClass<UHALVehiclePresentationComponent>()) { AddTickPrerequisiteComponent(P); }
}
void UHALSideShiftComponent::EndPlay(EEndPlayReason::Type Reason)
{ Shutdown(); ClearParticles(true); Super::EndPlay(Reason); }
void UHALSideShiftComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UHALSideShiftComponent,ChargeView,COND_OwnerOnly);
	DOREPLIFETIME(UHALSideShiftComponent,Visual);
}
double UHALSideShiftComponent::ServerTime() const
{
	const auto* S=GetWorld()?GetWorld()->GetGameState():nullptr;
	return S?S->GetServerWorldTimeSeconds():GetWorld()?GetWorld()->GetTimeSeconds():0;
}
float UHALSideShiftComponent::GetRechargeRemaining() const
{ return ChargeView.NextRechargeServerTime<0?0:float(FMath::Max(0.,ChargeView.NextRechargeServerTime-ServerTime())); }
bool UHALSideShiftComponent::UsesPrimary() const
{
	const auto* P=Cast<ATestVehiclePawn>(GetOwner());
	const auto* ASC=P?Cast<UHALAbilitySystemComponent>(P->GetAbilitySystemComponent()):nullptr;
	if(!ASC) { return false; }
	int32 Count=0; bool Match=false;
	for(const auto& S:ASC->GetActivatableAbilities()) if(S.Ability&&!S.PendingRemove&&S.SourceObject==P&&S.GetDynamicSpecSourceTags().HasTagExact(HALGameplayTags::Input_Ability_Primary))
	{ ++Count; Match=S.Ability->IsA<UHALSideShiftAbility>(); }
	return Count==1&&Match;
}
int32 UHALSideShiftComponent::ResourceCount() const
{ const auto* ASC=BoundASC.Get(); return ASC&&ResourceEffect.IsValid()?ASC->GetCurrentStackCount(ResourceEffect):0; }
bool UHALSideShiftComponent::AddCharge(int32 Count)
{
	auto* ASC=BoundASC.Get(); if(!ASC||!Config||Count<=0) { return false; }
	const int32 Add=FMath::Min(Count,Config->MaximumCharges-ResourceCount()); if(Add<=0) { return true; }
	FGameplayEffectContextHandle Context=ASC->MakeEffectContext(); Context.AddSourceObject(GetOwner());
	FGameplayEffectSpec Spec(GetDefault<UHALSideShiftChargeEffect>(),Context,1); Spec.SetStackCount(Add);
	ResourceEffect=ASC->ApplyGameplayEffectSpecToSelf(Spec); return ResourceEffect.IsValid();
}
void UHALSideShiftComponent::Initialize(UHALAbilitySystemComponent& ASC,UHALSideShiftConfig& Settings)
{
	if(!GetOwner()->HasAuthority()||ASC.GetAvatarActor()!=GetOwner()) { return; }
	if(BoundASC.Get()==&ASC&&Config==&Settings) { return; } // Rebinding cannot refill charges.
	FString Error; if(!Settings.Validate(Error)) { UE_LOG(LogHALAbility,Error,TEXT("SideShift config rejected: %s"),*Error); return; }
	Shutdown(); BoundASC=&ASC; Config=&Settings;
	StunHandle=ASC.RegisterGameplayTagEvent(HALGameplayTags::State_Stun,EGameplayTagEventType::NewOrRemoved).AddUObject(this,&ThisClass::TagChanged);
	ActiveTagHandle=ASC.RegisterGameplayTagEvent(HALGameplayTags::State_SideShift,EGameplayTagEventType::NewOrRemoved).AddUObject(this,&ThisClass::TagChanged);
	HealthHandle=ASC.GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).AddUObject(this,&ThisClass::HealthChanged);
	if(!AddCharge(Config->MaximumCharges)) { Shutdown(); return; }
	PublishCharges();
}
void UHALSideShiftComponent::PublishCharges()
{
	ChargeView.bConfigured=Config!=nullptr; ChargeView.Charges=ResourceCount();
	ChargeView.MaximumCharges=Config?Config->MaximumCharges:0; ChargeView.RechargeSeconds=Config?Config->RechargeSeconds:0;
	if(!Config||ChargeView.Charges>=ChargeView.MaximumCharges)
	{ GetWorld()->GetTimerManager().ClearTimer(RechargeTimer); ChargeView.NextRechargeServerTime=-1; }
	GetOwner()->ForceNetUpdate();
}
void UHALSideShiftComponent::Recharge()
{
	if(!Config||!BoundASC.IsValid()||BoundASC->GetAvatarActor()!=GetOwner()) { Shutdown(); return; }
	AddCharge(1); PublishCharges();
	if(ResourceCount()<Config->MaximumCharges)
	{
		ChargeView.NextRechargeServerTime=ServerTime()+Config->RechargeSeconds;
		GetWorld()->GetTimerManager().SetTimer(RechargeTimer,this,&ThisClass::Recharge,Config->RechargeSeconds,false);
	}
}
bool UHALSideShiftComponent::HasUsablePose() const
{
	const auto* P=Cast<ATestVehiclePawn>(GetOwner()); const auto* ASC=BoundASC.Get();
	const auto* M=GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	const auto* H=GetOwner()->FindComponentByClass<UVehicleHealthComponent>();
	const auto* R=GetOwner()->FindComponentByClass<UVehicleRecoveryComponent>();
	if(!P||!P->GetController()||P->GetController()->GetPawn()!=P||!P->IsConfigurationValid()||!ASC||ASC->GetAvatarActor()!=P
		||!H||!H->IsHealthReady()||H->GetCurrentHP()<=0||ASC->IsStunned()||!M||M->IsBounceBackActive()
		||(R&&R->IsExecuting())||!HALGameFlow::Permissions(GetWorld(),P).bAbilities) { return false; }
	const auto Pose=M->GetStabilityObservation();
	return Pose.bValid&&Pose.bGrounded&&Pose.bDrivingAllowed&&!Pose.Recovery.bActive&&FVector::DotProduct(Pose.Up,Pose.GroundNormal)>0;
}
bool UHALSideShiftComponent::CanStart() const
{
	return Config&&BoundASC.IsValid()&&GetOwner()->HasAuthority()&&UsesPrimary()&&PendingSide!=0&&ResourceCount()>0
		&&ServerTime()+.0001>=ReadyAt&&HasUsablePose()&&!UHALBattleTopComponent::IsActiveOn(GetOwner())&&!UHALBullChargeComponent::IsActiveOn(GetOwner());
}
bool UHALSideShiftComponent::Request(float Steering)
{
	if(!FMath::IsFinite(Steering)||!UsesPrimary()) { return false; }
	// Read the actual granted GA config on either endpoint, never a client-chosen class/config.
	const auto* P=Cast<ATestVehiclePawn>(GetOwner()); auto* ASC=P?Cast<UHALAbilitySystemComponent>(P->GetAbilitySystemComponent()):nullptr;
	if(!P||!P->IsLocallyControlled()||!ASC) { return false; }
	const UHALSideShiftConfig* Settings=nullptr;
	for(const auto& S:ASC->GetActivatableAbilities()) if(S.Ability&&S.Ability->IsA<UHALSideShiftAbility>()&&S.SourceObject==P)
	{ Settings=S.Ability->GetClass()->GetDefaultObject<UHALSideShiftAbility>()->SkillConfig; }
	if(!Settings||FMath::Abs(Steering)<Settings->DirectionDeadZone) { return false; }
	const int8 Side=Steering<0?-1:1; ++NextRequest; if(!NextRequest) { ++NextRequest; }
	if(GetOwner()->HasAuthority()) { ProcessRequest(Side,NextRequest); }
	else { ServerRequest(Side,NextRequest); }
	return true;
}
void UHALSideShiftComponent::ServerRequest_Implementation(int8 Side,uint32 Sequence) { ProcessRequest(Side,Sequence); }
void UHALSideShiftComponent::ProcessRequest(int8 Side,uint32 Sequence)
{
	if(!GetOwner()->HasAuthority()||(Side!=-1&&Side!=1)||!Sequence||!FVehicleNetInputData::IsNewerSequence(Sequence,LastRequest)) { return; }
	LastRequest=Sequence;
	TGuardValue<int8> Direction(PendingSide,Side);
	if(auto* ASC=BoundASC.Get();ASC&&UsesPrimary()) { ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary); }
}
bool UHALSideShiftComponent::Start()
{
	if(!CanStart()) { return false; }
	auto* ASC=BoundASC.Get(); auto* Bridge=GetOwner()->FindComponentByClass<UHALMovementEffectBridge>();
	auto* M=GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	if(!Bridge||!M) { return false; }
	FHALSideShiftMotion Motion=Config->Motion; Motion.Activation=FGuid::NewGuid(); Motion.bActive=true;
	const auto Pose=M->GetContactObservation();
	if(!Pose.bValid) { return false; }
	Motion.Direction=Pose.Rotation.GetRightVector().GetSafeNormal2D()*PendingSide;
	Motion.InitialLateralSpeed=FVector::DotProduct(Pose.Velocity,Motion.Direction);
	if(!Motion.IsValid(false)) { return false; }
	FGameplayEffectContextHandle Context=ASC->MakeEffectContext(); Context.AddSourceObject(GetOwner());
	FGameplayEffectSpec Spec(GetDefault<UHALSideShiftStateEffect>(),Context,1);
	Spec.SetDuration(FMath::Max(Motion.DurationSeconds,Motion.DriftSeconds)+.05f,true);
	const auto Effect=ASC->ApplyGameplayEffectSpecToSelf(Spec); if(!Effect.IsValid()) { return false; }
	// Replace the old tail only after the new transaction is valid. Stop never refills charges.
	Stop(); ActiveEffect=Effect;
	ASC->RemoveActiveGameplayEffect(ResourceEffect,1);
	Visual.Activation=Motion.Activation; Visual.FX=Config->FX; Visual.StartServerTime=ServerTime();
	Visual.ShiftSeconds=Motion.DurationSeconds; Visual.DriftSeconds=Motion.DriftSeconds;
	Visual.Direction=Motion.Direction; Visual.Side=PendingSide; Visual.bDriving=true;
	ReadyAt=Visual.StartServerTime+Motion.DurationSeconds;
	Bridge->SetSideShiftMotion(Motion);
	if(!GetWorld()->GetTimerManager().IsTimerActive(RechargeTimer))
	{
		ChargeView.NextRechargeServerTime=ServerTime()+Config->RechargeSeconds;
		GetWorld()->GetTimerManager().SetTimer(RechargeTimer,this,&ThisClass::Recharge,Config->RechargeSeconds,false);
	}
	PublishCharges();
	WorldHandle=FWorldDelegates::OnWorldPostActorTick.AddUObject(this,&ThisClass::WatchActive);
	GetWorld()->GetTimerManager().SetTimer(EndTimer,this,&ThisClass::Stop,FMath::Max(Motion.DurationSeconds,Motion.DriftSeconds)+.02f,false);
	OnRep_Visual(); GetOwner()->ForceNetUpdate();
	UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("SideShift started Pawn=%s Side=%d Charges=%d Distance=%.2f Duration=%.3f Drift=%.3f RechargeAt=%.3f Origin=%s Direction=%s"),
		*GetNameSafe(GetOwner()),int32(PendingSide),ResourceCount(),Motion.Distance,Motion.DurationSeconds,Motion.DriftSeconds,ChargeView.NextRechargeServerTime,*Pose.Position.ToString(),*Motion.Direction.ToString());
	return true;
}
void UHALSideShiftComponent::WatchActive(UWorld* World,ELevelTick,float)
{
	if(World==GetWorld()&&Visual.bDriving&&!HasUsablePose())
	{
		const auto* M=GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>(); const auto Pose=M?M->GetStabilityObservation():FVehicleStabilityObservation{};
		UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("SideShift interrupted Pawn=%s Age=%.3f Grounded=%d Drive=%d Recovery=%d"),
			*GetNameSafe(GetOwner()),ServerTime()-Visual.StartServerTime,Pose.bGrounded,Pose.bDrivingAllowed,Pose.Recovery.bActive);
		Stop();
	}
}
void UHALSideShiftComponent::TagChanged(FGameplayTag Tag,int32 Count)
{ if((Tag==HALGameplayTags::State_Stun&&Count>0)||(Tag==HALGameplayTags::State_SideShift&&Count==0)) { Stop(); } }
void UHALSideShiftComponent::HealthChanged(const FOnAttributeChangeData& Change) { if(Change.NewValue<=0) { Shutdown(); } }
void UHALSideShiftComponent::Stop()
{
	if(!GetOwner()||!GetOwner()->HasAuthority()||bStopping) { return; }
	TGuardValue<bool> Guard(bStopping,true);
	FWorldDelegates::OnWorldPostActorTick.Remove(WorldHandle); WorldHandle.Reset();
	if(auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(EndTimer); }
	if(Visual.bDriving)
	{
		const auto* M=GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
		UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("SideShift stopped Pawn=%s Age=%.3f Position=%s"),
			*GetNameSafe(GetOwner()),ServerTime()-Visual.StartServerTime,*(M?M->GetContactObservation().Position:GetOwner()->GetActorLocation()).ToString());
		Visual.bDriving=false;
		if(auto* Bridge=GetOwner()->FindComponentByClass<UHALMovementEffectBridge>()) { Bridge->EndSideShiftMotion(); }
		OnRep_Visual(); GetOwner()->ForceNetUpdate();
	}
	if(auto* ASC=BoundASC.Get();ASC&&ActiveEffect.IsValid()) { ASC->RemoveActiveGameplayEffect(ActiveEffect); }
	ActiveEffect.Invalidate();
}
void UHALSideShiftComponent::Shutdown()
{
	if(GetOwner()&&GetOwner()->HasAuthority())
	{
		Stop(); if(auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(RechargeTimer); }
		if(auto* ASC=BoundASC.Get())
		{
			ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Stun,EGameplayTagEventType::NewOrRemoved).Remove(StunHandle);
			ASC->RegisterGameplayTagEvent(HALGameplayTags::State_SideShift,EGameplayTagEventType::NewOrRemoved).Remove(ActiveTagHandle);
			ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).Remove(HealthHandle);
			if(ResourceEffect.IsValid()) { ASC->RemoveActiveGameplayEffect(ResourceEffect); }
		}
		ResourceEffect.Invalidate(); BoundASC.Reset(); Config=nullptr; ChargeView={}; ReadyAt=-1;
		GetOwner()->ForceNetUpdate();
	}
}
