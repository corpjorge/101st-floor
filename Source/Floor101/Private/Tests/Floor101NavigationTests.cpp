#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Floor101GameViewportClient.h"
#include "Floor101NavigationKeys.h"
#include "InputCoreTypes.h"

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

#endif
