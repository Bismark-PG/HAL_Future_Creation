#include "HALVehicleInputDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/World.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "TestVehiclePawn.h"
#include "UObject/UnrealType.h"
#include "VehicleDefinition.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HALVehicleInputDetails"

UVehicleLocalConfig* HALVehicleInputPreview::LocalConfig(const ATestVehiclePawn* Pawn)
{
	if (!IsValid(Pawn) || Pawn->GetConfigurationSource() != EVehicleConfigurationSource::Definition) { return nullptr; }
	const auto* Property = FindFProperty<FObjectPropertyBase>(ATestVehiclePawn::StaticClass(), TEXT("Definition"));
	const auto* Definition = Property ? Cast<UVehicleDefinition>(Property->GetObjectPropertyValue_InContainer(Pawn)) : nullptr;
	return IsValid(Definition) && IsValid(Definition->LocalConfig) ? Definition->LocalConfig.Get() : nullptr;
}

UObject* HALVehicleInputPreview::Input(const ATestVehiclePawn* Pawn, FName PropertyName)
{
	const auto* Local = LocalConfig(Pawn);
	const auto* Property = FindFProperty<FObjectPropertyBase>(UVehicleLocalConfig::StaticClass(), PropertyName);
	return Local && Property ? Property->GetObjectPropertyValue_InContainer(Local) : nullptr;
}

TSharedRef<IDetailCustomization> FHALVehicleInputDetails::MakeInstance()
{
	return MakeShared<FHALVehicleInputDetails>();
}

void FHALVehicleInputDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1) { return; }
	const TWeakObjectPtr<ATestVehiclePawn> Pawn = Cast<ATestVehiclePawn>(Objects[0].Get());
	if (!Pawn.IsValid() || (Pawn->GetWorld() && Pawn->GetWorld()->IsGameWorld())) { return; }

	const TWeakPtr<IPropertyUtilities> Utilities = DetailBuilder.GetPropertyUtilities();
	DetailBuilder.GetProperty(TEXT("ConfigurationSource"), ATestVehiclePawn::StaticClass())->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateLambda([Utilities] { if (const auto Pinned = Utilities.Pin()) { Pinned->RequestForceRefresh(); } }));
	if (Pawn->GetConfigurationSource() != EVehicleConfigurationSource::Definition) { return; }

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Vehicle|Input"));
	Category.AddCustomRow(LOCTEXT("LocalSourceSearch", "Local Config Input Source"))
	.NameContent() [ SNew(STextBlock).Font(IDetailLayoutBuilder::GetDetailFont()).Text(LOCTEXT("LocalSource", "Local Config")) ]
	.ValueContent().MinDesiredWidth(250)
	[
		SNew(SObjectPropertyEntryBox).Tag(TEXT("HALVehicleInputPreview_LocalConfig"))
		.AllowedClass(UVehicleLocalConfig::StaticClass())
		.ObjectPath_Lambda([Pawn] { const auto* Local = HALVehicleInputPreview::LocalConfig(Pawn.Get()); return Local ? Local->GetPathName() : FString(); })
		.AllowClear(false).AllowCreate(false).DisplayUseSelected(false).EnableContentPicker(false)
		.OnShouldSetAsset_Lambda([](const FAssetData&) { return false; })
		.ToolTipText(LOCTEXT("SourceHint", "Read-only Definition.LocalConfig source. Open the Local Config asset to edit shared bindings. Missing sources display None."))
	];

	for (TFieldIterator<FObjectPropertyBase> It(ATestVehiclePawn::StaticClass(), EFieldIterationFlags::None); It; ++It)
	{
		const FObjectPropertyBase* Property = *It;
		if (Property->GetMetaData(TEXT("Category")) != TEXT("Vehicle|Input")) { continue; }
		const FName Name = Property->GetFName();
		DetailBuilder.HideProperty(DetailBuilder.GetProperty(Name, ATestVehiclePawn::StaticClass()));
		Category.AddCustomRow(Property->GetDisplayNameText())
		.NameContent() [ SNew(STextBlock).Font(IDetailLayoutBuilder::GetDetailFont()).Text(Property->GetDisplayNameText()) ]
		.ValueContent().MinDesiredWidth(250)
		[
			SNew(SObjectPropertyEntryBox).Tag(FName(*(TEXT("HALVehicleInputPreview_") + Name.ToString())))
			.AllowedClass(Property->PropertyClass)
			.ObjectPath_Lambda([Pawn, Name] { const auto* Value = HALVehicleInputPreview::Input(Pawn.Get(), Name); return Value ? Value->GetPathName() : FString(); })
			.AllowClear(false).AllowCreate(false).DisplayUseSelected(false).EnableContentPicker(false)
			.OnShouldSetAsset_Lambda([](const FAssetData&) { return false; })
			.ToolTipText(LOCTEXT("InputHint", "Read-only input from the current Local Config. Shared changes appear here without copying defaults into this Blueprint."))
		];
	}
}

#undef LOCTEXT_NAMESPACE
