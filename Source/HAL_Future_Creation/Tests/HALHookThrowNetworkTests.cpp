#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "../HALHookThrow.h"
#include "../HALHookThrowAbility.h"
#include "../HALHookComponent.h"
#include "../HALMatchGameMode.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../HALAbilitySystemComponent.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../VehicleHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	struct FHookNetworkFixture
	{
		FHookNetworkFixture()
		{
			FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* W,const UWorld::InitializationValues&)
			{
				if(!W->IsGameWorld()||!FParse::Param(FCommandLine::Get(),TEXT("HALHookFixture"))) { return; }
				const auto* Saved=LoadObject<UHALHookThrowConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_HookThrow"));
				if(!Saved) { return; }
				auto* C=DuplicateObject<UHALHookThrowConfig>(Saved,GetTransientPackage()); C->CooldownSeconds=.8f;
				GetMutableDefault<UHALHookThrowAbility>()->SkillConfig=C;
				W->AddOnActorPreSpawnInitialization(FOnActorSpawned::FDelegate::CreateLambda([](AActor* A)
				{
					if(auto* M=Cast<AHALMatchGameMode>(A)) { M->DefaultPawnClass=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_05.BP_VehiclePawn_Car_05_C")); }
					if(auto* V=Cast<ATestVehiclePawn>(A);V&&V->GetVehicleDefinition()) { FHALVehicleAbilityTestAccess::Configure(V,UHALHookThrowAbility::StaticClass()); }
				}));
			});
		}
	} HookNetworkFixture;
	class FHookNetworkRun : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test; double Deadline=FPlatformTime::Seconds()+120,Done=-1,PlacedAt=-1,Attempt=-1,ChoiceAt=-1,ThrowAt=-1,InterruptedAt=-1,Swing2At=-1;
		bool Named=false,Floor=false,Placed=false,SawHook=false,SawTargetStun=false,SawOwnStun=false,SawThrow=false,Interrupted=false;
		bool SawSwing=false,SawDetach=false,CheckedContinuity=false,AcknowledgedThrow=false; FVector Attachment=FVector::ZeroVector,LastSwingVelocity=FVector::ZeroVector;
		int32 Stage=0; FGuid Choice,ObservedHook,FollowActivation; float InitialHP=-1,MinimumThrowY=0;
	public:
		explicit FHookNetworkRun(FAutomationTestBase* T):Test(T) {}
		bool Update() override
		{
			if(Done>=0) { return FPlatformTime::Seconds()-Done>3; }
			if(FPlatformTime::Seconds()>Deadline)
			{ Test->AddError(FString::Printf(TEXT("Hook network timeout stage=%d hook=%d capture=%d ownStun=%d throw=%d interrupt=%d"),Stage,SawHook,SawTargetStun,SawOwnStun,SawThrow,Interrupted)); return true; }
			UWorld* W=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(auto* X=C.World();X&&X->IsGameWorld()&&(X->GetNetMode()==NM_Client||X->GetNetMode()==NM_ListenServer)) { W=X; break; }
			if(!W) { return false; } const bool Host=W->GetNetMode()==NM_ListenServer;
			AHALPlayerController* PC=nullptr; for(auto It=W->GetPlayerControllerIterator();It;++It) if(auto* C=Cast<AHALPlayerController>(It->Get());C&&C->IsLocalController()) { PC=C; break; }
			auto* V=PC?Cast<ATestVehiclePawn>(PC->GetPawn()):nullptr; auto* Match=W->GetGameState<AHALMatchGameState>(); ATestVehiclePawn* Other=nullptr;
			for(TActorIterator<ATestVehiclePawn> It(W);It;++It) if(*It!=V&&It->GetPlayerState()) { Other=*It; break; }
			if(!V||!Other||!Match||Match->PlayerArray.Num()!=2||!V->GetAbilitySystemComponent()||!Other->GetAbilitySystemComponent()) { return false; }
			const double Now=Match->GetServerWorldTimeSeconds();
			if(!Named) { PC->SetName(Host?TEXT("HAL_Hook_ReadyH"):TEXT("HAL_Hook_ReadyC")); Named=true; }
			if(Host&&Match->GetMatchPhase()==EHALMatchPhase::WaitingForPlayers&&Other->GetPlayerState()->GetPlayerName().StartsWith(TEXT("HAL_Hook_"))) { PC->HALStartMatch(); }
			if(Match->GetMatchPhase()!=EHALMatchPhase::Playing) { return false; }
			if(!Floor)
			{
				auto* A=W->SpawnActor<AActor>(); auto* B=NewObject<UBoxComponent>(A); A->SetRootComponent(B); B->SetBoxExtent(FVector(4000,4000,50));
				B->SetWorldLocation(FVector(20000,0,-50)); B->SetCollisionObjectType(ECC_WorldStatic); B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				B->SetCollisionResponseToAllChannels(ECR_Block); B->RegisterComponent(); Floor=true;
			}
			auto* Source=Host?Other:V; auto* Target=Host?V:Other;
			auto* SourceASC=Cast<UHALAbilitySystemComponent>(Source->GetAbilitySystemComponent()); auto* TargetASC=Cast<UHALAbilitySystemComponent>(Target->GetAbilitySystemComponent());
			if(!SourceASC||!TargetASC||!SourceASC->IsHealthInitialized()) { return false; }
			if(InitialHP<0) { InitialHP=V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(); }
			const auto Place=[&]()
			{
				for(auto* P:{Source,Target})
				{
					P->SetActorLocationAndRotation(FVector(P==Source?20000:20750,0,155),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
					auto* B=CastChecked<UPrimitiveComponent>(P->GetRootComponent()); B->SetPhysicsLinearVelocity(FVector::ZeroVector); B->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); P->ForceNetUpdate();
				}
				PlacedAt=Now; Placed=true;
			};
			if(Host&&!Placed) { Place(); }
			auto* H=Source->FindComponentByClass<UHALHookComponent>()->GetOwnedHook();
			if(IsValid(H)&&ObservedHook!=H->GetActivation())
			{ ObservedHook=H->GetActivation(); Test->AddInfo(FString::Printf(TEXT("Hook network observed Host=%d Stage=%d Local=%d Phase=%d Activation=%s"),Host,Stage,Source->IsLocallyControlled(),int32(H->GetPhase()),*ObservedHook.ToString())); }
			if(TargetASC->IsStunned()&&!SawTargetStun)
			{
				int32 Actors=0; for(TActorIterator<AHALHookThrow> It(W);It;++It) { ++Actors; Test->AddInfo(FString::Printf(TEXT("Hook actor Host=%d Name=%s Owner=%s Phase=%d"),Host,*It->GetName(),*GetNameSafe(It->GetOwner()),int32(It->GetPhase()))); }
				Test->AddInfo(FString::Printf(TEXT("Hook capture observation Host=%d HookActors=%d Owned=%s ComponentReplicated=%d TargetY=%.2f"),Host,Actors,*GetNameSafe(H),Source->FindComponentByClass<UHALHookComponent>()->GetIsReplicated(),Target->GetActorLocation().Y));
			}
			SawHook|=IsValid(H); SawTargetStun|=TargetASC->IsStunned(); SawOwnStun|=SourceASC->IsStunned();
			if(IsValid(H)&&H->GetPhase()==EHALHookPhase::Swinging)
			{
				SawSwing=true;
				if(FollowActivation!=H->GetActivation()) { FollowActivation=H->GetActivation(); Attachment=Target->GetActorTransform().InverseTransformPosition(H->TipAt(Now)); }
				Test->TestTrue(TEXT("Hook remains connected to the moving attachment during swing"),FVector::Dist(H->TipAt(Now),Target->GetActorTransform().TransformPosition(Attachment))<.1);
				if(Host) { Test->TestTrue(TEXT("Authority retains Stun throughout connected swing"),TargetASC->IsStunned()); }
				if(Stage==1) { LastSwingVelocity=CastChecked<UPrimitiveComponent>(Target->GetRootComponent())->GetPhysicsLinearVelocity(); }
			}
			if(Stage==1&&SawSwing&&!TargetASC->IsStunned()&&(!IsValid(H)||H->GetPhase()==EHALHookPhase::Returning))
			{
				SawDetach=true;
				if(Host&&!CheckedContinuity)
				{
					const FVector VNow=CastChecked<UPrimitiveComponent>(Target->GetRootComponent())->GetPhysicsLinearVelocity();
					Test->TestTrue(TEXT("Detach keeps nonzero momentum without abrupt planar braking"),LastSwingVelocity.Size2D()>600&&VNow.Size2D()>600&&FVector::Dist2D(VNow,LastSwingVelocity)<400);
					Test->AddInfo(FString::Printf(TEXT("Hook exit continuity before=%s after=%s"),*LastSwingVelocity.ToString(),*VNow.ToString())); CheckedContinuity=true;
				}
			}
			if(SawTargetStun) { MinimumThrowY=FMath::Min(MinimumThrowY,float(Target->GetActorLocation().Y)); }
			if(SawDetach&&Target->GetActorLocation().Y<-80) { SawThrow=true; if(ThrowAt<0) { ThrowAt=Now; } }
			if(Host)
			{
				if(Stage==0&&Now-PlacedAt>2&&AHALHookThrow::CanStart(*Source))
				{ if(FParse::Param(FCommandLine::Get(),TEXT("HALHookNetTrace"))) { GEngine->Exec(W,TEXT("Log LogNetTraffic VeryVerbose")); } PC->SetName(TEXT("HAL_Hook_Round1")); Stage=1; }
				if(Stage==1&&SawTargetStun&&!IsValid(H)&&FParse::Param(FCommandLine::Get(),TEXT("HALHookNetTrace"))) { GEngine->Exec(W,TEXT("Log LogNetTraffic Warning")); }
				if(Stage==1&&SawThrow&&!IsValid(H)&&Now-ThrowAt>1&&Other->GetPlayerState()->GetPlayerName()==TEXT("HAL_Hook_ThrownC"))
				{ Place(); PC->SetName(TEXT("HAL_Hook_Round2")); Stage=2; }
				if(Stage==2&&IsValid(H)&&H->GetPhase()==EHALHookPhase::Swinging&&Swing2At<0) { Swing2At=Now; }
				if(Stage==2&&IsValid(H)&&H->GetPhase()==EHALHookPhase::Swinging&&Swing2At>=0&&Now-Swing2At>.1&&!Interrupted)
				{
					SourceASC->ApplyStun(.6f,Target); Interrupted=H->GetPhase()==EHALHookPhase::Interrupted; InterruptedAt=Now;
					Test->TestFalse(TEXT("Server interruption releases target immediately"),TargetASC->IsStunned());
				}
				if(Stage==2&&Interrupted&&!IsValid(H)&&Now-InterruptedAt>1&&!SourceASC->IsStunned()) { PC->SetName(TEXT("HAL_Hook_DoneH")); Stage=3; }
				if(Stage==3&&Other->GetPlayerState()->GetPlayerName()==TEXT("HAL_Hook_DoneC"))
				{
					Test->AddInfo(FString::Printf(TEXT("Hook authority target minimum lateral position=%.2f cm"),MinimumThrowY));
					Test->TestTrue(TEXT("Client direction RPC produced real authority sideways displacement"),SawHook&&SawTargetStun&&SawThrow);
					Test->TestTrue(TEXT("Connected swing and momentum-preserving detach observed"),SawSwing&&SawDetach&&CheckedContinuity);
					Test->TestTrue(TEXT("External Stun interrupted the second capture"),Interrupted&&SawOwnStun);
					Test->TestFalse(TEXT("Caster action lock cleaned"),SourceASC->HasMatchingGameplayTag(HALGameplayTags::State_HookCasting));
					Test->TestEqual(TEXT("No hook damage"),V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(),InitialHP); Done=FPlatformTime::Seconds();
				}
			}
			else
			{
				// Do not reset the target for round 2 until the observer acknowledges
				// the first detach. Property groups can arrive in different packets.
				if(Stage==1&&SawThrow&&!AcknowledgedThrow) { PC->SetName(TEXT("HAL_Hook_ThrownC")); AcknowledgedThrow=true; }
				const FString Name=Other->GetPlayerState()->GetPlayerName();
				const int32 Round=Name==TEXT("HAL_Hook_Round1")?1:Name==TEXT("HAL_Hook_Round2")?2:0;
				if(Round>Stage&&FMath::Abs(Source->GetActorLocation().X-20000)<100&&Now-Attempt>.5&&AHALHookThrow::CanStart(*Source))
				{ const bool Requested=Source->RequestPrimarySkill(); Test->AddInfo(FString::Printf(TEXT("Hook client request Round=%d Requested=%d"),Round,Requested)); if(Requested) { Stage=Round; } Attempt=Now; }
				if(IsValid(H)&&H->GetPhase()==EHALHookPhase::Choosing&&(Stage==1||Stage==2))
				{
					if(Choice!=H->GetActivation()) { Choice=H->GetActivation(); ChoiceAt=Now; Source->FindComponentByClass<UHALHookComponent>()->ObserveSteering(0); }
					if(Now-ChoiceAt>.25) { Source->FindComponentByClass<UHALHookComponent>()->ObserveSteering(Stage==1?-1:1); }
				}
				if(Name==TEXT("HAL_Hook_DoneH"))
				{
					Test->AddInfo(FString::Printf(TEXT("Hook client minimum lateral position=%.2f cm"),MinimumThrowY));
					Test->TestTrue(TEXT("Owner sees replicated hook"),SawHook);
					Test->TestTrue(TEXT("Observer sees captured target Stun"),SawTargetStun);
					Test->TestTrue(TEXT("Owner sees external interruption Stun"),SawOwnStun);
					Test->TestTrue(TEXT("Observer sees real directional displacement after capture"),SawThrow);
					Test->TestTrue(TEXT("Owner observes connected swing then detach"),SawSwing&&SawDetach);
					Test->TestFalse(TEXT("No stranded caster lock"),UHALHookComponent::BlocksActionsOn(Source));
					Test->TestFalse(TEXT("No stranded target Stun"),TargetASC->IsStunned());
					Test->TestEqual(TEXT("Client health unchanged"),V->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(),InitialHP);
					PC->SetName(TEXT("HAL_Hook_DoneC")); Done=FPlatformTime::Seconds();
				}
			}
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHookNetworkTest,"HAL.NetworkSmoke.HookThrow.ClientDirectionAuthorityThrowAndInterrupt",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHALHookNetworkTest::RunTest(const FString&)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHookNetworkRun>(this)); return true; }
#endif
