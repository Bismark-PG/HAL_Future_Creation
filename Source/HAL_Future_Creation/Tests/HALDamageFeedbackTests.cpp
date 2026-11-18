#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../HALDamageFeedbackConfig.h"
#include "../HALVehicleStatusFXSettings.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDamageFeedbackTimingTest,"HAL.FutureCreation.DamageFeedback.BoundedIndependentPulses",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALDamageFeedbackTimingTest::RunTest(const FString&)
{
	FHALDamageFeedbackStyle S; FHALDamageFeedbackPlayback A,B;
	TestTrue(TEXT("Default style valid"),S.IsValid()); TestTrue(TEXT("First hit accepted"),A.Receive(S,10));
	const auto Peak=A.Weights(S,10); TestTrue(TEXT("Body/white/RGB peak together"),Peak.Equals(FVector::OneVector,.0001));
	TestTrue(TEXT("Independent car remains untouched"),B.Weights(S,10).IsZero());
	TestTrue(TEXT("Second car starts at its own time"),B.Receive(S,10.1));
	TestTrue(TEXT("RGB and white finish before red"),A.Weights(S,10.12).X>0&&A.Weights(S,10.12).Y==0&&A.Weights(S,10.12).Z==0);
	A.Receive(S,10.05); TestEqual(TEXT("Fast repeated hit does not restart bright accents"),A.LastAccent,10.);
	TestTrue(TEXT("Body still refreshes for repeated damage"),A.LastBody==10.05&&A.Weights(S,10.1).X>0);
	for(int32 I=2;I<=12;++I) { A.Receive(S,10+I*.05); }
	TestTrue(TEXT("Continuous spam has a hard visible limit"),A.Weights(S,10.6).IsZero());
	TestFalse(TEXT("Brief rest rejects cosmetic retrigger only"),A.Receive(S,10.65));
	TestTrue(TEXT("Next burst can start after rest"),A.Receive(S,10.8));
	TestTrue(TEXT("An isolated hit finishes without an explicit remove event"),B.Weights(S,10.5).IsZero());
	FHALDamageFeedbackPlayback Gap; S.MinimumAccentInterval=.6; Gap.Receive(S,20); Gap.Receive(S,20.3);
	TestEqual(TEXT("Separated red pulses still honor global white/RGB frequency bound"),Gap.LastAccent,20.);
	TestTrue(TEXT("Late observer evaluates current age, never replays a stale flash"),Gap.Weights(S,21).IsZero());
	S.OutlinePixels=std::numeric_limits<float>::quiet_NaN(); TestFalse(TEXT("Nonfinite render values rejected"),S.IsValid());
	S={}; S.FirstStencil=252; TestFalse(TEXT("Reserved range may not overflow stencil byte"),S.IsValid());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHALDamageFeedbackConfigTest,"HAL.FutureCreation.DamageFeedback.SharedRecipeAndCookRoot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHALDamageFeedbackConfigTest::RunTest(const FString&)
{
	auto* D=LoadObject<UHALDamageFeedbackConfig>(nullptr,TEXT("/Game/Data/StatusFX/DA_DamageFeedback_Default")); FString Error;
	if(!TestNotNull(TEXT("Saved globally shared recipe"),D)) { return false; }
	TestTrue(TEXT("Post Process contract and all defaults valid"),D->Validate(Error));
	const auto* Settings=GetDefault<UHALVehicleStatusFXSettings>(); TestEqual(TEXT("Global setting resolves the shared DA"),Settings->DamageFeedbackConfig.ToSoftObjectPath(),FSoftObjectPath(D));
#if WITH_EDITOR
	TArray<FName> Packages; TestTrue(TEXT("Cook collector validates global received-damage dependencies"),Settings->CollectCookPackages(Packages,Error));
	TestTrue(TEXT("Damage recipe explicitly rooted for cooking"),Packages.Contains(D->GetPackage()->GetFName()));
#endif
	FHALDamageFeedbackStyle S; S.MinimumAccentInterval=.05; TestFalse(TEXT("Accents cannot run longer than the permitted pulse interval"),S.IsValid());
	return true;
}
#endif
