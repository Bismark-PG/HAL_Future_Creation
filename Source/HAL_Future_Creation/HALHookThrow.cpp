#include "HALHookThrow.h"
#include "HALHookThrowAbility.h"
#include "HALHookComponent.h"
#include "HALHookMotion.h"
#include "HALMovementEffectBridge.h"
#include "HALAbilitySystemComponent.h"
#include "HALGameplayTags.h"
#include "HALGameFlowPermissions.h"
#include "ArcadeVehicleMovementComponent.h"
#include "TestVehiclePawn.h"
#include "BasicBallActor.h"
#include "VehicleRecoveryComponent.h"
#include "VehicleHealthComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/GameStateBase.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> HookLog(TEXT("hal.HookLog"),0,TEXT("Hook authority casts, captures, direction requests, release and interruption."));
	UHALAbilitySystemComponent* HookASC(const AActor* Actor)
	{ const auto* V=Cast<ATestVehiclePawn>(Actor); return V?Cast<UHALAbilitySystemComponent>(V->GetAbilitySystemComponent()):nullptr; }
	UPrimitiveComponent* HookBody(const AActor* Actor) { return Actor?Cast<UPrimitiveComponent>(Actor->GetRootComponent()):nullptr; }
}
AHALHookThrow::AHALHookThrow()
{
	bReplicates=true; bAlwaysRelevant=true; SetReplicateMovement(false); SetNetUpdateFrequency(30); NetPriority=3;
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false; PrimaryActorTick.TickGroup=TG_PostPhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("HookRoot")));
	Chain=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Chain")); Chain->SetupAttachment(GetRootComponent());
	Hook=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookHead")); Hook->SetupAttachment(GetRootComponent());
	for(UPrimitiveComponent* C:{static_cast<UPrimitiveComponent*>(Chain),static_cast<UPrimitiveComponent*>(Hook)})
	{ C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetGenerateOverlapEvents(false); C->SetCastShadow(false); C->SetCanEverAffectNavigation(false); }
}
void AHALHookThrow::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHALHookThrow,State); DOREPLIFETIME(AHALHookThrow,Rules); DOREPLIFETIME(AHALHookThrow,FX); }
bool AHALHookThrow::CanStart(const ATestVehiclePawn& V)
{
	const auto* ASC=HookASC(&V); const auto* H=V.FindComponentByClass<UVehicleHealthComponent>();
	const auto* M=V.FindComponentByClass<UArcadeVehicleMovementComponent>(); const auto* C=V.FindComponentByClass<UHALHookComponent>();
	const auto* R=V.FindComponentByClass<UVehicleRecoveryComponent>();
	if(!ASC||ASC->GetAvatarActor()!=&V||!ASC->IsHealthInitialized()||!ASC->IsCombatInitialized()||!ASC->IsImpactInitialized()
		||ASC->IsStunned()||!V.IsConfigurationValid()||!V.GetController()||V.GetController()->GetPawn()!=&V
		||!HALGameFlow::Permissions(V.GetWorld(),&V).bAbilities||!H||!H->IsHealthReady()||H->GetCurrentHP()<=0
		||!M||M->IsBounceBackActive()||!C||IsValid(C->GetOwnedHook())||(R&&R->IsExecuting())) { return false; }
	const auto P=M->GetStabilityObservation();
	return P.bValid&&P.bGrounded&&P.bDrivingAllowed&&!P.Recovery.bActive&&FVector::DotProduct(P.Up,P.GroundNormal)>.5;
}
bool AHALHookThrow::Start(const UHALHookThrowConfig& C)
{
	auto* V=Cast<ATestVehiclePawn>(GetOwner()); FString Error;
	if(!HasAuthority()||!V||!CanStart(*V)||!C.Validate(Error)) { return false; }
	Rules=C.Rules; FX=C.FX; CasterASC=HookASC(V);
	State.Activation=FGuid::NewGuid(); State.StartTime=State.PhaseTime=ServerTime(); State.Phase=EHALHookPhase::Outbound;
	State.Forward=V->GetActorForwardVector().GetSafeNormal2D();
	if(State.Forward.IsNearlyZero()) { return false; }
	State.Point=Outlet(); PreviousTip=State.Point;
	auto Context=CasterASC->MakeEffectContext(); Context.AddSourceObject(this);
	FGameplayEffectSpec Spec(GetDefault<UHALHookCastingEffect>(),Context,1); Spec.SetDuration(Rules.MaximumSeconds(),true);
	CastingEffect=CasterASC->ApplyGameplayEffectSpecToSelf(Spec);
	if(!CastingEffect.IsValid()) { return false; }
	CasterASC->ForceReplication();
	V->FindComponentByClass<UHALHookComponent>()->SetOwnedHook(this);
	FHALHookMotion Motion; Motion.Activation=State.Activation; Motion.DurationSeconds=Rules.MaximumSeconds();
	Motion.Damping=Rules.Damping; Motion.MaximumAcceleration=Rules.MaximumAcceleration;
	V->FindComponentByClass<UHALMovementEffectBridge>()->SetHookMotion(Motion);
	StunHandle=CasterASC->RegisterGameplayTagEvent(HALGameplayTags::State_Stun,EGameplayTagEventType::NewOrRemoved).AddUObject(this,&ThisClass::CasterStun);
	SetActorTickEnabled(true); PublishState();
	UE_CLOG(HookLog.GetValueOnGameThread(),LogTemp,Log,TEXT("Hook cast Owner=%s Activation=%s Replicated=%d RemoteRole=%d AlwaysRelevant=%d Driver=%s Initialized=%d"),*GetNameSafe(V),*State.Activation.ToString(),GetIsReplicated(),int32(GetRemoteRole()),bAlwaysRelevant,*GetNetDriverName().ToString(),IsActorInitialized());
	return true;
}
double AHALHookThrow::ServerTime() const
{ const auto* G=GetWorld()?GetWorld()->GetGameState():nullptr; return G?G->GetServerWorldTimeSeconds():GetWorld()?GetWorld()->GetTimeSeconds():0; }
FVector AHALHookThrow::Outlet() const
{ const auto* B=HookBody(GetOwner()); return B?B->GetComponentTransform().TransformPosition(Rules.OutletOffset):State.Point; }
FVector AHALHookThrow::TipAt(double Now) const
{
	const double Age=FMath::Max(0.,Now-State.PhaseTime);
	if(State.Phase==EHALHookPhase::Outbound) { return State.Point+State.Forward*FMath::Min(double(Rules.Range),Age*Rules.OutboundSpeed); }
	if((State.Phase==EHALHookPhase::Choosing||State.Phase==EHALHookPhase::Swinging)&&IsValid(State.Target))
	{ return State.Target->GetActorTransform().TransformPosition(State.TargetOffset); }
	if(State.Phase==EHALHookPhase::Returning||State.Phase==EHALHookPhase::Interrupted)
	{ const FVector S=Outlet(); return S+(State.Point-S).GetClampedToMaxSize(FMath::Max(0.,FVector::Distance(State.Point,S)-Age*Rules.ReturnSpeed)); }
	return State.Point;
}
bool AHALHookThrow::LocksCaster() const
{ return State.Phase==EHALHookPhase::Outbound||State.Phase==EHALHookPhase::Choosing||State.Phase==EHALHookPhase::Swinging||State.Phase==EHALHookPhase::Returning; }
bool AHALHookThrow::CasterValid() const
{
	const auto* V=Cast<ATestVehiclePawn>(GetOwner()); const auto* ASC=CasterASC.Get();
	const auto* H=V?V->FindComponentByClass<UVehicleHealthComponent>():nullptr;
	return V&&V->IsConfigurationValid()&&V->GetController()&&V->GetController()->GetPawn()==V&&ASC&&ASC->GetAvatarActor()==V
		&&H&&H->IsHealthReady()&&H->GetCurrentHP()>0&&HALGameFlow::Permissions(GetWorld(),V).bAbilities
		&&(!LocksCaster()||ASC->GetActiveGameplayEffect(CastingEffect));
}
bool AHALHookThrow::IsInterrupted() const
{
	const auto* ASC=CasterASC.Get(); const auto* M=GetOwner()?GetOwner()->FindComponentByClass<UArcadeVehicleMovementComponent>():nullptr;
	const auto* B=HookBody(GetOwner()); const auto* R=GetOwner()?GetOwner()->FindComponentByClass<UVehicleRecoveryComponent>():nullptr;
	if(!ASC||ASC->IsStunned()||!M||!B||M->IsBounceBackActive()||(R&&R->IsExecuting())) { return true; }
	const auto P=M->GetStabilityObservation();
	return (P.bValid&&FVector::DotProduct(P.Up,FVector::UpVector)<.2)
		||(!P.bGrounded&&B->GetPhysicsLinearVelocity().Z>Rules.AirborneUpwardSpeed);
}
void AHALHookThrow::CasterStun(FGameplayTag,int32 Count) { if(Count>0) { Interrupt(false); } }
void AHALHookThrow::Sweep(const FVector& From,const FVector& To)
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(HookOutbound),false,GetOwner());
	for(TActorIterator<ABasicBallActor> It(GetWorld());It;++It) { Q.AddIgnoredActor(*It); }
	for(TActorIterator<ATestVehiclePawn> It(GetWorld());It;++It) { if(!It->CanReceiveSkillEffects()) { Q.AddIgnoredActor(*It); } }
	for(int32 Retry=0;Retry<16;++Retry)
	{
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits,From,To,FQuat::Identity,FCollisionObjectQueryParams::AllObjects,FCollisionShape::MakeSphere(Rules.HitRadius),Q);
		bool Retried=false;
		for(const auto& H:Hits)
		{
			auto* B=H.GetComponent(); if(!B) { continue; }
			if(auto* V=Cast<ATestVehiclePawn>(H.GetActor()))
			{
				if(B!=V->GetRootComponent()) { Q.AddIgnoredComponent(B); Retried=true; break; }
				if(!Capture(*V,H.Location)) { State.Point=H.Location; Return(false); }
				return;
			}
			if(B->IsPhysicsCollisionEnabled()&&(B->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block||B->GetCollisionResponseToChannel(ECC_PhysicsBody)==ECR_Block))
			{ State.Point=H.Location; State.PhaseTime=ServerTime(); State.Phase=EHALHookPhase::Returning; PublishState(true); return; }
			Q.AddIgnoredComponent(B); Retried=true; break;
		}
		if(!Retried) { return; }
	}
	// Bound malformed overlapping scene queries; never continue through an unresolved blocker.
	State.Point=From; State.Phase=EHALHookPhase::Returning; State.PhaseTime=ServerTime(); PublishState(true);
}
bool AHALHookThrow::Capture(ATestVehiclePawn& V,const FVector& Point)
{
	auto* C=V.FindComponentByClass<UHALHookComponent>(); auto* ASC=HookASC(&V); auto* B=HookBody(&V);
	if(!V.CanReceiveSkillEffects()||!C||!ASC||!B||!B->IsSimulatingPhysics()||&V==GetOwner()||!C->Claim(*this)) { return false; }
	TargetASC=ASC; State.Target=&V; TargetAnchor=B->GetComponentLocation(); CapturePivot=GetOwner()->GetActorLocation();
	auto Context=ASC->MakeEffectContext(); Context.AddInstigator(GetOwner(),this); Context.AddSourceObject(this);
	FGameplayEffectSpec Spec(GetDefault<UHALHookCapturedEffect>(),Context,1); Spec.SetDuration(Rules.ChoiceSeconds+Rules.SwingSeconds+.75f,true);
	CaptureEffect=ASC->ApplyGameplayEffectSpecToSelf(Spec); // Ordinary State.Stun callback immediately drops the ball.
	if(!CaptureEffect.IsValid()) { C->ReleaseClaim(*this); State.Target=nullptr; TargetASC.Reset(); return false; }
	State.Phase=EHALHookPhase::Choosing; State.PhaseTime=ServerTime(); State.Point=Point;
	State.TargetOffset=V.GetActorTransform().InverseTransformPosition(Point);
	State.Right=GetOwner()->GetActorRightVector().GetSafeNormal2D(); bDirectionArmed=false;
	FHALHookMotion Motion; Motion.Activation=State.Activation; Motion.bCapture=true; Motion.Anchor=TargetAnchor;
	Motion.DurationSeconds=Rules.ChoiceSeconds+.1f; Motion.Stiffness=Rules.CaptureStiffness; Motion.Damping=Rules.Damping; Motion.MaximumAcceleration=Rules.MaximumAcceleration;
	V.FindComponentByClass<UHALMovementEffectBridge>()->SetHookMotion(Motion);
	PublishState();
	UE_CLOG(HookLog.GetValueOnGameThread(),LogTemp,Log,TEXT("Hook captured Target=%s Window=%.2f"),*GetNameSafe(&V),Rules.ChoiceSeconds);
	return true;
}
void AHALHookThrow::AcceptDirection(FGuid Activation,uint32 Sequence,float Value)
{
	if(!HasAuthority()||State.Phase!=EHALHookPhase::Choosing||Activation!=State.Activation||!FMath::IsFinite(Value)||FMath::Abs(Value)>1
		||!FVehicleNetInputData::IsNewerSequence(Sequence,LastDirectionSequence)||ServerTime()-State.PhaseTime>=Rules.ChoiceSeconds) { return; }
	LastDirectionSequence=Sequence;
	if(!CasterValid()||IsInterrupted()) { Interrupt(false); return; }
	if(FMath::Abs(Value)<Rules.DirectionThreshold) { bDirectionArmed=true; return; }
	if(!bDirectionArmed) { return; }
	StartSwing(Value<0?-1:1);
}
void AHALHookThrow::StartSwing(int32 Side)
{
	auto* V=State.Target.Get(); auto* B=HookBody(V);
	if(!IsValid(V)||!B) { Return(false); return; }
	FHALHookMotion Motion; Motion.Activation=State.Activation; Motion.bCapture=Motion.bSwing=true;
	Motion.Anchor=B->GetComponentLocation(); Motion.Pivot=CapturePivot; Motion.DurationSeconds=Rules.SwingSeconds;
	Motion.AccelerationPower=Rules.SwingAccelerationPower; Motion.MaximumAcceleration=Rules.SwingMaximumAcceleration;
	const FVector Radial=(Motion.Anchor-Motion.Pivot).GetSafeNormal2D(); const double Radius=(Motion.Anchor-Motion.Pivot).Size2D();
	if(Radius<1) { Return(false); return; }
	const double Sign=FVector::DotProduct(FVector::CrossProduct(FVector::UpVector,Radial),State.Right*Side)>=0?1.:-1.;
	// Angle, travel and exit-speed settings are upper bounds. This avoids a large
	// sweep at long range and keeps the ease-in end speed within HorizontalSpeed.
	const double Length=FMath::Min3(Radius*FMath::DegreesToRadians(Rules.SwingAngle),double(Rules.SwingArcLength),
		double(Rules.HorizontalSpeed)*Rules.SwingSeconds/Rules.SwingAccelerationPower);
	Motion.SwingRadians=Sign*Length/Radius;
	if(!Motion.IsValid(false)) { Return(false); return; }
	V->FindComponentByClass<UHALMovementEffectBridge>()->SetHookMotion(Motion);
	State.Phase=EHALHookPhase::Swinging; State.PhaseTime=ServerTime(); PublishState(true);
	UE_CLOG(HookLog.GetValueOnGameThread(),LogTemp,Log,TEXT("Hook swing Target=%s Side=%d Angle=%.3f ArcLength=%.3f Seconds=%.3f"),*GetNameSafe(V),Side,FMath::RadiansToDegrees(Motion.SwingRadians),Length,Rules.SwingSeconds);
}
void AHALHookThrow::ReleaseTarget(bool Throw)
{
	auto* V=State.Target.Get(); auto* ASC=TargetASC.Get();
	State.Target=nullptr; TargetASC.Reset();
	if(ASC&&CaptureEffect.IsValid()) { ASC->RemoveActiveGameplayEffect(CaptureEffect); }
	CaptureEffect.Invalidate();
	if(!IsValid(V)) { return; }
	if(auto* C=V->FindComponentByClass<UHALHookComponent>()) { C->ReleaseClaim(*this); }
	if(auto* Bridge=V->FindComponentByClass<UHALMovementEffectBridge>())
	{
		if(Throw&&V->CanReceiveSkillEffects())
		{
			FHALHookMotion Motion; Motion.Activation=State.Activation; Motion.bThrow=Motion.bPreserveHorizontal=true; Motion.DurationSeconds=.1f;
			Motion.ThrowVelocity=FVector::UpVector*Rules.UpwardSpeed;
			Bridge->SetHookMotion(Motion); // Preserve the actual arc/blocked momentum; add minimum ascent only.
			if(auto* Other=V->FindComponentByClass<UHALHookComponent>()) { Other->NotifyExternalLaunch(); }
		}
		else { Bridge->EndHookMotion(State.Activation); }
	}
	UE_CLOG(HookLog.GetValueOnGameThread(),LogTemp,Log,TEXT("Hook released Target=%s Throw=%d PreserveHorizontal=1"),*GetNameSafe(V),Throw);
}
void AHALHookThrow::ClearCaster()
{
	if(auto* V=GetOwner())
	{
		if(auto* B=V->FindComponentByClass<UHALMovementEffectBridge>()) { B->EndHookMotion(State.Activation); }
	}
	if(auto* ASC=CasterASC.Get())
	{
		ASC->RegisterGameplayTagEvent(HALGameplayTags::State_Stun,EGameplayTagEventType::NewOrRemoved).Remove(StunHandle);
		if(CastingEffect.IsValid()) { ASC->RemoveActiveGameplayEffect(CastingEffect); }
		ASC->ForceReplication();
	}
	StunHandle.Reset(); CastingEffect.Invalidate();
}
void AHALHookThrow::Return(bool Interrupted)
{
	const FVector Point=State.Target?TipAt(ServerTime()):State.Point;
	ReleaseTarget(false); State.Point=Point; State.Phase=Interrupted?EHALHookPhase::Interrupted:EHALHookPhase::Returning;
	State.PhaseTime=ServerTime(); if(Interrupted) { ClearCaster(); } PublishState(true);
}
void AHALHookThrow::TargetLost() { if(HasAuthority()&&(State.Phase==EHALHookPhase::Choosing||State.Phase==EHALHookPhase::Swinging)) { Return(false); } }
void AHALHookThrow::Interrupt(bool OwnerExit)
{
	if(!HasAuthority()||bCleaning) { return; }
	if(OwnerExit) { Destroy(); return; }
	if(State.Phase==EHALHookPhase::Interrupted||State.Phase==EHALHookPhase::Finished) { return; }
	State.Point=TipAt(ServerTime()); Return(true);
	UE_CLOG(HookLog.GetValueOnGameThread(),LogTemp,Log,TEXT("Hook interrupted Owner=%s"),*GetNameSafe(GetOwner()));
}
void AHALHookThrow::Tick(float Delta)
{
	Super::Tick(Delta); const double Now=ServerTime();
	if(HasAuthority())
	{
		if(!CasterValid()||Now-State.StartTime>Rules.MaximumSeconds()) { Destroy(); return; }
		if(LocksCaster()&&IsInterrupted()) { Interrupt(false); }
		if(State.Phase==EHALHookPhase::Outbound)
		{
			const FVector Tip=TipAt(Now); Sweep(PreviousTip,Tip); PreviousTip=Tip;
			if(State.Phase==EHALHookPhase::Outbound&&Now-State.PhaseTime>=Rules.Range/Rules.OutboundSpeed)
			{ State.Point=Tip; State.Phase=EHALHookPhase::Returning; State.PhaseTime=Now; PublishState(true); }
		}
		else if(State.Phase==EHALHookPhase::Choosing)
		{
			const auto* Target=State.Target.Get(); const auto* ASC=TargetASC.Get();
			if(!IsValid(Target)||!Target->CanReceiveSkillEffects()||!ASC||ASC->GetAvatarActor()!=Target||!ASC->GetActiveGameplayEffect(CaptureEffect)
				||FVector::Dist2D(TargetAnchor,Target->GetActorLocation())>Rules.MaximumCaptureDrift||Now-State.PhaseTime>=Rules.ChoiceSeconds) { Return(false); }
		}
		else if(State.Phase==EHALHookPhase::Swinging)
		{
			const auto* Target=State.Target.Get(); const auto* ASC=TargetASC.Get();
			if(!IsValid(Target)||!Target->CanReceiveSkillEffects()||!ASC||ASC->GetAvatarActor()!=Target||!ASC->GetActiveGameplayEffect(CaptureEffect)) { Return(false); }
			else
			{
				const auto Status=Target->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus();
				const auto& Motion=Status.Definition.Hook;
				if(Status.bReady&&Motion.Activation==State.Activation&&Motion.bSwing)
				{
					if(FVector::Dist2D(Motion.SwingPosition(Status.ServerFrame),Target->GetActorLocation())>Rules.MaximumCaptureDrift) { Return(false); }
					else if(Status.ServerFrame>=Motion.EndFrame)
					{
						const FVector Point=TipAt(Now); ReleaseTarget(true);
						State.Point=Point; State.Phase=EHALHookPhase::Returning; State.PhaseTime=Now; PublishState(true);
					}
				}
				else if(Now-State.PhaseTime>Rules.SwingSeconds+.75f) { Return(false); }
			}
		}
		if((State.Phase==EHALHookPhase::Returning||State.Phase==EHALHookPhase::Interrupted)&&FVector::Distance(TipAt(Now),Outlet())<1) { Destroy(); return; }
		if(Rules.bDrawBounds) { DrawDebugSphere(GetWorld(),TipAt(Now),Rules.HitRadius,12,FColor::Cyan,false,0); }
	}
	if(State.Phase==EHALHookPhase::Choosing)
	{ if(auto* V=Cast<ATestVehiclePawn>(GetOwner())) { V->FindComponentByClass<UHALHookComponent>()->ObserveSteering(V->GetVehicleInputIntent().Steering); } }
	UpdateVisual();
}
void AHALHookThrow::PublishState(bool Reliable)
{
	++State.Revision; ForceNetUpdate();
	// A 0.3s swing/short reel-in may otherwise collapse into a later property update
	// under packet lag or bandwidth pressure. Only discrete phase edges are reliable.
	if(Reliable) { MulticastPhase(State); }
}
void AHALHookThrow::AcceptReplicatedState(const FHALHookState& Value)
{
	if(Value.Revision>=AcceptedState.Revision) { AcceptedState=Value; }
	State=AcceptedState; SetActorTickEnabled(State.Phase!=EHALHookPhase::Finished);
}
void AHALHookThrow::MulticastPhase_Implementation(FHALHookState Value)
{ if(!HasAuthority()) { AcceptReplicatedState(Value); } }
void AHALHookThrow::OnRep_State() { AcceptReplicatedState(State); }
void AHALHookThrow::Cleanup()
{
	if(bCleaning) { return; }
	bCleaning=true;
	if(HasAuthority())
	{
		ReleaseTarget(false); ClearCaster();
		if(auto* C=GetOwner()?GetOwner()->FindComponentByClass<UHALHookComponent>():nullptr; C&&C->GetOwnedHook()==this) { C->SetOwnedHook(nullptr); }
	}
}
void AHALHookThrow::Destroyed() { Cleanup(); Super::Destroyed(); }
void AHALHookThrow::EndPlay(EEndPlayReason::Type Reason) { Cleanup(); Super::EndPlay(Reason); }
void AHALHookThrow::UpdateVisual()
{
	if(!FApp::CanEverRender()||!FX.bEnabled||!FX.ChainMesh||!FX.HookMesh||!FX.MetalMaterial) { return; }
	if(!Metal)
	{
		Chain->SetStaticMesh(FX.ChainMesh); Hook->SetStaticMesh(FX.HookMesh); Metal=UMaterialInstanceDynamic::Create(FX.MetalMaterial,this);
		Chain->SetMaterial(0,Metal); Hook->SetMaterial(0,Metal);
		Metal->SetVectorParameterValue(TEXT("DarkColor"),FX.DarkColor); Metal->SetVectorParameterValue(TEXT("MiddleColor"),FX.MiddleColor);
		Metal->SetVectorParameterValue(TEXT("LightColor"),FX.LightColor); Metal->SetVectorParameterValue(TEXT("LightDirection"),FLinearColor(FX.LightDirection.GetSafeNormal()));
		Metal->SetScalarParameterValue(TEXT("ShadowThreshold"),FX.ShadowThreshold); Metal->SetScalarParameterValue(TEXT("HighlightThreshold"),FX.HighlightThreshold);
		Metal->SetScalarParameterValue(TEXT("ToneSoftness"),FX.ToneSoftness); Metal->SetScalarParameterValue(TEXT("GlintStrength"),FX.GlintStrength);
		Metal->SetScalarParameterValue(TEXT("GlintPower"),FX.GlintPower); Metal->SetScalarParameterValue(TEXT("ContourStrength"),FX.ContourStrength);
	}
	const double Now=ServerTime(); const FVector S=Outlet(),E=TipAt(Now); TArray<FTransform> Poses;
	const FQuat Facing=FRotationMatrix::MakeFromX((E-S).GetSafeNormal()).ToQuat();
	const FVector Connection=FVector::Distance(S,E)>FX.HookChainOffset.Size()*FX.HookScale
		?E+Facing.RotateVector(FX.HookChainOffset*FX.HookScale):S;
	HALHook::BuildChain(S,Connection,Now-State.StartTime,FX,State.Phase==EHALHookPhase::Choosing||State.Phase==EHALHookPhase::Swinging,Poses);
	Chain->ClearInstances(); Chain->AddInstances(Poses,false,true);
	const float Enter=FMath::SmoothStep(0.f,FX.EnterSeconds,float(Now-State.StartTime));
	Hook->SetWorldTransform(FTransform(Facing,E,FVector(FX.HookScale*Enter)));
}
