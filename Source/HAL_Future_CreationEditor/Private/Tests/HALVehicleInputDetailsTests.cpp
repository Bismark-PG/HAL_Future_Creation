#if WITH_DEV_AUTOMATION_TESTS

#include "HALVehicleInputDetails.h"
#include "Engine/Blueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Layout/Children.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "TestVehiclePawn.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "VehicleDefinition.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FObjectPropertyBase* ObjectProperty(UClass* Class, FName Name)
	{
		return FindFProperty<FObjectPropertyBase>(Class, Name);
	}
	void SetDefinition(ATestVehiclePawn* Pawn, UVehicleDefinition* Definition)
	{
		const auto* Source = FindFProperty<FEnumProperty>(ATestVehiclePawn::StaticClass(), TEXT("ConfigurationSource"));
		Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Pawn), uint64(EVehicleConfigurationSource::Definition));
		ObjectProperty(ATestVehiclePawn::StaticClass(), TEXT("Definition"))->SetObjectPropertyValue_InContainer(Pawn, Definition);
	}
	void FindPreviewWidgets(const TArray<TSharedRef<IDetailTreeNode>>& Nodes, TMap<FName, TSharedPtr<SWidget>>& Out)
	{
		for (const auto& Node : Nodes)
		{
			if (Node->GetNodeType() == EDetailNodeType::Item && !Node->CreatePropertyHandle().IsValid())
			{
				const FNodeWidgets Widgets = Node->CreateNodeWidgets();
				if (Widgets.ValueWidget && Widgets.ValueWidget->GetTag().ToString().StartsWith(TEXT("HALVehicleInputPreview_")))
				{
					Out.Add(Widgets.ValueWidget->GetTag(), Widgets.ValueWidget);
				}
			}
			TArray<TSharedRef<IDetailTreeNode>> Children;
			Node->GetChildren(Children, true);
			FindPreviewWidgets(Children, Out);
		}
	}
	FString ReadWidgetText(const TSharedRef<SWidget>& Widget)
	{
		FString Text;
		if (Widget->GetType() == TEXT("STextBlock")) { Text = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString(); }
		FChildren* Children = Widget->GetChildren();
		for (int32 I = 0; Children && I < Children->Num(); ++I) { Text += TEXT(" ") + ReadWidgetText(Children->GetChildAt(I)); }
		return Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleInputSourceTest, "HAL.Editor.VehicleInputPreview.LiveSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleInputSourceTest::RunTest(const FString&)
{
	UPackage* Package = CreatePackage(*(TEXT("/Temp/HALVehicleInputPreview_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Package->SetFlags(RF_Transient);
	TStrongObjectPtr<ATestVehiclePawn> Pawn(DuplicateObject<ATestVehiclePawn>(GetDefault<ATestVehiclePawn>(), Package, TEXT("PreviewPawn")));
	auto* Definition = NewObject<UVehicleDefinition>(Package);
	auto* Local = NewObject<UVehicleLocalConfig>(Package);
	Definition->LocalConfig = Local;
	SetDefinition(Pawn.Get(), Definition);
	TestFalse(TEXT("Draft is intentionally not gameplay-ready"), Definition->bReadyForUse);
	TestTrue(TEXT("Draft source is still inspectable"), HALVehicleInputPreview::LocalConfig(Pawn.Get()) == Local);
	int32 Fields = 0;
	for (TFieldIterator<FObjectPropertyBase> It(ATestVehiclePawn::StaticClass(), EFieldIterationFlags::None); It; ++It)
	{
		if (It->GetMetaData(TEXT("Category")) != TEXT("Vehicle|Input")) { continue; }
		++Fields;
		const auto* Source = ObjectProperty(UVehicleLocalConfig::StaticClass(), It->GetFName());
		if (!TestNotNull(TEXT("Each input resolves to Local Config"), Source)) { return false; }
		UObject* First = NewObject<UObject>(Package, Source->PropertyClass);
		UObject* Second = NewObject<UObject>(Package, Source->PropertyClass);
		UObject* Stored = It->GetObjectPropertyValue_InContainer(Pawn.Get());
		Source->SetObjectPropertyValue_InContainer(Local, First);
		TestTrue(TEXT("Preview ignores stale Pawn defaults"), HALVehicleInputPreview::Input(Pawn.Get(), It->GetFName()) == First);
		Source->SetObjectPropertyValue_InContainer(Local, Second);
		TestTrue(TEXT("Shared edits are read without recompiling the Pawn"), HALVehicleInputPreview::Input(Pawn.Get(), It->GetFName()) == Second);
		Source->SetObjectPropertyValue_InContainer(Local, nullptr);
		TestNull(TEXT("Clearing an input removes the preview reference"), HALVehicleInputPreview::Input(Pawn.Get(), It->GetFName()));
		TestTrue(TEXT("Stored Pawn input stays unchanged"), It->GetObjectPropertyValue_InContainer(Pawn.Get()) == Stored);
		TestFalse(TEXT("Definition input remains read-only"), Pawn->CanEditChange(*It));
	}
	TestEqual(TEXT("Mapping and all nine Actions are covered"), Fields, 10);
	auto* OtherLocal = NewObject<UVehicleLocalConfig>(Package);
	Definition->LocalConfig = OtherLocal;
	TestTrue(TEXT("Replacing Local Config updates the source"), HALVehicleInputPreview::LocalConfig(Pawn.Get()) == OtherLocal);
	Definition->LocalConfig = nullptr;
	TestNull(TEXT("Missing Local Config has no stale fallback"), HALVehicleInputPreview::Input(Pawn.Get(), TEXT("SteeringAction")));
	SetDefinition(Pawn.Get(), nullptr);
	TestNull(TEXT("Missing Definition is safe"), HALVehicleInputPreview::LocalConfig(Pawn.Get()));
	const auto* Source = FindFProperty<FEnumProperty>(ATestVehiclePawn::StaticClass(), TEXT("ConfigurationSource"));
	Source->GetUnderlyingProperty()->SetIntPropertyValue(Source->ContainerPtrToValuePtr<void>(Pawn.Get()), uint64(EVehicleConfigurationSource::Legacy));
	TestNull(TEXT("Legacy uses native input rows"), HALVehicleInputPreview::LocalConfig(Pawn.Get()));
	TestTrue(TEXT("Legacy editability remains intact"), Pawn->CanEditChange(ObjectProperty(ATestVehiclePawn::StaticClass(), TEXT("SteeringAction"))));
	TestFalse(TEXT("Read-only projection does not dirty the package"), Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALVehicleInputDetailsRowsTest, "HAL.Editor.VehicleInputPreview.DetailsRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHALVehicleInputDetailsRowsTest::RunTest(const FString&)
{
	if (!TestTrue(TEXT("Slate is initialized for actual Details widgets"), FSlateApplication::IsInitialized())) { return false; }
	for (const TCHAR* Name : { TEXT("BP_RoundedVehiclePawn"), TEXT("BP_VehiclePawn_Car_02") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Blueprints/Vehicles/%s.%s"), Name, Name);
		TStrongObjectPtr<UBlueprint> Blueprint(LoadObject<UBlueprint>(nullptr, *Path));
		const auto* Defaults = Blueprint.IsValid() && Blueprint->GeneratedClass ? Cast<ATestVehiclePawn>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		if (!TestNotNull(TEXT("Existing Blueprint CDO loaded read-only"), Defaults)) { return false; }
		const auto* SavedDefinition = Cast<UVehicleDefinition>(ObjectProperty(ATestVehiclePawn::StaticClass(), TEXT("Definition"))->GetObjectPropertyValue_InContainer(Defaults));
		FString ValidationError;
		const bool bSavedValid = SavedDefinition && SavedDefinition->Validate(ValidationError);
		AddInfo(FString::Printf(TEXT("%s saved Definition gameplay validation: %s%s"), Name,
			bSavedValid ? TEXT("valid") : TEXT("not ready: "), bSavedValid ? TEXT("") : *ValidationError));
		auto* SharedLocal = HALVehicleInputPreview::LocalConfig(Defaults);
		if (!TestNotNull(TEXT("Saved vehicle resolves its Local Config"), SharedLocal)) { return false; }
		const bool bBlueprintDirty = Blueprint->GetOutermost()->IsDirty(), bLocalDirty = SharedLocal->GetOutermost()->IsDirty();
		TStrongObjectPtr<ATestVehiclePawn> Pawn(DuplicateObject<ATestVehiclePawn>(Defaults, GetTransientPackage()));
		TStrongObjectPtr<UVehicleDefinition> Definition(NewObject<UVehicleDefinition>());
		TStrongObjectPtr<UVehicleLocalConfig> Local(DuplicateObject<UVehicleLocalConfig>(SharedLocal, GetTransientPackage()));
		Definition->LocalConfig = Local.Get();
		SetDefinition(Pawn.Get(), Definition.Get());
		const auto* Steering = ObjectProperty(UVehicleLocalConfig::StaticClass(), TEXT("SteeringAction"));
		UObject* OriginalAction = Steering->GetObjectPropertyValue_InContainer(Local.Get());
		if (!TestNotNull(TEXT("Saved steering Action exists"), OriginalAction)) { return false; }
		const auto* StoredProperty = ObjectProperty(ATestVehiclePawn::StaticClass(), TEXT("SteeringAction"));
		UObject* Stored = StoredProperty->GetObjectPropertyValue_InContainer(Pawn.Get());
		auto Rows = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreatePropertyRowGenerator(FPropertyRowGeneratorArgs());
		Rows->SetObjects({ Pawn.Get() });
		TMap<FName, TSharedPtr<SWidget>> Widgets;
		FindPreviewWidgets(Rows->GetRootTreeNodes(), Widgets);
		TestEqual(TEXT("Registered customization supplies source plus all ten input rows"), Widgets.Num(), 11);
		const auto* SteeringWidget = Widgets.Find(TEXT("HALVehicleInputPreview_SteeringAction"));
		if (!TestTrue(TEXT("Steering row is a real asset-entry widget"), SteeringWidget && SteeringWidget->IsValid())) { return false; }
		TestTrue(TEXT("Visible asset label reads Local Config even with draft Definition"), ReadWidgetText(SteeringWidget->ToSharedRef()).Contains(OriginalAction->GetName()));
		UObject* Replacement = ObjectProperty(UVehicleLocalConfig::StaticClass(), TEXT("LaunchAction"))->GetObjectPropertyValue_InContainer(Local.Get());
		if (!TestNotNull(TEXT("Alternative saved Action exists"), Replacement)) { return false; }
		Steering->SetObjectPropertyValue_InContainer(Local.Get(), Replacement);
		TestTrue(TEXT("Same visible widget reflects a shared input edit"), ReadWidgetText(SteeringWidget->ToSharedRef()).Contains(Replacement->GetName()));
		Steering->SetObjectPropertyValue_InContainer(Local.Get(), nullptr);
		TestFalse(TEXT("Cleared source removes the old visible asset label"), ReadWidgetText(SteeringWidget->ToSharedRef()).Contains(Replacement->GetName()));
		TestTrue(TEXT("Details never writes the Pawn's serialized input"), StoredProperty->GetObjectPropertyValue_InContainer(Pawn.Get()) == Stored);
		TestEqual(TEXT("Real Blueprint dirty state is unchanged"), Blueprint->GetOutermost()->IsDirty(), bBlueprintDirty);
		TestEqual(TEXT("Real shared Local Config dirty state is unchanged"), SharedLocal->GetOutermost()->IsDirty(), bLocalDirty);
		Rows->SetObjects({});
	}
	return true;
}

#endif
