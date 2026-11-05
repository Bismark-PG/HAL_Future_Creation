#include "HALWidgetExtensionTestWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

namespace
{
	UButton* AddButton(UWidgetTree* Tree,UVerticalBox* Panel,const TCHAR* Name)
	{
		auto* B=Tree->ConstructWidget<UButton>();
		auto* Text=Tree->ConstructWidget<UTextBlock>(); Text->SetText(FText::FromString(Name)); B->AddChild(Text); Panel->AddChild(B); return B;
	}
}
void UHALContractEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	auto* Panel=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("DesignerRoot")); WidgetTree->RootWidget=Panel;
	StateText=WidgetTree->ConstructWidget<UTextBlock>(); Panel->AddChild(StateText);
	HostButton=AddButton(WidgetTree,Panel,TEXT("CUSTOM ENTRY - HOST")); HostButton->OnClicked.AddDynamic(this,&ThisClass::ClickHost);
	SetMenuFocusTarget(HostButton);
}
void UHALContractEntryWidget::ReceiveEntryView_Implementation(const FHALEntryView& View)
{ ++Updates; if (StateText) { StateText->SetText(View.Message); } }
void UHALContractRoomWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Panel=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("DesignerRoot")); WidgetTree->RootWidget=Panel;
	StateText=WidgetTree->ConstructWidget<UTextBlock>(); Panel->AddChild(StateText);
	NextButton=AddButton(WidgetTree,Panel,TEXT("CUSTOM - NEXT")); NextButton->OnClicked.AddDynamic(this,&ThisClass::ClickNext);
	ConfirmButton=AddButton(WidgetTree,Panel,TEXT("CUSTOM - CONFIRM")); ConfirmButton->OnClicked.AddDynamic(this,&ThisClass::ClickConfirm);
	CancelButton=AddButton(WidgetTree,Panel,TEXT("CUSTOM - CHANGE")); CancelButton->OnClicked.AddDynamic(this,&ThisClass::ClickCancel);
	StartButton=AddButton(WidgetTree,Panel,TEXT("CUSTOM - START")); StartButton->OnClicked.AddDynamic(this,&ThisClass::ClickStart);
	ResumeButton=AddButton(WidgetTree,Panel,TEXT("CUSTOM - RESUME")); ResumeButton->OnClicked.AddDynamic(this,&ThisClass::ClickResume);
	ReturnButton=AddButton(WidgetTree,Panel,TEXT("CUSTOM - RETURN")); ReturnButton->OnClicked.AddDynamic(this,&ThisClass::ClickReturn);
	LeaveButton=AddButton(WidgetTree,Panel,TEXT("CUSTOM - LEAVE")); LeaveButton->OnClicked.AddDynamic(this,&ThisClass::ClickLeave);
	SetMenuFocusTarget(NextButton);
}
void UHALContractRoomWidget::ReceiveRoomView_Implementation(const FHALRoomView& V)
{
	++Updates; LastMatch=V.Match; LastWasResults=V.bShowResults; if (!Panel) { return; }
	Panel->SetVisibility((V.bMenuVisible || V.bSpectating || V.bShowResults)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	StateText->SetText(FText::FromString(GetClass()->GetName()+TEXT(" | ")+(V.bShowResults?V.ResultSummary:V.Status).ToString()));
	NextButton->SetIsEnabled(V.bCanCycleSpectator || (V.bCanSelect && V.bCatalogLoaded)); ConfirmButton->SetIsEnabled(V.bCanConfirm);
	CancelButton->SetIsEnabled(V.bCanCancel); StartButton->SetIsEnabled(V.bCanStart); ResumeButton->SetIsEnabled(V.bCanResume);
	ReturnButton->SetIsEnabled(V.bCanReturnToRoom); LeaveButton->SetIsEnabled(V.bCanLeave);
}
