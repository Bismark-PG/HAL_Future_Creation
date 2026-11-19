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
	DOREPLIFETIME_CONDITION(UHALSideShiftComponent,bConfigured,COND_OwnerOnly);
	DOREPLIFETIME(UHALSideShiftComponent,Visual);
}
double UHALSideShiftComponent::ServerTime() const
{
	const auto* S=GetWorld()?GetWorld()->GetGameState():nullptr;
	return S?S->GetServerWorldTimeSeconds():GetWorld()?GetWorld()->GetTimeSeconds():0;
}
float UHALSideShiftComponent::GetRechargeRemaining() const
{
	const auto View = GetChargeView();
	return View.NextRechargeServerTime < 0 ? 0 : float(FMath::Max(0., View.NextRechargeServerTime - ServerTime()));
}
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
FHALSideShiftView UHALSideShiftComponent::GetChargeView() const
{
	FHALSideShiftView View;
	const auto* Pawn = Cast<ATestVehiclePawn>(GetOwner());
	const auto* ASC = Pawn ? Cast<UHALAbilitySystemComponent>(Pawn->GetAbilitySystemComponent()) : nullptr;
	if (!bConfigured || !ASC || ASC->GetAvatarActor() != Pawn) { return View; }
	const auto Energy = ASC->GetUltimateEnergyView();
	if (Energy.Cost != HALUltimate::SideShiftCost) { return View; }
	View.bConfigured = true;
	View.MaximumCharges = HALUltimate::SideShiftCells;
	View.Charges = FMath::FloorToInt(Energy.Energy / Energy.Cost);
	View.RechargeSeconds = Energy.FullRechargeSeconds / View.MaximumCharges;
	if (Energy.bRecharging)
	{
		const float Missing = (View.Charges + 1) * Energy.Cost - Energy.Energy;
		View.NextRechargeServerTime = Energy.SampleServerTime + Missing * Energy.FullRechargeSeconds / HALUltimate::Capacity;
	}
	return View;
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
	bConfigured = true;
	GetOwner()->ForceNetUpdate();
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
	return PendingSide != 0 && CanStartWithDirection();
}
bool UHALSideShiftComponent::CanStartWithDirection() const
{
	return Config && BoundASC.IsValid() && GetOwner()->HasAuthority() && UsesPrimary() && BoundASC->CanPayUltimate(HALUltimate::SideShiftCost)
		&&ServerTime()+.0001>=ReadyAt&&HasUsablePose()&&!UHALBattleTopComponent::IsActiveOn(GetOwner())&&!UHALBullChargeComponent::IsActiveOn(GetOwner());
}
bool UHALSideShiftComponent::Request(float Steering,bool bHandbrake)
{
	if(!FMath::IsFinite(Steering)||!UsesPrimary()) { return false; }
	// Read the actual granted GA config on either endpoint, never a client-chosen class/config.
	const auto* P=Cast<ATestVehiclePawn>(GetOwner()); auto* ASC=P?Cast<UHALAbilitySystemComponent>(P->GetAbilitySystemComponent()):nullptr;
	if(!P||!P->IsLocallyControlled()||!ASC) { return false; }
	const UHALSideShiftConfig* Settings=nullptr;
	for(const auto& S:ASC->GetActivatableAbilities()) if(S.Ability&&S.Ability->IsA<UHALSideShiftAbility>()&&S.SourceObject==P)
	{ Settings=S.Ability->GetClass()->GetDefaultObject<UHALSideShiftAbility>()->SkillConfig; }
	if(!Settings||FMath::Abs(Steering)<Settings->DirectionDeadZone) { return false; }
	bHandbrake=bHandbrake&&FMath::Abs(Steering)>=Settings->Motion.DriftInputThreshold;
	const int8 Side=Steering<0?-1:1; ++NextRequest; if(!NextRequest) { ++NextRequest; }
	bAwaitingDriftRelease=bHandbrake;
	if(GetOwner()->HasAuthority()) { ProcessRequest(Side,NextRequest,bHandbrake); }
	else { ServerRequest(Side,NextRequest,bHandbrake); }
	return true;
}
void UHALSideShiftComponent::ServerRequest_Implementation(int8 Side,uint32 Sequence,bool bHandbrake) { ProcessRequest(Side,Sequence,bHandbrake); }
void UHALSideShiftComponent::ProcessRequest(int8 Side,uint32 Sequence,bool bHandbrake)
{
	if(!GetOwner()->HasAuthority()||(Side!=-1&&Side!=1)||!Sequence||!FVehicleNetInputData::IsNewerSequence(Sequence,LastRequest)) { return; }
	LastRequest=Sequence;
	TGuardValue<int8> Direction(PendingSide,Side);
	TGuardValue<bool> Drift(bPendingDrift,bHandbrake);
	if(auto* ASC=BoundASC.Get();ASC&&UsesPrimary()) { ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary); }
}
void UHALSideShiftComponent::ObserveHandbrake(bool bHeld)
{
	if(bHeld||!bAwaitingDriftRelease) { return; }
	bAwaitingDriftRelease=false;
	if(GetOwner()->HasAuthority()) { ProcessDriftRelease(NextRequest); }
	else { ServerReleaseDrift(NextRequest); }
}
void UHALSideShiftComponent::ServerReleaseDrift_Implementation(uint32 Sequence) { ProcessDriftRelease(Sequence); }
void UHALSideShiftComponent::ProcessDriftRelease(uint32 Sequence)
{
	if(!GetOwner()->HasAuthority()||!Sequence||Sequence!=LastRequest||!Visual.Activation.IsValid()) { return; }
	if(auto* Bridge=GetOwner()->FindComponentByClass<UHALMovementEffectBridge>()) { Bridge->ReleaseSideShiftDrift(Visual.Activation); }
}
bool UHALSideShiftComponent::Start()
{
	if(!CanStart()) { return false; }
	auto* ASC=BoundASC.Get(); auto* Bridge=GetOwner()->FindComponentByClass<UHALMovementEffectBridge>();
	auto* M=GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>();
	if(!Bridge||!M) { return false; }
	FHALSideShiftMotion Motion=Config->Motion; Motion.Activation=FGuid::NewGuid(); Motion.bActive=true;
	Motion.bDriftAtStart=bPendingDrift;
	const auto Pose=M->GetContactObservation();
	if(!Pose.bValid) { return false; }
	Motion.InitialForward=Pose.Rotation.GetForwardVector().GetSafeNormal2D();
	Motion.Direction=FVector::CrossProduct(FVector::UpVector,Motion.InitialForward)*PendingSide;
	Motion.Origin=Pose.Position; Motion.InitialVelocity=Pose.Velocity;
	if(const auto* Body=Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		const FVector LocalCOM=Body->GetComponentQuat().UnrotateVector(Body->GetCenterOfMass()-Body->GetComponentLocation());
		Motion.InitialVelocity-=FVector::CrossProduct(Pose.AngularVelocity,Pose.Rotation.RotateVector(LocalCOM));
	}
	Motion.InitialLateralSpeed=FVector::DotProduct(Pose.Velocity,Motion.Direction);
	if(!Motion.IsValid(false)) { return false; }
	FGameplayEffectContextHandle Context=ASC->MakeEffectContext(); Context.AddSourceObject(GetOwner());
	FGameplayEffectSpec Spec(GetDefault<UHALSideShiftStateEffect>(),Context,1);
	Spec.SetDuration(FMath::Max(Motion.DurationSeconds,Motion.DriftSeconds)+.05f,true);
	const auto Effect=ASC->ApplyGameplayEffectSpecToSelf(Spec); if(!Effect.IsValid()) { return false; }
	// Replace the old tail only after the new transaction is valid. Stop never refills charges.
	Stop(); ActiveEffect=Effect;
	Visual.Activation=Motion.Activation; Visual.FX=Config->FX; Visual.StartServerTime=ServerTime();
	Visual.ShiftSeconds=Motion.DurationSeconds; Visual.DriftSeconds=Motion.DriftSeconds;
	Visual.Direction=Motion.Direction; Visual.Side=PendingSide; Visual.bDriving=true;
	ReadyAt=Visual.StartServerTime+Motion.DurationSeconds;
	Bridge->SetSideShiftMotion(Motion);
	WorldHandle=FWorldDelegates::OnWorldPostActorTick.AddUObject(this,&ThisClass::WatchActive);
	GetWorld()->GetTimerManager().SetTimer(EndTimer,this,&ThisClass::Stop,FMath::Max(Motion.DurationSeconds,Motion.DriftSeconds)+.02f,false);
	OnRep_Visual(); GetOwner()->ForceNetUpdate();
	UE_CLOG(HALAbilities::ShouldLog(),LogHALAbility,Log,TEXT("SideShift started Pawn=%s Side=%d Distance=%.2f ArcSpan=%.2f Bend=%.2f Duration=%.3f Drift=%.3f Origin=%s Direction=%s"),
		*GetNameSafe(GetOwner()),int32(PendingSide),Motion.Distance,Motion.DriftLateralSpan,Motion.DriftArcBend,Motion.DurationSeconds,Motion.DriftSeconds,*Pose.Position.ToString(),*Motion.Direction.ToString());
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
		Stop();
		if(Visual.Activation.IsValid())
		{ if(auto* Bridge=GetOwner()->FindComponentByClass<UHALMovementEffectBridge>()) { Bridge->SetSideShiftMotion({}); } }
		if(auto* ASC=BoundASC.Get())
		{
			ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Stun,EGameplayTagEventType::NewOrRemoved).Remove(StunHandle);
			ASC->RegisterGameplayTagEvent(HALGameplayTags::State_SideShift,EGameplayTagEventType::NewOrRemoved).Remove(ActiveTagHandle);
			ASC->GetGameplayAttributeValueChangeDelegate(UHALHealthAttributeSet::GetCurrentHPAttribute()).Remove(HealthHandle);
		}
		BoundASC.Reset(); Config=nullptr; bConfigured=false; ReadyAt=-1;
		GetOwner()->ForceNetUpdate();
	}
}
