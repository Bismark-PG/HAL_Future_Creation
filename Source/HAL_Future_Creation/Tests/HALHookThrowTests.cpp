#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HALVehicleAbilityTestSupport.h"
#include "HALRoomTestSupport.h"
#include "../HALHookThrow.h"
#include "../HALHookThrowAbility.h"
#include "../HALHookComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALStunEffect.h"
#include "../HALMatchGameState.h"
#include "../HALPlayerController.h"
#include "../HALPlayerState.h"
#include "../ArcadeVehicleMovementComponent.h"
#include "../BallControlComponent.h"
#include "../BasicBallActor.h"
#include "../VehicleHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/BitWriter.h"
#include "Misc/ScopeExit.h"
#include <limits>

struct FHALHookTestAccess
{
	static void Pose(ATestVehiclePawn& V)
	{
		auto* M=V.FindComponentByClass<UArcadeVehicleMovementComponent>(); M->bShouldRunPhysics.Store(true);
		M->PublishedStability.bValid=M->PublishedStability.bGrounded=M->PublishedStability.bDrivingAllowed=true;
		M->PublishedStability.Up=M->PublishedStability.GroundNormal=FVector::UpVector;
		Snapshot(V);
	}
	static bool Capture(AHALHookThrow& H,ATestVehiclePawn& V) { return H.Capture(V,V.GetActorLocation()); }
	static void Sweep(AHALHookThrow& H,FVector End) { H.Sweep(H.Outlet(),End); }
	static void Timeout(AHALHookThrow& H) { H.State.PhaseTime-=H.Rules.ChoiceSeconds+.01; H.Tick(.016f); }
	static bool Acquire(ATestVehiclePawn& V,ABasicBallActor& B) { return V.FindComponentByClass<UBallControlComponent>()->AcquireBall(&B); }
	static FHALMovementSnapshot Snapshot(ATestVehiclePawn& V)
	{
		auto* M=V.FindComponentByClass<UArcadeVehicleMovementComponent>();
		auto Value=FHALRoomTestAccess::Flush(V.FindComponentByClass<UHALMovementEffectBridge>());
		// This rules fixture does not tick Chaos. Acknowledge its absolute command in frame order,
		// as the real solver does; otherwise the next publication must correctly wait for revision 1.
		if(Value.IsValidValues())
		{
			const auto Previous=M->PublishedMovementStatus.Definition;
			const int32 Frame=FMath::Max(10,M->PublishedMovementStatus.ServerFrame+1);
			Value.Stamp(Previous.IsValid()?&Previous:nullptr,Frame,1.f/60);
			M->PublishedMovementStatus.Definition=M->PublishedMovementStatus.Value=Value;
			M->PublishedMovementStatus.ServerFrame=Frame; M->PublishedMovementStatus.bReady=true;
		}
		return Value;
	}
	static FVector Right(const AHALHookThrow& H) { return H.State.Right; }
	static void ReplicatedPhase(AHALHookThrow& H,uint32 Revision,EHALHookPhase Phase)
	{ auto Copy=H.State; Copy.Revision=Revision; Copy.Phase=Phase; H.AcceptReplicatedState(Copy); }
	static void FinishSwing(AHALHookThrow& H,ATestVehiclePawn& V)
	{
		const auto S=Snapshot(V); auto* M=V.FindComponentByClass<UArcadeVehicleMovementComponent>();
		M->PublishedMovementStatus.ServerFrame=S.Hook.EndFrame; H.Tick(.016f);
	}
};
namespace HALHookTests
{
	struct FFixture
	{
		UWorld* W; UHALHookThrowConfig* Config; UHALHookThrowConfig* OldConfig; int32 Id=0;
		FFixture()
		{
			const auto Options=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
			auto* Match=W->SpawnActor<AHALMatchGameState>(); W->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
			auto* Ability=GetMutableDefault<UHALHookThrowAbility>(); OldConfig=Ability->SkillConfig;
			Config=NewObject<UHALHookThrowConfig>(Ability); Config->FX.bEnabled=false; Ability->SkillConfig=Config;
		}
		~FFixture() { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); GetMutableDefault<UHALHookThrowAbility>()->SkillConfig=OldConfig; }
		ATestVehiclePawn* Car()
		{
			auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_05.BP_VehiclePawn_Car_05_C"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* V=W->SpawnActor<ATestVehiclePawn>(Class,FVector(0,++Id*1200,60),FRotator::ZeroRotator,P);
			auto* D=FHALVehicleAbilityTestAccess::Configure(V,UHALHookThrowAbility::StaticClass()); D->Combat.Version=EHALCombatConfigVersion::AttackSnapshotV1;
			D->Impact.Version=EHALImpactVersion::TieredV1; FHALVehicleAbilityTestAccess::RuntimeConfig(V,D->Abilities);
			V->PreInitializeComponents(); V->InitializeComponents(); V->PostInitializeComponents();
			auto* PC=W->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController(); auto* PS=W->SpawnActor<AHALPlayerState>();
			PS->SetOwner(PC); PC->SetPlayerState(PS); PS->SetPlayerId(Id); PS->PreInitializeComponents(); PS->InitializeComponents(); PS->PostInitializeComponents(); PS->DispatchBeginPlay();
			PS->ChangeVehicleAvatar(V,1); V->DispatchBeginPlay(); FHALHookTestAccess::Pose(*V); return V;
		}
		AHALHookThrow* Hook(ATestVehiclePawn& V)
		{ FActorSpawnParameters P; P.Owner=&V; auto* H=W->SpawnActor<AHALHookThrow>(P); if(!H->Start(*Config)) { H->Destroy(); return nullptr; } return H; }
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHookMotionTest,"HAL.FutureCreation.HookThrow.BoundedHistoryAndDirectionGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALHookMotionTest::RunTest(const FString&)
{
	UHALHookThrowConfig* C=NewObject<UHALHookThrowConfig>(); FString Error; TestTrue(TEXT("Finite test defaults"),C->Validate(Error));
	FHALHookMotion Inactive; FBitWriter EmptyBits(16,true); Inactive.Serialize(EmptyBits);
	TestEqual(TEXT("Unused hook adds exactly one bit to each physics snapshot"),EmptyBits.GetNumBits(),int64(1));
	C->Rules.ChoiceSeconds=0; TestFalse(TEXT("Zero choice window rejected"),C->Validate(Error)); C->Rules.ChoiceSeconds=1.2f;
	C->Rules.HorizontalSpeed=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("NaN throw rejected"),C->Validate(Error));
	FHALHookMotion M; M.Activation=FGuid::NewGuid(); M.DurationSeconds=1; M.bCapture=true; M.Anchor=FVector(100,0,0); M.Stamp(nullptr,100,.02f);
	TestTrue(TEXT("Hold has a finite historical lease"),M.IsValid(true)&&M.IsHolding(100)&&!M.IsHolding(150));
	const auto A=M.Acceleration(FVector(-10000,0,0),FVector(0,300,-500),100);
	TestTrue(TEXT("Constraint is bounded and never cancels falling"),A.Size()<=M.MaximumAcceleration+.01&&A.Z==0&&A.X>0);
	FHALMovementSnapshot S; S.Epoch=FGuid::NewGuid(); S.Revision=1; S.Hook=M; S.Stamp(nullptr,100,.02f);
	TestTrue(TEXT("Hook participates in the real snapshot contract"),S.IsValid());
	TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); S.Serialize(Writer); FMemoryReader Reader(Bytes); FHALMovementSnapshot Copy; Copy.Serialize(Reader);
	TestTrue(TEXT("Historical hook command round trip"),!Reader.IsError()&&Copy.Equals(S));
	M.bThrow=true; M.ThrowVelocity=FVector(0,-1800,250); M.Stamp(&S.Hook,101,.02f);
	TestTrue(TEXT("An action scheduled after the callback still has a future release boundary"),M.StartFrame==102&&M.ReleaseDelta(FVector::ZeroVector,101).IsZero());
	const FVector Released=M.ReleaseDelta(FVector(50,200,400),M.StartFrame);
	TestTrue(TEXT("Throw chooses left, replaces planar velocity and preserves greater ascent"),Released.Equals(FVector(-50,-2000,0)));
	TestTrue(TEXT("The next frame cannot repeat the launch"),M.ReleaseDelta(FVector::ZeroVector,M.StartFrame+1).IsZero());
	S.Hook=M; S.Stamp(nullptr,101,.02f); TestTrue(TEXT("A one-frame future release is a valid snapshot"),S.IsValid());
	M={}; M.Activation=FGuid::NewGuid(); M.bCapture=M.bSwing=true; M.DurationSeconds=.3f;
	M.Anchor=FVector(750,0,50); M.Pivot=FVector(0,0,50); M.SwingRadians=-250.f/750; M.Stamp(nullptr,200,1.f/60);
	TestTrue(TEXT("Swing has its own future start and valid state"),M.StartFrame==201&&M.IsValid(true));
	const auto Half=M.SwingPosition(M.StartFrame+(M.EndFrame-M.StartFrame)/2),End=M.SwingPosition(M.EndFrame);
	TestTrue(TEXT("Actual path bends left on a fixed-radius arc without first pulling inward"),Half.Y<0&&End.X<Half.X&&End.Y<Half.Y&&FMath::Abs((End-M.Pivot).Size2D()-750)<.01);
	const auto EarlyV=(M.SwingPosition(M.StartFrame+1)-M.Anchor)*60;
	const auto EndV=(End-M.SwingPosition(M.EndFrame-1))*60;
	TestTrue(TEXT("Speed builds up and remains nonzero at the detach boundary"),EarlyV.Size2D()<150&&EndV.Size2D()>1500);
	const auto Bounded=M.SwingDelta(M.Anchor,FVector::ZeroVector,M.EndFrame-1,1.f/60);
	TestTrue(TEXT("Blocked/lagging motor remains bounded and planar"),Bounded.Size()<=M.MaximumAcceleration/60+.01&&Bounded.Z==0);
	Bytes.Reset(); FMemoryWriter SwingWriter(Bytes); M.Serialize(SwingWriter); FHALHookMotion SwingCopy; FMemoryReader SwingReader(Bytes); SwingCopy.Serialize(SwingReader);
	TestTrue(TEXT("Swing geometry and timing round-trip in history"),!SwingReader.IsError()&&SwingCopy==M);
	M={}; M.Activation=FGuid::NewGuid(); M.bThrow=M.bPreserveHorizontal=true; M.DurationSeconds=.1f; M.ThrowVelocity=FVector(0,0,250); M.Stamp(nullptr,220,1.f/60);
	TestTrue(TEXT("Detach preserves curved horizontal momentum"),M.ReleaseDelta(EndV,M.StartFrame).Equals(FVector(0,0,250)));
	TArray<FTransform> Poses; FHALHookFX FX; HALHook::BuildChain(FVector::ZeroVector,FVector(1000,0,0),1,FX,false,Poses);
	TestTrue(TEXT("Interlocking chain is bounded"),Poses.Num()>20&&Poses.Num()<=FX.MaximumLinks);
	TestTrue(TEXT("Adjacent rings alternate their planes"),FMath::Abs(FVector::DotProduct(Poses[0].GetRotation().GetUpVector(),Poses[1].GetRotation().GetUpVector()))<.3);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHookAuthorityTest,"HAL.FutureCreation.HookThrow.GASDropChoiceThrowAndIndependentStun",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALHookAuthorityTest::RunTest(const FString&)
{
	HALHookTests::FFixture F; auto* A=F.Car(); auto* B=F.Car(); auto* C=F.Car();
	B->SetActorLocation(A->GetActorLocation()+FVector(750,0,0));
	auto* ASC=CastChecked<UHALAbilitySystemComponent>(A->GetAbilitySystemComponent()); auto* TargetASC=CastChecked<UHALAbilitySystemComponent>(B->GetAbilitySystemComponent());
	auto* OwnerBall=F.W->SpawnActor<ABasicBallActor>(); OwnerBall->DispatchBeginPlay(); auto* Ball=F.W->SpawnActor<ABasicBallActor>(); Ball->DispatchBeginPlay();
	TestTrue(TEXT("Caster may start holding a ball"),FHALHookTestAccess::Acquire(*A,*OwnerBall));
	TestTrue(TEXT("Target initially holds"),FHALHookTestAccess::Acquire(*B,*Ball));
	const float HP=B->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP();
	if(!TestTrue(TEXT("Ordinary primary GAS activation"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary))) { return false; }
	auto* H=A->FindComponentByClass<UHALHookComponent>()->GetOwnedHook(); if(!TestNotNull(TEXT("Authority hook actor"),H)) { return false; }
	TestTrue(TEXT("Caster action lock"),UHALHookComponent::BlocksActionsOn(A));
	TestFalse(TEXT("Repeat primary blocked"),ASC->TryActivateAbilityInput(HALGameplayTags::Input_Ability_Primary));
	FVector Recoil; TestFalse(TEXT("Caster cannot fire held ball"),A->FindComponentByClass<UBallControlComponent>()->LaunchHeldBall(Recoil));
	TestEqual(TEXT("Caster keeps own ball during ordinary casting"),OwnerBall->GetBallState(),EBasicBallState::Controlled);
	TestTrue(TEXT("First capture"),FHALHookTestAccess::Capture(*H,*B));
	TestTrue(TEXT("Capture is ordinary Stun"),TargetASC->IsStunned());
	TestEqual(TEXT("Corrected design: capture immediately drops to Free"),Ball->GetBallState(),EBasicBallState::Free);
	TestEqual(TEXT("Capture deals no damage"),B->FindComponentByClass<UVehicleHealthComponent>()->GetCurrentHP(),HP);
	auto* Other=F.Hook(*C); if(!TestNotNull(TEXT("Second source exists"),Other)) { return false; }
	TestFalse(TEXT("A target cannot be claimed twice"),FHALHookTestAccess::Capture(*Other,*B)); Other->Destroy();
	H->AcceptDirection(FGuid::NewGuid(),1,-1); TestEqual(TEXT("Stale activation cannot throw"),H->GetPhase(),EHALHookPhase::Choosing);
	H->AcceptDirection(H->GetActivation(),1,-1); TestEqual(TEXT("Pre-held direction requires neutral"),H->GetPhase(),EHALHookPhase::Choosing);
	H->AcceptDirection(H->GetActivation(),2,0); H->AcceptDirection(H->GetActivation(),3,-1);
	TestEqual(TEXT("Valid left input begins connected swing"),H->GetPhase(),EHALHookPhase::Swinging);
	TestTrue(TEXT("Stun and exclusive claim remain throughout the swing"),TargetASC->IsStunned()&&B->FindComponentByClass<UHALHookComponent>()->GetCapturingHook()==H);
	const auto Swing=FHALHookTestAccess::Snapshot(*B).Hook;
	TestTrue(TEXT("Left command authors a left arc"),Swing.bSwing&&Swing.SwingPosition(Swing.EndFrame).Y<Swing.Anchor.Y);
	TestTrue(TEXT("Hook tip still follows the actual target"),FVector::Dist(H->TipAt(H->ServerTime()),B->GetActorLocation())<.01);
	FHALHookTestAccess::FinishSwing(*H,*B);
	TestEqual(TEXT("Only a consumed swing endpoint begins reel-in"),H->GetPhase(),EHALHookPhase::Returning);
	TestFalse(TEXT("Only hook Stun ends immediately on throw"),TargetASC->IsStunned());
	const auto Throw=FHALHookTestAccess::Snapshot(*B).Hook;
	TestTrue(TEXT("Detach is an authority command preserving the arc momentum"),Throw.bThrow&&Throw.bPreserveHorizontal&&Throw.ThrowVelocity.Z>0);
	TestTrue(TEXT("Caster stays locked until reel-in"),H->LocksCaster());
	ASC->ApplyStun(.5f,B); TestEqual(TEXT("External Stun directly interrupts return"),H->GetPhase(),EHALHookPhase::Interrupted);
	TestFalse(TEXT("Own action lock is removed while external Stun remains"),UHALHookComponent::BlocksActionsOn(A));
	TestTrue(TEXT("Interruption does not refund energy"),!ASC->GetUltimateEnergyView().bHasEnergy);
	H->Destroy();
	H=F.Hook(*C); if(!TestNotNull(TEXT("Independent Stun fixture"),H)) { return false; }
	TargetASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	TestTrue(TEXT("Impact immunity is not grab immunity"),FHALHookTestAccess::Capture(*H,*B));
	TestTrue(TEXT("A shorter external Stun is not swallowed by hook's remaining time"),TargetASC->ApplyStun(.25f,C));
	const FVector LockedRight=FHALHookTestAccess::Right(*H);
	H->AcceptDirection(H->GetActivation(),1,0); H->AcceptDirection(H->GetActivation(),2,1);
	const auto ImmuneSwing=FHALHookTestAccess::Snapshot(*B).Hook;
	const auto Tangent=FVector::CrossProduct(FVector::UpVector,(ImmuneSwing.Anchor-ImmuneSwing.Pivot).GetSafeNormal2D())*FMath::Sign(ImmuneSwing.SwingRadians);
	FHALHookTestAccess::FinishSwing(*H,*B);
	TestTrue(TEXT("Throw preserves independent Stun"),TargetASC->IsStunned());
	const auto ImmuneThrow=FHALHookTestAccess::Snapshot(*B).Hook;
	AddInfo(FString::Printf(TEXT("Immune throw phase=%d right=%s velocity=%s throw=%d"),int32(H->GetPhase()),*LockedRight.ToString(),*ImmuneThrow.ThrowVelocity.ToString(),ImmuneThrow.bThrow));
	TestTrue(TEXT("Swing and detach work under impact immunity on the caster's locked right"),ImmuneSwing.bSwing&&FVector::DotProduct(Tangent,LockedRight)>0&&ImmuneThrow.bThrow&&ImmuneThrow.bPreserveHorizontal);
	H->Destroy(); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHookBoundaryTest,"HAL.FutureCreation.HookThrow.WallsTimeoutAndOwnerCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALHookBoundaryTest::RunTest(const FString&)
{
	HALHookTests::FFixture F; auto* A=F.Car(); auto* B=F.Car(); B->SetActorLocation(A->GetActorLocation()+FVector(700,0,0));
	auto* Wall=F.W->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(10,300,180)); Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetWorldLocation(A->GetActorLocation()+FVector(400,0,100)); Box->RegisterComponent();
	auto* H=F.Hook(*A); if(!TestNotNull(TEXT("Wall fixture starts"),H)) { return false; }
	FHALHookTestAccess::Sweep(*H,B->GetActorLocation()+FVector(100,0,45));
	TestEqual(TEXT("Wall ends outbound"),H->GetPhase(),EHALHookPhase::Returning);
	TestFalse(TEXT("No capture through wall"),UHALAbilitySystemComponent::IsActorStunned(B));
	FHALHookTestAccess::ReplicatedPhase(*H,40,EHALHookPhase::Swinging); FHALHookTestAccess::ReplicatedPhase(*H,39,EHALHookPhase::Choosing);
	TestEqual(TEXT("Late property cannot rewind reliable swing phase"),H->GetPhase(),EHALHookPhase::Swinging);
	FHALHookTestAccess::ReplicatedPhase(*H,41,EHALHookPhase::Returning); FHALHookTestAccess::ReplicatedPhase(*H,40,EHALHookPhase::Swinging);
	TestEqual(TEXT("Late swing cannot reconnect an already detached hook"),H->GetPhase(),EHALHookPhase::Returning);
	H->Destroy(); Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	H=F.Hook(*A); if(!TestNotNull(TEXT("Timeout fixture"),H)) { return false; }
	if(!TestTrue(TEXT("Target capture for timeout"),FHALHookTestAccess::Capture(*H,*B))) { return false; }
	FHALHookTestAccess::Timeout(*H);
	TestEqual(TEXT("No direction automatically reels in"),H->GetPhase(),EHALHookPhase::Returning);
	TestFalse(TEXT("Timeout never launches"),FHALHookTestAccess::Snapshot(*B).Hook.bThrow);
	TestFalse(TEXT("Timeout releases target Stun"),UHALAbilitySystemComponent::IsActorStunned(B)); H->Destroy();
	H=F.Hook(*A); if(!TestNotNull(TEXT("Exit fixture"),H)) { return false; }
	TestTrue(TEXT("Capture before owner retirement"),FHALHookTestAccess::Capture(*H,*B));
	A->RetireForVehicleTransition();
	TestTrue(TEXT("Changing caster Avatar destroys the action"),H->IsActorBeingDestroyed());
	TestFalse(TEXT("Retirement cannot strand the target"),UHALAbilitySystemComponent::IsActorStunned(B));
	TestNull(TEXT("Target claim cleaned"),B->FindComponentByClass<UHALHookComponent>()->GetCapturingHook());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALHookAssetsTest,"HAL.FutureCreation.HookThrow.SavedMetalAndSkillAssetContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALHookAssetsTest::RunTest(const FString&)
{
	const auto* C=LoadObject<UHALHookThrowConfig>(nullptr,TEXT("/Game/Data/Abilities/DA_Ability_HookThrow"));
	if(!TestNotNull(TEXT("One skill DA"),C)) { return false; } FString Error;
	TestTrue(TEXT("Saved DA valid"),C->Validate(Error));
	TestNotNull(TEXT("Toon metal material"),C->FX.MetalMaterial.Get()); TestNotNull(TEXT("Interlocking link"),C->FX.ChainMesh.Get()); TestNotNull(TEXT("Hook head"),C->FX.HookMesh.Get());
	TestTrue(TEXT("Geometry has renderable bounds"),C->FX.ChainMesh&&C->FX.HookMesh&&C->FX.ChainMesh->GetBounds().BoxExtent.GetMin()>0&&C->FX.HookMesh->GetBounds().BoxExtent.GetMin()>0);
	return true;
}
#endif
