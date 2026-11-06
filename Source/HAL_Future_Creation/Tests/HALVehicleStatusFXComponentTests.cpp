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
#include "NativeGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/GarbageCollection.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatusFXComponentSecond, "State.Visual.StatusFXComponentSecond");

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
#endif
