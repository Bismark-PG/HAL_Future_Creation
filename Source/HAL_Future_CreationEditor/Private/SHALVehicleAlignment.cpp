#include "SHALVehicleAlignment.h"

#include "HALVehicleAlignment.h"
#include "VehicleDefinition.h"
#include "HALVehiclePresentationConfig.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HALVehicleAlignment"
namespace
{
	class SHALAlignmentPreview : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHALAlignmentPreview) {} SLATE_ARGUMENT(const HALVehicleAlignment::FReport*, Report) SLATE_END_ARGS()
		void Construct(const FArguments& Args) { Report = Args._Report; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(450, 450); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
			int32 Layer, const FWidgetStyle&, bool) const override
		{
			if (!Report || !Report->bMeasured) { return Layer; }
			const FVector2D Size = Geometry.GetLocalSize();
			FBox Total = Report->CollisionBounds;
			for (const auto& Wheel : Report->Wheels) { Total += Wheel.Bounds; Total += Wheel.Bounds.ShiftBy(FVector(0, 0, Report->DeltaZ)); }
			const double Scale = FMath::Min(FMath::Max(1., Size.X-60) / FMath::Max(1., Total.GetSize().X),
				FMath::Max(1., Size.Y-60) / FMath::Max(1., Total.GetSize().Z));
			const FVector Center = Total.GetCenter();
			const auto Pixel = [&](double X, double Z) { return FVector2f(Size.X*.5+(X-Center.X)*Scale, Size.Y*.5-(Z-Center.Z)*Scale); };
			const auto Box = [&](const FBox& B, FLinearColor Color) {
				TArray<FVector2f> Points = { Pixel(B.Min.X,B.Min.Z), Pixel(B.Max.X,B.Min.Z), Pixel(B.Max.X,B.Max.Z), Pixel(B.Min.X,B.Max.Z), Pixel(B.Min.X,B.Min.Z) };
				FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,2);
			};
			Box(Report->CollisionBounds, FLinearColor(.15f,.65f,1));
			for (const auto& Wheel : Report->Wheels) { Box(Wheel.Bounds, FLinearColor(1,.5f,.1f)); }
			for (const auto& Wheel : Report->Wheels) { Box(Wheel.Bounds.ShiftBy(FVector(0,0,Report->DeltaZ)), FLinearColor(.15f,1,.35f)); }
			return Layer+1;
		}
	private:
		const HALVehicleAlignment::FReport* Report = nullptr;
	};

	class SHALVehicleAlignment : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHALVehicleAlignment) {} SLATE_END_ARGS()
		void Construct(const FArguments&)
		{
			Status = LOCTEXT("Select", "Select one Vehicle Definition in the Content Browser, then Analyze Selected Vehicle.");
			ChildSlot [ SNew(SBox).MinDesiredWidth(1050).MinDesiredHeight(560) [ SNew(SBorder).Padding(12)
			[ SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
				[ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Intro", "Align the reference-pose wheel bottoms to the simple convex collision bottom. Analyze is read-only. Apply changes only Presentation DA local Z, supports Undo and never saves. Stop PIE before use.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
				[ SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[ SNew(SButton).Text(LOCTEXT("Analyze", "Analyze Selected Vehicle")).IsEnabled_Lambda([this] { return Idle(); }).OnClicked(this,&SHALVehicleAlignment::LoadSelected) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[ SNew(SButton).Text(LOCTEXT("Refresh", "Re-analyze")).IsEnabled_Lambda([this] { return Idle() && Vehicle.IsValid(); }).OnClicked(this,&SHALVehicleAlignment::Refresh) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[ SNew(SButton).Text(LOCTEXT("Apply", "Apply Suggested Z")).IsEnabled_Lambda([this] {
						return Idle() && Report.bCanApply && (Report.SharedUsers.IsEmpty() || bSharedAcknowledged) && FMath::Abs(Report.DeltaZ) > .0001;
					}).OnClicked(this,&SHALVehicleAlignment::Apply) ]
					+ SHorizontalBox::Slot().AutoWidth()
					[ SNew(SButton).Text(LOCTEXT("Open", "Open Presentation DA")).IsEnabled_Lambda([this] { return Idle() && Report.Presentation.IsValid(); }).OnClicked_Lambda([this] {
						GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Report.Presentation.Get()); return FReply::Handled();
					}) ]
				]
				+ SVerticalBox::Slot().FillHeight(1)
				[ SNew(SSplitter)
					+ SSplitter::Slot().Value(.52f) [ SNew(SScrollBox) + SScrollBox::Slot()
						[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(Report.Describe()); }) ] ]
					+ SSplitter::Slot().Value(.48f) [ SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Legend", "Side measurements (XZ)\nBlue: collision bounds\nOrange: current wheel bounds\nGreen: proposed wheel bounds")) ]
						+ SVerticalBox::Slot().FillHeight(1) [ SAssignNew(Preview,SHALAlignmentPreview).Report(&Report) ] ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,8)
				[ SNew(SCheckBox).Visibility_Lambda([this] { return Report.SharedUsers.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					.IsChecked_Lambda([this] { return bSharedAcknowledged ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bSharedAcknowledged = State == ECheckBoxState::Checked; })
					[ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Shared", "I understand Apply affects every listed vehicle sharing this Presentation DA. Different collision configurations must use separate DAs.")) ] ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,4)
				[ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return Status; }) ]
			] ] ];
			LoadSelected();
		}
	private:
		bool Idle() const { return GEditor && !GEditor->PlayWorld; }
		FReply LoadSelected()
		{
			if (!Idle()) { return FReply::Handled(); }
			TArray<FAssetData> Selected;
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
			auto* Definition = Selected.Num() == 1 ? Cast<UVehicleDefinition>(Selected[0].GetAsset()) : nullptr;
			Vehicle.Reset(Definition); Report = {}; bSharedAcknowledged = false;
			if (!Definition) { Status = LOCTEXT("Select", "Select one Vehicle Definition in the Content Browser, then Analyze Selected Vehicle."); Preview->Invalidate(EInvalidateWidgetReason::Paint); return FReply::Handled(); }
			return Refresh();
		}
		FReply Refresh()
		{
			if (!Idle() || !Vehicle.IsValid()) { return FReply::Handled(); }
			bSharedAcknowledged = false;
			const bool bOK = HALVehicleAlignment::Analyze(*Vehicle.Get(),Report);
			Status = FText::FromString(!bOK || !Report.bCanApply ? Report.Error
				: FMath::Abs(Report.DeltaZ) <= .0001 ? TEXT("Already aligned. No change needed.") : TEXT("Preview ready. Review heights and shared users, then Apply Suggested Z. No asset has been modified."));
			Preview->Invalidate(EInvalidateWidgetReason::Paint);
			return FReply::Handled();
		}
		FReply Apply()
		{
			FString Error; bool bChanged = false;
			if (!HALVehicleAlignment::Apply(Report,bSharedAcknowledged,bChanged,Error)) { Status = FText::FromString(Error); return FReply::Handled(); }
			Refresh();
			Status = bChanged ? LOCTEXT("Applied", "Applied to the Presentation DA in memory. Open it and Ctrl+S to save; Edit > Undo restores the previous offset. Restart the game to verify.") : LOCTEXT("Unchanged", "Already aligned. No asset change or transaction was made.");
			return FReply::Handled();
		}
		TStrongObjectPtr<UVehicleDefinition> Vehicle;
		HALVehicleAlignment::FReport Report;
		TSharedPtr<SHALAlignmentPreview> Preview;
		bool bSharedAcknowledged = false;
		FText Status;
	};
}

TSharedRef<SWidget> MakeHALVehicleAlignment() { return SNew(SHALVehicleAlignment); }
#undef LOCTEXT_NAMESPACE
