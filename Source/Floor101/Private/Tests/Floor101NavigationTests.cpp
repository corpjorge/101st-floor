#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Floor101GameViewportClient.h"
#include "Floor101NavigationKeys.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101EditingPawn.h"
#include "F101/Cad/EditController.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace
{
	constexpr EAutomationTestFlags NavigationTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101NavigationKeyMapTest, "Floor101.Navigation.KeyMap", NavigationTestFlags)
bool FFloor101NavigationKeyMapTest::RunTest(const FString& Parameters)
{
	using Command = EFloor101KeyCommand;
	auto Map = [](const FKey& Key, bool bNavigating, bool bControl = false, bool bShift = false, bool bRepeat = false)
	{
		return UFloor101GameViewportClient::MapKey(Key, bControl, bShift, bRepeat, false, bNavigating);
	};
	for (bool bWalk : {false, true})
	{
		for (const FKey& Key : {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::Q, EKeys::E})
		{
			TestTrue(TEXT("movement works in design and walk without N"), Map(Key, bWalk) == Command::NavigationMove);
		}
		TestTrue(TEXT("B selects walls"), Map(EKeys::B, bWalk) == Command::WallTool);
		TestTrue(TEXT("G selects furniture"), Map(EKeys::G, bWalk) == Command::FurnitureTool);
	}
	for (TCHAR Character : {TEXT('w'), TEXT('a'), TEXT('s'), TEXT('d'), TEXT('q'), TEXT('e'), TEXT('W')})
	{
		TestTrue(TEXT("movement characters never append CAD dimensions"), FFloor101NavigationKeys::IsMovementCharacter(Character));
	}
	for (TCHAR Character : {TEXT('3'), TEXT('m'), TEXT('c'), TEXT('.'), TEXT(',')})
	{
		TestFalse(TEXT("exact dimension characters remain available"), FFloor101NavigationKeys::IsMovementCharacter(Character));
	}
	for (const FKey& Key : {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::Q, EKeys::E})
	{
		TestTrue(*FString::Printf(TEXT("%s moves while navigating"), *Key.ToString()), Map(Key, true) == Command::NavigationMove);
		TestTrue(*FString::Printf(TEXT("held %s stays navigation input"), *Key.ToString()), Map(Key, true, false, false, true) == Command::NavigationMove);
		TestTrue(*FString::Printf(TEXT("Shift+%s stays navigation input"), *Key.ToString()), Map(Key, true, false, true) == Command::NavigationMove);
	}
	TestTrue(TEXT("N enters navigation"), Map(EKeys::N, false) == Command::NavigationToggle);
	TestTrue(TEXT("N leaves navigation"), Map(EKeys::N, true) == Command::NavigationToggle);
	TestTrue(TEXT("held N does not enter repeatedly"), Map(EKeys::N, false, false, false, true) == Command::None);
	TestTrue(TEXT("held N does not leave repeatedly"), Map(EKeys::N, true, false, false, true) == Command::None);
	TestTrue(TEXT("Ctrl+S still saves while navigating"), Map(EKeys::S, true, true) == Command::Save);
	TestTrue(TEXT("Ctrl+Shift+S still saves as while navigating"), Map(EKeys::S, true, true, true) == Command::SaveAs);
	TestTrue(TEXT("held Ctrl+S does not save or move again"), Map(EKeys::S, true, true, false, true) == Command::None);
	TestTrue(TEXT("Ctrl+W does not move or select walls"), Map(EKeys::W, true, true) == Command::None);
	TestTrue(TEXT("Ctrl+N does not toggle navigation"), Map(EKeys::N, true, true) == Command::None);
	TestTrue(TEXT("Escape remains the command handled to leave navigation"), Map(EKeys::Escape, true) == Command::Cancel);
	TestTrue(TEXT("fullscreen remains available while navigating"), Map(EKeys::F11, true) == Command::Fullscreen);
	TestTrue(TEXT("navigation wins over wall length input"),
		UFloor101GameViewportClient::MapKey(EKeys::W, false, false, false, true, true) == Command::NavigationMove);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101NavigationHeldKeysTest, "Floor101.Navigation.HeldKeys", NavigationTestFlags)
