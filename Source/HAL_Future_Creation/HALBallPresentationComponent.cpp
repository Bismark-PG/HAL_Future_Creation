#include "HALBallPresentationComponent.h"
#include "BasicBallActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

static TAutoConsoleVariable<int32> CVarBallRelativeVisual(TEXT("hal.BallRelativeVisual"),1,TEXT("Client held-ball relative mesh presentation: 0=legacy PI, 1=enabled."));
static TAutoConsoleVariable<int32> CVarBallPresentationLog(TEXT("hal.BallPresentationLog"),0,TEXT("Ball presentation mode changes and once-per-second sample diagnostics."));

namespace
{
	struct FBallPoseOutput : Chaos::FSimCallbackOutput
	{
		FVector Position=FVector::ZeroVector, Linear=FVector::ZeroVector, Angular=FVector::ZeroVector;
		FQuat Rotation=FQuat::Identity;
		int32 Frame=INDEX_NONE;
		void Reset() { Frame=INDEX_NONE; }
	};
}

/** No UObject/GT transform access in this callback. Both poses are from the same solved step. */
class FHALBallPoseCapture : public Chaos::TSimCallbackObject<Chaos::FSimCallbackNoInput,FBallPoseOutput,Chaos::ESimCallbackOptions::PostSolve>
{
public:
	TAtomic<bool> bActive{false};
	Chaos::FSingleParticlePhysicsProxy* BallProxy=nullptr;
	Chaos::FSingleParticlePhysicsProxy* HolderProxy=nullptr;
	// Chaos calls PreSimulate for registered simulation callbacks even when the
	// only requested extension hook is PostSolve. The base implementation asserts.
	virtual void OnPreSimulate_Internal() override {}
	virtual void OnPostSolve_Internal() override
	{
		if (!bActive.Load() || !BallProxy || !HolderProxy) { return; }
		const auto* B=BallProxy->GetHandle_LowLevel(); const auto* H=HolderProxy->GetHandle_LowLevel();
		if (!B || !H) { return; }
		const auto* BR=B->CastToRigidParticle(); const auto* HR=H->CastToRigidParticle();
		if (!BR || !HR) { return; }
		const FVector Offset=BR->GetP()-HR->GetP(); const FQuat Inverse=HR->GetQ().Inverse();
		auto& Out=GetProducerOutputData_Internal(); Out.Frame=static_cast<Chaos::FPBDRigidsSolver*>(GetSolver())->GetCurrentFrame();
		Out.Position=Inverse.RotateVector(Offset); Out.Rotation=(Inverse*BR->GetQ()).GetNormalized();
		Out.Linear=Inverse.RotateVector(BR->GetV()-HR->GetV()-FVector::CrossProduct(HR->GetW(),Offset));
		Out.Angular=Inverse.RotateVector(BR->GetW()-HR->GetW());
	}
};

UHALBallPresentationComponent::UHALBallPresentationComponent()
{
	SetIsReplicatedByDefault(true);
	bAutoActivate=true;
	PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickGroup=TG_PostPhysics;
}

void UHALBallPresentationComponent::Configure(const FHALBallPresentationConfig& InConfig)
{
	FString Error;
	Config=InConfig;
	if (!Config.Validate(Error))
	{ Config={}; Config.bEnabled=false; UE_LOG(LogTemp,Warning,TEXT("%s: %s Using existing ball PI display."),*GetNameSafe(GetOwner()),*Error); }
	if (HasBegunPlay()) { NotifyStateChanged(); }
}

void UHALBallPresentationComponent::BeginPlay()
{
	Super::BeginPlay(); Ball=Cast<ABasicBallActor>(GetOwner());
	if (!Ball.IsValid()) { SetComponentTickEnabled(false); return; }
	Visual=Ball->GetVisualMesh();
	if (Visual.IsValid())
	{
		AuthoredVisual=Visual->GetRelativeTransform();
		bAuthoredAbsoluteLocation=Visual->IsUsingAbsoluteLocation(); bAuthoredAbsoluteRotation=Visual->IsUsingAbsoluteRotation();
	}
	DisplayPose=FTransform(Ball->GetActorQuat(),Ball->GetActorLocation());
	NotifyStateChanged();
}

void UHALBallPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{ StopCapture(); RestoreVisual(); Super::EndPlay(Reason); }
void UHALBallPresentationComponent::OnUnregister()
{ StopCapture(); RestoreVisual(); Super::OnUnregister(); }

void UHALBallPresentationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UHALBallPresentationComponent,ReplicatedPose); }

void UHALBallPresentationComponent::NotifyStateChanged()
{
	if (!HasBegunPlay() || !Ball.IsValid() || !GetOwner()->HasAuthority()) { return; }
	if (Ball->GetBallState()!=EBasicBallState::Controlled || !Config.bEnabled
		|| CaptureSequence!=Ball->GetBallStateSequence() || CaptureHolder.Get()!=Ball->GetControlledBy())
	{ StopCapture(); ReplicatedPose={}; LastPublishSeconds=-1; }
	if (Ball->GetBallState()==EBasicBallState::Controlled && Config.bEnabled) { StartCapture(); }
}

