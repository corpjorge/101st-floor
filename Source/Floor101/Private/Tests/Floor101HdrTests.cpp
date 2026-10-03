#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Floor101Hdr.h"
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
#endif
