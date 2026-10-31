#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../HALElectricWallGimmick.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALShockedPresentationComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

namespace HALElectricPredictionTests
{
class FRun : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    bool Reject;
    bool Prepared=false,ReadySent=false,Aligned=false,Started=false,SawEarly=false,SawPredictedVelocity=false;
    double Deadline=FPlatformTime::Seconds()+100, StartAt=-1, Complete=-1;
    FString Diagnostic;
public:
    FRun(FAutomationTestBase* T,bool R):Test(T),Reject(R) {}
    bool Update() override
    {
        if (Complete>0) { return FPlatformTime::Seconds()-Complete>4; }
        if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Electric prediction timeout: ")+Diagnostic); return true; }
        UWorld* World=nullptr;
        for (const auto& C:GEngine->GetWorldContexts())
        { if (auto* W=C.World(); W && W->IsGameWorld() && (W->GetNetMode()==NM_Client || W->GetNetMode()==NM_ListenServer)) { World=W; break; } }
        if (!World) { return false; }
        const bool Host=World->GetNetMode()==NM_ListenServer;
        AHALPlayerController* PC=nullptr;
        for (auto I=World->GetPlayerControllerIterator();I;++I)
        { if (auto* C=Cast<AHALPlayerController>(I->Get()); C && C->IsLocalController()) { PC=C; break; } }
        auto* Own=PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
        ATestVehiclePawn* Other=nullptr;
        for (TActorIterator<ATestVehiclePawn> I(World);I;++I) { if (*I!=Own && I->GetPlayerState()) { Other=*I; break; } }
        auto* Match=World->GetGameState<AHALMatchGameState>();
        auto* ASC=Own ? Cast<UHALAbilitySystemComponent>(Own->GetAbilitySystemComponent()) : nullptr;
        auto* Remote=Other ? Cast<UHALAbilitySystemComponent>(Other->GetAbilitySystemComponent()) : nullptr;
        if (!Own || !Other || !Match || !ASC || !Remote || !ASC->IsHealthInitialized() || !Remote->IsHealthInitialized()) { return false; }
        auto* Movement=Own->FindComponentByClass<UArcadeVehicleMovementComponent>();
        auto* Body=CastChecked<UPrimitiveComponent>(Own->GetRootComponent());
        const auto Physics=Movement->GetMovementEffectStatus();
        if (!Prepared)
        {
            for (auto* V:{Own,Other}) { auto* B=CastChecked<UPrimitiveComponent>(V->GetRootComponent()); B->SetEnableGravity(false); B->SetLinearDamping(0); }
            FVehicleInputCmd Cmd; Cmd.Throttle=.1f; Movement->SetInputCommand(Cmd); Prepared=true;
        }
        if (Host && Match->GetMatchPhase()!=EHALMatchPhase::Playing) { PC->HALStartMatch(); return false; }
        if (!Host && Physics.bReady && !ReadySent) { PC->SetName(TEXT("HAL_PredReady")); ReadySent=true; }
        AHALElectricWallGimmick* Wall=nullptr;
        for (TActorIterator<AHALElectricWallGimmick> I(World);I;++I)
        { if (I->GetClass()==AHALElectricWallGimmick::StaticClass() && I->GetActorLocation().Equals(FVector(0,0,10000),1)) { Wall=*I; break; } }
        const FString OtherName=Other->GetPlayerState()->GetPlayerName();
        if (Host && !Aligned && Physics.bReady && OtherName==TEXT("HAL_PredReady"))
        {
            int32 Index=0;
            for (auto* V:{Own,Other})
            {
                auto* B=CastChecked<UPrimitiveComponent>(V->GetRootComponent());
                B->SetWorldLocationAndRotation(FVector(-600,Index++ ? 500 : -500,10300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
                B->SetPhysicsLinearVelocity(FVector::ZeroVector); B->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
                V->ForceNetUpdate();
            }
            FHALElectricWallConfig Config; Config.PillarSpacing=2400; Config.Height=600; Config.ScanInterval=.05f;
            const FTransform Pose(FRotator(0,90,0),FVector(0,0,10000));
            Wall=World->SpawnActorDeferred<AHALElectricWallGimmick>(AHALElectricWallGimmick::StaticClass(),Pose);
            if (!Wall || !Wall->Configure(Config)) { Test->AddError(TEXT("Prediction fixture configuration failed")); return true; }
            Wall->FinishSpawning(Pose); Wall->ForceNetUpdate();
            Aligned=true;
        }
        if (!Host && Wall && !Aligned && Own->GetActorLocation().Z>10200 && Physics.bReady)
        { Aligned=true; PC->SetName(TEXT("HAL_PredAligned")); }
        if (Host && Aligned && StartAt<0 && OtherName==TEXT("HAL_PredAligned"))
        {
            StartAt=World->GetTimeSeconds()+3;
            PC->SetName(FString::Printf(TEXT("HAL_PredAt_%.6f"),StartAt));
            FPacketSimulationSettings Settings; Settings.PktLag=150; Settings.PktLagVariance=40; Settings.PktLoss=10;
            World->GetNetDriver()->SetPacketSimulationSettings(Settings);
        }
        if (!Host && StartAt<0 && OtherName.StartsWith(TEXT("HAL_PredAt_"))) { StartAt=FCString::Atod(*OtherName.RightChop(11)); }
        if (!Wall || StartAt<0) { return false; }
        const double Now=Match->GetServerWorldTimeSeconds();
        if (Aligned && !Started && Now>=StartAt-(Host ? 0 : .2))
        {
            Movement->ResetInputCommand();
            if (!Reject)
            {
                // Give autonomous simulation a controlled lead, as input prediction does in play.
                Body->SetPhysicsLinearVelocity(FVector(4000,0,0));
                if (Host) { CastChecked<UPrimitiveComponent>(Other->GetRootComponent())->SetPhysicsLinearVelocity(FVector(4000,0,0)); }
            }
            else if (!Host)
            {
                if (!Movement->PredictBounceBack(FVector(-1,0,0),100,.3f,Wall->GetPhysicsSourceId())) { Test->AddError(TEXT("Speculative rejection fixture did not enqueue")); return true; }
            }
            Started=true;
        }
        FVehicleNetStateData State;
        Movement->CaptureNetState(State);
        if (!Host && Started && Movement->GetPredictedBounceCount()>0 && Body->GetPhysicsLinearVelocity().X < -1)
        { SawPredictedVelocity=true; SawEarly |= !ASC->IsStunned(); }
        Diagnostic=FString::Printf(TEXT("Host=%d Reject=%d Start=%d Count=%d Early=%d Velocity=%d Contacts=%u Seq=%u Active=%d Ack=%d PredFrame=%d Now=%.2f StartAt=%.2f"),
            Host,Reject,Started,Movement->GetPredictedBounceCount(),SawEarly,SawPredictedVelocity,Wall->GetAcceptedContactCount(),State.BounceBack.Sequence,State.BounceBack.bActive,
            Movement->GetBounceAuthorityFrame(),Movement->GetLastPredictedBounceFrame(),Now,StartAt);
        if (!Started || Now<StartAt+2.5 || ASC->IsStunned() || Remote->IsStunned() || State.BounceBack.bActive) { return false; }
        if (Wall->GetAcceptedContactCount()!=uint32(Reject ? 0 : 2)) { return false; }
        if (Host && OtherName!=TEXT("HAL_PredDone")) { return false; }
        if (!Host)
        {
            if (!SawPredictedVelocity || !SawEarly || State.BounceBack.Sequence!=uint32(Reject ? 0 : 1)) { return false; }
            Test->TestEqual(TEXT("Exactly one speculative local contact"),Movement->GetPredictedBounceCount(),1);
            Test->TestTrue(TEXT("Local reverse velocity precedes authoritative stun"),SawEarly);
            Test->TestTrue(TEXT("Owning history restores the authority bounce state"),Movement->GetBounceAuthorityFrame()>=Movement->GetLastPredictedBounceFrame());
            PC->SetName(TEXT("HAL_PredDone"));
        }
        Test->TestEqual(TEXT("No HP damage"),ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()),100.f);
        Test->TestEqual(TEXT("Authority determines final bounce sequence"),State.BounceBack.Sequence,uint32(Reject ? 0 : 1));
        UE_LOG(LogTemp,Display,TEXT("HAL ElectricPrediction: Complete=1 %s"),*Diagnostic);
        Complete=FPlatformTime::Seconds();
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricPredictionAcceptTest,"HAL.NetworkSmoke.ElectricPrediction.Accept",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricPredictionAcceptTest::RunTest(const FString& Parameters)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALElectricPredictionTests::FRun>(this,false)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricPredictionRejectTest,"HAL.NetworkSmoke.ElectricPrediction.Reject",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricPredictionRejectTest::RunTest(const FString& Parameters)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALElectricPredictionTests::FRun>(this,true)); return true; }
#endif
