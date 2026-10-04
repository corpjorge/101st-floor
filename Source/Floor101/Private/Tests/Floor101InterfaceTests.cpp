#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Floor101Preferences.h"
#include "Floor101GameViewportClient.h"
#include "Floor101Display.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Misc/ScopeExit.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Floor101Catalogue.h"
#include "Floor101FileActions.h"
#include "SFloor101Overlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CatalogueFilterTest, "Floor101.Interface.CatalogueFilters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101CatalogueFilterTest::RunTest(const FString& Parameters)
{
	using Floor101::Catalogue::Matches;
	TestTrue(TEXT("accent and case insensitive search"), Matches(TEXT("Sofá modular"), TEXT("  SOFA  "), false, false));
	TestTrue(TEXT("every query word can match in any order"), Matches(TEXT("Silla de roble"), TEXT("roble silla"), false, false));
	TestFalse(TEXT("all words must match"), Matches(TEXT("Silla de roble"), TEXT("roble cama"), false, true));
	TestFalse(TEXT("favorites restrict results"), Matches(TEXT("Silla"), TEXT("silla"), true, false));
	TestTrue(TEXT("favorite can match"), Matches(TEXT("Silla"), TEXT("silla"), true, true));
	TestTrue(TEXT("empty search shows all"), Matches(TEXT("Silla"), TEXT(""), false, false));
	TestFalse(TEXT("empty favorite list shows no items"), Matches(TEXT("Silla"), TEXT(""), true, false));
	const auto Original = Floor101::Catalogue::Favorites();
	ON_SCOPE_EXIT { Floor101::Catalogue::SaveFavorites(Original); };
	Floor101::Catalogue::SaveFavorites({TEXT("furniture:chair"), TEXT("finish:wood-oak"), TEXT("furniture:chair"), TEXT("")});
	const auto Saved = Floor101::Catalogue::Favorites();
	TestEqual(TEXT("favorites persist uniquely"), Saved.Num(), 2);
	TestTrue(TEXT("stable furniture key survives"), Saved.Contains(TEXT("furniture:chair")));
	TestTrue(TEXT("stable finish key survives"), Saved.Contains(TEXT("finish:wood-oak")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101RecentProjectsTest, "Floor101.Interface.RecentProjects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101RecentProjectsTest::RunTest(const FString& Parameters)
{
	TArray<FString> Original;
	GConfig->GetArray(TEXT("Floor101.Paths"), TEXT("RecentProjects"), Original, GGameUserSettingsIni);
	const FString Last = FFloor101FileActions::GetLastProjectPath();
	ON_SCOPE_EXIT
	{
		GConfig->SetString(TEXT("Floor101.Paths"), TEXT("LastProject"), *Last, GGameUserSettingsIni);
		GConfig->SetArray(TEXT("Floor101.Paths"), TEXT("RecentProjects"), Original, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	};
	GConfig->SetArray(TEXT("Floor101.Paths"), TEXT("RecentProjects"), {}, GGameUserSettingsIni);
	GConfig->SetString(TEXT("Floor101.Paths"), TEXT("LastProject"), TEXT(""), GGameUserSettingsIni);
	TestTrue(TEXT("first run has no recent projects"), FFloor101FileActions::GetRecentProjects().IsEmpty());
	for (int32 Index = 0; Index < 10; ++Index) { FFloor101FileActions::RememberProjectPath(FPaths::Combine(FPaths::ProjectSavedDir(), FString::Printf(TEXT("Recent%d.json"), Index))); }
	const auto Recent = FFloor101FileActions::GetRecentProjects();
	TestEqual(TEXT("history is bounded"), Recent.Num(), 8);
	TestTrue(TEXT("most recent comes first"), Recent[0].EndsWith(TEXT("Recent9.json")));
	TestFalse(TEXT("oldest is removed"), Recent.ContainsByPredicate([](const FString& Path) { return Path.EndsWith(TEXT("Recent0.json")); }));
	FFloor101FileActions::RememberProjectPath(Recent[3].ToUpper());
	const auto Reopened = FFloor101FileActions::GetRecentProjects();
	TestEqual(TEXT("Windows case variants do not duplicate files"), Reopened.Num(), 8);
	TestTrue(TEXT("reopening promotes a file"), Reopened[0].Equals(Recent[3], ESearchCase::IgnoreCase));
	TestFalse(TEXT("recent paths are absolute"), FPaths::IsRelative(Reopened[0]));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101HomeOptionsTest, "Floor101.Interface.HomeOptionsInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101HomeOptionsTest::RunTest(const FString& Parameters)
{
	auto* Client = NewObject<UFloor101GameViewportClient>(GEngine);
	const TSharedRef<SFloor101Overlay> Overlay = SNew(SFloor101Overlay).ViewportClient(Client);
	Overlay->PrepareReview(TEXT("home"));
	TestTrue(TEXT("home owns CAD and camera input"), Client->IsInterfaceOpen());
	Overlay->PrepareReview(TEXT("options"));
	const FInputKeyEventArgs Escape(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::Escape, IE_Pressed, 0ULL);
	TestTrue(TEXT("Escape closes the upper options modal"), Client->InputKey(Escape));
	TestTrue(TEXT("closing options keeps the home screen blocking CAD"), Client->IsInterfaceOpen());
	TestTrue(TEXT("typing cannot edit behind home"), Client->InputChar(nullptr, 0, TEXT('3')));
	TestTrue(TEXT("second Escape closes home"), Client->InputKey(Escape));
	TestFalse(TEXT("closing home returns input to workspace"), Client->IsInterfaceOpen());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101SearchInputTest, "Floor101.Interface.SearchInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101SearchInputTest::RunTest(const FString& Parameters)
{
	auto* Client = NewObject<UFloor101GameViewportClient>(GEngine);
	const TWeakPtr<SWidget> OriginalFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	const TSharedRef<SEditableTextBox> Field = SNew(SEditableTextBox);
	const TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(200.0, 60.0))[Field];
	FSlateApplication::Get().AddWindow(Window, false); // hidden native window, solely for a valid Slate focus path
	ON_SCOPE_EXIT
	{
		FSlateApplication::Get().ClearKeyboardFocus();
		Window->RequestDestroyWindow();
		if (const auto Previous = OriginalFocus.Pin()) { FSlateApplication::Get().SetKeyboardFocus(Previous); }
	};
	FSlateApplication::Get().SetKeyboardFocus(Field);
	TestTrue(TEXT("search field has text focus"), Field->HasKeyboardFocus() || Field->HasFocusedDescendants());
	for (const FKey& Key : {EKeys::W, EKeys::A, EKeys::M, EKeys::Delete})
	{
		TestTrue(TEXT("unhandled search keys do not reach CAD"), Client->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, IE_Pressed, 0ULL)));
	}
	TestTrue(TEXT("typing cannot append CAD dimensions"), Client->InputChar(nullptr, 0, TEXT('3')));
	TestTrue(TEXT("search focus prevents wheel zoom"), Client->InputAxis(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::MouseWheelAxis, IE_Axis, 0ULL)));
	return true;
}
#endif
