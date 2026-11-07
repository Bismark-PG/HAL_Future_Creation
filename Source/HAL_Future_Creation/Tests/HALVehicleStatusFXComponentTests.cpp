#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../HALVehicleStatusFXComponent.h"
#include "../HALVehicleStatusFXCatalog.h"
#include "../HALVehicleStatusFXDefinition.h"
#include "../HALVehicleStatusFXSettings.h"
#include "../HALVehicleStatusFXSubsystem.h"
#include "../HALVehiclePresentationComponent.h"
#include "../HALVehiclePresentationConfig.h"
#include "../HALGameplayTags.h"
#include "../TestVehiclePawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "AbilitySystemComponent.h"
#include "NativeGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/GarbageCollection.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatusFXComponentSecond, "State.Visual.StatusFXComponentSecond");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatusFXObservedParent, "State.Visual.StatusFXObservedParent");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatusFXObservedChild, "State.Visual.StatusFXObservedParent.Child");

struct FHALStatusFXComponentTestAccess
{
	static void Age(UHALVehicleStatusFXComponent* Component, double Seconds) { Component->Epoch -= Seconds; }
	static TMap<FSoftObjectPath,FHALStatusFXCatalogRecord>& Records(UHALVehicleStatusFXSubsystem* Cache)
	{
		return *FindFProperty<FMapProperty>(Cache->GetClass(), TEXT("Catalogs"))
			->ContainerPtrToValuePtr<TMap<FSoftObjectPath,FHALStatusFXCatalogRecord>>(Cache);
	}
};

