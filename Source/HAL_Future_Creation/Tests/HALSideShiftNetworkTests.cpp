#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALSideShiftTestSupport.h"
#include "HALVehicleAbilityTestSupport.h"
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
				GetMutableDefault<UHALSideShiftAbility>()->SkillConfig=LoadObject<UHALSideShiftConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_Car07_SideShift"));
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* A)
				{
					if(auto* Mode=Cast<AHALMatchGameMode>(A)) { Mode->DefaultPawnClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_07.BP_VehiclePawn_Car_07_C")); }
					if(auto* P=Cast<ATestVehiclePawn>(A);P&&P->GetVehicleDefinition()) { FHALVehicleAbilityTestAccess::Configure(P,UHALSideShiftAbility::StaticClass()); }
				}));
			});
		}
	} SideShiftNetworkFixture;
	class FSideShiftLive : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; double Deadline=FPlatformTime::Seconds()+90, Done=-1,FirstAt=-1,SecondAt=-1;
		bool Announced=false, Armed=false, First=false, Second=false, SawOwn=false, SawOther=false, SawJets=false, SawMarks=false, Finished=false, FloorReady=false, Placed=false;
		double SettledAt=-1;
		FVector Base=FVector::ZeroVector, Direction=FVector::ZeroVector; float Peak=0; double RechargeAt=-1;
		FGuid FirstActivation;
	public:
		explicit FSideShiftLive(FAutomationTestBase* T):Test(T){}
		bool Update() override
		{
			if(Done>=0) { return FPlatformTime::Seconds()-Done>3; }
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
			if(FMath::Abs(P->GetActorLocation().X-20000)>500||FMath::Abs(Other->GetActorLocation().X-20000)>500) { return false; }
			if(SettledAt<0) { SettledAt=Now; }
			if(Now-SettledAt<1.5) { return false; }
			const auto Pose=P->FindComponentByClass<UArcadeVehicleMovementComponent>()->GetStabilityObservation();
			if(!Armed&&Pose.bValid&&Pose.bGrounded&&Pose.bDrivingAllowed&&Name.StartsWith(TEXT("HAL_Shift_")))
			{ Armed=true; PC->SetName(Host?TEXT("HAL_Shift_ArmedH"):TEXT("HAL_Shift_ArmedC")); }
			if(!First&&Armed&&((Host&&Name==TEXT("HAL_Shift_ArmedC"))||(!Host&&Name==TEXT("HAL_Shift_FirstH"))))
			{
				Test->TestEqual(TEXT("Each owner starts with four independent charges"),C->GetChargeView().Charges,4);
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
				if(V.Activation.IsValid()&&!FirstActivation.IsValid()) { FirstActivation=V.Activation; RechargeAt=C->GetChargeView().NextRechargeServerTime; }
				if(!Second) { Peak=FMath::Max(Peak,float(FVector::DotProduct(P->GetActorLocation()-Base,Direction))); }
				if(FHALSideShiftTestAccess::Jets(*C).Num()==2)
				{
					bool Mounted=true; for(const auto& Jet:FHALSideShiftTestAccess::Jets(*C)) { Mounted&=Jet&&Jet->IsActive()&&Jet->GetAttachParent(); }
					SawJets|=Mounted;
				}
				const auto Marks=P->FindComponentByClass<UHALVehiclePresentationComponent>()->GetPresentationFrame();
				SawMarks|=(Marks.TireSkidReasons&4)&&Marks.TireSkidWheelMask;
			}
			if(First&&!Second&&Now-FirstAt>1.4&&SawOwn&&Peak>120)
			{
				Test->TestEqual(TEXT("Duplicate Primary request only spent once"),C->GetChargeView().Charges,3);
				FHALSideShiftTestAccess::SetInput(*P,Host?1.f:-1.f,true); Test->TestTrue(TEXT("Second shift with drift dispatches"),P->RequestPrimarySkill());
				Second=true; SecondAt=Now;
			}
			if(Second&&Now-SecondAt>.85) { FHALSideShiftTestAccess::SetInput(*P,0,true); }
			if(Second&&Now-FirstAt>5.4&&!Finished)
			{
				Test->TestEqual(TEXT("One charge refills after original five-second clock"),C->GetChargeView().Charges,3);
				Test->TestTrue(TEXT("Further spending retained original recharge cadence"),C->GetChargeView().NextRechargeServerTime>=RechargeAt+4.9);
				Test->TestTrue(TEXT("Other endpoint sees accepted effect phase"),SawOther);
				Test->TestTrue(TEXT("Actual first shift on unobstructed ground is near configured 300cm"),Peak>=260&&Peak<=330);
				Test->TestTrue(TEXT("Mounted spark and smoke layers actually created"),SawJets);
				Test->TestTrue(TEXT("Shared tire marks emitted from real wheel support"),SawMarks);
				Test->TestFalse(TEXT("Expired skill cannot leave drift/tire demand active"),C->IsSkidActive());
				FHALSideShiftTestAccess::SetInput(*P,0,false);
				Test->AddInfo(FString::Printf(TEXT("SideShift role=%s actualPeak=%.3f charges=%d own=%d observer=%d mounted=%d tireMarks=%d Base=%s InitialDirection=%s"),Host?TEXT("Host"):TEXT("Client"),Peak,C->GetChargeView().Charges,SawOwn,SawOther,SawJets,SawMarks,*Base.ToString(),*Direction.ToString()));
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
