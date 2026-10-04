#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Floor101Preferences.h"
#include "Floor101.h"
#include "Misc/FileHelper.h"
#include "Floor101GameViewportClient.h"
#include "Floor101Display.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Floor101DocumentSubsystem.h"
#include "F101/Cad/Document.h"
#include "HAL/FileManager.h"
#include "Floor101StudioEnvironment.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "DynamicRHI.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101DiagnosticPreferencesTest, "Floor101.Interface.DiagnosticPreferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101DiagnosticPreferencesTest::RunTest(const FString& Parameters)
{
	const FString OriginalIni = GGameUserSettingsIni;
	const ELogVerbosity::Type OriginalVerbosity = LogFloor101.GetVerbosity();
	const FString TestIni = FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("Diagnostics"), TEXT(".ini")));
	GGameUserSettingsIni = TestIni;
	ON_SCOPE_EXIT
	{
		GGameUserSettingsIni = OriginalIni;
		LogFloor101.SetVerbosity(OriginalVerbosity);
		GConfig->UnloadFile(TestIni);
		IFileManager::Get().Delete(*TestIni);
	};
	if (!TestTrue(TEXT("temporary preferences created"), FFileHelper::SaveStringToFile(TEXT("[SectionsToSave]\nbCanSaveAllSections=true\n"), *TestIni))) { return false; }
	GConfig->LoadFile(TestIni);
	TestFalse(TEXT("debug defaults to off"), Floor101::Preferences::DiagnosticMode());
	TestFalse(TEXT("console defaults to hidden"), Floor101::Preferences::ShowLogWindow());
	Floor101::Preferences::SetDiagnosticMode(true);
	TestEqual(TEXT("debug applies detailed product logging immediately"), LogFloor101.GetVerbosity(), ELogVerbosity::VeryVerbose);
	TestFalse(TEXT("debug never forces a separate window"), Floor101::Preferences::ShowLogWindow());
	Floor101::Preferences::SetShowLogWindow(true, false); // persistence check without creating a native test window
	GConfig->UnloadFile(TestIni);
	GConfig->LoadFile(TestIni);
	TestTrue(TEXT("debug survives restart"), Floor101::Preferences::DiagnosticMode());
	TestTrue(TEXT("window preference survives restart"), Floor101::Preferences::ShowLogWindow());
	Floor101::Preferences::SetDiagnosticMode(false);
	TestEqual(TEXT("normal logging is restored, not disabled"), LogFloor101.GetVerbosity(), ELogVerbosity::Log);
	TestTrue(TEXT("window remains independently enabled"), Floor101::Preferences::ShowLogWindow());
	Floor101::Preferences::SetShowLogWindow(false, false);
	TestFalse(TEXT("window can be disabled independently"), Floor101::Preferences::ShowLogWindow());
	return true;
}

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101UnsavedModalTest, "Floor101.Interface.UnsavedModal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101UnsavedModalTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101UnsavedModalWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* D = World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document"), D)) { return false; }
	(void)D->GetMutableDocument().addWall({0, 0}, {3000, 0}, 120, 2800);
	auto* Client = NewObject<UFloor101GameViewportClient>(GEngine);
	const TSharedRef<SFloor101Overlay> Overlay = SNew(SFloor101Overlay).Document(D).ViewportClient(Client);
	int32 Proceeds = 0;
	int32 Replaced = 0;
	const FSimpleDelegate Proceed = FSimpleDelegate::CreateLambda([&Proceeds]() { ++Proceeds; });
	const FInputKeyEventArgs Escape(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::Escape, IE_Pressed, 0ULL);
	const int32 WindowsBefore = FSlateApplication::Get().GetTopLevelWindows().Num();
	Overlay->PrepareReview(TEXT("home"));
	GEngine->GetWorldContextFromWorldChecked(World).GameViewport = Client;
	TestTrue(TEXT("replacement routes to the viewport prompt"), FFloor101FileActions::RequestReplace(*D, Proceed));
	TestTrue(TEXT("confirmation is visible inside overlay"), Overlay->IsUnsavedPromptOpen());
	TestEqual(TEXT("confirmation creates no separate window"), FSlateApplication::Get().GetTopLevelWindows().Num(), WindowsBefore);
	TestTrue(TEXT("confirmation blocks keyboard editing"), Client->InputChar(nullptr, 0, TEXT('3')));
	TestTrue(TEXT("confirmation blocks camera wheel"), Client->InputAxis(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::MouseWheelAxis, IE_Axis, 0ULL)));
	Client->InputKey(Escape);
	TestFalse(TEXT("Escape cancels confirmation"), Overlay->IsUnsavedPromptOpen());
	TestEqual(TEXT("cancel does not run pending action"), Proceeds, 0);
	TestTrue(TEXT("cancel preserves unsaved model"), D->HasUnsavedChanges() && D->GetDocument().project().walls.size() == 1);
	TestTrue(TEXT("cancel keeps underlying home modal active"), Client->IsInterfaceOpen());
	Client->InputKey(Escape);
	TestFalse(TEXT("leaving home restores viewport input"), Client->IsInterfaceOpen());
	Client->RequestUnsavedAction(true, Proceed);
	Client->RequestUnsavedAction(false, FSimpleDelegate::CreateLambda([&Replaced]() { ++Replaced; }));
	Overlay->ResolveUnsavedPrompt(EAppReturnType::No);
	Overlay->ResolveUnsavedPrompt(EAppReturnType::No);
	TestEqual(TEXT("discard continues exactly once"), Proceeds, 1);
	TestEqual(TEXT("repeated requests cannot replace pending action"), Replaced, 0);
	TestFalse(TEXT("discard releases modal input"), Client->IsInterfaceOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101UnsavedSaveTest, "Floor101.Interface.UnsavedSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101UnsavedSaveTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101UnsavedSaveWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101UnsavedSave.json"));
	const FString OriginalLast = FFloor101FileActions::GetLastProjectPath();
	TArray<FString> OriginalRecent;
	GConfig->GetArray(TEXT("Floor101.Paths"), TEXT("RecentProjects"), OriginalRecent, GGameUserSettingsIni);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().Delete(*Path);
		IFileManager::Get().DeleteDirectory(*(Path + TEXT(".tmp")), false, false);
		GConfig->SetString(TEXT("Floor101.Paths"), TEXT("LastProject"), *OriginalLast, GGameUserSettingsIni);
		GConfig->SetArray(TEXT("Floor101.Paths"), TEXT("RecentProjects"), OriginalRecent, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	};
	auto* D = World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document"), D) || !TestTrue(TEXT("initial save"), D->SaveProject(Path).ok())) { return false; }
	(void)D->GetMutableDocument().addWall({0, 0}, {3000, 0}, 120, 2800);
	auto* Client = NewObject<UFloor101GameViewportClient>(GEngine);
	const TSharedRef<SFloor101Overlay> Overlay = SNew(SFloor101Overlay).Document(D).ViewportClient(Client);
	int32 Proceeds = 0;
	Client->RequestUnsavedAction(false, FSimpleDelegate::CreateLambda([&Proceeds]() { ++Proceeds; }));
	TestTrue(TEXT("create a blocked temporary save path"), IFileManager::Get().MakeDirectory(*(Path + TEXT(".tmp")), false));
	AddExpectedMessagePlain(TEXT("F101: Cannot write"), ELogVerbosity::Warning);
	Overlay->ResolveUnsavedPrompt(EAppReturnType::Yes);
	TestEqual(TEXT("failed save never continues"), Proceeds, 0);
	TestTrue(TEXT("failed save keeps work and modal available"), D->HasUnsavedChanges() && Overlay->IsUnsavedPromptOpen() && Client->IsInterfaceOpen());
	Overlay->ResolveUnsavedPrompt(EAppReturnType::Cancel);
	TestFalse(TEXT("failed save can still be cancelled"), Overlay->IsUnsavedPromptOpen());
	IFileManager::Get().DeleteDirectory(*(Path + TEXT(".tmp")), false, false);
	Client->RequestUnsavedAction(false, FSimpleDelegate::CreateLambda([&Proceeds]() { ++Proceeds; }));
	Overlay->ResolveUnsavedPrompt(EAppReturnType::Yes);
	TestEqual(TEXT("successful save continues once"), Proceeds, 1);
	TestFalse(TEXT("save clears dirty state"), D->HasUnsavedChanges());
	TestFalse(TEXT("save closes confirmation"), Overlay->IsUnsavedPromptOpen());
	TestFalse(TEXT("save restores editing input"), Client->IsInterfaceOpen());
	// The asynchronous callback must also survive removal of the surface without executing.
	Client->UnsavedPrompt.Unbind();
	TestFalse(TEXT("missing UI refuses an unsaved action"), Client->RequestUnsavedAction(true, FSimpleDelegate()));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101PlanDestinationTest, "Floor101.Interface.PlanDestination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101PlanDestinationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101PlanDestinationWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	const FString Folder = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101PlanDestination"), FGuid::NewGuid().ToString());
	FString OriginalFolder;
	const bool bHadFolder = GConfig->GetString(TEXT("Floor101.Paths"), TEXT("LastPlanFolder"), OriginalFolder, GGameUserSettingsIni);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().DeleteDirectory(*Folder, false, true);
		if (bHadFolder) { GConfig->SetString(TEXT("Floor101.Paths"), TEXT("LastPlanFolder"), *OriginalFolder, GGameUserSettingsIni); }
		else { GConfig->RemoveKey(TEXT("Floor101.Paths"), TEXT("LastPlanFolder"), GGameUserSettingsIni); }
		GConfig->Flush(false, GGameUserSettingsIni);
		GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	};
	auto* D = World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document"), D)) { return false; }
	(void)D->GetMutableDocument().addWall({0, 0}, {3000, 0}, 120, 2800);
	IFileManager::Get().MakeDirectory(*Folder, true);
	GConfig->RemoveKey(TEXT("Floor101.Paths"), TEXT("LastPlanFolder"), GGameUserSettingsIni);
	TestFalse(TEXT("cancel does not export"), FFloor101FileActions::ExportPlanToChosenPath(*D, TEXT("svg"),
		[this, D](const FString& Proposed, FString& Path)
		{
			TestEqual(TEXT("suggestion follows current level/project"), Proposed, D->GetDefaultPlanPath(TEXT("svg")));
			return false;
		}));
	for (const FString Extension : {FString(TEXT("svg")), FString(TEXT("png"))})
	{
		const FString Chosen = FPaths::Combine(Folder, TEXT("mi-plano.") + Extension);
		const bool bNeedsRenderer = Extension == TEXT("png") && GUsingNullRHI;
		if (bNeedsRenderer) { AddExpectedMessagePlain(TEXT("F101: Exporting a PNG needs a renderer"), ELogVerbosity::Warning); }
		const bool bExported = FFloor101FileActions::ExportPlanToChosenPath(*D, Extension,
			[Chosen](const FString& Proposed, FString& Path) { Path = Chosen; return true; });
		TestEqual(TEXT("chosen export obeys renderer availability"), bExported, !bNeedsRenderer);
		TestEqual(TEXT("chosen file exists only after successful export"), IFileManager::Get().FileExists(*Chosen), !bNeedsRenderer);
	}
	TestFalse(TEXT("cancel after export does not write another file"), FFloor101FileActions::ExportPlanToChosenPath(*D, TEXT("svg"),
		[this, Folder](const FString& Proposed, FString& Path)
		{
			TestEqual(TEXT("next export starts in chosen folder"), FPaths::GetPath(Proposed), FPaths::ConvertRelativePathToFull(Folder));
			return false;
		}));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Folder / TEXT("*")), true, false);
	TestEqual(TEXT("cancel has no file side effects"), Files.Num(), GUsingNullRHI ? 1 : 2);
	TestTrue(TEXT("export preserves unsaved CAD work"), D->HasUnsavedChanges());
	TestEqual(TEXT("export preserves geometry"), D->GetDocument().project().walls.size(), std::size_t{1});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101FloorGridTest, "Floor101.Interface.FloorGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFloor101FloorGridTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101FloorGridWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	bool bOriginal = false;
	const bool bHadGrid = GConfig->GetBool(TEXT("Floor101.Interface"), TEXT("ShowGrid"), bOriginal, GGameUserSettingsIni);
	ON_SCOPE_EXIT
	{
		if (bHadGrid) { GConfig->SetBool(TEXT("Floor101.Interface"), TEXT("ShowGrid"), bOriginal, GGameUserSettingsIni); }
		else { GConfig->RemoveKey(TEXT("Floor101.Interface"), TEXT("ShowGrid"), GGameUserSettingsIni); }
		GConfig->Flush(false, GGameUserSettingsIni);
		GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	};
	GConfig->RemoveKey(TEXT("Floor101.Interface"), TEXT("ShowGrid"), GGameUserSettingsIni);
	TestTrue(TEXT("ground grid is visible by default"), Floor101::Preferences::ShowGrid());
	auto* D = World->GetSubsystem<UFloor101DocumentSubsystem>();
	auto* Floor = World->SpawnActor<AStaticMeshActor>();
	auto* Mesh = Floor->GetStaticMeshComponent();
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/MapTemplates/SM_Template_Map_Floor.SM_Template_Map_Floor")));
	UMaterialInterface* Original = Mesh->GetMaterial(0);
	if (!TestNotNull(TEXT("original template grid material"), Original)) { return false; }
	auto* Environment = World->SpawnActor<AFloor101StudioEnvironment>();
	D->SetConstructionAidsVisible(true);
	Environment->Tick(0.016f);
	TestTrue(TEXT("design keeps original grid across the floor"), Mesh->GetMaterial(0) == Original);
	Floor101::Preferences::SetShowGrid(false);
	Environment->Tick(0.016f);
	UMaterialInterface* Neutral = Mesh->GetMaterial(0);
	TestTrue(TEXT("grid can be hidden"), Neutral && Neutral != Original);
	Floor101::Preferences::SetShowGrid(true);
	D->SetConstructionAidsVisible(false);
	Environment->Tick(0.016f);
	TestTrue(TEXT("walk/photo retain neutral floor"), Mesh->GetMaterial(0) == Neutral);
	D->SetConstructionAidsVisible(true);
	Environment->Tick(0.016f);
	TestTrue(TEXT("returning to design restores grid"), Mesh->GetMaterial(0) == Original);
	(void)D->Edit([](f101::cad::EditController& E) { E.setShowServices(true); return f101::Status::success(); });
	UMaterialInterface* Ghosted = Mesh->GetMaterial(0);
	TestTrue(TEXT("services layer ghosts the ground"), Ghosted && Ghosted != Original && Ghosted != Neutral);
	Environment->Tick(0.016f);
	TestTrue(TEXT("grid refresh preserves services transparency"), Mesh->GetMaterial(0) == Ghosted);
	(void)D->Edit([](f101::cad::EditController& E) { E.setShowServices(false); return f101::Status::success(); });
	Environment->Tick(0.016f);
	TestTrue(TEXT("leaving services restores the grid"), Mesh->GetMaterial(0) == Original);
	TestTrue(TEXT("presentation never adds CAD geometry"), D->GetDocument().project().walls.empty());
	return true;
}
#endif
