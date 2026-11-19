#include "HAL/IConsoleManager.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/Blueprint.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HALGameFlowWidgets.h"
#include "AssetToolsModule.h"
#include "WidgetBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "UObject/SavePackage.h"

namespace HALUIAuthoring
{
	using FObject=TSharedPtr<FJsonObject>;
	TSharedPtr<FJsonValue> Value(FObject O) { return MakeShared<FJsonValueObject>(O); }
	struct FScreenDefinition
	{
		const TCHAR* Name;
		EHALGameFlowPanel Panel;
		const TCHAR* Title;
		const TCHAR* Detail;
	};
	const FScreenDefinition MissingScreens[] = {
		{TEXT("WBP_GF_LocalMenu"), EHALGameFlowPanel::LocalMenu, TEXT("MENU"), TEXT("The match continues while this menu is open.")},
		{TEXT("WBP_GF_Loading"), EHALGameFlowPanel::Loading, TEXT("PREPARING MATCH"), TEXT("PLAYERS LOADED  0 / 0")},
		{TEXT("WBP_GF_Countdown"), EHALGameFlowPanel::Countdown, TEXT("3"), TEXT("GET READY")},
		{TEXT("WBP_GF_SpectatorHUD"), EHALGameFlowPanel::Spectator, TEXT("SPECTATING"), TEXT("ARENA VIEW")},
		{TEXT("WBP_GF_Results"), EHALGameFlowPanel::Results, TEXT("MATCH RESULTS"), TEXT("Waiting for the final results.")},
		{TEXT("WBP_GF_Transition"), EHALGameFlowPanel::Transition, TEXT("PLEASE WAIT"), TEXT("Synchronizing room state...")}
	};

