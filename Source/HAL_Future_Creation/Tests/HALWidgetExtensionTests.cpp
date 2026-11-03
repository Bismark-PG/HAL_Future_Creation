#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALWidgetExtensionTestAccess.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWidgetClassFallbackTest,"HAL.FutureCreation.GameFlow.GF5.WidgetClassesAndNativeFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALWidgetClassFallbackTest::RunTest(const FString&)
{
	auto* GI=NewObject<UHALNetworkGameInstance>();
	TestEqual(TEXT("No custom entry uses current native layout"),FHALWidgetExtensionTestAccess::Entry(GI),UHALArcadeEntryWidget::StaticClass());
	TestEqual(TEXT("No custom room uses current native layout"),FHALWidgetExtensionTestAccess::Room(GI,nullptr),UHALPracticeRoomWidget::StaticClass());
	FHALWidgetExtensionTestAccess::Configure(GI,true);
	TestEqual(TEXT("Configured entry wins"),FHALWidgetExtensionTestAccess::Entry(GI),UHALContractEntryWidget::StaticClass());
	TestEqual(TEXT("Selection is independent of the optional pause class"),FHALWidgetExtensionTestAccess::Room(GI,nullptr),UHALContractSelectionWidget::StaticClass());
	FHALWidgetExtensionTestAccess::Configure(GI,false);
	TestEqual(TEXT("Clearing restores native room"),FHALWidgetExtensionTestAccess::Room(GI,nullptr),UHALPracticeRoomWidget::StaticClass());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALWidgetViewContractTest,"HAL.FutureCreation.GameFlow.GF5.ReadOnlyViewAndUnboundActions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALWidgetViewContractTest::RunTest(const FString&)
{
	for (const UScriptStruct* Type:{FHALEntryView::StaticStruct(),FHALRoomView::StaticStruct(),FHALRoomSeatView::StaticStruct(),FHALVehicleDisplayView::StaticStruct()})
	{ for (TFieldIterator<FProperty> It(Type);It;++It) { TestTrue(TEXT("Presentation values are Blueprint read-only"),It->HasAnyPropertyFlags(CPF_BlueprintReadOnly)); } }
	auto* Screen=NewObject<UHALPracticeRoomWidget>(); const auto V=Screen->GetRoomView();
	TestFalse(TEXT("Unbound screen cannot start or confirm"),V.bRoomAvailable || V.bCanStart || V.bCanConfirm || V.bCanReturnToRoom);
	Screen->StartRoomMatch(); Screen->ConfirmSelection(); Screen->CancelSelection(); Screen->ResumeDriving(); Screen->ReturnToRoom();
	TestFalse(TEXT("Requests cannot manufacture a room"),Screen->GetRoomView().bRoomAvailable);
	return true;
}
#endif
