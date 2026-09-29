// Unreal integration tests of the editing layer (ADR-0008): keyboard map, orbit camera, and the document subsystem driving
// the edit controller, the scene and project.json. Run with .\Scripts\Test.ps1 -Integration.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101EditingPawn.h"
#include "Floor101GameViewportClient.h"
#include "Floor101SceneNodeActor.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr double Tolerance = 1e-6;

	using f101::cad::EditController;

	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101InteractionTestWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}

		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	// Looking straight down at a plan point (mm), like a top view.
	f101::cad::Ray3 Down(double X, double Y)
	{
		return f101::cad::Ray3{X, Y, 10000.0, 0.0, 0.0, -1.0};
	}

	AFloor101SceneNodeActor* FindNode(UWorld* World, const FString& NodeId)
	{
		for (TActorIterator<AFloor101SceneNodeActor> It(World); It; ++It)
		{
			if (It->GetSceneNodeId() == NodeId && !It->IsActorBeingDestroyed())
			{
				return *It;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101KeyMapTest, "Floor101.Interaction.KeyMap", TestFlags)
bool FFloor101KeyMapTest::RunTest(const FString& Parameters)
{
	using Command = EFloor101KeyCommand;
	auto Map = [](const FKey& Key, bool bControl = false, bool bShift = false, bool bRepeat = false)
	{
		return UFloor101GameViewportClient::MapKey(Key, bControl, bShift, bRepeat);
	};
	TestTrue(TEXT("W: wall tool"), Map(EKeys::W) == Command::WallTool);
	TestTrue(TEXT("V: select"), Map(EKeys::V) == Command::SelectTool);
	TestTrue(TEXT("P: door"), Map(EKeys::P) == Command::DoorTool);
	TestTrue(TEXT("O: window"), Map(EKeys::O) == Command::WindowTool);
	TestTrue(TEXT("Ctrl+O is still open"), Map(EKeys::O, true) == Command::Open);
	TestTrue(TEXT("Esc: cancel"), Map(EKeys::Escape) == Command::Cancel);
	TestTrue(TEXT("Enter: confirm"), Map(EKeys::Enter) == Command::Confirm);
	TestTrue(TEXT("Delete: delete"), Map(EKeys::Delete) == Command::Delete);
	TestTrue(TEXT("F: frame"), Map(EKeys::F) == Command::Frame);
	TestTrue(TEXT("T: plan view"), Map(EKeys::T) == Command::TopView);
	TestTrue(TEXT("Backspace: backspace"), Map(EKeys::BackSpace) == Command::Backspace);
	TestTrue(TEXT("Ctrl+Z: undo"), Map(EKeys::Z, true) == Command::Undo);
	TestTrue(TEXT("Ctrl+Shift+Z: redo"), Map(EKeys::Z, true, true) == Command::Redo);
	TestTrue(TEXT("Ctrl+Y: redo"), Map(EKeys::Y, true) == Command::Redo);
	TestTrue(TEXT("Ctrl+S: save"), Map(EKeys::S, true) == Command::Save);
	TestTrue(TEXT("Ctrl+O: open"), Map(EKeys::O, true) == Command::Open);
	TestTrue(TEXT("Ctrl+W is not W"), Map(EKeys::W, true) == Command::None);
	TestTrue(TEXT("held Backspace repeats"), Map(EKeys::BackSpace, false, false, true) == Command::Backspace);
	TestTrue(TEXT("held Ctrl+Z repeats"), Map(EKeys::Z, true, false, true) == Command::Undo);
	TestTrue(TEXT("held Ctrl+S does not save again"), Map(EKeys::S, true, false, true) == Command::None);
	TestTrue(TEXT("held Delete does not repeat"), Map(EKeys::Delete, false, false, true) == Command::None);
	TestTrue(TEXT("digits are typed, not commands"), Map(EKeys::Three) == Command::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CameraPawnTest, "Floor101.Interaction.CameraPawn", TestFlags)
bool FFloor101CameraPawnTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AFloor101EditingPawn* Pawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	if (!TestNotNull(TEXT("pawn"), Pawn))
	{
		return false;
	}

	const FBox Wall(FVector(0.0, -6.0, 0.0), FVector(355.0, 6.0, 240.0));
	Pawn->Frame(Wall);
	TestEqual(TEXT("framing focuses the box centre"), Pawn->GetFocus(), Wall.GetCenter(), 1e-3f);
	const FVector ToFocus = Pawn->GetFocus() - Pawn->GetActorLocation();
	TestEqual(TEXT("camera at the focus distance"), ToFocus.Size(), Pawn->GetDistance(), 1e-6);
	TestTrue(TEXT("camera looks at the focus"), FVector::DotProduct(ToFocus.GetSafeNormal(), Pawn->GetActorForwardVector()) > 0.9999);
	TestTrue(TEXT("the whole box fits: distance > its radius"), Pawn->GetDistance() > Wall.GetExtent().Size());

	const double Yaw = Pawn->GetActorRotation().Yaw;
	Pawn->Orbit(FVector2D(40.0, 0.0));
	TestEqual(TEXT("40 px orbit = 10 degrees of yaw"), FRotator::NormalizeAxis(Pawn->GetActorRotation().Yaw - Yaw), 10.0, 1e-3);
	Pawn->Orbit(FVector2D(0.0, -100000.0));
	TestTrue(TEXT("pitch never goes above the horizon"), Pawn->GetActorRotation().Pitch <= -5.0 + 1e-3);
	Pawn->Orbit(FVector2D(0.0, 100000.0));
	TestTrue(TEXT("pitch never goes past straight down"), Pawn->GetActorRotation().Pitch >= -89.0 - 1e-3);
	TestEqual(TEXT("orbiting keeps the focus"), Pawn->GetFocus(), Wall.GetCenter(), 1e-3f);

	const double Distance = Pawn->GetDistance();
	Pawn->Zoom(1.0f);
	TestEqual(TEXT("one wheel notch in = 85 % of the distance"), Pawn->GetDistance(), Distance * 0.85, 1e-6);
	Pawn->Zoom(-1.0f);
	TestEqual(TEXT("and back"), Pawn->GetDistance(), Distance, 1e-6);

	const FVector Focus = Pawn->GetFocus();
	const FRotator Rotation = Pawn->GetActorRotation();
	Pawn->Pan(FVector2D(100.0, 0.0), FVector2D(1600.0, 900.0));
	const FVector Moved = Pawn->GetFocus() - Focus;
	TestTrue(TEXT("dragging right moves the focus left"), FVector::DotProduct(Moved, FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y)) < 0.0);
	TestEqual(TEXT("panning never rotates"), Pawn->GetActorRotation(), Rotation, 1e-3f);

	const FVector KeptFocus = Pawn->GetFocus();
	Pawn->TopView();
	const FRotationMatrix Plan(Pawn->GetActorRotation());
	TestTrue(TEXT("plan view looks (almost) straight down"), Plan.GetUnitAxis(EAxis::X).Z < -0.999);
	TestTrue(TEXT("north (scene +Y = Unreal -Y) is up on screen"), Plan.GetUnitAxis(EAxis::Z).Y < -0.99);
	TestTrue(TEXT("east (+X) is right on screen"), Plan.GetUnitAxis(EAxis::Y).X > 0.99);
	TestEqual(TEXT("plan view keeps the focus"), Pawn->GetFocus(), KeptFocus, 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101EditFlowTest, "Floor101.Interaction.EditFlow", TestFlags)
bool FFloor101EditFlowTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document subsystem in a game world"), Document))
	{
		return false;
	}
	UWorld* World = TestWorld.World;

	// Master prompt §19 through the same calls the viewport client makes: W, click, move, type "355", Enter.
	(void)Document->Edit([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Wall); return f101::Status::success(); });
	Document->UpdatePointer(Down(12.0, -18.0));
	TestNotNull(TEXT("snapped cursor shown as a handle"), FindNode(World, TEXT("marker/0")));
	TestTrue(TEXT("click starts the wall"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(12.0, -18.0)); }).ok());
	(void)Document->Edit([](EditController& Editor) { return Editor.pointerReleased(Down(12.0, -18.0)); });
	Document->UpdatePointer(Down(2987.0, 23.0));
	(void)Document->Edit([](EditController& Editor) { Editor.appendText("355"); return f101::Status::success(); });
	AFloor101SceneNodeActor* Preview = FindNode(World, TEXT("preview/wall/0"));
	if (TestNotNull(TEXT("preview actor while drawing"), Preview))
	{
		TestEqual(TEXT("preview follows the typed length"), Preview->GetMeshBounds().GetSize().X, 355.0, Tolerance);
	}
	TestTrue(TEXT("Enter creates the wall"), Document->Edit([](EditController& Editor) { return Editor.confirm(); }).ok());
	TestFalse(TEXT("no status error"), Document->IsStatusError());

	const f101::cad::Project& Project = Document->GetDocument().project();
	if (!TestEqual(TEXT("one wall"), static_cast<int32>(Project.walls.size()), 1))
	{
		return false;
	}
	const f101::cad::WallId WallId = Project.walls[0].id;
	TestTrue(TEXT("exactly (0,0)-(3550,0)"), Project.walls[0].start == f101::cad::Point2D{0, 0} && Project.walls[0].end == f101::cad::Point2D{3550, 0});
	AFloor101SceneNodeActor* WallActor = Cast<AFloor101SceneNodeActor>(Document->FindWallActor(WallId));
	if (!TestNotNull(TEXT("wall actor"), WallActor))
	{
		return false;
	}
	TestEqual(TEXT("wall actor is 355 cm"), WallActor->GetMeshBounds().GetSize().X, 355.0, Tolerance);

	// Esc twice: stop the chain, back to Select. Then a click on the wall selects it and tints it.
	(void)Document->Edit([](EditController& Editor) { Editor.cancel(); Editor.cancel(); return f101::Status::success(); });
	TestTrue(TEXT("back in Select mode"), Document->GetEditController().mode() == f101::cad::EditMode::Select);
	(void)Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(1000.0, 20.0)); });
	(void)Document->Edit([](EditController& Editor) { return Editor.pointerReleased(Down(1000.0, 20.0)); });
	TestTrue(TEXT("wall selected"), Document->GetEditController().selection() == WallId);
	TestNotNull(TEXT("selected wall is tinted"), Cast<UMaterialInstanceDynamic>(WallActor->GetDisplayMaterial()));
	TestNotNull(TEXT("end handle shown"), FindNode(World, TEXT("marker/1")));

	// Properties panel: typed length, then undo.
	TestTrue(TEXT("length 4.2 m"), Document->Edit([](EditController& Editor) { return Editor.setSelectionLength("4.2 m"); }).ok());
	TestEqual(TEXT("same actor now 420 cm"), WallActor->GetMeshBounds().GetSize().X, 420.0, Tolerance);
	TestTrue(TEXT("undo"), Document->Edit([](EditController& Editor) { return Editor.undo(); }).ok());
	TestEqual(TEXT("back to 355 cm"), WallActor->GetMeshBounds().GetSize().X, 355.0, Tolerance);

	// Errors reach the status bar (and the log, as warnings) and change nothing.
	AddExpectedMessagePlain(TEXT("Wall thickness must be positive"), ELogVerbosity::Warning);
	AddExpectedMessagePlain(TEXT("project.json.missing"), ELogVerbosity::Warning);
	TestFalse(TEXT("thickness 0 rejected"), Document->Edit([](EditController& Editor) { return Editor.setSelectionThickness("0"); }).ok());
	TestTrue(TEXT("status shows the error"), Document->IsStatusError() && !Document->GetStatusMessage().IsEmpty());
	TestEqual(TEXT("thickness unchanged"), Project.walls[0].thicknessMm, static_cast<int64>(120));

	// project.json round trip.
	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101"), TEXT("Interaction áé"), TEXT("project.json"));
	IFileManager::Get().Delete(*Path, false, true, true);
	TestTrue(TEXT("unsaved before the first save"), Document->HasUnsavedChanges());
	TestTrue(TEXT("save"), Document->SaveProject(Path).ok());
	TestFalse(TEXT("saved"), Document->HasUnsavedChanges());
	TestTrue(TEXT("file written"), IFileManager::Get().FileExists(*Path));
	TestTrue(TEXT("delete the selected wall"), Document->Edit([](EditController& Editor) { return Editor.deleteSelection(); }).ok());
	TestTrue(TEXT("no walls"), Project.walls.empty());
	TestNull(TEXT("its actor is gone"), Document->FindWallActor(WallId));
	TestTrue(TEXT("unsaved after deleting"), Document->HasUnsavedChanges());
	TestTrue(TEXT("open"), Document->OpenProject(Path).ok());
	TestEqual(TEXT("the wall is back"), static_cast<int32>(Project.walls.size()), 1);
	TestTrue(TEXT("with its id"), !Project.walls.empty() && Project.walls[0].id == WallId);
	TestNotNull(TEXT("and its actor"), Document->FindWallActor(WallId));
	TestFalse(TEXT("opening clears the history"), Document->GetDocument().canUndo());
	TestFalse(TEXT("opened file is not modified"), Document->HasUnsavedChanges());
	TestEqual(TEXT("current file"), Document->GetProjectPath(), Path);
	TestFalse(TEXT("a missing file is an error"), Document->OpenProject(Path + TEXT(".missing")).ok());
	TestTrue(TEXT("the failed open keeps the document"), Project.walls.size() == 1);
	IFileManager::Get().Delete(*Path, false, true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101RoomsTest, "Floor101.Interaction.Rooms", TestFlags)
