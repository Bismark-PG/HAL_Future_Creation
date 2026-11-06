#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraComponent.h"
#include "../HALElectricWallGimmick.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALShockedPresentationComponent.h"
#include "../HALHealthAttributeSet.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../TestVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
    bool Driving=false,Stopped=false;
    bool SawEarlyShockVisual=false;
    bool WasShockVisualActive=false;
    int32 ShockVisualStarts=0;
    bool SawObservedBounceVisual=false, MissedObservedBounceVisual=false;
    bool Prepared=false,ReadySent=false,Aligned=false,Started=false,SawEarly=false,SawPredictedVelocity=false;
    double Deadline=FPlatformTime::Seconds()+100, StartAt=-1, Complete=-1;
    FString Diagnostic;
    double TraceTime=-1,TraceX=0;
    float TraceVX=0,MaxPositionJump=0,MaxReacceleration=0,ForwardCorrection=0,PeakIncomingSpeed=0;
    bool TraceReversed=false;
    int32 ExtraReverseStarts=0;
    TArray<FString> TraceLines { TEXT("time,x,vx,sequence,active,elapsed,predictions,stun") };
public:
    FRun(FAutomationTestBase* T,bool R,bool D=false):Test(T),Reject(R),Driving(D) {}
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
            for (auto* V:{Own,Other}) { auto* B=CastChecked<UPrimitiveComponent>(V->GetRootComponent()); B->SetEnableGravity(Driving); B->SetLinearDamping(0); }
            if (Driving)
            {
                auto* FloorActor=World->SpawnActor<AActor>();
                auto* Floor=NewObject<UStaticMeshComponent>(FloorActor);
                FloorActor->AddInstanceComponent(Floor); FloorActor->SetRootComponent(Floor);
                Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
                Floor->SetCollisionProfileName(TEXT("BlockAll"));
                Floor->SetWorldLocation(FVector(0,0,9950)); Floor->SetWorldScale3D(FVector(200,200,1));
                Floor->RegisterComponent();
            }
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
                B->SetWorldLocationAndRotation(FVector(Driving ? -6000 : -600,Index++ ? 500 : -500,Driving ? 10000+B->Bounds.BoxExtent.Z+10 : 10300),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
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
        if (!Host && Wall && !Aligned && Own->GetActorLocation().Z>(Driving ? 10000 : 10200) && Physics.bReady)
        { Aligned=true; PC->SetName(TEXT("HAL_PredAligned")); }
        if (Host && Aligned && StartAt<0 && OtherName==TEXT("HAL_PredAligned"))
        {
            StartAt=World->GetTimeSeconds()+3;
            PC->SetName(FString::Printf(TEXT("HAL_PredAt_%.6f"),StartAt));
            FPacketSimulationSettings Settings; Settings.PktLag=150; Settings.PktLagVariance=40; Settings.PktLoss=10;
            World->GetNetDriver()->SetPacketSimulationSettings(Settings);
        }
        if (!Host && StartAt<0 && OtherName.StartsWith(TEXT("HAL_PredAt_"))) { StartAt=FCString::Atod(*OtherName.RightChop(11)); }
        if (!Wall || StartAt<0)
        {
            Diagnostic=FString::Printf(TEXT("Setup Host=%d Ready=%d Aligned=%d OwnZ=%.1f Wall=%d StartAt=%.2f Other=%s"),
                Host,Physics.bReady,Aligned,Own->GetActorLocation().Z,Wall!=nullptr,StartAt,*OtherName);
            return false;
        }
        const double Now=Match->GetServerWorldTimeSeconds();
        if (Aligned && !Started && Now>=StartAt-(!Driving && !Host ? .2 : 0))
        {
            Movement->ResetInputCommand();
            if (Driving)
            {
                FVehicleInputCmd Cmd; Cmd.Throttle=1; Movement->SetInputCommand(Cmd);
            }
            else if (!Reject)
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
        if (Driving && Started && !Stopped && (Body->GetPhysicsLinearVelocity().X<-1 || ASC->IsStunned()))
        { Movement->ResetInputCommand(); Stopped=true; }
        FVehicleNetStateData State;
        Movement->CaptureNetState(State);
        if (!Host && Started)
        {
            const double T=World->GetTimeSeconds(), X=Body->GetComponentLocation().X;
            const float VX=Body->GetPhysicsLinearVelocity().X;
            const double Dt=T-TraceTime;
            if (!TraceReversed) { PeakIncomingSpeed=FMath::Max(PeakIncomingSpeed,VX); }
            if (TraceReversed && TraceTime>=0 && Dt>0 && Dt<.1)
            {
                if (VX<=1 && TraceVX<=1) { ForwardCorrection+=FMath::Max(0.f,float(X-TraceX)); }
                MaxPositionJump=FMath::Max(MaxPositionJump,float(FMath::Abs(X-TraceX-.5*(VX+TraceVX)*Dt)));
                if (VX<-1 && TraceVX<-1) { MaxReacceleration=FMath::Max(MaxReacceleration,TraceVX-VX); }
                if (TraceVX>=-1 && VX<-100) { ++ExtraReverseStarts; }
            }
            TraceLines.Add(FString::Printf(TEXT("%.6f,%.3f,%.3f,%u,%d,%.6f,%d,%d"),T,X,VX,State.BounceBack.Sequence,
                State.BounceBack.bActive,State.BounceBack.Elapsed,Movement->GetPredictedBounceCount(),ASC->IsStunned()));
            TraceReversed |= VX<-1;
            TraceTime=T; TraceX=X; TraceVX=VX;
        }
        auto* ShockVisual=Own->FindComponentByClass<UHALShockedPresentationComponent>();
        if (!Host && Started && ShockVisual)
        {
            const bool bActive=ShockVisual->IsShocked();
            if (bActive && !WasShockVisualActive) { ++ShockVisualStarts; }
            WasShockVisualActive=bActive;
        }
        if (!Host && Started && Other->IsReplicatedElectricBounceActive())
        {
            const auto* ObservedVisual=Other->FindComponentByClass<UHALShockedPresentationComponent>();
            const bool bVisible=ObservedVisual && ObservedVisual->IsShocked();
            SawObservedBounceVisual |= bVisible;
            MissedObservedBounceVisual |= !bVisible;
        }
        if (!Host && Started && Movement->GetPredictedBounceCount()>0 && Body->GetPhysicsLinearVelocity().X < -1)
        {
            SawPredictedVelocity=true;
            SawEarly |= !ASC->IsStunned();
            SawEarlyShockVisual |= State.BounceBack.bActive && !ASC->IsStunned() && ShockVisual && ShockVisual->IsShocked();
        }
        Diagnostic=FString::Printf(TEXT("Host=%d Reject=%d Start=%d Count=%d Early=%d Velocity=%d Contacts=%u Seq=%u Active=%d Ack=%d PredFrame=%d Now=%.2f StartAt=%.2f"),
            Host,Reject,Started,Movement->GetPredictedBounceCount(),SawEarly,SawPredictedVelocity,Wall->GetAcceptedContactCount(),State.BounceBack.Sequence,State.BounceBack.bActive,
            Movement->GetBounceAuthorityFrame(),Movement->GetLastPredictedBounceFrame(),Now,StartAt);
        if (!Started || Now<StartAt+(Driving ? 5 : 2.5) || ASC->IsStunned() || Remote->IsStunned() || State.BounceBack.bActive) { return false; }
        if (Wall->GetAcceptedContactCount()!=uint32(Reject ? 0 : 2)) { return false; }
        if (Host && OtherName!=TEXT("HAL_PredDone")) { return false; }
        if (!Host)
        {
            if (!SawPredictedVelocity || !SawEarly || State.BounceBack.Sequence!=uint32(Reject ? 0 : 1)) { return false; }
            if (!Reject)
            {
                Test->TestTrue(TEXT("Observed bounce snapshot activates shock visual"),SawObservedBounceVisual);
                Test->TestFalse(TEXT("Observed bounce snapshot has no missing shock visual frame"),MissedObservedBounceVisual);
            }
            if (Driving)
            {
                Test->TestTrue(TEXT("Actual throttle input reaches meaningful collision speed"),PeakIncomingSpeed>1000);
                Test->TestEqual(TEXT("No second reverse pulse after initial bounce"),ExtraReverseStarts,0);
                Test->TestTrue(TEXT("No sudden renewed backward acceleration"),MaxReacceleration<100);
                Test->TestTrue(TEXT("No large single-frame pose correction"),MaxPositionJump<20);
                Test->TestTrue(TEXT("No large pull back toward the wall after rebound"),ForwardCorrection<20);
            }
            Test->TestTrue(TEXT("Shock visual starts during predicted rebound before authority stun"),SawEarlyShockVisual);
            Test->TestEqual(TEXT("One contact plays one shock visual episode"),ShockVisualStarts,1);
            Test->TestTrue(TEXT("Shock visual clears when rebound and stun have ended"),ShockVisual && !ShockVisual->IsShocked());
            Test->TestEqual(TEXT("Exactly one speculative local contact"),Movement->GetPredictedBounceCount(),1);
            Test->TestTrue(TEXT("Local reverse velocity precedes authoritative stun"),SawEarly);
            Test->TestTrue(TEXT("Owning history restores the authority bounce state"),Movement->GetBounceAuthorityFrame()>=Movement->GetLastPredictedBounceFrame());
            PC->SetName(TEXT("HAL_PredDone"));
        }
        if (FParse::Param(FCommandLine::Get(),TEXT("HALVerifyWallStrands")))
        {
            // Arc layers are authored on the Blueprint, one NiagaraComponent per layer.
            // Layers share the same local endpoints, so separation lives in the component transform.
            bool bFoundSeparatedStrands=false;
            for (TActorIterator<AHALElectricWallGimmick> It(World); It; ++It)
            {
                TInlineComponentArray<UNiagaraComponent*> Components(*It);
                TSet<FVector> WorldStarts;
                int32 StrandCount=0;
                for (const auto* Strand : Components)
                {
                    bool bStartValid=false, bEndValid=false;
                    const FVector Start=Strand->GetVariablePosition(TEXT("User.BeamStart"),bStartValid);
                    const FVector End=Strand->GetVariablePosition(TEXT("User.BeamEnd"),bEndValid);
                    if (bStartValid && bEndValid && !Start.Equals(End))
                    {
                        ++StrandCount;
                        WorldStarts.Add(Strand->GetComponentTransform().TransformPosition(Start));
                    }
                }
                bFoundSeparatedStrands |= StrandCount>=3 && WorldStarts.Num()==StrandCount;
            }
            Test->TestTrue(TEXT("Placed electric wall has separated Niagara paths"),bFoundSeparatedStrands);
        }
        Test->TestEqual(TEXT("No HP damage"),ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()),100.f);
        Test->TestEqual(TEXT("Authority determines final bounce sequence"),State.BounceBack.Sequence,uint32(Reject ? 0 : 1));
        UE_LOG(LogTemp,Display,TEXT("HAL ElectricPrediction: Complete=1 %s JumpCm=%.3f Reacceleration=%.3f ExtraReverseStarts=%d ForwardCorrection=%.3f Incoming=%.3f"),
            *Diagnostic,MaxPositionJump,MaxReacceleration,ExtraReverseStarts,ForwardCorrection,PeakIncomingSpeed);
        if (!Host)
        {
            const FString Path=FPaths::ProjectSavedDir()/TEXT("ElectricWall")/FString::Printf(TEXT("Motion_%s_%s.csv"),
                Driving ? TEXT("Driving") : Reject ? TEXT("Reject") : TEXT("Accept"),*FGuid::NewGuid().ToString());
            FFileHelper::SaveStringArrayToFile(TraceLines,*Path);
            UE_LOG(LogTemp,Display,TEXT("HAL ElectricPrediction trace: %s"),*Path);
        }
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricDrivingContinuityTest,"HAL.NetworkSmoke.ElectricPrediction.DrivingContinuity",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricDrivingContinuityTest::RunTest(const FString& Parameters)
{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALElectricPredictionTests::FRun>(this,false,true)); return true; }
#endif