bool FFloor101NavigationHeldKeysTest::RunTest(const FString& Parameters)
{
	FFloor101NavigationKeys Keys;
	TSet<FKey> Down;
	auto IsDown = [&Down](const FKey& Key) { return Down.Contains(Key); };
	auto Press = [&Keys, &Down](const FKey& Key)
	{
		Down.Add(Key);
		Keys.HandleKey(Key, IE_Pressed, true, false);
	};
	auto Release = [&Keys, &Down](const FKey& Key)
	{
		Down.Remove(Key);
		Keys.HandleKey(Key, IE_Released, true, false);
	};
	auto Check = [this, &Keys, &IsDown](const TCHAR* Label, const FVector& Expected)
	{
		TestEqual(Label, Keys.Direction(IsDown), Expected, 1e-6f);
	};

	Check(TEXT("no held keys means no movement"), FVector::ZeroVector);
	Press(EKeys::W);
	Check(TEXT("W moves forward"), FVector(1.0, 0.0, 0.0));
	Keys.HandleKey(EKeys::W, IE_Repeat, true, false);
	Check(TEXT("holding W keeps movement without accumulating"), FVector(1.0, 0.0, 0.0));
	Press(EKeys::S);
	Check(TEXT("W and S cancel"), FVector::ZeroVector);
	Release(EKeys::W);
	Check(TEXT("releasing W leaves backwards movement"), FVector(-1.0, 0.0, 0.0));
	Release(EKeys::S);
	Press(EKeys::A);
	Check(TEXT("A moves left"), FVector(0.0, -1.0, 0.0));
	Press(EKeys::D);
	Check(TEXT("A and D cancel"), FVector::ZeroVector);
	Release(EKeys::A);
	Check(TEXT("releasing A leaves right movement"), FVector(0.0, 1.0, 0.0));
	Release(EKeys::D);
	Press(EKeys::Q);
	Check(TEXT("Q moves down"), FVector(0.0, 0.0, -1.0));
	Press(EKeys::E);
	Check(TEXT("Q and E cancel"), FVector::ZeroVector);
	Release(EKeys::Q);
	Check(TEXT("releasing Q leaves up movement"), FVector(0.0, 0.0, 1.0));
	Press(EKeys::W);
	Press(EKeys::D);
	Check(TEXT("combined input reports raw axes for camera normalization"), FVector(1.0, 1.0, 1.0));
	Down.Empty();
	Check(TEXT("stale accepted keys cannot move after the viewport reports release"), FVector::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101NavigationShortcutsTest, "Floor101.Navigation.ShortcutsAndFocus", NavigationTestFlags)
bool FFloor101NavigationShortcutsTest::RunTest(const FString& Parameters)
{
	FFloor101NavigationKeys Keys;
	TSet<FKey> Down;
	auto IsDown = [&Down](const FKey& Key) { return Down.Contains(Key); };
	auto Check = [this, &Keys, &IsDown](const TCHAR* Label, const FVector& Expected)
	{
		TestEqual(Label, Keys.Direction(IsDown), Expected, 1e-6f);
	};

	Down.Add(EKeys::W);
	Keys.HandleKey(EKeys::W, IE_Pressed, false, false);
	Check(TEXT("presses without active camera input are not accepted"), FVector::ZeroVector);
	Keys.HandleKey(EKeys::W, IE_Repeat, true, false);
	Check(TEXT("restoring camera input while W is held requires a fresh press"), FVector::ZeroVector);
	Keys.HandleKey(EKeys::W, IE_Released, false, false);
	Keys.HandleKey(EKeys::W, IE_Pressed, true, false);
	Check(TEXT("fresh navigation press is accepted"), FVector(1.0, 0.0, 0.0));
	Keys.HandleKey(EKeys::W, IE_Released, false, false);
	Check(TEXT("release clears a key even after navigation was disabled"), FVector::ZeroVector);

	Down.Empty();
	Down.Add(EKeys::S);
	Keys.HandleKey(EKeys::S, IE_Pressed, true, false);
	Check(TEXT("S initially moves backwards"), FVector(-1.0, 0.0, 0.0));
	Keys.HandleKey(EKeys::LeftControl, IE_Pressed, true, true);
	Check(TEXT("pressing a shortcut modifier clears all motion"), FVector::ZeroVector);
	Keys.HandleKey(EKeys::S, IE_Pressed, true, true);
	Keys.HandleKey(EKeys::LeftControl, IE_Released, true, false);
	Keys.HandleKey(EKeys::S, IE_Repeat, true, false);
	Check(TEXT("releasing Ctrl with S held does not resume motion after save"), FVector::ZeroVector);
	Keys.HandleKey(EKeys::S, IE_Released, true, false);
	Keys.HandleKey(EKeys::S, IE_Pressed, true, false);
	Check(TEXT("fresh S press after the shortcut moves again"), FVector(-1.0, 0.0, 0.0));

	Keys.Reset();
	Check(TEXT("focus loss reset stops even keys still reported as down"), FVector::ZeroVector);
	Keys.HandleKey(EKeys::S, IE_Repeat, true, false);
	Check(TEXT("returning focus cannot resume from a key repeat"), FVector::ZeroVector);
	Keys.HandleKey(EKeys::S, IE_Released, true, false);
	Keys.HandleKey(EKeys::S, IE_Pressed, true, false);
	Check(TEXT("fresh input after focus returns works"), FVector(-1.0, 0.0, 0.0));
	Keys.HandleKey(EKeys::LeftAlt, IE_Pressed, true, true);
	Check(TEXT("Alt shortcut clears movement too"), FVector::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101NumberRowSpeedTest, "Floor101.Navigation.NumberRowSpeed", NavigationTestFlags)
bool FFloor101NumberRowSpeedTest::RunTest(const FString& Parameters)
{
	const FKey NumberRow[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NumberRow); ++Index)
	{
		const FKey& Key = NumberRow[Index];
		TestEqual(*FString::Printf(TEXT("%s selects level %d"), *Key.ToString(), Index + 1),
			FFloor101NavigationKeys::SpeedLevelForKey(Key, false, false), Index + 1);
		TestEqual(TEXT("wall length digits take priority"), FFloor101NavigationKeys::SpeedLevelForKey(Key, true, false), 0);
		TestEqual(TEXT("modified digits never select speed"), FFloor101NavigationKeys::SpeedLevelForKey(Key, false, true), 0);
	}
	for (const FKey& Key : {EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree,
		EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine, EKeys::W})
	{
		TestEqual(TEXT("numpad and movement do not select speed"), FFloor101NavigationKeys::SpeedLevelForKey(Key, false, false), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101NumberRowInputTest, "Floor101.Navigation.NumberRowInput", NavigationTestFlags)
bool FFloor101NumberRowInputTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101SpeedKeyTestWorld"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Client = NewObject<UFloor101GameViewportClient>(GEngine);
	auto* GameInstance = NewObject<UGameInstance>(GEngine);
	World->SetGameInstance(GameInstance);
	Context.OwningGameInstance = GameInstance;
	Client->Init(Context, GameInstance, false);
	const TSharedRef<SViewport> View = SNew(SViewport);
	FSceneViewport Viewport(View);
	Viewport.SetViewportClient(TStrongPtrVariant<FViewportClient>(Client));
	Client->Viewport = &Viewport;
	ON_SCOPE_EXIT { Client->Viewport = nullptr; };
	auto* Controller = World->SpawnActor<APlayerController>();
	auto* Pawn = World->SpawnActor<AFloor101EditingPawn>();
	auto* Document = World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("pawn"), Pawn) || !TestNotNull(TEXT("document"), Document)) { return false; }
	World->AddController(Controller); // Bare test worlds have not initialized actor registration.
	Controller->Possess(Pawn);
	if (!TestTrue(TEXT("viewport resolves the editing pawn"), World->GetFirstPlayerController() == Controller && Controller->GetPawn() == Pawn)) { return false; }
	const auto OriginalFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().ClearKeyboardFocus();
	ON_SCOPE_EXIT { if (OriginalFocus) { FSlateApplication::Get().SetKeyboardFocus(OriginalFocus); } };
	const FString OriginalIni = GGameUserSettingsIni;
	const FString TestIni = FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("SpeedKeys"), TEXT(".ini")));
	GGameUserSettingsIni = TestIni;
	ON_SCOPE_EXIT { GGameUserSettingsIni = OriginalIni; GConfig->UnloadFile(TestIni); IFileManager::Get().Delete(*TestIni); };
	if (!TestTrue(TEXT("temporary preferences created"), FFileHelper::SaveStringToFile(TEXT("[SectionsToSave]\nbCanSaveAllSections=true\n"), *TestIni))) { return false; }
	GConfig->LoadFile(TestIni);
	auto Key = [Client, &Viewport](const FKey& Value, EInputEvent Event = IE_Pressed)
	{
		return Client->InputKey(FInputKeyEventArgs(&Viewport, FInputDeviceId::CreateFromInternalId(0), Value, Event, 0ULL));
	};
	TestTrue(TEXT("top row reaches viewport speed handler"), Key(EKeys::Zero));
	TestEqual(TEXT("zero selects fastest speed"), Client->GetNavigationSpeedLevel(), 10);
	Key(EKeys::Three);
	TestEqual(TEXT("three selects level three"), Client->GetNavigationSpeedLevel(), 3);
	Key(EKeys::Zero, IE_Repeat);
	TestEqual(TEXT("repeat cannot overwrite a newer choice"), Client->GetNavigationSpeedLevel(), 3);
	Key(EKeys::NumPadNine);
	TestEqual(TEXT("numpad cannot change speed"), Client->GetNavigationSpeedLevel(), 3);
	Client->SetInterfaceOpen(true);
	Key(EKeys::Nine);
	TestEqual(TEXT("options keep speed shortcuts inactive"), Client->GetNavigationSpeedLevel(), 3);
	Client->SetInterfaceOpen(false);
	(void)Document->Edit([](f101::cad::EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Wall);
		return Editor.pointerPressed({0.0, 0.0, 10000.0, 0.0, 0.0, -1.0});
	});
	Key(EKeys::Nine);
	Client->InputChar(nullptr, 0, TEXT('9'));
	Client->InputChar(nullptr, 0, TEXT('.'));
	Client->InputChar(nullptr, 0, TEXT('5'));
	TestEqual(TEXT("drafting length does not change speed"), Client->GetNavigationSpeedLevel(), 3);
	TestTrue(TEXT("exact decimal dimension still reaches wall tool"), Document->GetEditController().wallTool().typedText() == "9.5");
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101LookInPlaceTest, "Floor101.Navigation.LookInPlaceGesture", NavigationTestFlags)
bool FFloor101LookInPlaceTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101LookTestWorld"));
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Client = NewObject<UFloor101GameViewportClient>(GEngine);
	auto* GameInstance = NewObject<UGameInstance>(GEngine);
	World->SetGameInstance(GameInstance);
	Context.OwningGameInstance = GameInstance;
	Client->Init(Context, GameInstance, false);
	const TSharedRef<SViewport> View = SNew(SViewport);
	FSceneViewport Viewport(View);
	Viewport.SetViewportClient(TStrongPtrVariant<FViewportClient>(Client));
	Client->Viewport = &Viewport;
	ON_SCOPE_EXIT { Client->Viewport = nullptr; };
	auto* Controller = World->SpawnActor<APlayerController>();
	auto* Pawn = World->SpawnActor<AFloor101EditingPawn>();
	auto* Document = World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("controller"), Controller) || !TestNotNull(TEXT("pawn"), Pawn) || !TestNotNull(TEXT("document"), Document)) return false;
	World->AddController(Controller);
	Controller->Possess(Pawn);
	Pawn->Frame(FBox(FVector::ZeroVector, FVector(400, 300, 280)));
	const auto OriginalFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().ClearKeyboardFocus();
	ON_SCOPE_EXIT { if (OriginalFocus) FSlateApplication::Get().SetKeyboardFocus(OriginalFocus); };
	auto Key = [Client, &Viewport](EInputEvent Event)
	{
		Client->InputKey(FInputKeyEventArgs(&Viewport, FInputDeviceId::CreateFromInternalId(0), EKeys::RightMouseButton, Event, 0ULL));
	};
	const FVector Start = Pawn->GetActorLocation();
	const FRotator Orientation = Pawn->GetActorRotation();
	const double Distance = Pawn->GetDistance();
	Client->ApplyRightDrag(FVector2D(80, 30), true);
	TestEqual(TEXT("no button press means no rotation"), Pawn->GetActorRotation(), Orientation, 1e-6f);
	Key(IE_Pressed);
	Client->ApplyRightDrag(FVector2D(80, -300), true);
	TestFalse(TEXT("Shift right drag changes direction without N"), Pawn->GetActorRotation().Equals(Orientation, 1e-6));
	TestEqual(TEXT("camera stays at exactly the same position"), Pawn->GetActorLocation(), Start, 1e-6f);
	TestEqual(TEXT("view distance is unchanged"), Pawn->GetDistance(), Distance, 1e-6);
	TestTrue(TEXT("can look above the horizon"), Pawn->GetActorForwardVector().Z > 0.0);
	Client->ApplyRightDrag(FVector2D(20, 10), false);
	TestEqual(TEXT("releasing Shift first does not start orbiting"), Pawn->GetActorLocation(), Start, 1e-6f);
	Key(IE_Released);
	const FRotator ReleasedOrientation = Pawn->GetActorRotation();
	Client->ApplyRightDrag(FVector2D(20, 10), true);
	TestEqual(TEXT("button release ends the gesture"), Pawn->GetActorRotation(), ReleasedOrientation, 1e-6f);
	const FVector OrbitFocus = Pawn->GetFocus();
	Key(IE_Pressed);
	Client->ApplyRightDrag(FVector2D(80, 0), false);
	TestEqual(TEXT("ordinary right drag retains the orbit focus"), Pawn->GetFocus(), OrbitFocus, 1e-6f);
	TestFalse(TEXT("ordinary orbit moves the camera around the focus"), Pawn->GetActorLocation().Equals(Start, 1e-6));
	Key(IE_Released);
	(void)Document->Edit([](f101::cad::EditController& E) { E.setMode(f101::cad::EditMode::Wall); return E.pointerPressed({0,0,10000,0,0,-1}); });
	Key(IE_Pressed);
	Client->ApplyRightDrag(FVector2D(0.1, 0.1), true);
	Key(IE_Released);
	TestTrue(TEXT("a tiny look does not cancel the wall being drawn"), Document->GetEditController().wallTool().state() == f101::cad::WallTool::State::Drafting);
	Key(IE_Pressed);
	Key(IE_Released);
	TestFalse(TEXT("ordinary right click still cancels"), Document->GetEditController().wallTool().state() == f101::cad::WallTool::State::Drafting);
	Client->SetPlanView(true);
	const FVector PlanPosition = Pawn->GetActorLocation();
	const FRotator PlanOrientation = Pawn->GetActorRotation();
	Key(IE_Pressed);
	Client->ApplyRightDrag(FVector2D(90, 90), true);
	TestEqual(TEXT("2D position remains fixed"), Pawn->GetActorLocation(), PlanPosition, 1e-6f);
	TestEqual(TEXT("2D orientation remains fixed"), Pawn->GetActorRotation(), PlanOrientation, 1e-6f);
	Key(IE_Released);
	Client->SetPlanView(false);
	Key(IE_Pressed);
	Client->ApplyRightDrag(FVector2D(20, 10), true);
	Client->LostFocus(&Viewport);
	const FRotator FocusLost = Pawn->GetActorRotation();
	Client->ApplyRightDrag(FVector2D(20, 10), true);
	TestEqual(TEXT("focus loss ends looking"), Pawn->GetActorRotation(), FocusLost, 1e-6f);
	Key(IE_Pressed);
	Client->SetInterfaceOpen(true);
	Client->ApplyRightDrag(FVector2D(20, 10), true);
	TestEqual(TEXT("options block camera rotation"), Pawn->GetActorRotation(), FocusLost, 1e-6f);
	Client->SetInterfaceOpen(false);
	Client->ApplyRightDrag(FVector2D(20, 10), true);
	TestEqual(TEXT("closing options requires a new mouse press"), Pawn->GetActorRotation(), FocusLost, 1e-6f);
	return true;
}

#endif
