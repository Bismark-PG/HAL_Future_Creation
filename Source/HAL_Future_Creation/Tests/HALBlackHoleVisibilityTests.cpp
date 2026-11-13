#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALBlackHole.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/LineBatchComponent.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UObject/UnrealType.h"

namespace
{
	bool bSawSavedField = false, bSawSavedFieldExpire = false;
	struct FBlackHoleEarlyObserver
	{
		FBlackHoleEarlyObserver()
		{
			FWorldDelegates::OnWorldPostActorTick.AddLambda([](UWorld* World, ELevelTick, float)
			{
				if (!FParse::Param(FCommandLine::Get(),TEXT("HALBlackHoleVisibilityFixture")) || !World->IsGameWorld()) { return; }
				int32 Count = 0;
				for (TActorIterator<AHALBlackHole> It(World); It; ++It)
				{
					++Count;
					if (!bSawSavedField)
					{
						bSawSavedField = true;
						UE_LOG(LogTemp,Display,TEXT("BlackHole early observer: loaded and active Time=%.3f Name=%s Stage=%d Radius=%.1f Location=%s"),
							World->GetTimeSeconds(), *It->GetName(), int32(It->GetStage()), It->GetCurrentRadius(), *It->GetActorLocation().ToString());
					}
				}
				if (bSawSavedField && Count == 0 && !bSawSavedFieldExpire)
				{
					bSawSavedFieldExpire = true;
					UE_LOG(LogTemp,Display,TEXT("BlackHole early observer: field removed at Time=%.3f before/while automation starts."),World->GetTimeSeconds());
				}
			});
		}
	} BlackHoleEarlyObserver;
	class FBlackHoleVisibilityProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase& Test;
		double Deadline = FPlatformTime::Seconds() + 30;
	public:
		explicit FBlackHoleVisibilityProbe(FAutomationTestBase& InTest) : Test(InTest) {}
		bool Update() override
		{
			if (bSawSavedFieldExpire) { Test.AddError(TEXT("Saved black hole loaded and expired before the visual capture; its BeginPlay lifetime elapsed during startup.")); return true; }
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* World = Context.World();
				if (!World || !World->IsGameWorld()) { continue; }
				for (TActorIterator<AHALBlackHole> It(World); It; ++It)
				{
					auto* Hole = *It;
					if (Hole->GetStage() == EHALBlackHoleStage::Ended) { continue; }
					auto* Batch = World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
					const auto* Debug = FindFProperty<FBoolProperty>(Hole->GetClass(), TEXT("bDebugDraw"));
					Test.AddInfo(FString::Printf(TEXT("Saved Match field %s Debug=%d Radius=%.1f Lines=%d Meshes=%d Location=%s"),
						*Hole->GetName(), Debug && Debug->GetPropertyValue_InContainer(Hole), Hole->GetCurrentRadius(), Batch->BatchedLines.Num(), Batch->BatchedMeshes.Num(), *Hole->GetActorLocation().ToString()));
					auto* Target = NewObject<UTextureRenderTarget2D>(World);
					Target->RenderTargetFormat = RTF_RGBA8; Target->InitAutoFormat(512,512); Target->UpdateResourceImmediate(true);
					auto* Capture = World->SpawnActor<ASceneCapture2D>();
					Capture->SetActorLocation(Hole->GetActorLocation()+FVector(0,0,1800));
					Capture->SetActorRotation(FRotator(-90,0,0));
					auto* Camera = Capture->GetCaptureComponent2D(); Camera->TextureTarget = Target;
					Camera->bCaptureEveryFrame = Camera->bCaptureOnMovement = false;
					Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
					Camera->ShowFlags.SetAntiAliasing(false);
					const auto Pixels = [&]() { Camera->CaptureScene(); TArray<FColor> Out; Target->GameThread_GetRenderTargetResource()->ReadPixels(Out); return Out; };
					Batch->SetVisibility(false); const auto Off = Pixels();
					Batch->SetVisibility(true); const auto On = Pixels();
					int32 Changed = 0;
					for (int32 Index = 0; Index < FMath::Min(Off.Num(),On.Num()); ++Index) { Changed += Off[Index] != On[Index]; }
					Test.AddInfo(FString::Printf(TEXT("Black hole rendered changed pixels=%d"), Changed));
					Test.TestTrue(TEXT("Saved game field actually renders debug pixels"), Changed > 100);
					TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(512,512,On,PNG);
					FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir()/TEXT("BlackHole/20261112/VisibilityDiagnosis/Game.png")));
					Capture->Destroy(); return true;
				}
			}
			if (FPlatformTime::Seconds() >= Deadline) { Test.AddError(TEXT("No active black hole found in saved game map before timeout.")); return true; }
			return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBlackHoleSavedVisibilityTest,"HAL.VisualSmoke.BlackHole.SavedMatchVisibility",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALBlackHoleSavedVisibilityTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBlackHoleVisibilityProbe>(*this)); return true;
}
#endif
