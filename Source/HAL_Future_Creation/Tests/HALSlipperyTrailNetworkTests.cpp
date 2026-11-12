#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALMatchGameState.h"
#include "../HALMovementAreaReceiver.h"
#include "../HALPlayerController.h"
#include "../HALSlipperyTrailAbility.h"
#include "../TestVehiclePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"

struct FHALSlipperyTrailNetworkAccess
{
	static bool HasReplicatedPatch(const AHALSlipperyTrail& Trail)
	{
		return Trail.StartTime >= 0 && Trail.Shape.InitialHalfWidth == 750 && Trail.Points.Num() == 1
			&& FMath::IsNearlyEqual(Trail.Points[0].Ground.Z, 10000., .2)
			&& FMath::IsNearlyEqual(Trail.Points[0].GroundNormal.Z, 1., .001)
			&& (!FParse::Param(FCommandLine::Get(), TEXT("HALSlipperyTrailDissolveFixture"))
				|| FMath::IsNearlyEqual(Trail.Presentation.DissolveSeconds, 1.2f))
			&& (!FParse::Param(FCommandLine::Get(), TEXT("HALSlipperyTrailPaintEdgeFixture"))
				|| (Trail.Presentation.PaintEdge.bEnabled && Trail.Presentation.PaintEdge.MaximumOutset == 12
					&& FMath::IsNearlyEqual(Trail.Presentation.PaintEdge.MaximumHalfWidthFraction, .06f)));
	}
	static UDynamicMeshComponent* GetGroundMesh(const AHALSlipperyTrail& Trail) { return Trail.GroundMesh; }
	static UMaterialInterface* GetGroundMaterial(const AHALSlipperyTrail& Trail) { return Trail.Presentation.GroundMaterial; }
	static float RenderHalfWidth(const AHALSlipperyTrail& Trail)
	{
		return Trail.Shape.MaximumHalfWidth + Trail.Presentation.PaintEdge.Padding(Trail.Shape.MaximumHalfWidth);
	}
	static float DissolveProgress(const AHALSlipperyTrail& Trail)
	{
		return Trail.Presentation.DissolveProgress(float(Trail.ServerTime() - Trail.StartTime), Trail.Shape.LifetimeSeconds);
	}
};

