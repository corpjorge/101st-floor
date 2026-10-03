#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Floor101Hdr.h"
#include "Floor101Display.h"
#include "Floor101GameViewportClient.h"
#include "InputCoreTypes.h"
#include "HAL/IConsoleManager.h"

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
#endif