void UHALBallPresentationComponent::StartCapture()
{
	if (Capture || !Ball.IsValid() || !IsValid(Ball->GetControlledBy())) { return; }
	auto* HolderBody=Cast<UPrimitiveComponent>(Ball->GetControlledBy()->GetRootComponent());
	auto* B=Ball->GetPhysicsRoot()->GetBodyInstance(); auto* H=HolderBody ? HolderBody->GetBodyInstance() : nullptr;
	if (!B || !H || !GetWorld()->GetPhysicsScene()) { return; }
	const auto BP=B->GetPhysicsActor(); const auto HP=H->GetPhysicsActor();
	if (!BP || !HP) { return; }
	CaptureHolder=Ball->GetControlledBy(); CaptureSequence=Ball->GetBallStateSequence();
	CaptureSolver=GetWorld()->GetPhysicsScene()->GetSolver();
	Capture=CaptureSolver->CreateAndRegisterSimCallbackObject_External<FHALBallPoseCapture>();
	Capture->BallProxy=BP; Capture->HolderProxy=HP; Capture->bActive.Store(true);
}

void UHALBallPresentationComponent::StopCapture()
{
	if (Capture)
	{
		// Revoke first: the solver may retain callbacks after removal. Drain in-flight
		// work before an exiting holder/ball can remove its physics body.
		Capture->bActive.Store(false); CaptureSolver->WaitOnPendingTasks_External();
		CaptureSolver->UnregisterAndFreeSimCallbackObject_External(Capture);
		Capture=nullptr; CaptureSolver=nullptr;
	}
	CaptureHolder.Reset(); CaptureSequence=0;
}

void UHALBallPresentationComponent::PollCapture()
{
	if (!Capture) { return; }
	FBallPoseOutput Latest; bool bFound=false;
	while (auto Out=Capture->PopFutureOutputData_External()) { Latest=*Out; bFound=true; }
	const float Now=GetWorld()->GetTimeSeconds();
	if (!bFound || !CaptureHolder.IsValid() || (LastPublishSeconds>=0 && Now-LastPublishSeconds<1.f/Config.SendRateHz)) { return; }
	FHALBallRelativePose Next; Next.Holder=CaptureHolder.Get(); Next.StateSequence=CaptureSequence;
	Next.PhysicsFrame=Latest.Frame; Next.ServerPublishSeconds=Now; Next.Position=Latest.Position; Next.Rotation=Latest.Rotation;
	Next.LinearVelocity=Latest.Linear; Next.AngularVelocity=Latest.Angular;
	if (!Next.IsValid(Config.MaximumRelativeDistance) || !Next.IsNewerThan(ReplicatedPose)) { return; }
	ReplicatedPose=Next; LastPublishSeconds=Now; ++PublishedCount; GetOwner()->ForceNetUpdate();
}

void UHALBallPresentationComponent::OnRep_RelativePose()
{
	if (!ReplicatedPose.IsValid(Config.MaximumRelativeDistance) || !ReplicatedPose.IsNewerThan(AcceptedPose)) { return; }
	AcceptedPose=ReplicatedPose; LastReceiveSeconds=GetWorld()->GetTimeSeconds(); ++ReceivedCount;
}

void UHALBallPresentationComponent::SetMode(EHALBallVisualMode Next)
{
	if (Next!=Mode && CVarBallPresentationLog.GetValueOnGameThread()>0)
	{ UE_LOG(LogTemp,Log,TEXT("Ball presentation: %s Mode=%d->%d Seq=%u Frame=%d Age=%.3f Samples=%u RelativeFrames=%u"),
		*GetNameSafe(GetOwner()),int32(Mode),int32(Next),AcceptedPose.StateSequence,AcceptedPose.PhysicsFrame,SampleAge,ReceivedCount,RelativeFrames); }
	Mode=Next;
}

void UHALBallPresentationComponent::RestoreVisual()
{
	if (bVisualOverride && Visual.IsValid())
	{
		Visual->SetAbsolute(bAuthoredAbsoluteLocation,bAuthoredAbsoluteRotation,Visual->IsUsingAbsoluteScale());
		Visual->SetRelativeTransform(AuthoredVisual);
	}
	bVisualOverride=false; DisplayHolder.Reset(); DisplaySequence=0; Mode=EHALBallVisualMode::Physics;
}