namespace
{
	class FSlipperyTrailProbe : public IAutomationLatentCommand
	{
	public:
		explicit FSlipperyTrailProbe(FAutomationTestBase& InTest) : Test(InTest) {}
		bool Update() override
		{
			if (CompleteAt > 0) { return FPlatformTime::Seconds() >= CompleteAt; }
			if (FPlatformTime::Seconds() >= Deadline)
			{
				Test.AddError(FString::Printf(TEXT("Slippery Trail smoke timed out at stage %d; requires paired NetTest processes with -HALSlipperyTrailFixture."), Stage));
				return true;
			}
			UWorld* World = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* Candidate = Context.World();
				if (Candidate && Candidate->IsGameWorld() && (Candidate->IsNetMode(NM_Client) || Candidate->IsNetMode(NM_ListenServer)))
				{ World = Candidate; break; }
			}
			if (!World) { return false; }
			const bool bHost = World->GetNetMode() == NM_ListenServer;
			auto* PC = Cast<AHALPlayerController>(World->GetFirstPlayerController());
			auto* Own = PC ? Cast<ATestVehiclePawn>(PC->GetPawn()) : nullptr;
			ATestVehiclePawn* Other = nullptr;
			for (TActorIterator<ATestVehiclePawn> It(World); It; ++It)
			{
				if (*It != Own && It->GetPlayerState()) { Other = *It; break; }
			}
			auto* Match = World->GetGameState<AHALMatchGameState>();
			if (!Own || !Other || !Own->GetPlayerState() || !Match) { return false; }
			auto* Source = bHost ? Own : Other;
			auto* Target = bHost ? Other : Own;
			auto* SourceASC = Cast<UHALAbilitySystemComponent>(Source->GetAbilitySystemComponent());
			auto* TargetASC = Cast<UHALAbilitySystemComponent>(Target->GetAbilitySystemComponent());
			if (!SourceASC || !TargetASC || !SourceASC->IsHealthInitialized() || !TargetASC->IsHealthInitialized()) { return false; }
			if (Stage == 0)
			{
				auto* Actor = World->SpawnActor<AActor>();
				auto* Floor = NewObject<UStaticMeshComponent>(Actor);
				Actor->AddInstanceComponent(Floor);
				Actor->SetRootComponent(Floor);
				Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				Floor->SetCollisionProfileName(TEXT("BlockAll"));
				Floor->SetWorldLocation(FVector(0, 0, 9950));
				Floor->SetWorldScale3D(FVector(40, 40, 1));
				Floor->RegisterComponent();
				if (!bHost) { PC->SetName(TEXT("HAL_Trail_Ready")); }
				Stage = 1;
			}
			const FString OtherName = Other->GetPlayerState()->GetPlayerName();
			if (Match->GetMatchPhase() != EHALMatchPhase::Playing)
			{
				if (bHost && OtherName == TEXT("HAL_Trail_Ready")) { PC->HALStartMatch(); }
				return false;
			}
			auto* Movement = Target->FindComponentByClass<UArcadeVehicleMovementComponent>();
			const auto Status = Movement->GetMovementEffectStatus();
			const bool bAffected = TargetASC->HasMatchingGameplayTag(HALGameplayTags::State_Movement_SlipperyArea);
			auto* TargetReceiver = Target->FindComponentByClass<UHALMovementAreaReceiver>();
			auto* SourceReceiver = Source->FindComponentByClass<UHALMovementAreaReceiver>();
			if (bHost && Stage == 1)
			{
				FPacketSimulationSettings Lag;
				Lag.PktLag = 80;
				World->GetNetDriver()->SetPacketSimulationSettings(Lag);
				for (auto* Vehicle : {Source, Target})
				{
					auto* Body = CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent());
					const auto Bounds = Body->CalcBounds(FTransform(Body->GetComponentScale()));
					Body->SetWorldLocationAndRotation(FVector(Vehicle == Source ? 0 : -520, 0, 10000 + Bounds.BoxExtent.Z - Bounds.Origin.Z + 2),
						FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
					Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
					Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
					Vehicle->ForceNetUpdate();
				}
				CastAfter = World->GetTimeSeconds() + 1.0;
				Stage = 2;
				return false;
			}
			if (bHost && Stage == 2 && World->GetTimeSeconds() >= CastAfter && AHALSlipperyTrail::CanLay(*Source) && Status.bReady)
			{
				if (!Test.TestTrue(TEXT("Host requests the configured skill through Primary"), Source->RequestPrimarySkill())) { return true; }
				Stage = 3;
			}
			TActorIterator<AHALSlipperyTrail> Trails(World);
			auto* Trail = Trails ? *Trails : nullptr;
			if (bCheckGround && Trail)
			{
				if (auto* Mesh = FHALSlipperyTrailNetworkAccess::GetGroundMesh(*Trail); Mesh && Mesh->IsRegistered() && Mesh->IsVisible())
				{
					if (!bSawGround)
					{
						bSawGround = true;
						GroundMesh = Mesh;
						if (bCheckDeformation)
						{
							auto* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
							Test.TestTrue(TEXT("Replicated deformation creates an area-local material with the authored height"),
								MID && MID->K2_GetScalarParameterValue(TEXT("HAL_GroundBulgeHeight")) == 8.f);
						}
						else
						{
							const auto* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
							Test.TestTrue(TEXT("Local ground mesh or dissolve MID uses the replicated material"),
								MID ? MID->Parent == FHALSlipperyTrailNetworkAccess::GetGroundMaterial(*Trail)
									: Mesh->GetMaterial(0) == FHALSlipperyTrailNetworkAccess::GetGroundMaterial(*Trail));
						}
						Test.TestEqual(TEXT("Ground rendering cannot affect queries or physics"),
							Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
						if (FParse::Param(FCommandLine::Get(), TEXT("HALSlipperyTrailPaintEdgeFixture")))
						{
							auto* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
							Test.TestTrue(TEXT("Paint mode is written to an area-local MID"),
								MID && MID->K2_GetScalarParameterValue(TEXT("HAL_GroundPaintEdgeEnabled")) == 1.f);
						}
					}
					bSawFullWidth |= FMath::IsNearlyEqual(Mesh->Bounds.BoxExtent.Y,
						FHALSlipperyTrailNetworkAccess::RenderHalfWidth(*Trail), .1);
				}
			}
			const bool bReadySway = Status.bReady && Status.Definition.AreaSway.Amplitude == 5.f && Status.Definition.SwayPeriodFrames > 0
				&& FMath::IsNearlyEqual(Status.Value.GripScale, .3f, .01f);
			if (bHost && Stage == 3 && Trail && bAffected && bReadySway)
			{
				Test.TestEqual(TEXT("Whole cast is one regional source"), TargetReceiver->GetAreaCount(), 1);
				Test.TestEqual(TEXT("Caster is exempt from own footprint"), SourceReceiver->GetAreaCount(), 0);
				Test.TestTrue(TEXT("Caster has the configured cooldown"), SourceASC->HasMatchingGameplayTag(HALGameplayTags::Cooldown_SlipperyTrail));
				Test.TestTrue(TEXT("GAS keeps the cast active while the actor exists"), SourceASC->GetActivatableAbilities()[0].IsActive());
				Stage = 4;
			}
			else if (!bHost && Stage == 1 && Trail && Trail->GetOwner() == Source
				&& FHALSlipperyTrailNetworkAccess::HasReplicatedPatch(*Trail) && bAffected && bReadySway)
			{
				PC->SetName(TEXT("HAL_Trail_Affected"));
				Stage = 4;
			}
			if (Stage != 4) { return false; }
			if (bCheckDissolve && Trail)
			{
				auto* Mesh = FHALSlipperyTrailNetworkAccess::GetGroundMesh(*Trail);
				auto* MID = Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr;
				const float Expected = FHALSlipperyTrailNetworkAccess::DissolveProgress(*Trail);
				const float Progress = MID ? MID->K2_GetScalarParameterValue(TEXT("HAL_GroundDissolveProgress")) : 0;
				if (Expected > .05f && Expected < .9f && Mesh && Mesh->IsVisible() && Progress > 0)
				{
					bSawDissolveClock |= FMath::IsNearlyEqual(Progress, Expected, .1f);
					if (!bAffected && Status.Definition.AreaSway.Amplitude == 0)
					{
						bSawGameplayOffWhileVisible = true;
						if (bHost)
						{
							Test.TestEqual(TEXT("Retiring pixels do not retain the authority source"), TargetReceiver->GetAreaCount(), 0);
							Test.TestFalse(TEXT("Visual retirement does not keep GAS active"), SourceASC->GetActivatableAbilities()[0].IsActive());
						}
					}
				}
			}
			bSawGripRecovery |= Status.Definition.AreaSway.Amplitude == 0
				&& Status.Value.GripScale > .3f && Status.Value.GripScale < .99f;
			if (!Trail && !bAffected && Status.bReady && FMath::IsNearlyEqual(Status.Value.GripScale, 1.f, .001f))
			{
				if (bHost && OtherName != TEXT("HAL_Trail_Done")) { return false; }
				if (bCheckGround)
				{
					Test.TestTrue(TEXT("Ground mesh appeared and expanded to full width"), bSawGround && bSawFullWidth);
					Test.TestTrue(TEXT("Expired trail no longer renders a ground mesh"), !GroundMesh.IsValid() || !GroundMesh->IsRegistered());
					UE_LOG(LogTemp, Display, TEXT("SlipperyTrail ground: Role=%s Mesh=%d FullWidth=%d"),
						bHost ? TEXT("Host") : TEXT("Client"), bSawGround, bSawFullWidth);
				}
				Test.TestTrue(TEXT("Grip recovers after sway stops"), bSawGripRecovery);
				if (bCheckDissolve)
				{
					Test.TestTrue(TEXT("Both roles observe clock-driven dissolution"), bSawDissolveClock);
					Test.TestTrue(TEXT("Gameplay ends while retiring pixels are still visible"), bSawGameplayOffWhileVisible);
					UE_LOG(LogTemp, Display, TEXT("SlipperyTrail dissolve: Role=%s Clock=%d GameplayOffWhileVisible=%d"),
						bHost ? TEXT("Host") : TEXT("Client"), bSawDissolveClock, bSawGameplayOffWhileVisible);
				}
				Test.TestEqual(TEXT("No active sway at expiry"), Status.Definition.AreaSway.Amplitude, 0.f);
				if (bHost)
				{
					Test.TestEqual(TEXT("Expiry cleared the authority source"), TargetReceiver->GetAreaCount(), 0);
					Test.TestFalse(TEXT("Expiry ended the active GAS cast"), SourceASC->GetActivatableAbilities()[0].IsActive());
				}
				else { PC->SetName(TEXT("HAL_Trail_Done")); }
				UE_LOG(LogTemp, Display, TEXT("SlipperyTrail smoke: Role=%s Recovered=%d Revision=%u"),
					bHost ? TEXT("Host") : TEXT("Client"), bSawGripRecovery, Status.Value.Revision);
				CompleteAt = FPlatformTime::Seconds() + 4;
			}
			return false;
		}
	private:
		FAutomationTestBase& Test;
		double Deadline = FPlatformTime::Seconds() + 75;
		double CompleteAt = 0;
		double CastAfter = 0;
		int32 Stage = 0;
		bool bSawGripRecovery = false;
		const bool bCheckGround = FParse::Param(FCommandLine::Get(), TEXT("HALSlipperyTrailGroundFixture"));
		const bool bCheckDeformation = FParse::Param(FCommandLine::Get(), TEXT("HALGroundSurfaceFixture"));
		const bool bCheckDissolve = FParse::Param(FCommandLine::Get(), TEXT("HALSlipperyTrailDissolveFixture"));
		bool bSawDissolveClock = false;
		bool bSawGameplayOffWhileVisible = false;
		bool bSawGround = false;
		bool bSawFullWidth = false;
		TWeakObjectPtr<UDynamicMeshComponent> GroundMesh;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALSlipperyTrailNetworkTest, "HAL.NetworkSmoke.SlipperyTrail.AreaLifecycle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FHALSlipperyTrailNetworkTest::RunTest(const FString&)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSlipperyTrailProbe>(*this));
	return true;
}
#endif