bool FFloor101RoomsTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document subsystem"), Document))
	{
		return false;
	}
	// 4 x 3 m on the wall axes, 120 mm walls: 3.88 x 2.88 m inside.
	f101::cad::Document& Model = Document->GetMutableDocument();
	const f101::cad::WallId South = Model.addWall({0, 0}, {4000, 0}, 120, 2400).value();
	const f101::cad::WallId East = Model.addWall({4000, 0}, {4000, 3000}, 120, 2400).value();
	TestTrue(TEXT("an open L encloses nothing"), Document->GetRooms().IsEmpty());
	(void)Model.addWall({4000, 3000}, {0, 3000}, 120, 2400);
	(void)Model.addWall({0, 3000}, {0, 0}, 120, 2400);

	if (!TestEqual(TEXT("closing the walls makes a room"), Document->GetRooms().Num(), 1))
	{
		return false;
	}
	const FFloor101RoomView Room = Document->GetRooms()[0];
	TestEqual(TEXT("net area 3.88 x 2.88 m"), Room.AreaM2, 3.88 * 2.88, 1e-9);
	TestEqual(TEXT("label at the middle of the floor (x)"), Room.LabelLocation.X, 200.0, 1e-6);
	TestEqual(TEXT("label at the middle of the floor (y mirrored)"), Room.LabelLocation.Y, -150.0, 1e-6);
	const AFloor101SceneNodeActor* Floor = Cast<AFloor101SceneNodeActor>(Document->FindRoomFloorActor(Room.Key));
	if (TestNotNull(TEXT("floor actor"), Floor))
	{
		const FVector Size = Floor->GetMeshBounds().GetSize();
		TestEqual(TEXT("floor 388 cm"), Size.X, 388.0, 1e-6);
		TestEqual(TEXT("floor 288 cm"), Size.Y, 288.0, 1e-6);
		TestEqual(TEXT("floor 1 cm thick"), Size.Z, 1.0, 1e-6);
	}
	const AFloor101SceneNodeActor* SouthWall = Cast<AFloor101SceneNodeActor>(Document->FindWallActor(South));
	if (TestNotNull(TEXT("south wall actor"), SouthWall))
	{
		TestEqual(TEXT("mitred corners: the wall reaches both outer corners (412 cm)"), SouthWall->GetMeshBounds().GetSize().X, 412.0, 1e-6);
	}

	// Dragging the south-east corner: the room follows while dragging, and both walls move as one step.
	(void)Document->Edit([&South](EditController& Editor) { return Editor.select(South); });
	TestTrue(TEXT("press on the corner handle"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(4000.0, 0.0)); }).ok());
	Document->UpdatePointer(Down(4510.0, 10.0));
	// The east wall turns diagonal: the floor is a trapezoid 2.88 m high, 4.369172 m wide at the bottom and
	// 3.889172 m at the top (inner faces 60 mm in from each axis).
	constexpr double TrapezoidM2 = 11.89201644;
	if (TestEqual(TEXT("one room while dragging"), Document->GetRooms().Num(), 1))
	{
		TestEqual(TEXT("its area follows the preview"), Document->GetRooms()[0].AreaM2, TrapezoidM2, 1e-6);
	}
	TestTrue(TEXT("release"), Document->Edit([](EditController& Editor) { return Editor.pointerReleased(Down(4510.0, 10.0)); }).ok());
	const f101::cad::Project& Project = Document->GetDocument().project();
	TestTrue(TEXT("south wall moved"), Project.findWall(South)->end == f101::cad::Point2D{4500, 0});
	TestTrue(TEXT("east wall followed"), Project.findWall(East)->start == f101::cad::Point2D{4500, 0});
	TestEqual(TEXT("same room key"), Document->GetRooms()[0].Key, Room.Key);
	TestEqual(TEXT("the applied room is the previewed one"), Document->GetRooms()[0].AreaM2, TrapezoidM2, 1e-6);
	TestTrue(TEXT("undo"), Document->Edit([](EditController& Editor) { return Editor.undo(); }).ok());
	TestEqual(TEXT("area back"), Document->GetRooms()[0].AreaM2, 3.88 * 2.88, 1e-9);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
