#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALSideShiftTestSupport.h"
#include "HALVehiclePresentationTestSupport.h"
#include "../TestVehiclePawn.h"
#include "../HALSideShiftAbility.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "NiagaraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	struct FSideShiftNetworkFixture
	{
		FSideShiftNetworkFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if(!W->IsGameWorld()||!FParse::Param(FCommandLine::Get(),TEXT("HALSideShiftFixture"))) { return; }
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* A)
				{
					if(auto* Mode=Cast<AHALMatchGameMode>(A)) { Mode->DefaultPawnClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_07.BP_VehiclePawn_Car_07_C")); }
				}));
			});
		}
	} SideShiftNetworkFixture;
	class FSideShiftLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; double Deadline=FPlatformTime::Seconds()+90, Done=-1,FirstAt=-1,SecondAt=-1;
		bool Announced=false, Armed=false, First=false, Second=false, SawOwn=false, SawOther=false, SawJets=false, SawMarks=false, SawFastTires=false, Finished=false, FloorReady=false, Placed=false;
		double SettledAt=-1;
		bool ReleaseChecked=false;
		bool CoreChecked=false,MovingCast=false;
		double DriveAt=-1,DriftAt=-1,MovingAt=-1;
		float MovingYaw=0;
		FVector MovingForward=FVector::ZeroVector,MovingSide=FVector::ZeroVector;
		FVector Base=FVector::ZeroVector, Direction=FVector::ZeroVector; float Peak=0, SecondYaw=0; double RechargeAt=-1;
		FVector SecondBase=FVector::ZeroVector, SecondForward=FVector::ZeroVector, SecondSide=FVector::ZeroVector;
		FGuid FirstActivation;
		double FirstServerStart = -1;
		bool SawTwoCharges = false;
	public:
		explicit FSideShiftLive(FAutomationTestBase* T):Test(T){}
		bool Update() override
		{
			if(Done>=0&&FPlatformTime::Seconds()-Done>3) { return true; }
			if(FPlatformTime::Seconds()>Deadline)
			{ Test->AddError(FString::Printf(TEXT("SideShift live timeout arm=%d first=%d second=%d own=%d other=%d jets=%d marks=%d peak=%.3f"),Armed,First,Second,SawOwn,SawOther,SawJets,SawMarks,Peak)); return true; }
			UWorld* W=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(auto* X=C.World();X&&X->IsGameWorld()&&(X->GetNetMode()==NM_Client||X->GetNetMode()==NM_ListenServer)) { W=X; break; }
			if(!W) { return false; } const bool Host=W->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr; for(auto It=W->GetPlayerControllerIterator();It;++It) if(auto* C=Cast<AHALPlayerController>(It->Get());C&&C->IsLocalController()) { PC=C; break; }
			auto* P=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr; auto* Match=W->GetGameState<AHALMatchGameState>();
			ATestVehiclePawn* Other=nullptr; for(TActorIterator<ATestVehiclePawn> It(W);It;++It) if(*It!=P&&It->GetPlayerState()) { Other=*It; break; }
			if(!P||!Other||!Match||Match->PlayerArray.Num()!=2) { return false; }
			auto* C=P->FindComponentByClass<UHALSideShiftComponent>(); auto* Remote=Other->FindComponentByClass<UHALSideShiftComponent>();
			if(!C||!Remote||!C->GetChargeView().bConfigured) { return false; }
			const auto* Grant = P->GetVehicleAbilityConfig().Grants.FindByPredicate([](const FHALAbilityGrant& G)
			{ return G.InputTag == HALGameplayTags::Input_Ability_Primary; });
			const auto* Ability = Grant ? Cast<UHALSideShiftAbility>(Grant->Ability.GetDefaultObject()) : nullptr;
			if (!Ability || !Ability->SkillConfig) { Test->AddError(TEXT("Car07 must grant its saved side-shift ability")); return true; }
			const auto& Tuning = Ability->SkillConfig->Motion;
			if(Done>=0)
			{
				if(!ReleaseChecked&&FPlatformTime::Seconds()-Done>1)
				{
					const auto Motion=P->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetMovementEffectStatus().Definition.SideShift;
					Test->TestTrue(TEXT("Releasing after the skill window reaches physics history on each endpoint"),Motion.bDriftAtStart&&Motion.bDriftReleased);
					ReleaseChecked=true;
				}
				return false;
			}
			const FString Name=Other->GetPlayerState()->GetPlayerName(); const double Now=Match->GetServerWorldTimeSeconds();
			if(!Announced) { PC->SetName(Host?TEXT("HAL_Shift_ReadyH"):TEXT("HAL_Shift_ReadyC")); Announced=true; }
			if(Host&&Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers&&Name.StartsWith(TEXT("HAL_Shift_"))) { PC->HALStartMatch(); }
			if(Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			// One-time fixture setup, far from map gimmicks. Neither endpoint moves a pawn during the measured skill.
			if(!FloorReady)
			{
				auto* Floor=W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Box);
				Box->SetBoxExtent(FVector(4000,4000,50)); Box->SetWorldLocation(FVector(20000,0,-50));
				Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); FloorReady=true;
			}
			if(Host&&!Placed)
			{
				for(auto* Vehicle:{P,Other})
				{
					Vehicle->SetActorLocationAndRotation(FVector(20000,Vehicle==P?-1500:1500,155),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
					auto* Body=CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent()); Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Vehicle->ForceNetUpdate();
				}
				Placed=true;
			}
			if(!First&&(FMath::Abs(P->GetActorLocation().X-20000)>500||FMath::Abs(Other->GetActorLocation().X-20000)>500)) { return false; }
			if(SettledAt<0) { SettledAt=Now; }
			if(Now-SettledAt<1.5) { return false; }
			const auto Pose=P->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetStabilityObservation();
			if(!Armed&&Pose.bValid&&Pose.bGrounded&&Pose.bDrivingAllowed&&Name.StartsWith(TEXT("HAL_Shift_")))
			{ Armed=true; PC->SetName(Host?TEXT("HAL_Shift_ArmedH"):TEXT("HAL_Shift_ArmedC")); }
			if(!First&&Armed&&((Host&&Name==TEXT("HAL_Shift_ArmedC"))||(!Host&&Name==TEXT("HAL_Shift_FirstH"))))
			{
				Test->TestEqual(TEXT("Each owner starts with four independent charges"),C->GetChargeView().Charges,4);
				Test->TestTrue(TEXT("Production vehicle retains its CCD requirement"),CastChecked<UPrimitiveComponent>(P->GetRootComponent())->GetBodyInstance()->bUseCCD);
				Base=P->GetActorLocation(); const float Side=Host?-1.f:1.f; Direction=P->GetActorRightVector().GetSafeNormal2D()*Side;
				FHALSideShiftTestAccess::SetInput(*P,Side,false);
				Test->TestTrue(TEXT("Normal Primary input dispatches directional skill"),P->RequestPrimarySkill());
				P->RequestPrimarySkill(); // Same translation rejects an additional request/cost.
				First=true; FirstAt=Now; PC->SetName(Host?TEXT("HAL_Shift_FirstH"):TEXT("HAL_Shift_FirstC"));
			}
			const auto& V=FHALSideShiftTestAccess::Visual(*C); const auto& R=FHALSideShiftTestAccess::Visual(*Remote);
			if(First)
			{
				SawOwn|=V.Activation.IsValid()&&V.bDriving; SawOther|=R.Activation.IsValid()&&R.bDriving;
				if(V.Activation.IsValid()&&!FirstActivation.IsValid()) { FirstActivation=V.Activation; FirstServerStart=V.StartServerTime; }
				if(FirstActivation.IsValid()&&RechargeAt<0) { RechargeAt=C->GetChargeView().NextRechargeServerTime; }
				if(!Second) { Peak=FMath::Max(Peak,float(FVector::DotProduct(P->GetActorLocation()-Base,Direction))); }
				if(FHALSideShiftTestAccess::Jets(*C).Num()==2)
				{
					bool Mounted=true; for(const auto& Jet:FHALSideShiftTestAccess::Jets(*C)) { Mounted&=Jet&&Jet->IsActive()&&Jet->GetAttachParent(); }
					SawJets|=Mounted;
				}
				const auto Marks=P->FindComponentByClass<UHALVehiclePresentationComponent>()->GetPresentationFrame();
				SawMarks|=(Marks.TireSkidReasons&4)&&Marks.TireSkidWheelMask;
				for(int32 Slot=0;Slot<4;++Slot)
				if(const auto* Particle=FHALPresentationTestAccess::TireSkid(P->FindComponentByClass<UHALVehiclePresentationComponent>(),Slot);Particle&&Particle->IsActive())
				{ SawFastTires|=FMath::IsNearlyEqual(Particle->GetCustomTimeDilation(),V.FX.TireFXPlayRate)&&Particle->GetCustomTimeDilation()>3; }
			}
			if(First&&!Second&&Now-FirstAt>1.4&&SawOwn&&Peak>120)
			{
				Test->TestEqual(TEXT("Duplicate Primary request only spent once"),C->GetChargeView().Charges,3);
				SecondYaw=P->GetActorRotation().Yaw;
				const auto* Body=CastChecked<UPrimitiveComponent>(P->GetRootComponent());
				Test->AddInfo(FString::Printf(TEXT("DriftArc actual localCOM=%s"),*Body->GetComponentTransform().InverseTransformPosition(Body->GetCenterOfMass()).ToString()));
				SecondBase=P->GetActorLocation(); SecondForward=P->GetActorForwardVector().GetSafeNormal2D(); SecondSide=P->GetActorRightVector().GetSafeNormal2D()*(Host?1.f:-1.f);
				FHALSideShiftTestAccess::SetInput(*P,Host?1.f:-1.f,true); Test->TestTrue(TEXT("Second shift with drift dispatches"),P->RequestPrimarySkill());
				Second=true; SecondAt=Now;
			}
			if(Second&&!CoreChecked&&Now-SecondAt>.85) { FHALSideShiftTestAccess::SetInput(*P,0,true); }
			const auto Charges = C->GetChargeView();
			if (Second && Charges.Charges == 2) { SawTwoCharges = true; }
			// Measure against the authority's cast and recharge samples. Input and replication travel independently.
			if(Second&&!CoreChecked&&RechargeAt>=0&&Now>=RechargeAt&&(Charges.Charges==3||Now>RechargeAt+1.5))
			{
				Test->TestTrue(TEXT("Owner observed both payments before the first cell refilled"), SawTwoCharges);
				Test->TestEqual(TEXT("One new cell is replicated after its deadline"),Charges.Charges,3);
				Test->TestTrue(TEXT("First cell deadline starts at the accepted cast"), FMath::IsNearlyEqual(RechargeAt, FirstServerStart+Charges.RechargeSeconds, .12));
				Test->TestTrue(TEXT("Further spending retained original recharge cadence"), FMath::IsNearlyEqual(Charges.NextRechargeServerTime, RechargeAt+Charges.RechargeSeconds, .12));
				Test->TestTrue(TEXT("Other endpoint sees accepted effect phase"),SawOther);
				Test->TestTrue(TEXT("First shift reaches the saved straight-line distance"), FMath::IsNearlyEqual(Peak, Tuning.Distance, 40.f));
				Test->TestTrue(TEXT("Mounted spark and smoke layers actually created"),SawJets);
				Test->TestTrue(TEXT("Shared tire marks emitted from real wheel support"),SawMarks);
				Test->TestTrue(TEXT("Skill tire particles actually use accelerated per-component playback"),SawFastTires);
				Test->TestFalse(TEXT("Expired skill cannot leave drift/tire demand active"),C->IsSkidActive());
				const float Quarter=FMath::FindDeltaAngleDegrees(SecondYaw,P->GetActorRotation().Yaw)*(Host?1.f:-1.f);
				const FVector ArcTravel=P->GetActorLocation()-SecondBase;
				const float Lateral=FVector::DotProduct(ArcTravel,SecondSide), Forward=FVector::DotProduct(ArcTravel,SecondForward);
				Test->TestTrue(TEXT("Held drift turns with the input on each endpoint"),FMath::IsNearlyEqual(Quarter, Tuning.DriftTurnDegrees, 4.f));
				Test->TestTrue(TEXT("Enhanced drift reaches the saved lateral span"),FMath::IsNearlyEqual(Lateral, Tuning.DriftLateralSpan, 40.f));
				Test->TestTrue(TEXT("Enhanced drift reaches the saved forward depth"),FMath::IsNearlyEqual(Forward, Tuning.DriftLateralSpan*Tuning.DriftArcBend, 40.f));
				Test->TestTrue(TEXT("Held turn has no residual spinning"),CastChecked<UPrimitiveComponent>(P->GetRootComponent())->GetPhysicsAngularVelocityInRadians().Size()<.1);
				Test->AddInfo(FString::Printf(TEXT("DriftArc role=%s inputAlignedAngle=%.3f lateral=%.3f forward=%.3f"),Host?TEXT("Host"):TEXT("Client"),Quarter,Lateral,Forward));
				FHALSideShiftTestAccess::SetInput(*P,0,false);
				Test->AddInfo(FString::Printf(TEXT("SideShift role=%s actualPeak=%.3f charges=%d own=%d observer=%d mounted=%d tireMarks=%d Base=%s InitialDirection=%s"),Host?TEXT("Host"):TEXT("Client"),Peak,C->GetChargeView().Charges,SawOwn,SawOther,SawJets,SawMarks,*Base.ToString(),*Direction.ToString()));
				CoreChecked=true; DriveAt=Now; FHALSideShiftTestAccess::DriveInput(*P,0,false,1);
			}
			if(CoreChecked&&!MovingCast&&Now-DriveAt>.35)
			{
				const auto* Body=CastChecked<UPrimitiveComponent>(P->GetRootComponent());
				if(Body->GetPhysicsLinearVelocity().Size2D()>600)
				{
					if(DriftAt<0) { DriftAt=Now; FHALSideShiftTestAccess::DriveInput(*P,Host?1.f:-1.f,true,1); }
					else if(Now-DriftAt>.06)
					{
						MovingYaw=P->GetActorRotation().Yaw; MovingForward=P->GetActorForwardVector().GetSafeNormal2D(); MovingSide=P->GetActorRightVector().GetSafeNormal2D()*(Host?1.f:-1.f);
						Test->TestTrue(TEXT("An already moving and drifting owner can cast a full arc"),P->RequestPrimarySkill());
						MovingCast=true; MovingAt=Now;
						Test->AddInfo(FString::Printf(TEXT("MovingArc entry role=%s speed=%.3f yawRate=%.3f"),Host?TEXT("Host"):TEXT("Client"),Body->GetPhysicsLinearVelocity().Size2D(),Body->GetPhysicsAngularVelocityInRadians().Z));
					}
				}
			}
			if(MovingCast&&!Finished&&Now-MovingAt>.65)
			{
				const FVector Velocity=CastChecked<UPrimitiveComponent>(P->GetRootComponent())->GetPhysicsLinearVelocity();
				const float Turn=FMath::FindDeltaAngleDegrees(MovingYaw,P->GetActorRotation().Yaw)*(Host?1.f:-1.f);
				const float Alignment=FVector::DotProduct(Velocity.GetSafeNormal2D(),P->GetActorForwardVector().GetSafeNormal2D());
				Test->TestTrue(TEXT("Moving cast turns in the chosen direction despite cast/input latency"),Turn>Tuning.DriftTurnDegrees-25 && Turn<Tuning.DriftTurnDegrees+35);
				Test->TestTrue(TEXT("Moving cast returns to driving along its new heading"),Velocity.Size2D()>300&&Alignment>.85);
				Test->TestTrue(TEXT("Exit velocity is not restored to the old forward direction"),FVector::DotProduct(Velocity,MovingSide)>.5*Velocity.Size2D());
				Test->AddInfo(FString::Printf(TEXT("MovingArc exit role=%s turn=%.3f speed=%.3f headingDot=%.5f initialForwardSpeed=%.3f sideSpeed=%.3f"),Host?TEXT("Host"):TEXT("Client"),Turn,Velocity.Size2D(),Alignment,FVector::DotProduct(Velocity,MovingForward),FVector::DotProduct(Velocity,MovingSide)));
				FHALSideShiftTestAccess::DriveInput(*P,0,false,0);
				Finished=true; PC->SetName(Host?TEXT("HAL_Shift_DoneH"):TEXT("HAL_Shift_DoneC"));
			}
			if(Finished&&Name.StartsWith(TEXT("HAL_Shift_Done"))) { Done=FPlatformTime::Seconds(); }
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSideShiftNetworkTest,"HAL.NetworkSmoke.SideShift.ChargesPhysicalMotionAndFX",EAutomationTestFlags::ClientContext|EAutomationTestFlags::ServerContext|EAutomationTestFlags::EngineFilter)
bool FHALSideShiftNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSideShiftLive>(this)); return true; }
#endif
