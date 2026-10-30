#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALLegacyCombatTestRules.h"
#include "../HALBallPresentationComponent.h"
#include "../BasicBallActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include <limits>

struct FHALBallPresentationTestAccess
{
	static void State(ABasicBallActor* B,EBasicBallState State,uint32 Sequence,AActor* Holder)
	{ B->RepState.State=State; B->RepState.StateSequence=Sequence; B->RepState.Holder=Holder; B->OnRep_BallRepState(); }
	static void Receive(UHALBallPresentationComponent* C,const FHALBallRelativePose& Pose)
	{ C->ReplicatedPose=Pose; C->OnRep_RelativePose(); }
	static void Tick(UHALBallPresentationComponent* C,float Delta) { C->UpdateClient(Delta); }
	static void Expire(UHALBallPresentationComponent* C) { C->LastReceiveSeconds=-1; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBallRelativeMathTest,"HAL.FutureCreation.Ball.RelativePoseContract",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALBallRelativeMathTest::RunTest(const FString&)
{
	FHALBallPresentationConfig Config; FString Error; TestTrue(TEXT("Default presentation config valid"),Config.Validate(Error));
	Config.SendRateHz=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite presentation rate rejected"),Config.Validate(Error));
	Config={}; Config.MaximumExtrapolationSeconds=.2f; Config.StaleSeconds=.1f;
	TestFalse(TEXT("Extrapolation cannot outlive stale timeout"),Config.Validate(Error));
	const FTransform Holder(FRotator(0,90,0),FVector(1000,2000,0),FVector(3));
	const FTransform Relative(FRotator(0,0,40),FVector(200,0,20));
	const auto World=HALBallPresentation::Compose(Holder,Relative);
	TestTrue(TEXT("Vehicle turn immediately rotates the cm offset, without applying body scale twice"),World.GetLocation().Equals(FVector(1000,2200,20),.001));
	FHALBallRelativePose Older; Older.StateSequence=3; Older.PhysicsFrame=100;
	auto Newer=Older; Newer.PhysicsFrame=101;
	TestTrue(TEXT("New physics sample accepted"),Newer.IsNewerThan(Older));
	TestFalse(TEXT("Duplicate sample rejected"),Older.IsNewerThan(Older));
	Older.StateSequence=2; Older.PhysicsFrame=10000; TestFalse(TEXT("Old holder epoch cannot win with newer frame"),Older.IsNewerThan(Newer));
	Newer.StateSequence=1; Older.StateSequence=MAX_uint32; TestTrue(TEXT("Nonzero sequence wrap handled"),Newer.IsNewerThan(Older));
	FHALBallRelativePose Motion; Motion.Position=FVector(200,0,0); Motion.LinearVelocity=FVector(100,0,0); Motion.AngularVelocity=FVector(0,0,PI);
	const auto Predicted=HALBallPresentation::Extrapolate(Motion,10,.1f);
	TestTrue(TEXT("Long stall extrapolation is bounded to 10cm"),Predicted.GetLocation().Equals(FVector(210,0,0),.001));
	TestTrue(TEXT("Authoritative angular motion keeps ball rolling"),FMath::IsNearlyEqual(Predicted.Rotator().Yaw,18.,.01));
	TestTrue(TEXT("Negative age cannot rewind the sample"),HALBallPresentation::Extrapolate(Motion,-1,.1f).GetLocation().Equals(Motion.Position));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBallRelativeLifecycleTest,"HAL.FutureCreation.Ball.RelativeVisualLifecycleAndPhysicsIsolation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALBallRelativeLifecycleTest::RunTest(const FString&)
{
	FScopedHALLegacyCombatTestRules Legacy;
	const auto Options=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Options);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	auto* Holder=W->SpawnActor<AActor>(); auto* Anchor=NewObject<USphereComponent>(Holder);
	Holder->AddInstanceComponent(Anchor); Holder->SetRootComponent(Anchor); Anchor->SetCollisionEnabled(ECollisionEnabled::NoCollision); Anchor->RegisterComponent();
	auto* B=W->SpawnActor<ABasicBallActor>(); B->GetVisualMesh()->SetRelativeLocation(FVector(5,0,0));
	B->DispatchBeginPlay(); B->SetRole(ROLE_SimulatedProxy);
	auto* C=B->FindComponentByClass<UHALBallPresentationComponent>();
	if (!TestNotNull(TEXT("Native component available on ball"),C)) { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); return false; }
	FHALBallPresentationConfig Config; Config.RelativeBlendSeconds=0; C->Configure(Config);
	const auto CollisionMode=B->GetPhysicsRoot()->GetCollisionEnabled(); const auto Responses=B->GetPhysicsRoot()->GetCollisionResponseToChannels();
	FHALBallPresentationTestAccess::State(B,EBasicBallState::Controlled,1,Holder);
	FHALBallRelativePose Pose; Pose.Holder=Holder; Pose.StateSequence=1; Pose.PhysicsFrame=50; Pose.Position=FVector(200,0,0);
	FHALBallPresentationTestAccess::Receive(C,Pose);
	for (int I=0;I<12;++I) { FHALBallPresentationTestAccess::Tick(C,.02f); }
	const auto PhysicsBefore=B->GetActorTransform(); const auto V=B->GetPhysicsRoot()->GetPhysicsLinearVelocity();
	Holder->SetActorLocationAndRotation(FVector(1000,0,0),FRotator(0,90,0));
	FHALBallPresentationTestAccess::Tick(C,.02f);
	TestEqual(TEXT("Relative mode active"),C->GetVisualMode(),EHALBallVisualMode::Relative);
	TestTrue(TEXT("Visible mesh follows turn/translation immediately, including authored offset"),B->GetVisualMesh()->GetComponentLocation().Equals(FVector(1000,205,0),.01));
	TestTrue(TEXT("Physics root not moved to local car"),B->GetActorTransform().Equals(PhysicsBefore));
	TestTrue(TEXT("No client velocity/impulse added"),B->GetPhysicsRoot()->GetPhysicsLinearVelocity().Equals(V));
	TestTrue(TEXT("Controlled Chaos state alive"),B->GetPhysicsRoot()->IsSimulatingPhysics());
	TestEqual(TEXT("Collision mode preserved"),B->GetPhysicsRoot()->GetCollisionEnabled(),CollisionMode);
	TestEqual(TEXT("Vehicle response stays ignored"),B->GetPhysicsRoot()->GetCollisionResponseToChannel(ECC_Pawn),ECR_Ignore);
	// A stale epoch with a large frame cannot take ownership of the visible ball.
	auto Stale=Pose; Stale.StateSequence=MAX_uint32; Stale.PhysicsFrame=9999; Stale.Position=FVector(1500,0,0);
	FHALBallPresentationTestAccess::Receive(C,Stale); FHALBallPresentationTestAccess::Tick(C,.02f);
	TestTrue(TEXT("Old sample cannot drag the ball away"),B->GetVisualMesh()->GetComponentLocation().Equals(FVector(1000,205,0),.01));
	const auto BeforeRelease=B->GetVisualMesh()->GetComponentLocation();
	FHALBallPresentationTestAccess::State(B,EBasicBallState::Launched,2,nullptr);
	FHALBallPresentationTestAccess::Tick(C,.02f);
	TestTrue(TEXT("State handoff starts at last visible world pose"),B->GetVisualMesh()->GetComponentLocation().Equals(BeforeRelease,.01));
	for (int I=0;I<15;++I) { FHALBallPresentationTestAccess::Tick(C,.02f); }
	TestEqual(TEXT("Handoff returns to PI within configured bound"),C->GetVisualMode(),EHALBallVisualMode::Physics);
	TestFalse(TEXT("Authored relative attachment restored"),B->GetVisualMesh()->IsUsingAbsoluteLocation());
	TestTrue(TEXT("Authored visual offset restored"),B->GetVisualMesh()->GetRelativeLocation().Equals(FVector(5,0,0)));
	TestTrue(TEXT("Leaving Controlled restores collision responses"),B->GetPhysicsRoot()->GetCollisionResponseToChannels()==Responses);
	TestTrue(TEXT("Leaving Controlled restores gravity"),B->GetPhysicsRoot()->IsGravityEnabled());
	FHALBallPresentationTestAccess::State(B,EBasicBallState::Controlled,3,Holder);
	Pose.StateSequence=3; Pose.PhysicsFrame=80; FHALBallPresentationTestAccess::Receive(C,Pose);
	for (int I=0;I<12;++I) { FHALBallPresentationTestAccess::Tick(C,.02f); }
	FHALBallPresentationTestAccess::Expire(C);
	for (int I=0;I<15;++I) { FHALBallPresentationTestAccess::Tick(C,.02f); }
	TestEqual(TEXT("Packet stall has bounded fallback"),C->GetVisualMode(),EHALBallVisualMode::Physics);
	Pose.PhysicsFrame=81; FHALBallPresentationTestAccess::Receive(C,Pose); FHALBallPresentationTestAccess::Tick(C,.02f);
	TestEqual(TEXT("Fresh sample resumes relative presentation"),C->GetVisualMode(),EHALBallVisualMode::Relative);
	Holder->Destroy();
	for (int I=0;I<15;++I) { FHALBallPresentationTestAccess::Tick(C,.02f); }
	TestEqual(TEXT("Holder destruction cannot leave absolute visual stuck"),C->GetVisualMode(),EHALBallVisualMode::Physics);
	W->DestroyWorld(false); GEngine->DestroyWorldContext(W);
	return true;
}
#endif
