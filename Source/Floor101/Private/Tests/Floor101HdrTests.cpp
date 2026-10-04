#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Floor101Hdr.h"
#include "Floor101Display.h"
#include "Floor101GameViewportClient.h"
#include "InputCoreTypes.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101HDRUnavailableTest, "Floor101.Rendering.HDRUnavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101HDRUnavailableTest::RunTest(const FString& Parameters)
{
	const IConsoleVariable* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.EnableHDROutput"));
	const int32 Before = Enabled ? Enabled->GetInt() : 0;
	const Floor101::HDR::FStatus Status = Floor101::HDR::Query(nullptr);
	TestFalse(TEXT("no viewport has no HDR display"), Status.bAvailable);
	TestFalse(TEXT("cannot enable HDR without a window"), Floor101::HDR::SetEnabled(nullptr, true, false));
	TestFalse(TEXT("reports SDR"), Status.bEnabled);
	TestTrue(TEXT("explains the Windows requirement"), Floor101::HDR::Tooltip(Status).ToString().Contains(TEXT("Windows")));
	TestEqual(TEXT("rejected request leaves output untouched"), Enabled ? Enabled->GetInt() : 0, Before);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101DisplayUnavailableTest, "Floor101.Rendering.DisplayUnavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101DisplayUnavailableTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("cannot enter fullscreen without a window"), Floor101::Display::ToggleFullscreen(nullptr, false));
	TestFalse(TEXT("cannot apply a resolution without a monitor"), Floor101::Display::SetResolution(nullptr, FIntPoint(1920, 1080), false));
	TestEqual(TEXT("no window has no selectable monitor modes"), Floor101::Display::Resolutions(nullptr).Num(), 0);
	TestTrue(TEXT("F11 toggles fullscreen"), UFloor101GameViewportClient::MapKey(EKeys::F11, false, false, false) == EFloor101KeyCommand::Fullscreen);
	TestTrue(TEXT("holding F11 does not repeatedly toggle"), UFloor101GameViewportClient::MapKey(EKeys::F11, false, false, true) == EFloor101KeyCommand::None);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101VSyncTest, "Floor101.Rendering.VSync", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101VSyncTest::RunTest(const FString& Parameters)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	if (!TestNotNull(TEXT("settings"), Settings) || !TestNotNull(TEXT("VSync runtime variable"), Variable)) { return false; }
	const int32 OriginalValue = Variable->GetInt();
	const bool bOriginalPreference = Settings->IsVSyncEnabled();
	ON_SCOPE_EXIT
	{
		Variable->SetWithCurrentPriority(OriginalValue);
		Settings->SetVSyncEnabled(bOriginalPreference);
	};
	const FIntPoint Resolution = Settings->GetScreenResolution();
	const EWindowMode::Type WindowMode = Settings->GetFullscreenMode();
	const bool bHDR = Settings->IsHDREnabled();
	const float FrameCap = Settings->GetFrameRateLimit();
	const float RenderScale = Settings->GetResolutionScaleNormalized();
	for (const bool bEnabled : {true, false})
	{
		TestTrue(TEXT("VSync change succeeds"), Floor101::Display::SetVSyncEnabled(bEnabled, false));
		TestEqual(TEXT("effective runtime state matches button"), Floor101::Display::IsVSyncEnabled(), bEnabled);
		TestEqual(TEXT("preference matches effective state"), Settings->IsVSyncEnabled(), bEnabled);
		TestEqual(TEXT("resolution is preserved"), Settings->GetScreenResolution(), Resolution);
		TestTrue(TEXT("window mode is preserved"), Settings->GetFullscreenMode() == WindowMode);
		TestEqual(TEXT("HDR preference is preserved"), Settings->IsHDREnabled(), bHDR);
		TestEqual(TEXT("frame cap is preserved"), Settings->GetFrameRateLimit(), FrameCap);
		TestEqual(TEXT("render scale is preserved"), Settings->GetResolutionScaleNormalized(), RenderScale);
	}
	return true;
}
#endif