void UHALBallPresentationComponent::UpdateClient(float DeltaTime)
{
	if (!Ball.IsValid() || !Visual.IsValid() || !FMath::IsFinite(DeltaTime) || DeltaTime<=0) { return; }
	// RepNotify can precede holder NetGUID resolution or the ball's own state notification.
	OnRep_RelativePose();
	const float Now=GetWorld()->GetTimeSeconds();
	float ServerNow=Now;
	if (const auto* GS=GetWorld()->GetGameState()) { ServerNow=GS->GetServerWorldTimeSeconds(); }
	SampleAge=FMath::Max(0.f,ServerNow-AcceptedPose.ServerPublishSeconds);
	AActor* Holder=Ball->GetControlledBy();
	const bool bUsable=Config.bEnabled && CVarBallRelativeVisual.GetValueOnGameThread()>0
		&& Ball->GetBallState()==EBasicBallState::Controlled && IsValid(Holder) && AcceptedPose.Holder==Holder
		&& AcceptedPose.StateSequence==Ball->GetBallStateSequence() && AcceptedPose.IsValid(Config.MaximumRelativeDistance)
		&& LastReceiveSeconds>=0 && Now-LastReceiveSeconds<=Config.StaleSeconds && SampleAge<=Config.StaleSeconds
		&& !Visual->IsSimulatingPhysics() && !bAuthoredAbsoluteLocation && !bAuthoredAbsoluteRotation;
	const FTransform PhysicsPose(Ball->GetActorQuat(),Ball->GetActorLocation());
	if (!bVisualOverride) { DisplayPose=PhysicsPose; }
	if (bUsable)
	{
		FTransform TargetRelative=HALBallPresentation::Extrapolate(AcceptedPose,SampleAge,Config.MaximumExtrapolationSeconds);
		TargetRelative.SetLocation(TargetRelative.GetLocation().GetClampedToMaxSize(Config.MaximumRelativeDistance));
		const bool bEntering=Mode!=EHALBallVisualMode::Relative || DisplayHolder.Get()!=Holder || DisplaySequence!=AcceptedPose.StateSequence;
		if (bEntering) { RelativePose=TargetRelative; }
		else { RelativePose=HALBallPresentation::Blend(RelativePose,TargetRelative,Config.RelativeBlendSeconds>0 ? 1.f-FMath::Exp(-DeltaTime/Config.RelativeBlendSeconds) : 1.f); }
		const FTransform Target=HALBallPresentation::Compose(Holder->GetActorTransform(),RelativePose);
		if (bEntering)
		{
			EntryOffset=DisplayPose.GetLocation()-Target.GetLocation();
			EntryRotation=(DisplayPose.GetRotation()*Target.GetRotation().Inverse()).GetNormalized(); EntryAge=0;
			DisplayHolder=Holder; DisplaySequence=AcceptedPose.StateSequence;
		}
		const float Weight=1.f-FMath::SmoothStep(0.f,Config.HandoffSeconds,EntryAge);
		DisplayPose=FTransform((FQuat::Slerp(FQuat::Identity,EntryRotation,Weight)*Target.GetRotation()).GetNormalized(),Target.GetLocation()+EntryOffset*Weight);
		EntryAge+=DeltaTime; ++RelativeFrames; SetMode(EHALBallVisualMode::Relative);
	}
	else if (bVisualOverride)
	{
		if (Mode!=EHALBallVisualMode::Handoff) { HandoffStart=DisplayPose; HandoffAge=0; SetMode(EHALBallVisualMode::Handoff); }
		DisplayPose=HALBallPresentation::Blend(HandoffStart,PhysicsPose,FMath::SmoothStep(0.f,Config.HandoffSeconds,HandoffAge));
		HandoffAge+=DeltaTime;
		if (HandoffAge>Config.HandoffSeconds+DeltaTime) { RestoreVisual(); DisplayPose=PhysicsPose; return; }
	}
	else { return; }
	if (!bVisualOverride) { Visual->SetAbsolute(true,true,Visual->IsUsingAbsoluteScale()); bVisualOverride=true; }
	const FTransform MeshPose=AuthoredVisual*FTransform(DisplayPose.GetRotation(),DisplayPose.GetLocation(),Ball->GetPhysicsRoot()->GetComponentScale());
	Visual->SetWorldLocationAndRotation(MeshPose.GetLocation(),MeshPose.GetRotation());
}

void UHALBallPresentationComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime,TickType,TickFunction);
	if (!Ball.IsValid()) { return; }
	if (GetOwner()->HasAuthority()) { NotifyStateChanged(); PollCapture(); }
	else { UpdateClient(DeltaTime); }
	LogAge+=DeltaTime;
	if (CVarBallPresentationLog.GetValueOnGameThread()>0 && LogAge>=1 && Ball->GetBallState()==EBasicBallState::Controlled)
	{
		LogAge=0; UE_LOG(LogTemp,Log,TEXT("Ball presentation sample: %s Authority=%d Seq=%u Frame=%d Published=%u Received=%u Mode=%d Age=%.3f"),
			*GetNameSafe(GetOwner()),GetOwner()->HasAuthority(),Ball->GetBallStateSequence(),GetOwner()->HasAuthority() ? ReplicatedPose.PhysicsFrame : AcceptedPose.PhysicsFrame,
			PublishedCount,ReceivedCount,int32(Mode),SampleAge);
	}
}
