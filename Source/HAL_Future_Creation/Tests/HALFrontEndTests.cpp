#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HALRoomTestSupport.h"
#include "../HALFrontEndConfig.h"
#include "../HALMenuFeedback.h"
#include "../HALVehicleCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFrontEndMetadataTest,"HAL.FutureCreation.GameFlow.GF5.CosmeticsDoNotChangeVehicleAllowlist",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFrontEndMetadataTest::RunTest(const FString&)
{
	auto* C=FHALRoomTestAccess::Config(GetTransientPackage(),EHALRoomProfile::Development);
	FString Error; auto* Catalog=C->Catalog.Get();
	UClass* Class=Catalog->Resolve(TEXT("Car_01"),Error); TestNotNull(TEXT("Existing authored driving vehicle"),Class);
	Catalog->Vehicles[0].Icon=TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Missing_Optional_Icon.Missing_Optional_Icon")));
	Catalog->Vehicles[0].PreviewImage=TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Missing_Optional_Preview.Missing_Optional_Preview")));
	Catalog->Vehicles[0].DisplayName=FText::GetEmpty();
	TestTrue(TEXT("Absent cosmetics must not invalidate authoritative vehicle config"),Catalog->Validate(Error));
	TestEqual(TEXT("Artwork cannot select or replace the driving class"),Catalog->Resolve(TEXT("Car_01"),Error),Class);
	TestNull(TEXT("Presentation never allows unknown IDs"),Catalog->Resolve(TEXT("Unlisted"),Error));
	const auto* Front=GetDefault<UHALFrontEndConfig>();
	TestFalse(TEXT("Native title works without a DA"),Front->Title.IsEmpty());
	TestTrue(TEXT("No mandatory artwork"),!Front->Background && !Front->Logo);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALFrontEndFeedbackTest,"HAL.FutureCreation.GameFlow.GF5.ActionableConnectionErrors",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALFrontEndFeedbackTest::RunTest(const FString&)
{
	for (const auto* Raw:{TEXT("Room is full"),TEXT("Match already started"),TEXT("Host closed the room"),TEXT("ConnectionTimeout: /Script/Engine.PendingNetGame"),TEXT("Enter a numeric host IPv4 address")})
	{
		const auto Message=HALMenuFeedback::Connection(FText::FromString(Raw)).ToString();
		TestFalse(TEXT("Readable nonempty recovery instruction"),Message.IsEmpty());
		TestFalse(TEXT("No engine type or asset path in player feedback"),Message.Contains(TEXT("/Script/")));
	}
	TestTrue(TEXT("Full room invites alternate host"),HALMenuFeedback::Connection(FText::FromString(TEXT("Room full"))).ToString().Contains(TEXT("another host")));
	TestTrue(TEXT("Started room explains next round"),HALMenuFeedback::Connection(FText::FromString(TEXT("Room locked"))).ToString().Contains(TEXT("next round")));
	return true;
}
#endif
