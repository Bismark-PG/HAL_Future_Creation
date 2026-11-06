#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALVehicleStatusFXDefinition.h"
#include "../HALVehicleStatusFXCatalog.h"
#include "../HALVehicleStatusFXSettings.h"
#include "../HALVehicleStatusFXSubsystem.h"
#include "../HALGameplayTags.h"
#include "../VehicleDefinition.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "NativeGameplayTags.h"
#include "HAL/IConsoleManager.h"
#include <limits>

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatusFXFixture, "State.Visual.StatusFXFixture");

namespace
{
	UHALVehicleStatusFXDefinition* Recipe(UObject* Outer = GetTransientPackage())
	{
		auto* D = NewObject<UHALVehicleStatusFXDefinition>(Outer);
		D->OverlayMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
		return D;
	}
	UHALVehicleStatusFXCatalog* Catalog(UObject* Outer = GetTransientPackage(), FName Name = NAME_None)
	{
		auto* C = NewObject<UHALVehicleStatusFXCatalog>(Outer, Name);
		FHALStatusFXCatalogEntry Entry; Entry.Tag = HALGameplayTags::State_Visual_Shocked; Entry.Definition = Recipe(C);
		C->Effects.Add(Entry); return C;
	}
	UHALVehicleStatusFXSubsystem* Cache(UGameInstance* GI)
	{ return NewObject<UHALVehicleStatusFXSubsystem>(GI); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXRecipeTest,"HAL.FutureCreation.StatusFX.Configuration.RecipeValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXRecipeTest::RunTest(const FString&)
{
	FString Error;
	auto* D = NewObject<UHALVehicleStatusFXDefinition>();
	TestFalse(TEXT("Unconfigured recipe is diagnosed without Ready checkbox"), D->Validate(Error));
	D = Recipe();
	TestTrue(TEXT("A surface-only recipe is sufficient"), D->Validate(Error));
	D->Scalars.Add({TEXT("Brightness"),8});
	D->Colors.Add({TEXT("Tint"),FLinearColor::Blue});
	FHALStatusFXCurve Curve; Curve.Parameter=TEXT("FilamentPower");
	Curve.Value.GetRichCurve()->AddKey(0,4); Curve.Value.GetRichCurve()->AddKey(2,12); D->Curves.Add(Curve);
	TestTrue(TEXT("Finite named surface parameters and second-based curve accepted"), D->Validate(Error));
	D->Colors[0].Parameter=TEXT("Brightness");
	TestFalse(TEXT("Cross-type parameter ambiguity rejected"), D->Validate(Error)); D->Colors[0].Parameter=TEXT("Tint");
	D->Scalars[0].Value=std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Nonfinite scalar rejected"), D->Validate(Error)); D->Scalars[0].Value=8;
	D->FadeOutSeconds=-1; TestFalse(TEXT("Negative transition rejected"), D->Validate(Error)); D->FadeOutSeconds=0;
	D->ResidueSeconds=31; TestFalse(TEXT("Residue bound enforced"), D->Validate(Error)); D->ResidueSeconds=4;
	D->Curves[0].Value.GetRichCurve()->Reset();
	TestFalse(TEXT("Empty authored curve rejected"), D->Validate(Error));
	D->Curves.Empty(); D->OverlayMaterial=nullptr;
	TestFalse(TEXT("Parameters cannot silently apply without a material"), D->Validate(Error));
	D->Scalars.Empty(); D->Colors.Empty();
	FHALStatusFXNiagaraLayer Layer; Layer.LayerId=TEXT("Envelope"); Layer.BoundsSystem=NewObject<UNiagaraSystem>(D);
	D->NiagaraLayers.Add(Layer);
	TestTrue(TEXT("Particle-only envelope accepted by resource schema"), D->Validate(Error));
	D->NiagaraLayers.Add(Layer);
	TestFalse(TEXT("Duplicate layer identity rejected"), D->Validate(Error)); D->NiagaraLayers.Pop();
	D->NiagaraLayers[0].PaddingFraction=std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite geometric scale rejected"), D->Validate(Error)); D->NiagaraLayers[0].PaddingFraction=.1f;
	D->NiagaraLayers[0].Binding=EHALStatusFXBinding::MeshSurface;
	D->NiagaraLayers[0].BoundsSystem=nullptr; D->NiagaraLayers[0].StaticSurfaceSystem=Layer.BoundsSystem;
	TestFalse(TEXT("Universal surface recipe requires skeletal adapter as well"), D->Validate(Error));
	D->NiagaraLayers[0].SkeletalSurfaceSystem=Layer.BoundsSystem;
	TestTrue(TEXT("Both surface templates accepted without promising rendering"), D->Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXCatalogTest,"HAL.FutureCreation.StatusFX.Configuration.ExactCatalogAndDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXCatalogTest::RunTest(const FString&)
{
	FString Error; auto* C=Catalog();
	TestTrue(TEXT("Valid visual mapping"), C->Validate(Error));
	TestTrue(TEXT("Exact tag selects definition"), C->FindExact(HALGameplayTags::State_Visual_Shocked)==C->Effects[0].Definition);
	TestNull(TEXT("Parent does not match a child's effect"), C->FindExact(FGameplayTag::RequestGameplayTag(TEXT("State.Visual"))));
	const FHALStatusFXCatalogEntry Duplicate=C->Effects[0]; C->Effects.Add(Duplicate);
	TestFalse(TEXT("Duplicate tags reject catalog"), C->Validate(Error));
	TestNull(TEXT("Duplicate lookup never picks first authoring entry"), C->FindExact(HALGameplayTags::State_Visual_Shocked)); C->Effects.Pop();
	C->Effects[0].Tag=HALGameplayTags::State_Movement_Boost;
	TestFalse(TEXT("Gameplay tag cannot accidentally become visual-state identity"), C->Validate(Error));
	C->Effects[0].Tag=HALGameplayTags::State_Visual_Shocked; C->Effects[0].Definition=nullptr;
	TestFalse(TEXT("Missing definition diagnosed"), C->Validate(Error));
	TestTrue(TEXT("Diagnostic includes tag and entry"), Error.Contains(TEXT("Entry 0")) && Error.Contains(TEXT("State.Visual.Shocked")));
	C->Effects[0].Definition=Recipe(C); C->Effects[0].Definition->FadeInSeconds=-1;
	TestFalse(TEXT("Nested recipe failure rejects complete catalog atomically"), C->Validate(Error));
	C->Effects[0].Definition->FadeInSeconds=0;
	FHALStatusFXCatalogEntry Second; Second.Tag=TAG_StatusFXFixture; Second.Definition=Recipe(C); C->Effects.Add(Second);
	Swap(C->Effects[0],C->Effects[1]);
	TestTrue(TEXT("Different effects remain exact after authoring reorder"), C->FindExact(TAG_StatusFXFixture)==Second.Definition);
	TestTrue(TEXT("Multiple unique visual tags valid"), C->Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXSelectionTest,"HAL.FutureCreation.StatusFX.Configuration.InheritanceIsolationAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXSelectionTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> GI1(NewObject<UGameInstance>()), GI2(NewObject<UGameInstance>());
	TStrongObjectPtr<UHALVehicleStatusFXSubsystem> A(Cache(GI1.Get())), B(Cache(GI2.Get()));
	auto* Settings=GetMutableDefault<UHALVehicleStatusFXSettings>();
	auto* Shared=Catalog(); auto* Override=Catalog();
	FHALStatusFXCatalogEntry Common; Common.Tag=TAG_StatusFXFixture; Common.Definition=Recipe(Shared); Shared->Effects.Add(Common);
	const TGuardValue<TSoftObjectPtr<UHALVehicleStatusFXCatalog>> Guard(Settings->DefaultCatalog,Shared);
	auto* Vehicle=NewObject<UVehicleDefinition>();
	TestTrue(TEXT("New vehicle inherits without filling another DA reference"), UHALVehicleStatusFXSubsystem::SelectCatalog(Vehicle).Get()==Shared);
	FString Error;
	TestEqual(TEXT("Pure lookup does not start loading"), A->GetCatalogState(Shared,Error), EHALStatusFXLoadState::NotRequested);
	TestNull(TEXT("No hidden synchronous load on hit lookup"), A->FindEffect(HALGameplayTags::State_Visual_Shocked,Vehicle));
	A->PrepareForVehicle(Vehicle);
	TestEqual(TEXT("Already loaded valid catalog prepares immediately"), A->GetCatalogState(Shared,Error), EHALStatusFXLoadState::Ready);
	TestEqual(TEXT("Other GameInstance does not share runtime readiness"), B->GetCatalogState(Shared,Error), EHALStatusFXLoadState::NotRequested);
	TestTrue(TEXT("Read-only recipe lookup returns shared definition"), A->FindEffect(HALGameplayTags::State_Visual_Shocked,Vehicle)==Shared->Effects[0].Definition);
	Vehicle->StatusFXCatalogOverride=Override; A->PrepareForVehicle(Vehicle);
	TestTrue(TEXT("Rare vehicle override replaces its listed tag"), A->FindEffect(HALGameplayTags::State_Visual_Shocked,Vehicle)==Override->Effects[0].Definition);
	TestTrue(TEXT("Unlisted tags inherit default without copying whole catalog"), A->FindEffect(TAG_StatusFXFixture,Vehicle)==Common.Definition);
	auto* Bad=Catalog(); Bad->Effects[0].Definition=nullptr; Vehicle->StatusFXCatalogOverride=Bad;
	AddExpectedMessage(TEXT("Status FX catalog failed"), ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
	A->PrepareForVehicle(Vehicle); A->PrepareForVehicle(Vehicle);
	TestEqual(TEXT("Invalid override failed once"), A->GetCatalogState(Bad,Error), EHALStatusFXLoadState::Failed);
	TestNull(TEXT("Invalid explicit override cannot silently use default recipe"), A->FindEffect(HALGameplayTags::State_Visual_Shocked,Vehicle));
	TestEqual(TEXT("Shared authoring data unchanged"), Shared->Effects.Num(),2);
	const auto* Saved=LoadObject<UVehicleDefinition>(nullptr,TEXT("/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1.DA_Vehicle_Player_TestBaseline_v1"));
	if (TestNotNull(TEXT("Saved driving definition"),Saved))
	{
		auto* Copy=DuplicateObject<UVehicleDefinition>(Saved,GetTransientPackage()); Copy->StatusFXCatalogOverride=Bad;
		TestTrue(TEXT("Cosmetic configuration is never a new vehicle readiness gate"),Copy->Validate(Error));
	}
	A->Deinitialize(); B->Deinitialize();
	A->PrepareCatalog(Shared);
	TestEqual(TEXT("Shutdown discards leases and rejects new work"), A->GetCatalogState(Shared,Error), EHALStatusFXLoadState::NotRequested);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXCookTest,"HAL.FutureCreation.StatusFX.Configuration.CookRootAndHardDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXCookTest::RunTest(const FString&)
{
	auto* C=Catalog(); auto* D=C->Effects[0].Definition.Get();
	auto* Curve=NewObject<UCurveFloat>(D); Curve->FloatCurve.AddKey(0,1);
	FHALStatusFXCurve Parameter; Parameter.Parameter=TEXT("Brightness"); Parameter.Value.ExternalCurve=Curve; D->Curves.Add(Parameter);
	FHALStatusFXNiagaraLayer Layer; Layer.LayerId=TEXT("Envelope"); Layer.BoundsSystem=NewObject<UNiagaraSystem>(D); D->NiagaraLayers.Add(Layer);
	TArray<UObject*> Refs;
	FReferenceFinder Finder(Refs,nullptr,false,true,false,false);
	Finder.FindReferences(C); TestTrue(TEXT("Catalog strongly references recipes"),Refs.Contains(D));
	Refs.Reset(); Finder.FindReferences(D);
	TestTrue(TEXT("Material is a runtime hard dependency"),Refs.Contains(D->OverlayMaterial));
	TestTrue(TEXT("Niagara template is a runtime hard dependency"),Refs.Contains(Layer.BoundsSystem));
	TestTrue(TEXT("External curve is a runtime hard dependency"),Refs.Contains(Curve));
	const auto* Property=FindFProperty<FObjectProperty>(UVehicleDefinition::StaticClass(),TEXT("StatusFXCatalogOverride"));
	TestTrue(TEXT("Rare override is hard and not editor-only for cook traversal"),Property && !Property->HasAnyPropertyFlags(CPF_EditorOnly));
#if WITH_EDITOR
	auto* Settings=GetMutableDefault<UHALVehicleStatusFXSettings>();
	const TGuardValue<TSoftObjectPtr<UHALVehicleStatusFXCatalog>> Guard(Settings->DefaultCatalog,nullptr);
	TArray<FName> Packages; FString Error;
	TestTrue(TEXT("Unconfigured global path is optional"),Settings->CollectCookPackages(Packages,Error));
	TestTrue(TEXT("No fictitious cook package added"),Packages.IsEmpty());
	// In-memory package exercises the real root collector without writing any asset.
	const FString Name=TEXT("/Game/Automation/StatusFX_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	auto* Package=CreatePackage(*Name); auto* CookCatalog=Catalog(Package,TEXT("Catalog"));
	Settings->DefaultCatalog=CookCatalog;
	TestTrue(TEXT("Configured soft catalog is explicitly rooted for cook"),Settings->CollectCookPackages(Packages,Error));
	Settings->CollectCookPackages(Packages,Error);
	TestEqual(TEXT("Root is unique"),Packages.Num(),1); TestEqual(TEXT("Root uses package name"),Packages[0],FName(*Name));
	CookCatalog->Effects.Empty(); Packages.Empty();
	TestFalse(TEXT("Invalid configured catalog is rejected before cook"),Settings->CollectCookPackages(Packages,Error));
	TestTrue(TEXT("Invalid root does not contaminate package output"),Packages.IsEmpty());
	Package->SetDirtyFlag(false);
#endif
	return true;
}

namespace
{
	class FHALStatusFXAsyncProbe : public IAutomationLatentCommand
	{
		FAutomationTestBase* Test;
		TStrongObjectPtr<UGameInstance> GI;
		TStrongObjectPtr<UHALVehicleStatusFXSubsystem> FX;
		TStrongObjectPtr<UHALVehicleStatusFXSubsystem> Cancelled;
		TSoftObjectPtr<UHALVehicleStatusFXCatalog> WrongType{FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))};
		IConsoleVariable* Delay=nullptr;
		int32 OldDelay=1, Notifications=0, CancelNotifications=0, Phase=0;
		double Deadline=0;
		uint64 CancelFrame=0;
	public:
		explicit FHALStatusFXAsyncProbe(FAutomationTestBase* T):Test(T),GI(NewObject<UGameInstance>()),FX(Cache(GI.Get())),Cancelled(Cache(GI.Get())) {}
		~FHALStatusFXAsyncProbe()
		{
			FX->Deinitialize(); Cancelled->Deinitialize();
			if (Delay) { Delay->Set(OldDelay,ECVF_SetByCode); }
		}
		virtual bool Update() override
		{
			FString Error;
			if (Phase==0)
			{
				Delay=IConsoleManager::Get().FindConsoleVariable(TEXT("s.StreamableDelegateDelayFrames"));
				if (Delay) { OldDelay=Delay->GetInt(); Delay->Set(2,ECVF_SetByCode); }
				FX->OnCatalogChanged().AddLambda([this](const FSoftObjectPath&,EHALStatusFXLoadState) { ++Notifications; });
				Cancelled->OnCatalogChanged().AddLambda([this](const FSoftObjectPath&,EHALStatusFXLoadState) { ++CancelNotifications; });
				FX->PrepareCatalog(WrongType); FX->PrepareCatalog(WrongType);
				Test->TestEqual(TEXT("Real streamable request starts loading"),FX->GetCatalogState(WrongType,Error),EHALStatusFXLoadState::Loading);
				Cancelled->PrepareCatalog(WrongType); Cancelled->Deinitialize();
				Deadline=FPlatformTime::Seconds()+15; CancelFrame=GFrameCounter; Phase=1; return false;
			}
			if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Async FX preparation did not complete within 15 seconds.")); return true; }
			if (FX->GetCatalogState(WrongType,Error)==EHALStatusFXLoadState::Loading || GFrameCounter < CancelFrame+5) { return false; }
			Test->TestEqual(TEXT("Wrong asset class fails through asynchronous completion"),FX->GetCatalogState(WrongType,Error),EHALStatusFXLoadState::Failed);
			Test->TestEqual(TEXT("Duplicate prepare produces one completion"),Notifications,1);
			Test->TestEqual(TEXT("Cancelled game instance receives no late completion"),CancelNotifications,0);
			Test->TestEqual(TEXT("Cancelled catalog cannot repopulate cache"),Cancelled->GetCatalogState(WrongType,Error),EHALStatusFXLoadState::NotRequested);
			return true;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALStatusFXAsyncTest,"HAL.FutureCreation.StatusFX.Configuration.AsyncFailureIdempotencyAndShutdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALStatusFXAsyncTest::RunTest(const FString&)
{
	AddExpectedMessage(TEXT("Status FX catalog failed"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
	ADD_LATENT_AUTOMATION_COMMAND(FHALStatusFXAsyncProbe(this));
	return true;
}
#endif
