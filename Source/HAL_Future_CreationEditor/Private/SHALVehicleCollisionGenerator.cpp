#include "SHALVehicleCollisionGenerator.h"

#include "HALVehicleCollisionGenerator.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HALVehicleCollisionGenerator"

namespace
{
	class SHALCollisionPreview : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHALCollisionPreview) {} SLATE_ARGUMENT(UHALVehicleCollisionGeneratorSettings*, Settings) SLATE_END_ARGS()
		void Construct(const FArguments& Args) { Settings = Args._Settings; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(430, 390); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
			int32 Layer, const FWidgetStyle&, bool) const override
		{
			if (!Settings.IsValid()) { return Layer; }
			const FVector2D Size = Geometry.GetLocalSize();
			const auto Text = [&](FVector2D Position, const FText& Label) {
				FSlateDrawElement::MakeText(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2D(1,1), FSlateLayoutTransform(Position)),
					Label, FAppStyle::GetFontStyle("NormalFont"), ESlateDrawEffect::None, FLinearColor::White);
			};
			Text(FVector2D(12, 10), LOCTEXT("Top", "Top (XY) - front +X ->"));
			Text(FVector2D(12, Size.Y * 0.5 + 10), LOCTEXT("Side", "Side (XZ) - flat top and bottom"));
			HALVehicleCollision::FGeometry Shape;
			FString Error;
			if (!HALVehicleCollision::BuildGeometry(Settings->Shape, Shape, Error)) { return Layer + 1; }
			const auto Draw = [&](const TArray<FVector2D>& Points, double Width, double Height, double CenterY, FLinearColor Color) {
				const double Scale = FMath::Min(FMath::Max(1.0, Size.X - 64) / Width, FMath::Max(1.0, Size.Y * 0.5 - 76) / Height);
				const FVector2D Center(Size.X * 0.5, CenterY);
				TArray<FVector2f> Pixels;
				for (const auto& Point : Points) { Pixels.Add(FVector2f(Center + FVector2D(Point.X, -Point.Y) * Scale)); }
				const FVector2f FirstPixel = Pixels[0];
				Pixels.Add(FirstPixel);
				FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Pixels, ESlateDrawEffect::None, Color, true, 2);
				TArray<FVector2f> Axis = { FVector2f(Center + FVector2D(-6,0)), FVector2f(Center + FVector2D(6,0)) };
				FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Axis, ESlateDrawEffect::None, FLinearColor::Yellow);
			};
			const auto& P = Settings->Shape;
			Draw(Shape.Outline, P.LengthCm, P.WidthCm, Size.Y * 0.25 + 12, FLinearColor(0.1f, 0.8f, 0.4f));
			Draw({ FVector2D(-P.LengthCm/2,-P.HeightCm/2), FVector2D(P.LengthCm/2,-P.HeightCm/2),
				FVector2D(P.LengthCm/2,P.HeightCm/2), FVector2D(-P.LengthCm/2,P.HeightCm/2) },
				P.LengthCm, P.HeightCm, Size.Y * 0.75 + 12, FLinearColor(0.2f, 0.65f, 1));
			return Layer + 1;
		}
	private:
		TWeakObjectPtr<UHALVehicleCollisionGeneratorSettings> Settings;
	};

	class SHALCollisionGenerator : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHALCollisionGenerator) {} SLATE_END_ARGS()
		void Construct(const FArguments&)
		{
			Settings.Reset(NewObject<UHALVehicleCollisionGeneratorSettings>(GetTransientPackage(), NAME_None, RF_Transactional));
			FDetailsViewArgs Args;
			Args.bAllowSearch = false;
			Args.bHideSelectionTip = true;
			Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(Args);
			Details->SetObject(Settings.Get());
			Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&) {
				Preview->Invalidate(EInvalidateWidgetReason::Paint);
			});
			ChildSlot
			[
				SNew(SBorder).Padding(12)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
					[ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Intro", "Dimensions are in cm. +X is forward; pivot is at the bounds center. Corner radii affect the XY outline only. Create makes a new asset; save it before using it.")) ]
					+ SVerticalBox::Slot().FillHeight(1)
					[
						SNew(SSplitter)
						+ SSplitter::Slot().Value(0.46f) [ Details.ToSharedRef() ]
						+ SSplitter::Slot().Value(0.54f)
						[ SNew(SBorder).Padding(6) [ SAssignNew(Preview, SHALCollisionPreview).Settings(Settings.Get()) ] ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,8)
					[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] {
						FString Error;
						return FText::FromString(Validate(Error) ? TEXT("Ready. Radius <= width/2; front radius + rear radius <= length.") : Error);
					}) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
						[ SNew(SButton).Text(LOCTEXT("Create", "Create New Mesh"))
							.IsEnabled_Lambda([this] { FString Error; return GEditor && !GEditor->PlayWorld && Validate(Error); })
							.OnClicked(this, &SHALCollisionGenerator::Create) ]
						+ SHorizontalBox::Slot().AutoWidth()
						[ SNew(SButton).Text(LOCTEXT("Read", "Load Selected Mesh Recipe"))
							.OnClicked(this, &SHALCollisionGenerator::LoadSelected) ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
					[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Status; }) ]
				]
			];
		}
	private:
		bool Validate(FString& Error) const
		{
			return Settings->Shape.Validate(Error) && HALVehicleCollision::ValidateOutput(Settings->OutputFolder, Settings->AssetName, Error);
		}
		FReply Create()
		{
			if (!GEditor || GEditor->PlayWorld) { Status = LOCTEXT("PIE", "Stop PIE / simulation before generating collision assets."); return FReply::Handled(); }
			FString Error;
			UStaticMesh* Mesh = HALVehicleCollision::CreateAsset(*Settings, Error);
			if (!Mesh) { Status = FText::FromString(Error); return FReply::Handled(); }
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets(TArray<UObject*>{ Mesh });
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Mesh);
			Status = FText::Format(LOCTEXT("Created", "Created {0}. Save the mesh (Ctrl+S), then set Vehicle Definition > Physics > Collision Mesh."), FText::FromString(Mesh->GetPathName()));
			return FReply::Handled();
		}
		FReply LoadSelected()
		{
			TArray<FAssetData> Selected;
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
			UStaticMesh* Mesh = Selected.Num() == 1 ? Cast<UStaticMesh>(Selected[0].GetAsset()) : nullptr;
			FString Error;
			if (!Mesh || !HALVehicleCollision::ReadRecipe(*Mesh, Settings->Shape, Error))
			{
				Status = FText::FromString(Mesh ? Error : TEXT("Select exactly one generated Static Mesh in the Content Browser."));
				return FReply::Handled();
			}
			Settings->AssetName = Mesh->GetName();
			Settings->OutputFolder = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName());
			Details->ForceRefresh();
			Preview->Invalidate(EInvalidateWidgetReason::Paint);
			Status = LOCTEXT("Loaded", "Recipe loaded. Create New Mesh will make a new version with a unique name; the selected asset is preserved.");
			return FReply::Handled();
		}
		TStrongObjectPtr<UHALVehicleCollisionGeneratorSettings> Settings;
		TSharedPtr<IDetailsView> Details;
		TSharedPtr<SHALCollisionPreview> Preview;
		FText Status;
	};
}

TSharedRef<SWidget> MakeHALVehicleCollisionGenerator() { return SNew(SHALCollisionGenerator); }

#undef LOCTEXT_NAMESPACE
