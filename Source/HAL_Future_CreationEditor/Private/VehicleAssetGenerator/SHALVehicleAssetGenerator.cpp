#include "VehicleAssetGenerator/HALVehicleAssetGenerator.h"
#include "VehicleAssetGenerator/HALVehicleAssetGeneratorSettings.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Physics/NetworkPhysicsSettingsComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HALVehicleAssetGenerator"

namespace
{
	class SHALVehicleAssetGenerator : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHALVehicleAssetGenerator) {} SLATE_END_ARGS()
		void Construct(const FArguments&)
		{
			Settings.Reset(NewObject<UHALVehicleAssetGeneratorSettings>());
			FDetailsViewArgs Args;
			Args.bAllowSearch = false;
			Args.bHideSelectionTip = true;
			Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(Args);
			Details->SetObject(Settings.Get());
			Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&) { Refresh(); });
			ChildSlot
			[
				SNew(SBorder).Padding(16)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
						[ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Intro", "Choose SKM_<model> and the existing shared vehicle network physics DA. Creates an independent Vehicle Definition, a Presentation, a player Pawn Blueprint and an empty AnimBP. Each model owns its vehicle parameters; input and camera use the shared LocalConfig.")) ]
						+ SVerticalBox::Slot().AutoHeight() [ Details.ToSharedRef() ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
						[ SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(FString(TEXT("Shared LocalConfig: ")) + HALVehicleAssets::SharedLocalConfigPath)) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,12)
						[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Output; }) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
						[ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Calibration", "Vehicle parameters start from C++ defaults. Configure this model's Collision Mesh and a valid visual path: a ready skeletal Presentation or a static Visual Mesh. Static fallback is optional when the skeletal Presentation is valid. Both DAs start with Ready For Use = false. Complete the empty AnimGraph, wheel calibration, model alignment and vehicle configuration before enabling the vehicle.")) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
						[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Validation; }) ]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
							[ SNew(SButton).Text(LOCTEXT("Selected", "Use Selected Sources")).OnClicked(this, &SHALVehicleAssetGenerator::UseSelected) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
							[ SNew(SButton).Text(LOCTEXT("Create", "Create Asset Set"))
								.IsEnabled_Lambda([this] { return bValid && (!GEditor || !GEditor->PlayWorld) && !HasUnsavedSet(); })
								.OnClicked(this, &SHALVehicleAssetGenerator::Create) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
							[ SNew(SButton).Text(LOCTEXT("Save", "Save Generated Assets"))
								.IsEnabled_Lambda([this] { return Generated.Assets.Num() == 4 && (!GEditor || !GEditor->PlayWorld); })
								.OnClicked(this, &SHALVehicleAssetGenerator::Save) ]
							+ SHorizontalBox::Slot().AutoWidth()
							[ SNew(SButton).Text(LOCTEXT("Locate", "Locate Generated Assets"))
								.IsEnabled_Lambda([this] { return Generated.Assets.Num() == 4; })
								.OnClicked(this, &SHALVehicleAssetGenerator::Locate) ]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
						[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Status; }) ]
					]
				]
			];
			Refresh();
		}
	private:
		bool HasUnsavedSet() const { return !Generated.Assets.IsEmpty() && Generated.SavedPackages.Num() != 4; }
		void Refresh()
		{
			HALVehicleAssets::FOutputPaths Paths;
			FString Error;
			bValid = HALVehicleAssets::Validate(Settings->Mesh, Settings->SharedNetworkPhysics, Paths, Error);
			Validation = FText::FromString(bValid ? TEXT("Ready to create. Existing output names block the entire generation.") : Error);
			if (!Settings->Mesh || !HALVehicleAssets::BuildOutputPaths(Settings->Mesh->GetName(), Paths, Error))
			{
				Output = LOCTEXT("Paths", "Output paths appear after selecting SKM_<model>.");
				return;
			}
			Output = FText::FromString(TEXT("Outputs:\n") + FString::Join(Paths.Packages(), TEXT("\n")));
		}
		FReply UseSelected()
		{
			TArray<FAssetData> Selected;
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
			// Exactly one of each source type; never pick a source according to selection order.
			USkeletalMesh* Mesh = nullptr;
			UNetworkPhysicsSettingsDataAsset* Network = nullptr;
			int32 MeshCount = 0, NetworkCount = 0;
			for (const auto& Asset : Selected)
			{
				UObject* Object = Asset.GetAsset();
				if (auto* MeshValue = Cast<USkeletalMesh>(Object)) { Mesh = MeshValue; ++MeshCount; }
				else if (auto* NetworkValue = Cast<UNetworkPhysicsSettingsDataAsset>(Object)) { Network = NetworkValue; ++NetworkCount; }
			}
			if (MeshCount > 1 || NetworkCount > 1)
			{
				Status = LOCTEXT("Ambiguous", "Select at most one Skeletal Mesh and one shared vehicle network physics DA.");
				return FReply::Handled();
			}
			if (Mesh) { Settings->Mesh = Mesh; }
			if (Network) { Settings->SharedNetworkPhysics = Network; }
			Details->ForceRefresh();
			Refresh();
			return FReply::Handled();
		}
		FReply Create()
		{
			if (HasUnsavedSet()) { Status = LOCTEXT("Unsaved", "Save the previous generated set before creating another."); return FReply::Handled(); }
			Generated = HALVehicleAssets::FGeneratedAssets();
			FString Error;
			if (HALVehicleAssets::Create(Settings->Mesh, Settings->SharedNetworkPhysics, Generated, Error))
			{
				Status = LOCTEXT("Created", "Created 4 unsaved assets and linked BP -> VehicleDefinition -> Presentation -> Mesh / AnimBP. Save Generated Assets writes only these four packages. AnimGraph and calibration remain pending.");
				Locate();
			}
			else { Status = FText::FromString(Error); }
			Refresh();
			return FReply::Handled();
		}
		FReply Save()
		{
			FString Message;
			HALVehicleAssets::Save(Generated, Message);
			Status = FText::FromString(Message);
			Refresh();
			return FReply::Handled();
		}
		FReply Locate()
		{
			TArray<UObject*> Assets;
			for (const auto& Asset : Generated.Assets) { Assets.Add(Asset.Get()); }
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets(Assets);
			return FReply::Handled();
		}
		TStrongObjectPtr<UHALVehicleAssetGeneratorSettings> Settings;
		TSharedPtr<IDetailsView> Details;
		HALVehicleAssets::FGeneratedAssets Generated;
		FText Output, Validation, Status;
		bool bValid = false;
	};
}

TSharedRef<SWidget> HALVehicleAssets::MakePanel() { return SNew(SHALVehicleAssetGenerator); }

#undef LOCTEXT_NAMESPACE
