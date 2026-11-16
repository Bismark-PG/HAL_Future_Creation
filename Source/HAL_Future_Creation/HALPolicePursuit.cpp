#include "HALPolicePursuit.h"
#include "HALPolicePursuitAbility.h"
#include "HALPursuitTargetComponent.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "HALGameFlowPermissions.h"
#include "TestVehiclePawn.h"
#include "BasicBallActor.h"
#include "VehicleHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
namespace { TAutoConsoleVariable<int32> LogPursuit(TEXT("hal.PursuitLog"),0,TEXT("Car06 pursuit start/arrest/cleanup."));
bool Range(float V,float A,float B) { return FMath::IsFinite(V)&&V>=A&&V<=B; } }
bool FHALPursuitRules::IsValid() const
{
	return Range(Duration,1,30)&&Range(Radius,100,3000)&&Range(ExitBuffer,0,100)&&Range(MaximumHeightDifference,50,1000)
		&&Range(Threshold,1,1000)&&Range(GainPerSecond,1,1000)&&Range(DecayPerSecond,1,1000)&&Range(StunSeconds,.1,10)
		&&Range(ProtectionSeconds,.1,10)&&Range(ScanInterval,.02,.2);
}
float FHALPursuitRules::Integrate(float V,float Dt,bool Inside,bool Stunned,bool Protected) const
{ return FMath::Clamp(V+FMath::Max(0.f,Dt)*(Protected||!Inside?-DecayPerSecond:Stunned?0:GainPerSecond),0.f,Threshold); }
bool FHALPursuitFX::IsValid() const
{
	for(auto C:{Red,Blue,StarColor,StarOutline}) if(!FMath::IsFinite(C.R)||!FMath::IsFinite(C.G)||!FMath::IsFinite(C.B)||!FMath::IsFinite(C.A)) { return false; }
	return RoofMount.IsValid()&&Range(RoofLift,0,100)&&Range(LightSpacing,5,100)&&Range(BeaconRadius,2,30)
		&&Range(FlashHalfPeriod,.1,2)&&Range(LightIntensity,0,10000)&&Range(LightRadius,50,500)&&Range(SignalGlow,0,10)
		&&Range(RangeOpacity,0,1)&&Range(RangeRingOpacity,0,1)&&Range(ScanSpeed,.05,4)&&Range(ScanWidth,.02,.5)
		&&Range(ScanOpacity,0,1)&&Range(RingWidth,.005,.1)&&Range(SirenInnerRadius,50,1000)&&Range(SirenFalloff,100,5000)
		&&Range(GroundLift,1,30)&&Range(ArrestSize,10,150)&&Range(ArrestHeight,10,200)&&Range(ArrestPulseSpeed,.1,5)
		&&StarCount>=1&&StarCount<=10&&Range(StarSize,8,40)&&Range(StarGap,0,15)&&Range(StarsAboveHealth,20,150)&&Range(SirenVolume,0,2);
}
AHALPolicePursuit::AHALPolicePursuit()
{
	bReplicates=true; bAlwaysRelevant=true; SetReplicateMovement(false); SetNetUpdateFrequency(20);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PursuitRoot")));
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
}
bool AHALPolicePursuit::VehicleReady(const ATestVehiclePawn* V)
{
	const auto* H=V?V->FindComponentByClass<UVehicleHealthComponent>():nullptr;
	return V&&!V->IsActorBeingDestroyed()&&V->IsConfigurationValid()&&V->GetController()&&V->GetController()->GetPawn()==V
		&&H&&H->IsHealthReady()&&H->GetCurrentHP()>0&&HALGameFlow::Permissions(V->GetWorld(),V).bAbilities;
}
bool AHALPolicePursuit::OwnerReady() const { return VehicleReady(Cast<ATestVehiclePawn>(GetOwner())); }
double AHALPolicePursuit::ServerTime() const
{ const auto* S=GetWorld()->GetGameState(); return S?S->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds(); }
bool AHALPolicePursuit::Start(const UHALPolicePursuitConfig& Config)
{
	FString Error;
	if(!HasAuthority()||State.Activation.IsValid()||!OwnerReady()||!Config.Validate(Error)) { return false; }
	State.Activation=FGuid::NewGuid(); State.Start=LastScan=ServerTime(); State.Rules=Config.Rules; State.FX=Config.FX;
	SetActorLocation(GetOwner()->GetActorLocation()); SetActorTickEnabled(true); ForceNetUpdate();
	GetWorldTimerManager().SetTimer(ScanTimer,this,&ThisClass::Scan,State.Rules.ScanInterval,true);
	UE_CLOG(LogPursuit.GetValueOnGameThread(),LogTemp,Log,TEXT("Pursuit start %s owner=%s"),*GetName(),*GetNameSafe(GetOwner())); return true;
}
bool AHALPolicePursuit::InRange(ATestVehiclePawn& Target,bool WasInside) const
{
	const auto* Source=GetOwner(); if(!Source) { return false; }
	const FVector A=Source->GetActorLocation(),B=Target.GetActorLocation(); const auto& R=State.Rules;
	if(FMath::Abs(A.Z-B.Z)>R.MaximumHeightDifference||(A-B).SizeSquared2D()>FMath::Square(R.Radius+(WasInside?R.ExitBuffer:0))) { return false; }
	FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(PursuitVisibility),false,Source); Query.AddIgnoredActor(&Target);
	TArray<FHitResult> Hits; GetWorld()->LineTraceMultiByObjectType(Hits,A,B,Objects,Query);
	for(const auto& Hit:Hits)
	{
		const auto* C=Hit.GetComponent(); if(!C||!C->IsPhysicsCollisionEnabled()||Cast<ATestVehiclePawn>(Hit.GetActor())||Cast<ABasicBallActor>(Hit.GetActor())) { continue; }
		if(C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block||C->GetCollisionResponseToChannel(ECC_PhysicsBody)==ECR_Block) { return false; }
	}
	return true;
}
void AHALPolicePursuit::Scan()
{
	if(!HasAuthority()) { return; } if(!OwnerReady()) { Destroy(); return; }
	const double Now=ServerTime(),End=State.Start+State.Rules.Duration;
	const float Dt=float(FMath::Max(0.,FMath::Min(Now,End)-LastScan)); LastScan=FMath::Min(Now,End);
	State.Records.RemoveAll([](const FHALPursuitRecord& R){return !IsValid(R.Target)||!R.Target->CanReceiveSkillEffects();});
	for(TActorIterator<ATestVehiclePawn> It(GetWorld());It;++It)
	{
		auto* V=*It; if(V==GetOwner()||!V->CanReceiveSkillEffects()) { continue; }
		auto* ASC=Cast<UHALAbilitySystemComponent>(V->GetAbilitySystemComponent()); auto* Receiver=V->FindComponentByClass<UHALPursuitTargetComponent>();
		if(!ASC||ASC->GetAvatarActor()!=V||!Receiver) { continue; }
		auto* Record=State.Records.FindByPredicate([&](const auto& R){return R.Target==V;});
		const bool Inside=InRange(*V,Record&&Record->bInside);
		if(!Record) { if(!Inside) { continue; } Record=&State.Records.AddDefaulted_GetRef(); Record->Target=V; }
		Record->bInside=Inside; if(Record->bArrested) { continue; }
		Record->Value=State.Rules.Integrate(Record->Value,Dt,Inside,ASC->IsStunned(),ASC->HasMatchingGameplayTag(HALGameplayTags::State_PursuitProtected));
		if(Record->Value>=State.Rules.Threshold)
		{
			PendingTarget=V; const bool Accepted=Receiver->AcceptArrest(*this); PendingTarget.Reset();
			if(Accepted) { Record->Value=0; Record->bArrested=true; }
			UE_CLOG(LogPursuit.GetValueOnGameThread(),LogTemp,Log,TEXT("Pursuit arrest target=%s accepted=%d"),*V->GetName(),Accepted);
		}
	}
	if(Now>=End) { Destroy(); }
}
bool AHALPolicePursuit::AuthorizesArrest(const AActor* Target) const
{ return HasAuthority()&&!IsActorBeingDestroyed()&&OwnerReady()&&PendingTarget.Get()==Target&&ServerTime()<=State.Start+State.Rules.Duration+State.Rules.ScanInterval; }
void AHALPolicePursuit::Tick(float Dt)
{
	Super::Tick(Dt); if(State.Activation.IsValid()&&GetOwner()) { SetActorLocation(GetOwner()->GetActorLocation()); UpdatePresentation(); }
#if !UE_BUILD_SHIPPING
	if(State.Rules.bDrawBounds&&GetOwner()) { DrawDebugCircle(GetWorld(),GetActorLocation(),State.Rules.Radius,48,FColor::Red,false,0,0,2,FVector::ForwardVector,FVector::RightVector,false); }
#endif
}
void AHALPolicePursuit::OnRep_State() { SetActorTickEnabled(State.Activation.IsValid()); }
void AHALPolicePursuit::EndPlay(EEndPlayReason::Type Reason)
{ GetWorldTimerManager().ClearTimer(ScanTimer); PendingTarget.Reset(); State.Records.Reset(); StopPresentation(); Super::EndPlay(Reason); }
void AHALPolicePursuit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHALPolicePursuit,State); }
