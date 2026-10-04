#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Floor101Preferences.h"
#include "Floor101GameViewportClient.h"
#include "Floor101Display.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101QualityIsolationTest, "Floor101.Interface.QualityIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101QualityIsolationTest::RunTest(const FString& Parameters)
{
	const auto Original = Scalability::GetQualityLevels();
	ON_SCOPE_EXIT { Scalability::SetQualityLevels(Original); };
	const bool VSync = Floor101::Display::IsVSyncEnabled();
	const IConsoleVariable* HDR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.EnableHDROutput"));
	const int32 HDROutput = HDR ? HDR->GetInt() : 0;
	for (int32 Profile = -1; Profile <= 3; ++Profile)
	{
		const auto Policy = Floor101::Preferences::QualityLevels(Profile);
		TestEqual(TEXT("quality preserves native render policy"), Policy.ResolutionQuality, 100.0f);
		TestTrue(TEXT("Lumen GI remains enabled"), Policy.GlobalIlluminationQuality >= 2);
		TestTrue(TEXT("Lumen reflections remain enabled"), Policy.ReflectionQuality >= 2);
		Floor101::Preferences::SetGraphicsQuality(Profile, false);
		TestEqual(TEXT("quality leaves VSync alone"), Floor101::Display::IsVSyncEnabled(), VSync);
		TestEqual(TEXT("quality leaves HDR alone"), HDR ? HDR->GetInt() : 0, HDROutput);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101OptionsInputTest, "Floor101.Interface.OptionsInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101OptionsInputTest::RunTest(const FString& Parameters)
{
	auto* Client = NewObject<UFloor101GameViewportClient>(GEngine);
	int32 Closed = 0;
	Client->SetInterfaceOpen(true, FSimpleDelegate::CreateLambda([Client, &Closed]() { ++Closed; Client->SetInterfaceOpen(false); }));
	TestTrue(TEXT("movement is intercepted while options own input"), Client->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::W, IE_Pressed, 0ULL)));
	TestTrue(TEXT("typed length is intercepted"), Client->InputChar(nullptr, 0, TEXT('3')));
	TestTrue(TEXT("mouse wheel is intercepted"), Client->InputAxis(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::MouseWheelAxis, IE_Axis, 0ULL)));
	TestTrue(TEXT("Escape closes options before CAD cancellation"), Client->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::Escape, IE_Pressed, 0ULL)));
	TestEqual(TEXT("one close request"), Closed, 1);
	return true;
}
#endif
