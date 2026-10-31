#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "../HALElectricWallGimmick.h"
#include "../HALShockedPresentationComponent.h"
#include "../HALAbilitySystemComponent.h"
#include "../HALGameplayTags.h"
#include "../HALHealthAttributeSet.h"
#include "../HALPlayerState.h"
#include "../HALPlayerController.h"
#include "../HALMatchGameState.h"
#include "../HALImpactAttributeSet.h"
#include "../HALImpactReceiverComponent.h"
#include "../TestVehiclePawn.h"
#include "../BasicBallActor.h"
#include "../BallControlComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include <limits>
#include "../VehicleNetPhysicsData.h"
#include "../HALImpactSourceComponent.h"
#include "../VehicleKnockbackSettings.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

struct FHALElectricWallTestAccess
{
	static void Scan(AHALElectricWallGimmick* G) { G->ScanContacts(); }
	static UPrimitiveComponent* Surface(AHALElectricWallGimmick* G,int32 I) { return I==0 ? static_cast<UPrimitiveComponent*>(G->PillarA) : I==1 ? static_cast<UPrimitiveComponent*>(G->PillarB) : G->ElectricField.Get(); }
	static bool Contact(AHALElectricWallGimmick* G,ATestVehiclePawn* P,int32 I) { return G->ApplyContact(P,Surface(G,I),P->GetActorLocation()); }
	static FVector Direction(AHALElectricWallGimmick* G,int32 I,FVector Current,FVector Previous,FVector V=FVector::ZeroVector)
	{ return G->GetPushDirection(Surface(G,I),Current,Previous,V); }
	static FVector EntryPush(AHALElectricWallGimmick* G,ATestVehiclePawn* P) { return G->Contacts.FindChecked(P).IncomingPushDirection; }
};
namespace HALElectricWallTests
{
	struct FWorld
	{
		UWorld* World;
		ATestVehiclePawn* Pawn;
		UHALAbilitySystemComponent* ASC;
		AHALElectricWallGimmick* Wall;
		FWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* Match=World->SpawnActor<AHALMatchGameState>(); World->SetGameState(Match); Match->SetMatchPhase(EHALMatchPhase::Playing);
			auto* PC=World->SpawnActor<AHALPlayerController>(); PC->SetAsLocalPlayerController();
			auto* State=World->SpawnActor<AHALPlayerState>(); State->SetOwner(PC); PC->SetPlayerState(State);
			State->PreInitializeComponents(); State->InitializeComponents(); State->PostInitializeComponents(); State->DispatchBeginPlay();
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Class=LoadClass<ATestVehiclePawn>(nullptr,TEXT("/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn.BP_RoundedVehiclePawn_C"));
			Pawn=World->SpawnActor<ATestVehiclePawn>(Class,FVector(0,600,1150),FRotator::ZeroRotator,Spawn);
			if (Pawn) { Pawn->PreInitializeComponents(); Pawn->InitializeComponents(); Pawn->PostInitializeComponents(); PC->Possess(Pawn); Pawn->DispatchBeginPlay(); }
			ASC=State->GetHALAbilitySystemComponent();
			Wall=World->SpawnActor<AHALElectricWallGimmick>(AHALElectricWallGimmick::StaticClass(),FVector(0,0,1000),FRotator::ZeroRotator,Spawn);
			Wall->DispatchBeginPlay();
		}
		~FWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
	class FWait : public IAutomationLatentCommand
	{
		TSharedRef<FWorld> F; FAutomationTestBase* Test; double Target,Deadline; uint64 Last=MAX_uint64; TFunction<void()> Check;
	public:
		FWait(TSharedRef<FWorld> In,FAutomationTestBase* T,float Seconds,TFunction<void()> Fn)
			: F(In),Test(T),Target(In->World->GetTimeSeconds()+Seconds),Deadline(FPlatformTime::Seconds()+30),Check(MoveTemp(Fn)) {}
		bool Update() override
		{
			if (F->World->GetTimeSeconds()>=Target) { Check(); return true; }
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Shocked expiry timeout")); return true; }
			if (Last!=GFrameCounter) { Last=GFrameCounter; F->World->Tick(LEVELTICK_All,.05f); } return false;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricWallGeometryTest,"HAL.FutureCreation.ElectricWall.ConfigurationAndDirections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricWallGeometryTest::RunTest(const FString& Parameters)
{
	FHALElectricWallConfig C; FString Error; TestTrue(TEXT("Defaults valid"),C.Validate(Error));
	C.PillarSpacing=2*C.PillarRadius; TestFalse(TEXT("No overlapping pillars"),C.Validate(Error));
	C={}; C.StunSeconds=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite duration refused"),C.Validate(Error));
	C={}; C.BounceBackSeconds=.001f; TestFalse(TEXT("Invalid bounce time rejected"),C.Validate(Error));
	HALElectricWallTests::FWorld F;
	for (const FVector Travel : {FVector::ForwardVector,-FVector::ForwardVector,FVector::RightVector,-FVector::RightVector,FVector(1,1,0).GetSafeNormal()})
	{
		for (int32 Surface=0; Surface<3; ++Surface)
		{
			TestTrue(TEXT("All surfaces push opposite incoming world-space travel"),FHALElectricWallTestAccess::Direction(F.Wall,Surface,Travel*100,FVector::ZeroVector).Equals(-Travel));
			TestTrue(TEXT("Already bounced velocity cannot reverse intended push"),FHALElectricWallTestAccess::Direction(F.Wall,Surface,Travel*100,FVector::ZeroVector,-Travel*100).Equals(-Travel));
			TestTrue(TEXT("First stationary sample uses incoming velocity"),FHALElectricWallTestAccess::Direction(F.Wall,Surface,FVector::ZeroVector,FVector::ZeroVector,Travel*100).Equals(-Travel));
		}
	}
	TestTrue(TEXT("Wall returns positive side of approach"),FHALElectricWallTestAccess::Direction(F.Wall,2,FVector(0,-300,1150),FVector(0,300,1150)).Equals(FVector::RightVector));
	TestTrue(TEXT("Wall returns negative side of approach"),FHALElectricWallTestAccess::Direction(F.Wall,2,FVector(0,300,1150),FVector(0,-300,1150)).Equals(-FVector::RightVector));
	TestTrue(TEXT("Pillar pushes radially away"),FHALElectricWallTestAccess::Direction(F.Wall,0,FVector(-700,0,1150),FVector(-700,0,1150)).Equals(-FVector::ForwardVector));
	F.Wall->SetActorRotation(FRotator(0,90,0));
	TestTrue(TEXT("Rotated field uses its own plane"),FHALElectricWallTestAccess::Direction(F.Wall,2,FVector(300,0,1150),FVector(-300,0,1150)).Equals(-FVector::ForwardVector));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricWallIncomingDirectionTest,"HAL.FutureCreation.ElectricWall.IncomingDirectionPersistsUntilExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricWallIncomingDirectionTest::RunTest(const FString& Parameters)
{
	HALElectricWallTests::FWorld F; if (!TestNotNull(TEXT("Saved vehicle"),F.Pawn)) { return false; }
	FHALElectricWallTestAccess::Scan(F.Wall);
	F.Pawn->SetActorLocation(FVector(0,0,1150),false,nullptr,ETeleportType::TeleportPhysics); FHALElectricWallTestAccess::Scan(F.Wall);
	TestTrue(TEXT("Southbound approach pushes north"),FHALElectricWallTestAccess::EntryPush(F.Wall,F.Pawn).Equals(FVector::RightVector));
	F.Pawn->SetActorLocation(FVector(0,10,1150),false,nullptr,ETeleportType::TeleportPhysics); FHALElectricWallTestAccess::Scan(F.Wall);
	TestTrue(TEXT("Departing motion while overlapping does not pull back into wall"),FHALElectricWallTestAccess::EntryPush(F.Wall,F.Pawn).Equals(FVector::RightVector));
	F.Pawn->SetActorLocation(FVector(0,-600,1150),false,nullptr,ETeleportType::TeleportPhysics); FHALElectricWallTestAccess::Scan(F.Wall);
	F.Pawn->SetActorLocation(FVector(0,0,1150),false,nullptr,ETeleportType::TeleportPhysics); FHALElectricWallTestAccess::Scan(F.Wall);
	TestTrue(TEXT("Opposite new entry updates direction even during cooldown"),FHALElectricWallTestAccess::EntryPush(F.Wall,F.Pawn).Equals(-FVector::RightVector));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricWallContactTest,"HAL.FutureCreation.ElectricWall.ContactDedupeAndIndependentStun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricWallContactTest::RunTest(const FString& Parameters)
{
	HALElectricWallTests::FWorld F; if (!TestNotNull(TEXT("Saved vehicle"),F.Pawn)) { return false; }
	auto* Visual=F.Pawn->FindComponentByClass<UHALShockedPresentationComponent>();
	TestFalse(TEXT("Cannot mark shocked outside stun"),F.ASC->MarkShockedForCurrentStun(F.Wall));
	F.ASC->AddLooseGameplayTag(HALGameplayTags::State_Combat_ImpactImmune);
	TestFalse(TEXT("Pillar A rejects electrical contact"),FHALElectricWallTestAccess::Contact(F.Wall,F.Pawn,0));
	TestFalse(TEXT("Pillar B rejects electrical contact"),FHALElectricWallTestAccess::Contact(F.Wall,F.Pawn,1));
	TestFalse(TEXT("Pillars cannot stun or mark shocked"),F.ASC->IsStunned() || Visual->IsShocked());
	TestEqual(TEXT("Pillars cannot consume the field entry"),F.Wall->GetAcceptedContactCount(),uint32(0));
	const auto* Profile = F.Wall->FindComponentByClass<UHALImpactSourceComponent>();
	FHALCollisionImpactRules Rules; GetDefault<UVehicleKnockbackSettings>()->GetCollisionImpactRules(Rules);
	TestEqual(TEXT("Pillars use ordinary measured wall impact"),Profile->Mode,EHALCollisionSourceMode::AutomaticCap);
	TestEqual(TEXT("Pillars use the common environment cap"),Profile->Tier,Rules.EnvironmentTierCap);
	for (int32 I : {0,1})
	{
		const auto* Pillar = FHALElectricWallTestAccess::Surface(F.Wall,I);
		TestEqual(TEXT("Pillars keep physical blocking"),Pillar->GetCollisionEnabled(),ECollisionEnabled::QueryAndPhysics);
		TestEqual(TEXT("Pillars block vehicles"),Pillar->GetCollisionResponseToChannel(ECC_PhysicsBody),ECR_Block);
	}
	TestTrue(TEXT("Field applies despite impact immunity"),FHALElectricWallTestAccess::Contact(F.Wall,F.Pawn,2));
	TestEqual(TEXT("Bounce does not enter Impact receiver"),F.Pawn->FindComponentByClass<UHALImpactReceiverComponent>()->GetLastImpact().EffectiveTier,EHALImpactTier::None);
	TestTrue(TEXT("Stun and shocked survive impact immunity"),F.ASC->IsStunned() && Visual->IsShocked());
	TestFalse(TEXT("Pillar remains electrically inactive"),FHALElectricWallTestAccess::Contact(F.Wall,F.Pawn,1));
	TestFalse(TEXT("Wall shares same cooldown"),FHALElectricWallTestAccess::Contact(F.Wall,F.Pawn,2));
	TestEqual(TEXT("Only one field contact"),F.Wall->GetAcceptedContactCount(),uint32(1));
	TestEqual(TEXT("No contact damage"),F.ASC->GetNumericAttribute(UHALHealthAttributeSet::GetCurrentHPAttribute()),100.f);
	TestTrue(TEXT("Repeated presentation mark is idempotent"),F.ASC->MarkShockedForCurrentStun(F.Wall));
	TestEqual(TEXT("One shocked tag"),F.ASC->GetTagCount(HALGameplayTags::State_Visual_Shocked),1);
	Visual->Unbind(); Visual->Bind(F.ASC); TestTrue(TEXT("Late binding restores active visual state"),Visual->IsShocked());
	F.ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(HALGameplayTags::Effect_Stun)));
	TestFalse(TEXT("Stun dispel ends shocked"),Visual->IsShocked());
	F.ASC->ApplyStun(3,F.Wall); TestFalse(TEXT("Plain stun does not become shocked"),Visual->IsShocked());
	F.ASC->MarkShockedForCurrentStun(F.Wall); F.ASC->ClearAvatarIfCurrent(F.Pawn);
	TestFalse(TEXT("Avatar exit clears shocked tag"),F.ASC->HasMatchingGameplayTag(HALGameplayTags::State_Visual_Shocked));
	TestFalse(TEXT("Avatar exit clears presentation"),Visual->IsShocked());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricWallSweepTest,"HAL.FutureCreation.ElectricWall.OverlapAndFastCrossing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricWallSweepTest::RunTest(const FString& Parameters)
{
	for (int32 Mode=0; Mode<2; ++Mode)
	{
		HALElectricWallTests::FWorld F; if (!TestNotNull(TEXT("Saved vehicle"),F.Pawn)) { return false; }
		FHALElectricWallTestAccess::Scan(F.Wall);
		TestEqual(TEXT("Far vehicle unaffected"),F.Wall->GetAcceptedContactCount(),uint32(0));
		const FVector End = Mode==0 ? FVector(0,0,1150) : FVector(0,-600,1150);
		F.Pawn->SetActorLocation(End,false,nullptr,ETeleportType::TeleportPhysics);
		FHALElectricWallTestAccess::Scan(F.Wall);
		TestEqual(TEXT("Field overlap and full fast crossing are detected"),F.Wall->GetAcceptedContactCount(),uint32(1));
		TestTrue(TEXT("Detection applies stun"),F.ASC->IsStunned());
		FHALElectricWallTestAccess::Scan(F.Wall);
		TestEqual(TEXT("Repeated scan cannot duplicate"),F.Wall->GetAcceptedContactCount(),uint32(1));
		F.Pawn->SetActorLocation(FVector(0,600,1150),false,nullptr,ETeleportType::TeleportPhysics); FHALElectricWallTestAccess::Scan(F.Wall);
		F.Pawn->SetActorLocation(FVector(0,0,1150),false,nullptr,ETeleportType::TeleportPhysics); FHALElectricWallTestAccess::Scan(F.Wall);
		TestEqual(TEXT("The bounce return cannot retrigger its own interaction"),F.Wall->GetAcceptedContactCount(),uint32(1));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricWallPillarScanTest,"HAL.FutureCreation.ElectricWall.PillarsAreOrdinaryWalls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricWallPillarScanTest::RunTest(const FString& Parameters)
{
	HALElectricWallTests::FWorld F;
	if (!TestNotNull(TEXT("Saved vehicle"),F.Pawn)) { return false; }
	const auto* Body=CastChecked<UPrimitiveComponent>(F.Pawn->GetRootComponent());
	// Place outside the field's inner edge, while intersecting the solid pillar.
	const double X=-440-Body->Bounds.BoxExtent.X-5;
	F.Pawn->SetActorLocation(FVector(X,600,1150),false,nullptr,ETeleportType::TeleportPhysics);
	FHALElectricWallTestAccess::Scan(F.Wall);
	F.Pawn->SetActorLocation(FVector(X,0,1150),false,nullptr,ETeleportType::TeleportPhysics);
	FHALElectricWallTestAccess::Scan(F.Wall);
	TestEqual(TEXT("Pure pillar overlap/sweep does not trigger electricity"),F.Wall->GetAcceptedContactCount(),uint32(0));
	TestFalse(TEXT("Pure pillar scan never stuns"),F.ASC->IsStunned());
	TestFalse(TEXT("Pure pillar scan never marks shocked"),F.Pawn->FindComponentByClass<UHALShockedPresentationComponent>()->IsShocked());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALElectricWallRetriggerTest,"HAL.FutureCreation.ElectricWall.OneBouncePerEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALElectricWallRetriggerTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<HALElectricWallTests::FWorld>();
	if (!TestNotNull(TEXT("Saved vehicle"),F->Pawn)) { return false; }
	F->Pawn->SetActorLocation(FVector(0,0,1150),false,nullptr,ETeleportType::TeleportPhysics);
	FHALElectricWallTestAccess::Scan(F->Wall);
	TestEqual(TEXT("Initial overlap"),F->Wall->GetAcceptedContactCount(),uint32(1));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALElectricWallTests::FWait>(F,this,1.6f,[this,F]()
	{
		TestEqual(TEXT("Continuous overlap never repeats after time passes"),F->Wall->GetAcceptedContactCount(),uint32(1));
		F->Pawn->SetActorLocation(FVector(0,600,1150),false,nullptr,ETeleportType::TeleportPhysics);
		FHALElectricWallTestAccess::Scan(F->Wall); // Departing sweep still touches the wall.
		FHALElectricWallTestAccess::Scan(F->Wall); // Fully clear sample rearms the entry latch.
		F->Pawn->SetActorLocation(FVector(0,0,1150),false,nullptr,ETeleportType::TeleportPhysics);
		FHALElectricWallTestAccess::Scan(F->Wall);
		TestEqual(TEXT("Re-entry after a clear sample accepts next bounce"),F->Wall->GetAcceptedContactCount(),uint32(2));
		TestTrue(TEXT("New pulse stuns again"),F->ASC->IsStunned());
	}));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALShockedExpiryTest,"HAL.FutureCreation.ElectricWall.ShockedFollowsExtendedStun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALShockedExpiryTest::RunTest(const FString& Parameters)
{
	auto F=MakeShared<HALElectricWallTests::FWorld>();
	if (!TestNotNull(TEXT("Saved vehicle"),F->Pawn)) { return false; }
	F->ASC->ApplyStun(.2f,F->Wall); F->ASC->MarkShockedForCurrentStun(F->Wall);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALElectricWallTests::FWait>(F,this,.1f,[this,F]()
	{
		F->ASC->ApplyStun(.4f,nullptr); F->Wall->Destroy();
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALElectricWallTests::FWait>(F,this,.25f,[this,F]()
		{
			TestTrue(TEXT("Later stun extends shocked even after source destruction"),F->ASC->IsStunned() && F->Pawn->FindComponentByClass<UHALShockedPresentationComponent>()->IsShocked());
			FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<HALElectricWallTests::FWait>(F,this,.3f,[this,F]()
			{
				TestFalse(TEXT("Natural expiry clears stun"),F->ASC->IsStunned());
				TestFalse(TEXT("Natural expiry clears shocked"),F->Pawn->FindComponentByClass<UHALShockedPresentationComponent>()->IsShocked());
			}));
		}));
	}));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALBounceHistoryTest,"HAL.FutureCreation.ElectricWall.BounceHistoryContract",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALBounceHistoryTest::RunTest(const FString& Parameters)
{
 FVehiclePhysicsHistoryState A,B;
 A.State.BounceBack.Sequence=1; A.State.BounceBack.bActive=true;
 A.State.BounceBack.Direction=FVector(-1,0,0); A.State.BounceBack.Distance=100;
 A.State.BounceBack.Duration=.3f; A.State.BounceBack.Elapsed=.1f;
 TestFalse(TEXT("Missing bounce requires history correction"),A.CompareData(B));
 TArray<uint8> Bytes; bool Success=false;
 FMemoryWriter Writer(Bytes); A.NetSerialize(Writer,nullptr,Success);
 TestTrue(TEXT("Bounce history serializes"),Success);
 FMemoryReader Reader(Bytes); B.NetSerialize(Reader,nullptr,Success);
 TestTrue(TEXT("Bounce state survives round trip"),Success && A.State.BounceBack.Equals(B.State.BounceBack));
 TestTrue(TEXT("Matching bounce compares equal"),A.CompareData(B));
 B.State.BounceBack.Elapsed+=.05f;
 TestFalse(TEXT("Elapsed mismatch requires correction"),A.CompareData(B));
 B.State.BounceBack.Direction.Z=1;
 TestFalse(TEXT("Nonhorizontal bounce rejected"),B.State.BounceBack.IsValid());
 return true;
}
#endif
