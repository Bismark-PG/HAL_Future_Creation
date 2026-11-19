#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALSideShiftTestSupport.h"
#include "HALVehicleAbilityTestSupport.h"
#include "HALRoomTestSupport.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALMatchGameState.h"
#include "../VehicleHealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/ScopeExit.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSideShiftProfileTest,"HAL.FutureCreation.SideShift.ProfileHistoryAndValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSideShiftProfileTest::RunTest(const FString&)
{
	FHALSideShiftMotion M; TestTrue(TEXT("Defaults finite"),M.IsValidTuning()); M.Activation=FGuid::NewGuid(); M.bActive=true; M.Direction=FVector::RightVector;
	M.Stamp(nullptr,100,1.f/60); TestTrue(TEXT("Stamped state valid"),M.IsValid(true));
	float Distance=0; for(int32 F=100;F<200;++F) { Distance+=M.StepSpeed(F,1.f/60)/60; }
	TestTrue(TEXT("Average profile integrates to authored 300 cm"),FMath::IsNearlyEqual(Distance,300.f,.01f));
	const int32 TailFrame=M.StartFrame+M.ShiftFrames+1;
	TestTrue(TEXT("Drift tail outlasts translation"),!M.IsSliding(TailFrame)&&M.IsDrifting(TailFrame));
	TestFalse(TEXT("Window ends at configured drift time"),M.IsDrifting(M.StartFrame+M.DriftFrames));
	TestTrue(TEXT("Heading error generates turn authority"),M.HeadingAcceleration(1,0,1.f/60)>10);
	TestTrue(TEXT("At target the controller brakes yaw"),M.HeadingAcceleration(0,2,1.f/60)<0);
	auto Invalid=M; Invalid.Distance=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("NaN rejected"),Invalid.IsValid(true));
	Invalid=M; Invalid.Direction.Z=.5; TestFalse(TEXT("Nonhorizontal direction rejected"),Invalid.IsValid(true));
	FHALMovementSnapshot S; S.Epoch=FGuid::NewGuid(); S.Revision=1; S.SideShift=M; S.Stamp(nullptr,100,1.f/60);
	auto Later=S; Later.Revision=2; Later.GripScale=.2; Later.Stamp(&S,120,1.f/60);
	TestEqual(TEXT("Unrelated movement changes retain activation start"),Later.SideShift.StartFrame,100);
	auto Stop=Later; Stop.Revision=3; Stop.SideShift.bActive=false; Stop.Stamp(&Later,130,1.f/60);
	TestFalse(TEXT("Cancelled side motion cannot resume"),Stop.SideShift.IsSliding(131));
	TestFalse(TEXT("Cancelled drift does not resume"),Stop.SideShift.IsDrifting(131));
	FHALMovementTimeline Timeline; Timeline.Append(S,128); Timeline.Append(Later,128); Timeline.Append(Stop,128); Timeline.ExtendTo(160,128);
	TArray<uint8> Bytes; { FMemoryWriter A(Bytes); TestTrue(TEXT("Side shift timeline serializes"),Timeline.Serialize(A)); }
	FHALMovementTimeline Read; { FMemoryReader A(Bytes); TestTrue(TEXT("Side shift timeline cold reads"),Read.Serialize(A)); }
	FHALMovementSnapshot Past,Ended; TestTrue(TEXT("Past movement is queryable"),Read.Query(115,Past)&&Past.SideShift.IsSliding(115));
	TestTrue(TEXT("Stopped movement retained in history"),Read.Query(140,Ended)&&!Ended.SideShift.IsDrifting(140));
	auto* C=NewObject<UHALSideShiftConfig>(); FString Error; TestTrue(TEXT("Default skill configuration valid"),C->Validate(Error));
	TestEqual(TEXT("Four equal energy payments fill the pool"), GetDefault<UHALSideShiftAbility>()->GetUltimateCost()*4, HALUltimate::Capacity);
	FHALVehicleAbilityConfig Vehicle; Vehicle.FullRechargeSeconds=0; TestFalse(TEXT("Zero recharge rejected on vehicle"),Vehicle.Validate(Error)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSideShiftQuarterTurnTest,"HAL.FutureCreation.SideShift.QuarterTurnLatchReleaseAndReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSideShiftQuarterTurnTest::RunTest(const FString&)
{
	FHALSideShiftMotion M; M.Activation=FGuid::NewGuid(); M.bActive=true; M.Direction=FVector::RightVector; M.Stamp(nullptr,0,1.f/60);
	for(float Sign:{-1.f,1.f})
	{
		M.InitialForward=FRotator(0,170,0).Vector(); M.Direction=FVector::CrossProduct(FVector::UpVector,M.InitialForward)*Sign;
		FHALSideShiftTurnState State; float Yaw=170, Rate=0, Peak=0; FHALSideShiftTurnState Checkpoint; float SavedYaw=0,SavedRate=0,ExpectedA=0;
		for(int32 Frame=0;Frame<90;++Frame)
		{
			float A=0; TestTrue(TEXT("Holding drift retains one captured target beyond activation window"),State.Update(M,Frame,Yaw,Rate,Sign,true,1.f/60,A));
			if(Frame==7) { Checkpoint=State; SavedYaw=Yaw; SavedRate=Rate; ExpectedA=A; }
			Rate+=A/60; Yaw=FMath::UnwindDegrees(Yaw+FMath::RadiansToDegrees(Rate/60)); Peak=FMath::Max(Peak,FMath::Abs(FMath::FindDeltaAngleDegrees(170,Yaw)));
		}
		TestTrue(TEXT("Left input turns left and right input turns right across the yaw wrap"),FMath::Abs(FMath::FindDeltaAngleDegrees(170,Yaw)-Sign*90)<1);
		TestTrue(TEXT("No full spin or overshoot from held input"),Peak<92&&FMath::Abs(Rate)<.01);
		FVehiclePhysicsHistoryState Stored; Stored.State.SideShiftTurn=Checkpoint; TArray<uint8> Bytes; bool OK=false;
		{ FMemoryWriter Ar(Bytes); Stored.NetSerialize(Ar,nullptr,OK); } TestTrue(TEXT("Turn latch checkpoint serializes"),OK);
		FVehiclePhysicsHistoryState Restored; { FMemoryReader Ar(Bytes); Restored.NetSerialize(Ar,nullptr,OK); }
		TestTrue(TEXT("Turn checkpoint survives cold history restore"),OK&&Restored.State.SideShiftTurn.Equals(Checkpoint));
		float Replayed=0; Restored.State.SideShiftTurn.Update(M,7,SavedYaw,SavedRate,Sign,true,1.f/60,Replayed);
		TestEqual(TEXT("Rewind does not choose a new heading or spend another angle"),Replayed,ExpectedA);
		auto Wrong=Restored; Wrong.State.SideShiftTurn.TargetYaw+=5;
		TestFalse(TEXT("Different predicted target requests correction"),Stored.CompareData(Wrong));
		float Brake=0; State.Update(M,90,Yaw,3,Sign,false,1.f/60,Brake); TestTrue(TEXT("Release brakes rather than restarting"),Brake<0&&State.bReleased);
		State.Update(M,91,Yaw,0,-Sign,true,1.f/60,Brake); TestTrue(TEXT("Re-press cannot buy a second turn in the same activation"),State.bReleased);
		State.Cancel(M.Activation); TestFalse(TEXT("Interrupt cannot resume the old turn"),State.Update(M,1,Yaw,0,Sign,true,1.f/60,Brake));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSideShiftArcGeometryTest,"HAL.FutureCreation.SideShift.ArcSpanBendAndHistory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSideShiftArcGeometryTest::RunTest(const FString&)
{
	FHALSideShiftMotion M; M.Activation=FGuid::NewGuid(); M.bActive=true; M.Direction=FVector::RightVector; M.Stamp(nullptr,0,1.f/60);
	for(float Bend:{0.f,.5f,1.f,2.f})
	{
		M.DriftArcBend=Bend; const auto End=M.ArcOffset(1,M.DriftLateralSpan);
		TestTrue(TEXT("Bend never shrinks the configured lateral span"),FMath::Abs(End.Y-450)<.01);
		TestTrue(TEXT("Bend controls independent forward wrapping depth"),FMath::Abs(End.X-450*Bend)<.01);
	}
	M.DriftArcBend=1; FHALSideShiftTurnState S; float A=0;
	S.Update(M,0,0,0,1,true,1.f/60,A); FVector Sum=FVector::ZeroVector;
	for(int32 F=0;F<=M.ShiftFrames;++F) { Sum+=S.ArcStepVelocity(M,F,1.f/60)/60; }
	TestTrue(TEXT("Exact fixed-step curve integrates to 450cm side and forward"),Sum.Equals(FVector(450,450,0),.01));
	const FVector Middle=M.ArcOffset(.5f,450);
	TestTrue(TEXT("Vehicle-center arc enters forward and turns about the selected side"),FMath::Abs(FVector::Distance(Middle,FVector(0,450,0))-450)<.01);
	for(float Bend:{.5f,1.f,2.f}) for(float P:{.1f,.3f,.5f,.7f,.9f})
	{
		M.DriftArcBend=Bend;
		const FVector Tangent=(M.ArcOffset(P+.0001f,450)-M.ArcOffset(P-.0001f,450)).GetSafeNormal();
		const FVector Heading=FRotator(0,90*M.ArcHeadingProgress(P),0).Vector();
		TestTrue(TEXT("Heading reference follows the same ellipse tangent as translation"),FVector::DotProduct(Tangent,Heading)>.9999);
	}
	M.DriftArcBend=1; M.InitialVelocity=FVector(1200,0,0); S={}; S.Update(M,0,0,0,1,true,1.f/60,A);
	TestTrue(TEXT("Forward entry preserves current driving direction"),S.ArcBackgroundVelocity(M,0).X>1199);
	TestTrue(TEXT("Exit driving momentum follows the new heading instead of snapping to original forward"),S.ArcBackgroundVelocity(M,M.ShiftFrames).Equals(FVector(0,1200,0),.01));
	S.TrackGameplayImpulse(M,4,FVector(-200,0,0));
	TestTrue(TEXT("External recoil stays world-directed while driving momentum turns"),S.ArcBackgroundVelocity(M,M.ShiftFrames).Equals(FVector(-200,1200,0),.01));
	const auto MidState=S; S.Cancel(M.Activation); TestFalse(TEXT("Cancelled arc never resumes its motor"),S.IsArcDriving(M,1));
	TestTrue(TEXT("Captured arc itself validates"),MidState.IsValid());
	M.bDriftAtStart=true; S={};
	TestTrue(TEXT("Cast modifier starts the full arc despite stale continuous steering and handbrake"),S.Update(M,0,0,0,0,false,1.f/60,A));
	TestTrue(TEXT("Stale continuous release cannot truncate a cast-time arc"),S.ArcFrames==M.ShiftFrames&&!S.bReleased);
	M.bDriftReleased=true; S.Update(M,2,-5,-2,0,false,1.f/60,A);
	TestTrue(TEXT("Reliable release brakes even if no continuous held packet ever arrived"),S.bReleased&&A>0&&S.ArcStepVelocity(M,2,1.f/60).IsZero());
	TestTrue(TEXT("Early release freezes background heading instead of continuing the scripted turn"),S.ArcBackgroundVelocity(M,2).Equals(S.ArcBackgroundVelocity(M,12),.01));
	M.bDriftReleased=false; S.Update(M,3,-5,0,1,true,1.f/60,A);
	TestTrue(TEXT("Re-press after the edge cannot restart this arc"),S.bReleased);
	TArray<uint8> Bytes; { FMemoryWriter Ar(Bytes); M.Serialize(Ar); }
	FHALSideShiftMotion Read; { FMemoryReader Ar(Bytes); Read.Serialize(Ar); }
	TestTrue(TEXT("Cast-time intent survives movement history transport"),Read==M);
	return true;
}

namespace
{
	struct FSideShiftTestLifetime
	{
		UWorld* W; UHALSideShiftConfig* Previous;
		FSideShiftTestLifetime(UWorld* In):W(In),Previous(GetMutableDefault<UHALSideShiftAbility>()->SkillConfig) {}
		~FSideShiftTestLifetime() { GetMutableDefault<UHALSideShiftAbility>()->SkillConfig=Previous; W->DestroyWorld(false); GEngine->DestroyWorldContext(W); }
	};
	class FAdvanceSideShift : public IAutomationLatentCommand
	{
		TSharedRef<FSideShiftTestLifetime> Life; double Until; uint64 LastFrame=MAX_uint64; TFunction<void()> Check;
	public:
		FAdvanceSideShift(TSharedRef<FSideShiftTestLifetime> L,float Seconds,TFunction<void()> C):Life(L),Until(L->W->GetTimeSeconds()+Seconds),Check(MoveTemp(C)){}
		bool Update() override
		{ if(Life->W->GetTimeSeconds()>=Until) { Check(); return true; } if(LastFrame!=GFrameCounter) { LastFrame=GFrameCounter; Life->W->Tick(LEVELTICK_All,.05f); } return false; }
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSideShiftGASTest,"HAL.FutureCreation.SideShift.ChargesInputBallInterruptAndCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALSideShiftGASTest::RunTest(const FString&)
{
	const auto Opt=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
	auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Opt); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
	auto Life=MakeShared<FSideShiftTestLifetime>(W); auto* Config=NewObject<UHALSideShiftConfig>(GetTransientPackage()); GetMutableDefault<UHALSideShiftAbility>()->SkillConfig=Config;
	auto* Match=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
	auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_07.BP_VehiclePawn_Car_07_C"));
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* P=W->SpawnActor<ATestVehiclePawn>(Class,FVector(0,0,2000),FRotator::ZeroRotator,Params);
	if(!TestNotNull(TEXT("Actual Car07 BP spawns without editing it"),P)) { return false; }
	auto* Definition=FHALVehicleAbilityTestAccess::Configure(P,UHALSideShiftAbility::StaticClass()); FHALVehicleAbilityTestAccess::RuntimeConfig(P,Definition->Abilities);
	P->PreInitializeComponents(); P->InitializeComponents(); P->PostInitializeComponents();
	auto* PC=W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController(); auto* PS=W->SpawnActor<AHALPlayerState>();
	PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(7); PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
	PS->ChangeVehicleAvatar(P,1); P->DispatchBeginPlay(); FHALSideShiftTestAccess::Pose(P);
	auto* ASC=CastChecked<UHALAbilitySystemComponent>(P->GetAbilitySystemComponent()); auto* C=P->FindComponentByClass<UHALSideShiftComponent>();
	TestEqual(TEXT("GAS grants exactly four initial charges"),C->GetChargeView().Charges,4);
	TestFalse(TEXT("No direction is rejected before cost"),C->Request(0)); TestEqual(TEXT("No-direction cost stays zero"),C->GetChargeView().Charges,4);
	FHALSideShiftTestAccess::Pose(P,false); C->Request(-1); TestEqual(TEXT("Airborne request does not consume"),C->GetChargeView().Charges,4); FHALSideShiftTestAccess::Pose(P);
	auto* BallClass=LoadClass<ABasicBallActor>(nullptr,TEXT("/Game/Blueprints/Balls/BP_BasicBall.BP_BasicBall_C"));
	auto* Ball=W->SpawnActor<ABasicBallActor>(BallClass,P->GetActorLocation()+FVector(200,0,0),FRotator::ZeroRotator,Params); Ball->PreInitializeComponents(); Ball->DispatchBeginPlay();
	TestTrue(TEXT("Normal controlled ball acquired"),FHALSideShiftTestAccess::Acquire(P,Ball));
	TestTrue(TEXT("Stationary left request dispatched"),C->Request(-1,true)); TestEqual(TEXT("One activation costs one charge"),C->GetChargeView().Charges,3);
	auto* Bridge=P->FindComponentByClass<UHALMovementEffectBridge>();
	TestTrue(TEXT("GAS publishes the cast-time drift modifier"),FHALRoomTestAccess::Flush(Bridge).SideShift.bDriftAtStart);
	C->ObserveHandbrake(false);
	// This fixture does not advance Chaos: a second revision must wait for the
	// first authority frame. The release must remain queued, not be discarded.
	TestTrue(TEXT("A rapid release queues physics publication without a continuous input packet"),FHALRoomTestAccess::HasPendingPublication(Bridge));
	TestTrue(TEXT("Ball remains controlled without skill launch/drop"),Ball->GetBallState()==EBasicBallState::Controlled);
	TestFalse(TEXT("Side shift grants no impact immunity"),ASC->HasMatchingGameplayTag(HALGameplayTags::State_Combat_ImpactImmune));
	const double Due=C->GetChargeView().NextRechargeServerTime;
	C->Request(1); TestEqual(TEXT("Reentry during translation cannot stack"),C->GetChargeView().Charges,3);
	FHALSideShiftTestAccess::ReplayRequest(*C,-1,3); TestEqual(TEXT("Duplicate request cannot consume again"),C->GetChargeView().Charges,3);
	const auto Motion=FHALRoomTestAccess::Flush(P->FindComponentByClass<UHALMovementEffectBridge>());
	TestTrue(TEXT("Direction is locked to initial vehicle-left"),Motion.SideShift.Direction.Equals(FVector(0,-1,0)));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceSideShift>(Life,1.1f,[this,Life,P,C,ASC,Config,Due,Ball]()
	{
		FHALSideShiftTestAccess::Pose(P); C->Request(1,true); TestEqual(TEXT("Second activation costs one"),C->GetChargeView().Charges,2);
		TestTrue(TEXT("Further spending preserves time to the next cell"), FMath::IsNearlyEqual(C->GetChargeView().NextRechargeServerTime,Due,.002));
		C->Initialize(*ASC,*Config); TestEqual(TEXT("Idempotent binding does not refill"),C->GetChargeView().Charges,2);
		P->SetActorRotation(FRotator(0,90,0)); FVector Recoil;
		TestTrue(TEXT("Normal launch works during side shift"),P->FindComponentByClass<UBallControlComponent>()->LaunchHeldBall(Recoil));
		// This GAS fixture does not advance Chaos. Use the actual launch transaction's
		// recoil vector here; integrated ball velocity belongs to the solver/network tests.
		TestTrue(TEXT("Normal launch uses actual current forward and retains recoil"),FVector::DotProduct(Recoil.GetSafeNormal(),-P->GetActorForwardVector())>.99
			&&Ball->GetBallState()==EBasicBallState::Launched&&Ball->GetLaunchedBy()==P);
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceSideShift>(Life,4.05f,[this,Life,P,C,ASC,Config]()
		{
			TestEqual(TEXT("Five seconds restores only one charge"),C->GetChargeView().Charges,3);
			C->ObserveHandbrake(false);
			TestTrue(TEXT("Release remains queued even after the visual skill window expired"),FHALRoomTestAccess::HasPendingPublication(P->FindComponentByClass<UHALMovementEffectBridge>()));
			FHALSideShiftTestAccess::Pose(P); C->Request(-1); TestEqual(TEXT("Third activation consumes exactly one"),C->GetChargeView().Charges,2);
			TestTrue(TEXT("Shared stun interrupts immediately"),ASC->ApplyStun(6,P));
			TestFalse(TEXT("Interrupted side shift presentation cannot resume"),FHALSideShiftTestAccess::Visual(*C).bDriving);
			TestEqual(TEXT("Interrupted activation is not refunded"),C->GetChargeView().Charges,2);
			FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceSideShift>(Life,5.05f,[this,Life,P,C,ASC]()
			{
				TestEqual(TEXT("Charge recovery continues through stun"),C->GetChargeView().Charges,3);
				FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceSideShift>(Life,1.1f,[this,Life,P,C,ASC]()
				{
					FHALSideShiftTestAccess::Pose(P); C->Request(-1); TestEqual(TEXT("After stun expires the third charge is usable"),C->GetChargeView().Charges,2);
					FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceSideShift>(Life,1.1f,[this,Life,P,C,ASC]()
					{
						FHALSideShiftTestAccess::Pose(P); C->Request(1); TestEqual(TEXT("Next charge is independent"),C->GetChargeView().Charges,1);
						FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceSideShift>(Life,1.1f,[this,Life,P,C,ASC]()
						{
							FHALSideShiftTestAccess::Pose(P); C->Request(-1); TestEqual(TEXT("Final stack can be consumed"),C->GetChargeView().Charges,0);
							FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAdvanceSideShift>(Life,1.1f,[this,Life,P,C,ASC]()
							{
								FHALSideShiftTestAccess::Pose(P); const auto Previous=FHALSideShiftTestAccess::Visual(*C).Activation;
								C->Request(1); TestEqual(TEXT("Exhausted charges cannot activate or become negative"),C->GetChargeView().Charges,0);
								TestEqual(TEXT("Exhausted request cannot create another activation"),FHALSideShiftTestAccess::Visual(*C).Activation,Previous);
								const float Partial = ASC->GetUltimateEnergyView().Energy;
								TestTrue(TEXT("A partially charged cell retains its points"), Partial > 0 && Partial < HALUltimate::SideShiftCost);
								const float Missing = HALUltimate::SideShiftCost - Partial;
								TestTrue(TEXT("A pickup completes exactly the partial cell"), ASC->GrantUltimateEnergy(Missing) > 0);
								TestEqual(TEXT("Cell display reflects the same GAS pool"), C->GetChargeView().Charges, 1);
								C->Request(1);
								TestEqual(TEXT("Completed cell is immediately spendable"), C->GetChargeView().Charges, 0);
								ASC->SetNumericAttributeBase(UHALHealthAttributeSet::GetCurrentHPAttribute(),0);
								TestFalse(TEXT("Defeat clears resource and configuration"),C->GetChargeView().bConfigured);
								TestEqual(TEXT("Defeat retires recharge timer"),C->GetChargeView().NextRechargeServerTime,-1.);
							}));
						}));
					}));
				}));
			}));
		}));
	}));
	return true;
}
#endif
