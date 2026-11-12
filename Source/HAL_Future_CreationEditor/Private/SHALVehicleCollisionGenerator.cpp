#include "SHALVehicleCollisionGenerator.h"

#include "HALVehicleAlignment.h"
#include "HALVehicleCollisionGenerator.h"
#include "VehicleDefinition.h"
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
			Text(FVector2D(12, Size.Y * 0.5 + 10), LOCTEXT("Side", "Side (XZ) - rounded front/rear edges"));
			HALVehicleCollision::FGeometry Shape;
			FString Error;
			if (!HALVehicleCollision::BuildGeometry(Settings->Shape, Shape, Error)) { return Layer + 1; }
			const auto Draw = [&](const TArray<FVector2D>& Points, double Width, double MinY, double MaxY,
				double CenterY, FLinearColor Color, TOptional<double> WheelBottom) {
				const double Scale = FMath::Min(FMath::Max(1.0, Size.X - 64) / Width,
					FMath::Max(1.0, Size.Y * 0.5 - 76) / (MaxY - MinY));
				const FVector2D Center(Size.X * 0.5, CenterY);
				const double MidY = (MinY + MaxY) * 0.5;
				const auto ToPixel = [&](const FVector2D& Point) {
					return FVector2f(Center + FVector2D(Point.X, MidY - Point.Y) * Scale);
				};
				if (WheelBottom.IsSet())
				{
					const float FloorY = ToPixel(FVector2D(0, WheelBottom.GetValue())).Y;
					const TArray<FVector2f> Floor = { FVector2f(12, FloorY), FVector2f(Size.X - 12, FloorY) };
					FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Floor,
						ESlateDrawEffect::None, FLinearColor(1, 0.55f, 0.2f));
				}
				TArray<FVector2f> Pixels;
				for (const auto& Point : Points) { Pixels.Add(ToPixel(Point)); }
				const FVector2f FirstPixel = Pixels[0];
				Pixels.Add(FirstPixel);
				FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Pixels, ESlateDrawEffect::None, Color, true, 2);
				const FVector2f Origin = ToPixel(FVector2D::ZeroVector);
				TArray<FVector2f> Axis = { Origin + FVector2f(-6, 0), Origin + FVector2f(6, 0) };
				FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Axis, ESlateDrawEffect::None, FLinearColor::Yellow);
			};
			const auto& P = Settings->Shape;
			const double Bottom = P.BottomZCm.Get(-P.HeightCm * 0.5);
			Draw(Shape.Outline, P.LengthCm, -P.WidthCm * 0.5, P.WidthCm * 0.5,
				Size.Y * 0.25 + 12, FLinearColor(0.1f, 0.8f, 0.4f), {});
			Draw(Shape.SideOutline, P.LengthCm, FMath::Min(0.0, Bottom), FMath::Max(0.0, Bottom + P.HeightCm),
				Size.Y * 0.75 + 12, FLinearColor(0.2f, 0.65f, 1), P.BottomZCm);
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
					[ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Intro", "Select a Vehicle Definition in the Content Browser to fit the collision bottom to the tires. Height extends upward from that plane. Dimensions are local cm.")) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
						[ SNew(SButton).Tag(TEXT("HALCollision.LoadVehicle")).Text(LOCTEXT("LoadVehicle", "Load Selected Vehicle"))
							.IsEnabled_Lambda([this] { return Idle(); }).OnClicked(this, &SHALCollisionGenerator::LoadVehicle) ]
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
						[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return DescribeAnchor(); }) ]
					]
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
						[ SNew(SButton).Tag(TEXT("HALCollision.Create")).Text(LOCTEXT("Create", "Create New Mesh"))
							.IsEnabled_Lambda([this] { FString Error; return Idle() && Validate(Error); })
							.OnClicked(this, &SHALCollisionGenerator::Create) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
						[ SNew(SButton).Tag(TEXT("HALCollision.Load")).Text(LOCTEXT("Read", "Load Selected Mesh Recipe"))
							.OnClicked(this, &SHALCollisionGenerator::LoadSelected) ]
						+ SHorizontalBox::Slot().AutoWidth()
						[ SNew(SButton).Tag(TEXT("HALCollision.Update")).Text(LOCTEXT("Update", "Update Loaded Mesh"))
							.ToolTipText(LOCTEXT("UpdateTip", "Rebuild the loaded asset in place. All references use the updated shape. Manual vertex edits are replaced. Save the mesh after updating."))
							.IsEnabled_Lambda([this] { FString Error; return LoadedMesh.IsValid() && Idle()
								&& Settings->Shape.Validate(Error); })
							.OnClicked(this, &SHALCollisionGenerator::UpdateLoaded) ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
					[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] {
						return LoadedMesh.IsValid()
							? FText::Format(LOCTEXT("Target", "Update target: {0}\nAll references to this mesh will use the updated shape."), FText::FromString(LoadedMesh->GetPathName()))
							: LOCTEXT("NoTarget", "Update target: none. Load one generated mesh from the Content Browser.");
					}) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
					[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Status; }) ]
				]
			];
		}
	private:
		bool Idle() const { return GEditor && !GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor; }
		UObject* SelectedAsset() const
		{
			TArray<FAssetData> Selected;
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
			return Selected.Num() == 1 ? Selected[0].GetAsset() : nullptr;
		}
		FText DescribeAnchor() const
		{
			const auto& P = Settings->Shape;
			if (!P.BottomZCm.IsSet())
			{
				return Vehicle.IsValid() ? LOCTEXT("NoMeasurement", "Vehicle wheel bottom could not be measured.")
					: LOCTEXT("Centered", "Standalone shape: centered on Z = 0.");
			}
			const double Bottom = P.BottomZCm.GetValue();
			const FString Source = Vehicle.IsValid() ? Vehicle->GetName() : TEXT("Loaded mesh");
			return FText::FromString(FString::Printf(TEXT("%s | Wheel bottom Z: %.3f cm | Collision top Z: %.3f cm"),
				*Source, Bottom, Bottom + P.HeightCm));
		}
		bool Validate(FString& Error) const
		{
			if (Vehicle.IsValid() && !Settings->Shape.BottomZCm.IsSet())
			{
				Error = TEXT("A valid wheel-bottom measurement is required for the loaded vehicle.");
				return false;
			}
			return Settings->Shape.Validate(Error) && HALVehicleCollision::ValidateOutput(Settings->OutputFolder, Settings->AssetName, Error);
		}
		bool MeasureVehicle(FString& Error)
		{
			Settings->Shape.BottomZCm.Reset();
			HALVehicleAlignment::FReport Report;
			if (Vehicle->CollisionShape != EVehicleCollisionShape::ConvexMesh)
			{
				Error = TEXT("The Vehicle Definition must use the Convex Mesh collision shape.");
				return false;
			}
			if (!HALVehicleAlignment::AnalyzeWheels(*Vehicle, Report))
			{
				Error = Report.Error;
				return false;
			}
			Settings->Shape.BottomZCm = Report.WheelBottomZ;
			return true;
		}
		FReply LoadVehicle()
		{
			LoadedMesh.Reset();
			Settings->Shape.BottomZCm.Reset();
			Vehicle.Reset(Cast<UVehicleDefinition>(SelectedAsset()));
			Preview->Invalidate(EInvalidateWidgetReason::Paint);
			if (!Vehicle.IsValid())
			{
				Status = LOCTEXT("SelectVehicle", "Select exactly one Vehicle Definition in the Content Browser.");
				return FReply::Handled();
			}
			FString Error;
			UStaticMesh* Mesh = Vehicle->CollisionMesh;
			FHALVehicleCollisionParameters Recipe;
			const bool bHasRecipe = Mesh && HALVehicleCollision::ReadRecipe(*Mesh, Recipe, Error);
			if (bHasRecipe) { Settings->Shape = Recipe; }
			if (!MeasureVehicle(Error))
			{
				Status = FText::FromString(Error);
			}
			else if (bHasRecipe && HALVehicleCollision::CanUpdateAsset(*Mesh, Error)
				&& HALVehicleCollision::ResolveUpdateAnchor(*Mesh, Recipe, Error))
			{
				LoadUpdateTarget(*Mesh);
				Status = LOCTEXT("VehicleLoaded", "Vehicle wheels measured and its collision recipe loaded. Update Loaded Mesh keeps the current vehicle reference.");
			}
			else
			{
				Status = bHasRecipe ? FText::FromString(Error)
					: LOCTEXT("NoVehicleRecipe", "Vehicle wheels measured. Create a mesh, then assign it to this Vehicle Definition.");
			}
			Details->ForceRefresh();
			return FReply::Handled();
		}
		FReply Create()
		{
			if (!Idle()) { Status = LOCTEXT("PIE", "Stop PIE / simulation before generating collision assets."); return FReply::Handled(); }
			FString Error;
			const bool bHasAnchor = Vehicle.IsValid() ? MeasureVehicle(Error)
				: !LoadedMesh.IsValid() || HALVehicleCollision::ResolveUpdateAnchor(*LoadedMesh.Get(), Settings->Shape, Error);
			Preview->Invalidate(EInvalidateWidgetReason::Paint);
			if (!bHasAnchor) { Status = FText::FromString(Error); return FReply::Handled(); }
			UStaticMesh* Mesh = HALVehicleCollision::CreateAsset(*Settings, Error);
			if (!Mesh) { Status = FText::FromString(Error); return FReply::Handled(); }
			LoadedMesh.Reset();
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().SyncBrowserToAssets(TArray<UObject*>{ Mesh });
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Mesh);
			Status = FText::Format(LOCTEXT("Created", "Created {0}. Save the mesh (Ctrl+S), then assign it to the intended Vehicle Definition > Physics > Collision Mesh."), FText::FromString(Mesh->GetPathName()));
			return FReply::Handled();
		}
		FReply LoadSelected()
		{
			LoadedMesh.Reset();
			Vehicle.Reset();
			Settings->Shape.BottomZCm.Reset();
			Preview->Invalidate(EInvalidateWidgetReason::Paint);
			UStaticMesh* Mesh = Cast<UStaticMesh>(SelectedAsset());
			FString Error;
			FHALVehicleCollisionParameters Recipe;
			if (!Mesh || !HALVehicleCollision::CanUpdateAsset(*Mesh, Error)
				|| !HALVehicleCollision::ReadRecipe(*Mesh, Recipe, Error)
				|| !HALVehicleCollision::ResolveUpdateAnchor(*Mesh, Recipe, Error))
			{
				Status = FText::FromString(Mesh ? Error : TEXT("Select exactly one generated Static Mesh in the Content Browser."));
				return FReply::Handled();
			}
			Settings->Shape = Recipe;
			LoadUpdateTarget(*Mesh);
			Details->ForceRefresh();
			Status = LOCTEXT("Loaded", "Recipe loaded. Update Loaded Mesh rebuilds the target shown above. Create New Mesh creates a separate asset. Output name/folder only affect creation.");
			return FReply::Handled();
		}
		void LoadUpdateTarget(UStaticMesh& Mesh)
		{
			LoadedMesh = &Mesh;
			Settings->AssetName = Mesh.GetName();
			Settings->OutputFolder = FPackageName::GetLongPackagePath(Mesh.GetOutermost()->GetName());
		}
		FReply UpdateLoaded()
		{
			FString Error;
			UStaticMesh* Mesh = LoadedMesh.Get();
			if (Vehicle.IsValid() && Vehicle->CollisionMesh != Mesh)
			{
				LoadedMesh.Reset();
				Status = LOCTEXT("VehicleMeshChanged", "The vehicle's collision mesh changed. Load the Vehicle Definition again.");
				return FReply::Handled();
			}
			if (!Mesh || !HALVehicleCollision::UpdateAsset(*Mesh, Settings->Shape, Error))
			{
				Status = FText::FromString(Mesh ? Error : TEXT("Load a generated mesh before updating."));
				return FReply::Handled();
			}
			HALVehicleCollision::ReadRecipe(*Mesh, Settings->Shape, Error);
			Preview->Invalidate(EInvalidateWidgetReason::Paint);
			Status = FText::Format(LOCTEXT("Updated", "Updated {0} in place, including its simple collision and recipe. Save the mesh (Ctrl+S). Existing references are retained; restart Play to test."), FText::FromString(Mesh->GetPathName()));
			return FReply::Handled();
		}
		TStrongObjectPtr<UHALVehicleCollisionGeneratorSettings> Settings;
		TStrongObjectPtr<UVehicleDefinition> Vehicle;
		TWeakObjectPtr<UStaticMesh> LoadedMesh;
		TSharedPtr<IDetailsView> Details;
		TSharedPtr<SHALCollisionPreview> Preview;
		FText Status;
	};
}

TSharedRef<SWidget> MakeHALVehicleCollisionGenerator() { return SNew(SHALCollisionGenerator); }

#undef LOCTEXT_NAMESPACE