namespace
{
	struct FStatusFXWorld
	{
		UWorld* World;
		TStrongObjectPtr<UGameInstance> GI{NewObject<UGameInstance>()};
		TStrongObjectPtr<UHALVehicleStatusFXSubsystem> Cache{NewObject<UHALVehicleStatusFXSubsystem>(GI.Get())};
		TStrongObjectPtr<UHALVehicleStatusFXCatalog> Catalog{NewObject<UHALVehicleStatusFXCatalog>()};
		TSoftObjectPtr<UHALVehicleStatusFXCatalog> OldDefault;
		FStatusFXWorld()
		{
			const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false)
				.ShouldSimulatePhysics(false).EnableTraceCollision(true).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			auto* Recipe = NewObject<UHALVehicleStatusFXDefinition>(Catalog.Get());
			Recipe->OverlayMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
			Recipe->FadeInSeconds = 0; Recipe->FadeOutSeconds = 0.2f; Recipe->ResidueSeconds = 0.3f;
			Recipe->Scalars.Add({TEXT("Brightness"), 7});
			FHALStatusFXNiagaraLayer Layer; Layer.LayerId = TEXT("Envelope"); Layer.BoundsSystem = NewObject<UNiagaraSystem>(Recipe);
			Recipe->NiagaraLayers.Add(Layer);
			FHALStatusFXCatalogEntry Entry; Entry.Tag = HALGameplayTags::State_Visual_Shocked; Entry.Definition = Recipe;
			Catalog->Effects.Add(Entry); Entry.Tag = TAG_StatusFXComponentSecond; Catalog->Effects.Add(Entry);
			auto* Settings = GetMutableDefault<UHALVehicleStatusFXSettings>(); OldDefault = Settings->DefaultCatalog;
			Settings->DefaultCatalog = Catalog.Get();
		}
		~FStatusFXWorld()
		{
			World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
			Cache->Deinitialize(); GetMutableDefault<UHALVehicleStatusFXSettings>()->DefaultCatalog = OldDefault;
		}
		UStaticMeshComponent* Mesh(AActor* Owner, USceneComponent* Parent = nullptr)
		{
			auto* Result = NewObject<UStaticMeshComponent>(Owner); Result->SetMobility(EComponentMobility::Movable);
			Result->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Result->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			if (Parent) { Result->SetupAttachment(Parent); } else { Owner->SetRootComponent(Result); }
			Result->RegisterComponent(); return Result;
		}
	};
	struct FStatusFXCar
	{
		AActor* Owner;
		UStaticMeshComponent* Body;
		UStaticMeshComponent* Visual;
		UHALVehiclePresentationComponent* Presentation;
		UHALVehicleStatusFXComponent* FX;
		explicit FStatusFXCar(FStatusFXWorld& F)
		{
			Owner = F.World->SpawnActor<AActor>(); Body = F.Mesh(Owner); Visual = F.Mesh(Owner, Body);
			Presentation = NewObject<UHALVehiclePresentationComponent>(Owner); Presentation->RegisterComponent();
			Presentation->Configure(nullptr, Body, Visual, nullptr, true);
			FX = NewObject<UHALVehicleStatusFXComponent>(Owner); FX->RegisterComponent();
			FX->Bind(Presentation, F.Cache.Get(), nullptr, Owner);
		}
		TArray<FHALStatusFXRenderRequest> Requests() const
		{ TArray<FHALStatusFXRenderRequest> Result; FX->CaptureRenderRequests(Result); return Result; }
		FHALStatusFXSourceToken Start(FGameplayTag Tag = HALGameplayTags::State_Visual_Shocked)
		{ const auto Source = FX->AcquireSource(); FX->Submit(Source, Tag); return Source; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXOwnershipTest, "HAL.FutureCreation.StatusFX.Component.IndependentDormantResources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXOwnershipTest::RunTest(const FString&)
{
	FStatusFXWorld F; FStatusFXCar A(F), B(F);
	TestFalse(TEXT("Idle car does not tick"), A.FX->IsComponentTickEnabled());
	auto* Recipe = F.Catalog->Effects[0].Definition.Get();
	A.Visual->SetOverlayMaterial(Recipe->OverlayMaterial); // Sentinel for the existing rendering owner.
	const auto AS = A.Start(), BS = B.Start();
	auto AR = A.Requests(), BR = B.Requests();
	if (!TestEqual(TEXT("One target per car"), AR.Num(), 1) || !TestEqual(TEXT("Second car prepared"), BR.Num(), 1)) { return false; }
	TestTrue(TEXT("Materials belong to separate runtime components"), AR[0].Surface->GetOuter() == A.FX && BR[0].Surface->GetOuter() == B.FX);
	TestTrue(TEXT("Two cars never share a MID"), AR[0].Surface != BR[0].Surface);
	TestTrue(TEXT("Particle has car ownership"), AR[0].Particles[0].Component->GetOwner() == A.Owner);
	TestTrue(TEXT("Two cars never share particles"), AR[0].Particles[0].Component != BR[0].Particles[0].Component);
	TestFalse(TEXT("Prepared particles do not register"), AR[0].Particles[0].Component->IsRegistered());
	TestFalse(TEXT("Prepared particles do not activate"), AR[0].Particles[0].Component->IsActive());
	TestTrue(TEXT("Old overlay owner remains untouched"), A.Visual->GetOverlayMaterial() == Recipe->OverlayMaterial);
	const auto Extra = A.Start(); A.FX->Submit(AS, HALGameplayTags::State_Visual_Shocked);
	auto Again = A.Requests();
	TestEqual(TEXT("Same tag aggregates two sources"), Again[0].State.SourceCount, 2);
	TestTrue(TEXT("Refresh reuses MID and particle"), Again[0].Surface == AR[0].Surface && Again[0].Particles[0].Component == AR[0].Particles[0].Component);
	A.Start(TAG_StatusFXComponentSecond); Again = A.Requests();
	TestEqual(TEXT("Two tags keep separate requests"), Again.Num(), 2);
	TestTrue(TEXT("Same mesh/base material can support future single-owner composition"), Again[0].Surface == Again[1].Surface);
	TestTrue(TEXT("Particle layers remain instance-specific"), Again[0].Particles[0].Component != Again[1].Particles[0].Component);
	TestEqual(TEXT("Shared recipe remains immutable"), Recipe->Scalars[0].Value, 7.f);
	A.FX->RemoveSource(Extra); A.FX->Unbind();
	TestTrue(TEXT("Other car request survives first car exit"), B.FX->IsRequestCurrent(BR[0]));
	TestFalse(TEXT("Other car cannot use foreign lease"), B.FX->Submit(AS, HALGameplayTags::State_Visual_Shocked));
	TestTrue(TEXT("Other car still owns its source"), B.FX->RemoveSource(BS));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXLateRecipeTest, "HAL.FutureCreation.StatusFX.Component.LateRecipeAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXLateRecipeTest::RunTest(const FString&)
{
	FStatusFXWorld F;
	const FSoftObjectPath Path(F.Catalog.Get());
	auto& Record = FHALStatusFXComponentTestAccess::Records(F.Cache.Get()).Add(Path); // Model the cache before its real async completion.
	Record.State = EHALStatusFXLoadState::Loading;
	FStatusFXCar A(F), B(F);
	const auto Live = A.Start(), Ended = B.Start();
	TestTrue(TEXT("No recipe yields no fabricated resource"), A.Requests().IsEmpty());
	TestFalse(TEXT("Cache-only wait does not poll"), A.FX->IsComponentTickEnabled());
	B.FX->RemoveSource(Ended);
	FHALStatusFXComponentTestAccess::Age(A.FX, 5);
	Record.Catalog = F.Catalog.Get(); Record.State = EHALStatusFXLoadState::Ready;
	F.Cache->OnCatalogChanged().Broadcast(Path, Record.State);
	const auto Ready = A.Requests();
	if (TestEqual(TEXT("Live demand resumes from cache event"), Ready.Num(), 1))
	{
		TestTrue(TEXT("Recipe delay preserves first-observed curve age"), Ready[0].State.ClockSeconds >= 5);
		TestEqual(TEXT("Only one source after late completion"), Ready[0].State.SourceCount, 1);
	}
	TestTrue(TEXT("Ended demand does not replay a tail on late completion"), B.Requests().IsEmpty());
	A.FX->Unbind(); F.Cache->OnCatalogChanged().Broadcast(Path, Record.State);
	TestTrue(TEXT("Late event after exit creates no resource"), A.Requests().IsEmpty());
	TestFalse(TEXT("Old source is revoked"), A.FX->Submit(Live, HALGameplayTags::State_Visual_Shocked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXMeshRebindTest, "HAL.FutureCreation.StatusFX.Component.MeshGenerationAndGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXMeshRebindTest::RunTest(const FString&)
{
	FStatusFXWorld F; FStatusFXCar A(F); const auto Source = A.Start();
	const auto First = A.Requests()[0];
	A.Visual->SetRelativeLocation(FVector(20,30,40));
	const auto Moved = A.Requests()[0];
	TestEqual(TEXT("Transform change keeps resources"), Moved.MeshGeneration, First.MeshGeneration);
	TestTrue(TEXT("Geometry is refreshed without allocation"), Moved.Surface == First.Surface && !Moved.Target.MeshToWorld.Equals(First.Target.MeshToWorld));
	FHALStatusFXComponentTestAccess::Age(A.FX, 2);
	A.Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	TestFalse(TEXT("Silent same-component asset swap invalidates borrowed request immediately"), A.FX->IsRequestCurrent(First));
	const auto Rebound = A.Requests()[0];
	TestTrue(TEXT("New mesh has a new resource generation"), Rebound.MeshGeneration != First.MeshGeneration && Rebound.Surface != First.Surface);
	TestTrue(TEXT("Old particle was actually destroyed"), !First.Particles[0].Component.IsValid() || First.Particles[0].Component->IsBeingDestroyed());
	TestEqual(TEXT("Mesh rebind retains logical instance"), Rebound.State.InstanceId, First.State.InstanceId);
	TestTrue(TEXT("Mesh rebind retains curve clock"), Rebound.State.ClockSeconds >= 2);
	auto* Part = F.Mesh(A.Owner, A.Body);
	TestTrue(TEXT("Explicit multipart registration accepted"), A.Presentation->RegisterStatusFXPart(TEXT("Spoiler"), Part));
	TestFalse(TEXT("Target-change event invalidates the old generation"), A.FX->IsRequestCurrent(Rebound));
	TestEqual(TEXT("Target-change event prepares both meshes"), A.Requests().Num(), 2);
	A.Presentation->UnregisterStatusFXPart(TEXT("Spoiler"));
	A.Visual->SetHiddenInGame(true); A.Requests();
	TestEqual(TEXT("No usable mesh waits without losing demand"), A.FX->GetRuntimeSnapshots()[0].Phase, EHALStatusFXPhase::Waiting);
	A.FX->RemoveSource(Source); A.Visual->SetHiddenInGame(false);
	TestTrue(TEXT("Ended mesh-wait demand never replays"), A.Requests().IsEmpty());
	TestFalse(TEXT("Idle after mesh-wait cancellation"), A.FX->IsComponentTickEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXCleanupTest, "HAL.FutureCreation.StatusFX.Component.ExpiryAvatarAndOwnerCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXCleanupTest::RunTest(const FString&)
{
	FStatusFXWorld F; FStatusFXCar A(F); const auto Source = A.Start(); const auto First = A.Requests()[0];
	A.FX->Bind(A.Presentation, F.Cache.Get(), nullptr, A.Owner);
	TestTrue(TEXT("Repeated identical binding preserves resources"), A.FX->IsRequestCurrent(First));
	A.FX->RemoveSource(Source);
	TestEqual(TEXT("Last source starts configured fade"), A.Requests()[0].State.Phase, EHALStatusFXPhase::Fading);
	FHALStatusFXComponentTestAccess::Age(A.FX, 1);
	TestFalse(TEXT("Delayed request is rejected after expiry, even before the next Tick"), A.FX->IsRequestCurrent(First));
	TestTrue(TEXT("Fade and residue eventually release requests"), A.Requests().IsEmpty());
	TestTrue(TEXT("Expiry destroys the actual particle"), !First.Particles[0].Component.IsValid() || First.Particles[0].Component->IsBeingDestroyed());
	// Use Editor's normal keep flags: loaded standalone map assets belong to other tests/the editor.
	// Per-car transient MIDs have no standalone flag and must still be collected.
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("Unassigned expired MID is collectable"), First.Surface.IsValid());
	const auto NewSource = A.Start(); const auto BeforeAvatar = A.Requests()[0];
	A.FX->Bind(A.Presentation, F.Cache.Get(), nullptr, A.Visual);
	TestFalse(TEXT("Avatar replacement rejects old request"), A.FX->IsRequestCurrent(BeforeAvatar));
	TestFalse(TEXT("Avatar replacement revokes old lease"), A.FX->Submit(NewSource, HALGameplayTags::State_Visual_Shocked));
	A.Start(); const auto BeforeExit = A.Requests()[0];
	A.Owner->Destroy();
	TestTrue(TEXT("Actor exit destroys actual particle components"), !BeforeExit.Particles[0].Component.IsValid() || BeforeExit.Particles[0].Component->IsBeingDestroyed());
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("Actor exit releases the actual MID"), BeforeExit.Surface.IsValid());
	const auto* PawnCDO = GetDefault<ATestVehiclePawn>();
	TestNotNull(TEXT("All vehicle BPs inherit the C++ runtime component"), PawnCDO->FindComponentByClass<UHALVehicleStatusFXComponent>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXAvatarLossTest, "HAL.FutureCreation.StatusFX.Component.WeakAvatarAndUnconfiguredCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXAvatarLossTest::RunTest(const FString&)
{
	FStatusFXWorld F; FStatusFXCar A(F);
	A.FX->Bind(A.Presentation, nullptr, nullptr, A.Owner);
	const auto Waiting = A.Start();
	TestTrue(TEXT("Unconfigured cosmetics never fabricate resources"), A.Requests().IsEmpty());
	TestTrue(TEXT("Unconfigured demand can be cancelled"), A.FX->RemoveSource(Waiting));
	A.FX->Bind(A.Presentation, F.Cache.Get(), nullptr, A.Visual);
	const auto Old = A.Start(); const auto Request = A.Requests()[0];
	A.Visual->DestroyComponent();
	TestFalse(TEXT("Weak Avatar destruction invalidates a borrowed request"), A.FX->IsRequestCurrent(Request));
	TestTrue(TEXT("Destroyed Avatar forces full cleanup"), A.Requests().IsEmpty());
	TestFalse(TEXT("Destroyed Avatar revokes leases"), A.FX->Submit(Old, HALGameplayTags::State_Visual_Shocked));
	TestFalse(TEXT("Destroyed Avatar stops component updates"), A.FX->IsComponentTickEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXTemplateSelectionTest, "HAL.FutureCreation.StatusFX.Component.StaticSkeletalLayerPreparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXTemplateSelectionTest::RunTest(const FString&)
{
	FStatusFXWorld F;
	auto* Recipe = F.Catalog->Effects[0].Definition.Get();
	FHALStatusFXNiagaraLayer Layer; Layer.LayerId = TEXT("Surface"); Layer.Binding = EHALStatusFXBinding::MeshSurface;
	Layer.StaticSurfaceSystem = NewObject<UNiagaraSystem>(Recipe); Layer.SkeletalSurfaceSystem = NewObject<UNiagaraSystem>(Recipe);
	Recipe->NiagaraLayers.Add(Layer); // Transient fixture authoring, before the cache validates/holds it.
	FStatusFXCar A(F); A.Start(); const auto Static = A.Requests()[0];
	TestTrue(TEXT("Static path prepares its own sampling template"), Static.Particles[1].Component->GetAsset() == Layer.StaticSurfaceSystem);
	auto* Rig = NewObject<USkeletalMeshComponent>(A.Owner); Rig->SetupAttachment(A.Body);
	Rig->SetCollisionEnabled(ECollisionEnabled::NoCollision); Rig->RegisterComponent();
	auto* Config = LoadObject<UHALVehiclePresentationConfig>(nullptr, TEXT("/Game/Data/Car_Presentation/DA_Car_01_Presentation.DA_Car_01_Presentation"));
	if (!TestNotNull(TEXT("Existing read-only presentation config"), Config)) { return false; }
	A.Presentation->Configure(Config, A.Body, A.Visual, Rig, true);
	const auto Requests = A.Requests();
	if (!TestEqual(TEXT("Skeletal path exposes one real mesh"), Requests.Num(), 1)) { return false; }
	TestEqual(TEXT("Actual target kind drives the choice"), Requests[0].Target.Kind, EHALVehicleFXMeshKind::Skeletal);
	TestTrue(TEXT("Skeletal template prepared without triangle sampling yet"), Requests[0].Particles[1].Component->GetAsset() == Layer.SkeletalSurfaceSystem);
	TestTrue(TEXT("Kind switch preserves logical instance"), Requests[0].State.InstanceId == Static.State.InstanceId);
	TestFalse(TEXT("Old static request revoked"), A.FX->IsRequestCurrent(Static));
	return true;
}
static UAbilitySystemComponent* AttachStatusFXASC(FStatusFXWorld& F, FStatusFXCar& Car)
{
	auto* ASC = NewObject<UAbilitySystemComponent>(Car.Owner); ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Car.Owner, Car.Owner);
	Car.FX->Bind(Car.Presentation, F.Cache.Get(), nullptr, ASC);
	return ASC;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXInitialObservationTest, "HAL.FutureCreation.StatusFX.Observation.InitialAndAggregateCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXInitialObservationTest::RunTest(const FString&)
{
	FStatusFXWorld F; FStatusFXCar A(F); auto* ASC = AttachStatusFXASC(F, A);
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Visual_Shocked, 2);
	TestTrue(TEXT("Bind reads an already existing state"), A.FX->ObserveAbilitySystem(ASC));
	auto D = A.FX->CaptureDiagnostics();
	if (!TestEqual(TEXT("One visual instance despite two GAS counts"), D.Instances.Num(), 1)) { return false; }
	TestEqual(TEXT("ASC supplies existence, not inferred GE identities"), D.ASCSources, 1);
	TestEqual(TEXT("One visual source"), D.Instances[0].SourceCount, 1);
	const auto First = A.Requests()[0]; FHALStatusFXComponentTestAccess::Age(A.FX, 1);
	TestTrue(TEXT("Repeated observe is idempotent"), A.FX->ObserveAbilitySystem(ASC));
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Visual_Shocked);
	const auto Refreshed = A.Requests()[0];
	TestTrue(TEXT("Count refresh retains instance, material and clock"), Refreshed.State.InstanceId == First.State.InstanceId
		&& Refreshed.Surface == First.Surface && Refreshed.State.ClockSeconds >= 1);
	ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Visual_Shocked, 2);
	TestEqual(TEXT("Partial GAS count removal retains visual source"), A.FX->CaptureDiagnostics().ASCSources, 1);
	ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Visual_Shocked);
	D = A.FX->CaptureDiagnostics();
	TestEqual(TEXT("Final GAS removal revokes source"), D.ASCSources, 0);
	TestEqual(TEXT("Last source uses configured fade"), D.Instances[0].Phase, EHALStatusFXPhase::Fading);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXHierarchyTest, "HAL.FutureCreation.StatusFX.Observation.ExactHierarchyAndDebugAggregation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXHierarchyTest::RunTest(const FString&)
{
	FStatusFXWorld F;
	for (auto Tag : {FGameplayTag(TAG_StatusFXObservedParent), FGameplayTag(TAG_StatusFXObservedChild)})
	{ FHALStatusFXCatalogEntry E; E.Tag = Tag; E.Definition = F.Catalog->Effects[0].Definition; F.Catalog->Effects.Add(E); }
	FStatusFXCar A(F); auto* ASC = AttachStatusFXASC(F, A); A.FX->ObserveAbilitySystem(ASC);
	ASC->AddLooseGameplayTag(TAG_StatusFXObservedChild);
	TestEqual(TEXT("Implied parent is not fabricated as a source"), A.FX->CaptureDiagnostics().ASCSources, 1);
	ASC->AddLooseGameplayTag(TAG_StatusFXObservedParent);
	TestEqual(TEXT("Explicit parent entry is seen while child keeps aggregate count positive"), A.FX->CaptureDiagnostics().ASCSources, 2);
	ASC->RemoveLooseGameplayTag(TAG_StatusFXObservedParent);
	TestEqual(TEXT("Explicit parent exit is seen while child persists"), A.FX->CaptureDiagnostics().ASCSources, 1);
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Stun);
	TestEqual(TEXT("Nonvisual state is ignored"), A.FX->CaptureDiagnostics().ASCSources, 1);
	ASC->AddLooseGameplayTag(HALGameplayTags::State_Visual_Shocked);
	const int32 ParticlesBeforeDebug = A.FX->CaptureDiagnostics().Particles;
	A.FX->SetDebugSource(TEXT("A"), HALGameplayTags::State_Visual_Shocked, true);
	A.FX->SetDebugSource(TEXT("B"), HALGameplayTags::State_Visual_Shocked, true);
	auto D = A.FX->CaptureDiagnostics();
	const auto* Shock = D.Instances.FindByPredicate([](const auto& I) { return I.Tag == HALGameplayTags::State_Visual_Shocked; });
	if (!TestNotNull(TEXT("Shock is prepared"), Shock)) { return false; }
	TestEqual(TEXT("ASC plus two debug sources share one instance"), Shock->SourceCount, 3);
	TestEqual(TEXT("Sources do not multiply active or fading particle instances"), D.Particles, ParticlesBeforeDebug);
	const uint64 Id = Shock->InstanceId;
	ASC->RemoveLooseGameplayTag(HALGameplayTags::State_Visual_Shocked); A.FX->ClearDebugSources();
	D = A.FX->CaptureDiagnostics();
	TestEqual(TEXT("Debug clear preserves independent ASC state"), D.ASCSources, 1);
	TestEqual(TEXT("Debug clear removes only debug leases"), D.DebugSources, 0);
	Shock = D.Instances.FindByPredicate([](const auto& I) { return I.Tag == HALGameplayTags::State_Visual_Shocked; });
	TestTrue(TEXT("All shock inputs exit through the same fading instance"), Shock && Shock->InstanceId == Id && Shock->Phase == EHALStatusFXPhase::Fading);
	TestTrue(TEXT("Debug clear does not remove gameplay stun"), ASC->HasMatchingGameplayTag(HALGameplayTags::State_Stun));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXObserverReplacementTest, "HAL.FutureCreation.StatusFX.Observation.ReplacementAndStaleCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXObserverReplacementTest::RunTest(const FString&)
{
	FStatusFXWorld F; FStatusFXCar A(F); auto* Old = AttachStatusFXASC(F, A);
	A.FX->ObserveAbilitySystem(Old); Old->AddLooseGameplayTag(HALGameplayTags::State_Visual_Shocked);
	const auto First = A.Requests()[0];
	const auto Root = FGameplayTag::RequestGameplayTag(TEXT("State.Visual"));
	auto OldCallbacks = Old->RegisterGameplayTagEvent(Root, EGameplayTagEventType::AnyCountChange);
	auto* Current = AttachStatusFXASC(F, A); Current->AddLooseGameplayTag(HALGameplayTags::State_Visual_Shocked);
	A.FX->ObserveAbilitySystem(Current); const auto Next = A.Requests()[0];
	TestFalse(TEXT("Rebinding invalidates old resource generation"), A.FX->IsRequestCurrent(First));
	TestTrue(TEXT("Replacing ASC recreates resources"), Next.Surface != First.Surface && Next.State.Binding != First.State.Binding);
	Old->RemoveLooseGameplayTag(HALGameplayTags::State_Visual_Shocked); OldCallbacks.Broadcast(Root, 0);
	TestEqual(TEXT("Old callback cannot revoke current source"), A.FX->CaptureDiagnostics().ASCSources, 1);
	TestTrue(TEXT("Old callback cannot replace current resource"), A.Requests()[0].Surface == Next.Surface);
	TestFalse(TEXT("Foreign ASC is rejected without replacing current observer"), A.FX->ObserveAbilitySystem(Old));
	Current->InitAbilityActorInfo(A.Owner, F.World->SpawnActor<AActor>());
	TestFalse(TEXT("Avatar replacement invalidates even between ticks"), A.FX->IsRequestCurrent(Next));
	TestFalse(TEXT("Stale observer binding fully clears"), A.FX->CaptureDiagnostics().bBound);
	TestTrue(TEXT("Old particle actually destroyed"), !Next.Particles[0].Component.IsValid() || Next.Particles[0].Component->IsBeingDestroyed());
	Current = AttachStatusFXASC(F, A); A.FX->ObserveAbilitySystem(Current);
	Current->AddLooseGameplayTag(HALGameplayTags::State_Visual_Shocked); const auto BeforeDestroy = A.Requests()[0];
	Current->DestroyComponent();
	TestFalse(TEXT("Weak ASC destruction clears observation"), A.FX->CaptureDiagnostics().bBound);
	TestFalse(TEXT("Destroyed ASC request is invalid"), A.FX->IsRequestCurrent(BeforeDestroy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXObserverPendingTest, "HAL.FutureCreation.StatusFX.Observation.PendingRecipeAndUnconfigured",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXObserverPendingTest::RunTest(const FString&)
{
	FStatusFXWorld F;
	for (auto Tag : {FGameplayTag(TAG_StatusFXObservedParent), FGameplayTag(TAG_StatusFXObservedChild)})
	{ FHALStatusFXCatalogEntry E; E.Tag = Tag; E.Definition = F.Catalog->Effects[0].Definition; F.Catalog->Effects.Add(E); }
	const FSoftObjectPath Path(F.Catalog.Get());
	auto& Record = FHALStatusFXComponentTestAccess::Records(F.Cache.Get()).Add(Path); Record.State = EHALStatusFXLoadState::Loading;
	FStatusFXCar A(F), B(F); auto* Active = AttachStatusFXASC(F, A); auto* Ended = AttachStatusFXASC(F, B);
	for (auto* ASC : {Active, Ended}) { ASC->AddLooseGameplayTag(HALGameplayTags::State_Visual_Shocked); }
	A.FX->ObserveAbilitySystem(Active); B.FX->ObserveAbilitySystem(Ended);
	TestEqual(TEXT("ASC demand waits without a fake instance"), A.FX->CaptureDiagnostics().PendingRecipes, 1);
	TestFalse(TEXT("Recipe wait does not poll"), A.FX->IsComponentTickEnabled());
	Ended->RemoveLooseGameplayTag(HALGameplayTags::State_Visual_Shocked);
	FStatusFXCar C(F); auto* Hierarchy = AttachStatusFXASC(F, C); C.FX->ObserveAbilitySystem(Hierarchy);
	Hierarchy->AddLooseGameplayTag(TAG_StatusFXObservedChild); Hierarchy->AddLooseGameplayTag(TAG_StatusFXObservedParent);
	Hierarchy->RemoveLooseGameplayTag(TAG_StatusFXObservedParent); // UE can omit the root callback; do not query/refresh before readiness.
	FHALStatusFXComponentTestAccess::Age(A.FX, 3);
	Record.Catalog = F.Catalog.Get(); Record.State = EHALStatusFXLoadState::Ready; F.Cache->OnCatalogChanged().Broadcast(Path, Record.State);
	TestTrue(TEXT("Still-active ASC demand retains observation age"), A.Requests()[0].State.ClockSeconds >= 3);
	TestTrue(TEXT("Ended ASC demand does not replay after load"), B.Requests().IsEmpty());
	const auto LateHierarchy = C.Requests();
	TestTrue(TEXT("Readiness rechecks missed removals before preparing any expired parent or tail"), LateHierarchy.Num() == 1
		&& LateHierarchy[0].State.Tag == TAG_StatusFXObservedChild && C.FX->CaptureDiagnostics().ASCSources == 1);
	B.FX->Bind(B.Presentation, nullptr, nullptr, Ended); B.FX->ObserveAbilitySystem(Ended);
	Ended->AddLooseGameplayTag(HALGameplayTags::State_Visual_Shocked);
	const auto D = B.FX->CaptureDiagnostics();
	TestTrue(TEXT("Unconfigured presentation observes without blocking gameplay"), D.bObservingASC && D.ASCSources == 1 && D.PendingRecipes == 1 && D.Instances.IsEmpty());
	TestTrue(TEXT("Presentation never removes gameplay tag"), Ended->HasMatchingGameplayTag(HALGameplayTags::State_Visual_Shocked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXDebugGuardTest, "HAL.FutureCreation.StatusFX.Observation.DebugGuardsAndDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXDebugGuardTest::RunTest(const FString&)
{
	FStatusFXWorld F; FStatusFXCar A(F); auto* ASC = AttachStatusFXASC(F, A); A.FX->ObserveAbilitySystem(ASC);
	TestFalse(TEXT("Debug rejects nonvisual gameplay tag"), A.FX->SetDebugSource(TEXT("A"), HALGameplayTags::State_Stun, true));
	TestFalse(TEXT("Empty name rejected"), A.FX->SetDebugSource(NAME_None, HALGameplayTags::State_Visual_Shocked, true));
	for (int32 I = 0; I < 16; ++I)
	{ TestTrue(TEXT("Bounded named source accepted"), A.FX->SetDebugSource(FName(*FString::FromInt(I)), HALGameplayTags::State_Visual_Shocked, true)); }
	TestFalse(TEXT("New source beyond cap rejected"), A.FX->SetDebugSource(TEXT("Overflow"), HALGameplayTags::State_Visual_Shocked, true));
	TestTrue(TEXT("Source refresh allowed at cap"), A.FX->SetDebugSource(TEXT("0"), HALGameplayTags::State_Visual_Shocked, true));
	TestFalse(TEXT("Source cannot silently switch tag"), A.FX->SetDebugSource(TEXT("0"), TAG_StatusFXComponentSecond, true));
	const auto D = A.FX->CaptureDiagnostics();
	TestEqual(TEXT("Diagnostics identify debug sources"), D.DebugSources, 16);
	TestEqual(TEXT("No fabricated ASC sources"), D.ASCSources, 0);
	TestEqual(TEXT("Sources do not multiply material candidates"), D.Materials, 1);
	TestEqual(TEXT("Sources do not multiply particles"), D.Particles, 1);
	TestTrue(TEXT("Resources remain dormant"), D.RegisteredParticles == 0 && D.ActiveParticles == 0);
	TestFalse(TEXT("Debug never writes GAS tags"), ASC->HasMatchingGameplayTag(HALGameplayTags::State_Visual_Shocked));
	A.FX->ClearDebugSources();
	TestEqual(TEXT("Clear revokes named leases"), A.FX->CaptureDiagnostics().DebugSources, 0);
	TestTrue(TEXT("Repeated off is idempotent"), A.FX->SetDebugSource(TEXT("0"), HALGameplayTags::State_Visual_Shocked, false));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFXParticleBudgetTest,"HAL.FutureCreation.StatusFX.Renderer.BoundsBudgetAndExplicitFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFXParticleBudgetTest::RunTest(const FString&)
{
	FStatusFXWorld F; auto* D=F.Catalog->Effects[0].Definition.Get(); F.Catalog->Effects[0].Tag=HALGameplayTags::State_Visual_DebugA;
	D->bEnableRendering=true; D->OverlayMaterial=nullptr; D->Scalars.Reset(); D->NiagaraLayers.Reset();
	auto* Bounds=NewObject<UNiagaraSystem>(D);
	for (const TCHAR* N : {TEXT("User.FX_Intensity"),TEXT("User.FX_Age"),TEXT("User.FX_Residue"),TEXT("User.FX_SpawnRate")})
	{ Bounds->GetExposedParameters().AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(),FName(N))); }
	for (const TCHAR* N : {TEXT("User.FX_BoundsCenter"),TEXT("User.FX_BoundsExtent")})
	{ Bounds->GetExposedParameters().AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),FName(N))); }
	const FNiagaraVariable Max(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.FX_MaxParticles")); Bounds->GetExposedParameters().AddParameter(Max);
	FHALStatusFXNiagaraLayer A; A.LayerId=TEXT("Envelope"); A.BoundsSystem=Bounds; A.MaximumParticles=80; D->NiagaraLayers.Add(A);
	FHALStatusFXNiagaraLayer B; B.LayerId=TEXT("ExplicitFallback"); B.Binding=EHALStatusFXBinding::MeshSurface; B.MaximumParticles=80;
	B.StaticSurfaceSystem=NewObject<UNiagaraSystem>(D); B.SkeletalSurfaceSystem=NewObject<UNiagaraSystem>(D); B.SurfaceFallbackBoundsSystem=Bounds;
	D->NiagaraLayers.Add(B); B.LayerId=TEXT("NoFallback"); B.SurfaceFallbackBoundsSystem=nullptr; D->NiagaraLayers.Add(B);
	const TGuardValue<int32> Budget(GetMutableDefault<UHALVehicleStatusFXSettings>()->MaximumParticlesPerVehicle,100);
	FStatusFXCar Car(F); Car.Visual->SetRelativeTransform(FTransform(FRotator(0,35,0),FVector(40,10,4),FVector(-2,.5,1)));
	const auto Source=Car.Start(HALGameplayTags::State_Visual_DebugA); const auto Requests=Car.Requests();
	if (!TestEqual(TEXT("One visual target has three independent prepared layers"),Requests.Num(),1)
		|| !TestEqual(TEXT("All layer identities retained"),Requests[0].Particles.Num(),3)) { return false; }
	auto* First=Requests[0].Particles[0].Component.Get(); auto* Fallback=Requests[0].Particles[1].Component.Get();
	TestTrue(TEXT("Explicit fallback selects only the configured substitute"),Fallback->GetAsset()==Bounds && Fallback->GetAttachParent()==Car.Visual);
	TestEqual(TEXT("Valid first layer has configured budget"),First->GetOverrideParameters().GetParameterValue<int32>(Max),80);
	TestEqual(TEXT("Second layer receives remaining vehicle budget"),Fallback->GetOverrideParameters().GetParameterValue<int32>(Max),20);
	TestTrue(TEXT("Full mirrored/rotated transform follows real visual target"),First->GetComponentTransform().Equals(Car.Visual->GetComponentTransform()));
	TestEqual(TEXT("Layer without explicit fallback remains rejected"),Car.FX->CaptureDiagnostics().RejectedParticles,1);
	Car.FX->RemoveSource(Source); FHALStatusFXComponentTestAccess::Age(Car.FX,1);
	TestTrue(TEXT("Last tail expiry drops real particle objects"),Car.Requests().IsEmpty() && !Requests[0].Particles[0].Component.IsValid());
	return true;
}
#endif