	UTextBlock* ScreenText(UWidgetTree* Tree, FName Name, const TCHAR* Content, int32 Size, FLinearColor Color)
	{
		auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Content));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		Text->bIsVariable = true;
		return Text;
	}

	TMap<FName, EHALGameFlowUIAction> ScreenActions(EHALGameFlowPanel Panel)
	{
		using A = EHALGameFlowUIAction;
		if (Panel == EHALGameFlowPanel::LocalMenu)
		{ return {{TEXT("Btn_Resume"), A::Resume}, {TEXT("Btn_CancelSelection"), A::CancelSelection}, {TEXT("Btn_Start"), A::Start}, {TEXT("Btn_Leave"), A::Leave}}; }
		if (Panel == EHALGameFlowPanel::Spectator)
		{ return {{TEXT("Btn_PreviousSpectator"), A::PreviousSpectator}, {TEXT("Btn_NextSpectator"), A::NextSpectator}}; }
		if (Panel == EHALGameFlowPanel::Results)
		{ return {{TEXT("Btn_Return"), A::Return}, {TEXT("Btn_Leave"), A::Leave}}; }
		return {};
	}

	void BuildScreenLayout(UWidgetBlueprint* BP, const FScreenDefinition& Definition)
	{
		auto* Tree = BP->WidgetTree.Get();
		const FLinearColor Accent(.12f, .93f, .72f, 1.f), Muted(.65f, .74f, .86f, 1.f);
		auto* Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
		Tree->RootWidget = Root;
		const bool bSpectator = Definition.Panel == EHALGameFlowPanel::Spectator;
		if (!bSpectator)
		{
			auto* Backdrop = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
			Backdrop->SetBrushColor(FLinearColor(.015f, .025f, .065f, .72f));
			Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* Slot = Root->AddChildToOverlay(Backdrop);
			Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
		}
		auto* Width = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelWidth"));
		Width->SetWidthOverride(Definition.Panel == EHALGameFlowPanel::Results ? 880.f : 620.f);
		auto* RootSlot = Root->AddChildToOverlay(Width);
		RootSlot->SetHorizontalAlignment(HAlign_Center);
		RootSlot->SetVerticalAlignment(bSpectator ? VAlign_Bottom : VAlign_Center);
		RootSlot->SetPadding(FMargin(24.f));
		auto* Card = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
		Card->SetBrushColor(FLinearColor(.035f, .065f, .14f, .97f));
		Card->SetPadding(FMargin(28.f, 22.f)); Width->AddChild(Card);
		auto* Content = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentStack"));
		Card->AddChild(Content);
		auto AddText = [&](FName Name, const TCHAR* ContentText, int32 Size, FLinearColor Color, float Bottom)
		{
			auto* Text = ScreenText(Tree, Name, ContentText, Size, Color);
			auto* Slot = Content->AddChildToVerticalBox(Text);
			Slot->SetPadding(FMargin(0, 0, 0, Bottom));
			return Text;
		};
		AddText(TEXT("BroadcastLabel"), TEXT("GALACTIC ARENA  /  LIVE"), 12, Accent, 12.f);
		auto* LineHeight = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AccentHeight"));
		LineHeight->SetHeightOverride(3.f);
		auto* Line = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AccentLine"));
		Line->SetBrushColor(Accent); Line->SetPadding(FMargin(0)); LineHeight->AddChild(Line);
		Content->AddChildToVerticalBox(LineHeight)->SetPadding(FMargin(0, 0, 0, 16.f));
		const bool bCountdown = Definition.Panel == EHALGameFlowPanel::Countdown;
		auto* Title = AddText(TEXT("TitleText"), Definition.Title, bCountdown ? 104 : 36, FLinearColor::White, 10.f);
		auto* Detail = AddText(TEXT("DetailText"), Definition.Detail, 18, Muted, 12.f);
		if (bCountdown) { Title->SetJustification(ETextJustify::Center); Detail->SetJustification(ETextJustify::Center); }
		AddText(TEXT("StatusText"), TEXT(""), 12, Muted, 8.f);

		TArray<UButton*> Buttons;
		auto AddButton = [&](FName Name, const TCHAR* Label, bool bPrimary)
		{
			auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
			Button->bIsVariable = true;
			Button->SetBackgroundColor(bPrimary ? Accent : FLinearColor(.15f, .22f, .34f, 1.f));
			auto* Height = Tree->ConstructWidget<USizeBox>(); Height->SetMinDesiredHeight(44.f);
			auto* Padding = Tree->ConstructWidget<UBorder>(); Padding->SetPadding(FMargin(16.f, 8.f));
			Padding->SetBrushColor(FLinearColor::Transparent);
			auto* LabelText = ScreenText(Tree, FName(*(Name.ToString() + TEXT("_Label"))), Label, 16,
				bPrimary ? FLinearColor(.02f, .06f, .1f, 1.f) : FLinearColor::White);
			LabelText->SetJustification(ETextJustify::Center);
			Padding->AddChild(LabelText); Height->AddChild(Padding); Button->AddChild(Height);
			Buttons.Add(Button); return Button;
		};
		if (Definition.Panel == EHALGameFlowPanel::LocalMenu)
		{
			const auto AddMenu = [&](FName Name, const TCHAR* Label, bool bPrimary)
			{ Content->AddChildToVerticalBox(AddButton(Name, Label, bPrimary))->SetPadding(FMargin(0, 5.f)); };
			AddMenu(TEXT("Btn_Resume"), TEXT("RESUME DRIVING"), true);
			AddMenu(TEXT("Btn_CancelSelection"), TEXT("CHANGE VEHICLE"), false);
			AddMenu(TEXT("Btn_Start"), TEXT("HOST: START MATCH"), false);
			AddMenu(TEXT("Btn_Leave"), TEXT("LEAVE ROOM"), false);
		}
		else if (Definition.Panel == EHALGameFlowPanel::Spectator || Definition.Panel == EHALGameFlowPanel::Results)
		{
			auto* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ActionRow"));
			Content->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 8.f, 0, 0));
			const auto AddAction = [&](FName Name, const TCHAR* Label, bool bPrimary)
			{
				auto* Slot = Row->AddChildToHorizontalBox(AddButton(Name, Label, bPrimary));
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Slot->SetPadding(FMargin(4.f, 0));
			};
			if (bSpectator)
			{ AddAction(TEXT("Btn_PreviousSpectator"), TEXT("< PREVIOUS PLAYER"), false); AddAction(TEXT("Btn_NextSpectator"), TEXT("NEXT PLAYER >"), true); }
			else
			{ AddAction(TEXT("Btn_Return"), TEXT("RETURN TO ROOM"), true); AddAction(TEXT("Btn_Leave"), TEXT("LEAVE ROOM"), false); }
		}
		for (int32 I = 0; I < Buttons.Num(); ++I)
		{
			Buttons[I]->SetNavigationRuleExplicit(EUINavigation::Up, Buttons[(I + Buttons.Num() - 1) % Buttons.Num()]);
			Buttons[I]->SetNavigationRuleExplicit(EUINavigation::Down, Buttons[(I + 1) % Buttons.Num()]);
			if (bSpectator || Definition.Panel == EHALGameFlowPanel::Results)
			{
				Buttons[I]->SetNavigationRuleExplicit(EUINavigation::Left, Buttons[(I + Buttons.Num() - 1) % Buttons.Num()]);
				Buttons[I]->SetNavigationRuleExplicit(EUINavigation::Right, Buttons[(I + 1) % Buttons.Num()]);
			}
		}
	}

	void PrepareMissingScreens(bool bCreate)
	{
		auto Report = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Assets;
		bool bAllValid = true;
		for (const auto& Definition : MissingScreens)
		{
			const FString PackagePath = FString(TEXT("/Game/UI/GameFlow/Screens/")) + Definition.Name;
			const FString ObjectPath = PackagePath + TEXT(".") + Definition.Name;
			auto Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("path"), PackagePath);
			auto* BP = FindObject<UWidgetBlueprint>(nullptr, *ObjectPath);
			if (!BP && FPackageName::DoesPackageExist(PackagePath)) { BP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath); }
			bool bCreated = false, bSaved = false;
			if (!BP && bCreate)
			{
				auto* Factory = NewObject<UWidgetBlueprintFactory>(); Factory->ParentClass = UHALGameFlowScreenWidget::StaticClass();
				BP = Cast<UWidgetBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
					Definition.Name, TEXT("/Game/UI/GameFlow/Screens"), UWidgetBlueprint::StaticClass(), Factory));
				if (BP && BP->WidgetTree)
				{
					bCreated = true; BuildScreenLayout(BP, Definition);
					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
					FKismetEditorUtilities::CompileBlueprint(BP);
					if (auto* Defaults = BP->GeneratedClass ? Cast<UHALGameFlowScreenWidget>(BP->GeneratedClass->GetDefaultObject()) : nullptr)
					{
						Defaults->Panel = Definition.Panel; Defaults->Actions = ScreenActions(Definition.Panel);
						Defaults->SetIsFocusable(!Defaults->Actions.IsEmpty());
						BP->MarkPackageDirty();
						if (BP->Status != BS_Error)
						{
							FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone; SaveArgs.SaveFlags = SAVE_NoError;
							bSaved = UPackage::SavePackage(BP->GetPackage(), BP,
								*FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension()), SaveArgs);
						}
					}
				}
			}
			const auto* Defaults = BP && BP->GeneratedClass ? Cast<UHALGameFlowScreenWidget>(BP->GeneratedClass->GetDefaultObject()) : nullptr;
			bool bValid = BP && BP->ParentClass == UHALGameFlowScreenWidget::StaticClass() && BP->Status == BS_UpToDate
				&& BP->WidgetTree && Defaults && Defaults->Panel == Definition.Panel && (!bCreated || bSaved)
				&& BP->WidgetTree->FindWidget<UTextBlock>(TEXT("TitleText")) && BP->WidgetTree->FindWidget<UTextBlock>(TEXT("DetailText"))
				&& BP->WidgetTree->FindWidget<UTextBlock>(TEXT("StatusText"));
			const auto ExpectedActions = ScreenActions(Definition.Panel);
			if (Defaults && BP && BP->WidgetTree)
			{
				bValid &= Defaults->Actions.Num() == ExpectedActions.Num();
				for (const auto& Action : ExpectedActions)
				{ const auto* Found = Defaults->Actions.Find(Action.Key); bValid &= Found && *Found == Action.Value && BP->WidgetTree->FindWidget<UButton>(Action.Key); }
			}
			Entry->SetBoolField(TEXT("created"), bCreated); Entry->SetBoolField(TEXT("saved"), bSaved);
			Entry->SetBoolField(TEXT("valid"), bValid); Entry->SetNumberField(TEXT("action_count"), ExpectedActions.Num());
			Entry->SetStringField(TEXT("parent"), BP ? GetPathNameSafe(BP->ParentClass) : TEXT("missing"));
			Assets.Add(Value(Entry)); bAllValid &= bValid;
			UE_LOG(LogTemp, Display, TEXT("HAL_UI_SIX_SCREEN %s created=%d saved=%d valid=%d"), Definition.Name, bCreated, bSaved, bValid);
		}
		Report->SetArrayField(TEXT("assets"), Assets); Report->SetBoolField(TEXT("all_valid"), bAllValid);
		Report->SetStringField(TEXT("operation"), bCreate ? TEXT("create_missing_only") : TEXT("verify_only"));
		FString JSON; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&JSON));
		const FString Output = FPaths::ProjectSavedDir() / TEXT("UIIntegration/SixScreens/") / (bCreate ? TEXT("Creation.json") : TEXT("Verification.json"));
		FFileHelper::SaveStringToFile(JSON, *Output);
		UE_LOG(LogTemp, Display, TEXT("HAL_UI_SIX_SCREENS_COMPLETE valid=%d report=%s"), bAllValid, *Output);
	}

	void Run(const TArray<FString>& Args)
	{
		if (Args.Num() && Args[0].Equals(TEXT("CreateScreens"), ESearchCase::IgnoreCase)) { PrepareMissingScreens(true); }
		else if (Args.Num() && Args[0].Equals(TEXT("VerifyScreens"), ESearchCase::IgnoreCase)) { PrepareMissingScreens(false); }
		else { UE_LOG(LogTemp, Display, TEXT("HAL.UIFlow CreateScreens | VerifyScreens")); }
	}
	FAutoConsoleCommand Command(TEXT("HAL.UIFlow"), TEXT("CreateScreens for six missing GameFlow pages, or VerifyScreens. Existing pages are never overwritten."), FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
