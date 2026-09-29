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
#include "Floor101FinishLibrary.h"
#include "Floor101GameViewportClient.h"
#include "Floor101SceneNodeActor.h"
#include "Camera/CameraComponent.h"
#include "DynamicRHI.h"
#include "F101/Drawing/PlanDrawing.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <algorithm>

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
	TestTrue(TEXT("M: paint"), Map(EKeys::M) == Command::PaintTool);
	TestTrue(TEXT("Ctrl+O is still open"), Map(EKeys::O, true) == Command::Open);
	TestTrue(TEXT("Esc: cancel"), Map(EKeys::Escape) == Command::Cancel);
	TestTrue(TEXT("Enter: confirm"), Map(EKeys::Enter) == Command::Confirm);
	TestTrue(TEXT("Delete: delete"), Map(EKeys::Delete) == Command::Delete);
	TestTrue(TEXT("F: frame"), Map(EKeys::F) == Command::Frame);
	TestTrue(TEXT("T: plan view"), Map(EKeys::T) == Command::TopView);
	TestTrue(TEXT("Page Up: level up"), Map(EKeys::PageUp) == Command::LevelUp);
	TestTrue(TEXT("Page Down: level down"), Map(EKeys::PageDown) == Command::LevelDown);
	TestTrue(TEXT("L: every level"), Map(EKeys::L) == Command::AllLevels);
	TestTrue(TEXT("Space: flip the hinges"), Map(EKeys::SpaceBar) == Command::Flip);
	TestTrue(TEXT("Tab: 2D plan"), Map(EKeys::Tab) == Command::PlanView);
	TestTrue(TEXT("M typed in a length is text ('3.55 m')"),
		UFloor101GameViewportClient::MapKey(EKeys::M, false, false, false, true) == Command::None);
	TestTrue(TEXT("W still switches while typing"), UFloor101GameViewportClient::MapKey(EKeys::W, false, false, false, true) == Command::WallTool);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101LevelsTest, "Floor101.Interaction.Levels", TestFlags)
bool FFloor101LevelsTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document subsystem"), Document))
	{
		return false;
	}
	// A 4 x 3 m room on the ground floor (2800 mm floor to floor, 250 mm slab).
	f101::cad::Document& Model = Document->GetMutableDocument();
	const f101::cad::WallId South = Model.addWall({0, 0}, {4000, 0}, 120, 2800).value();
	(void)Model.addWall({4000, 0}, {4000, 3000}, 120, 2800);
	(void)Model.addWall({4000, 3000}, {0, 3000}, 120, 2800);
	(void)Model.addWall({0, 3000}, {0, 0}, 120, 2800);
	if (!TestEqual(TEXT("a room on the ground floor"), Document->GetRooms().Num(), 1))
	{
		return false;
	}
	const FString GroundRoom = Document->GetRooms()[0].Key;
	TestNull(TEXT("no slab over the level being edited"), Document->FindRoomSlabActor(GroundRoom));

	// A new level: it becomes the edited one and the ground floor turns into context with its slab.
	TestTrue(TEXT("add a level"), Document->Edit([](EditController& Editor) { return Editor.addLevel(); }).ok());
	const f101::cad::Project& Project = Document->GetDocument().project();
	TestEqual(TEXT("two levels"), static_cast<int32>(Project.levels.size()), 2);
	TestTrue(TEXT("the new level is edited"), Document->GetEditController().activeLevel() == Project.levels[1].id);
	TestTrue(TEXT("its rooms, none yet, are the ones listed"), Document->GetRooms().IsEmpty());
	const AFloor101SceneNodeActor* Slab = Cast<AFloor101SceneNodeActor>(Document->FindRoomSlabActor(GroundRoom));
	if (TestNotNull(TEXT("slab over the ground floor room"), Slab))
	{
		const FBox Bounds = Slab->GetMeshBounds();
		TestEqual(TEXT("slab bottom 255 cm"), Bounds.Min.Z, 255.0, 1e-6);
		TestEqual(TEXT("slab top 280 cm"), Bounds.Max.Z, 280.0, 1e-6);
		TestEqual(TEXT("between the walls: 388 cm"), Bounds.GetSize().X, 388.0, 1e-6);
	}
	const AFloor101SceneNodeActor* SouthWall = Cast<AFloor101SceneNodeActor>(Document->FindWallActor(South));
	const UMaterialInstanceDynamic* Dimmed = SouthWall ? Cast<UMaterialInstanceDynamic>(SouthWall->GetDisplayMaterial()) : nullptr;
	if (TestNotNull(TEXT("the level below is dimmed"), Dimmed))
	{
		FLinearColor Color;
		TestTrue(TEXT("tinted"), Dimmed->GetVectorParameterValue(FHashedMaterialParameterInfo(AFloor101SceneNodeActor::ColorParameterName), Color));
		TestTrue(TEXT("its finish, darker"), Color.Equals(UFloor101FinishLibrary::ContextTint, 1e-3f));
	}

	// Drawing on the upper level: the ray is taken to its floor at 2800 mm and the wall stands there.
	TestTrue(TEXT("wall tool"), Document->Edit([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Wall); return f101::Status::success(); }).ok());
	TestTrue(TEXT("start"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(0.0, 0.0)); }).ok());
	Document->UpdatePointer(Down(3990.0, 12.0));
	TestTrue(TEXT("create"), Document->Edit([](EditController& Editor) { return Editor.confirm(); }).ok());
	const f101::cad::Wall& Upper = Project.walls.back();
	TestTrue(TEXT("on the upper level"), Upper.level == Project.levels[1].id);
	TestEqual(TEXT("as tall as its level"), Upper.heightMm, int64(2800));
	const AFloor101SceneNodeActor* UpperActor = Cast<AFloor101SceneNodeActor>(Document->FindWallActor(Upper.id));
	if (TestNotNull(TEXT("upper wall actor"), UpperActor))
	{
		TestEqual(TEXT("stands at 280 cm"), UpperActor->GetMeshBounds().Min.Z, 280.0, 1e-6);
	}

	// Back to the ground floor: the level above is hidden, the ground slabs too; every level shows them all.
	TestTrue(TEXT("level down"), Document->Edit([](EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Select);
		return Editor.levelDown();
	}).ok());
	TestNull(TEXT("upper wall hidden"), Document->FindWallActor(Upper.id));
	TestNull(TEXT("ground slab hidden"), Document->FindRoomSlabActor(GroundRoom));
	TestEqual(TEXT("the ground room is listed again"), Document->GetRooms().Num(), 1);
	TestTrue(TEXT("every level"), Document->Edit([](EditController& Editor) { Editor.setShowAllLevels(true); return f101::Status::success(); }).ok());
	TestNotNull(TEXT("upper wall shown"), Document->FindWallActor(Upper.id));
	TestNotNull(TEXT("ground slab shown"), Document->FindRoomSlabActor(GroundRoom));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101FinishesInteractionTest, "Floor101.Interaction.Finishes", TestFlags)
bool FFloor101FinishesInteractionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!TestNotNull(TEXT("document subsystem"), Document) || !TestNotNull(TEXT("finish library"), Library))
	{
		return false;
	}
	f101::cad::Document& Model = Document->GetMutableDocument();
	const f101::cad::WallId South = Model.addWall({0, 0}, {4000, 0}, 120, 2800).value();
	(void)Model.addWall({4000, 0}, {4000, 3000}, 120, 2800);
	(void)Model.addWall({4000, 3000}, {0, 3000}, 120, 2800);
	(void)Model.addWall({0, 3000}, {0, 0}, 120, 2800);

	// Paint: the inner (north, left) face of the south wall, then the floor.
	TestTrue(TEXT("paint mode with bricks"), Document->Edit([](EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Paint);
		return Editor.setPaintFinish("brick-red");
	}).ok());
	TestTrue(TEXT("paint the wall face"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(2000.0, 30.0)); }).ok());
	TestTrue(TEXT("left face is brick"), Document->GetDocument().project().findWall(South)->finishLeft == "brick-red");
	const AFloor101SceneNodeActor* Wall = Cast<AFloor101SceneNodeActor>(Document->FindWallActor(South));
	if (TestNotNull(TEXT("wall actor"), Wall))
	{
		TestTrue(TEXT("the actor shows the bricks"), Wall->GetDisplayMaterial() == Library->GetFinishMaterial(TEXT("brick-red")));
	}
	TestTrue(TEXT("oak"), Document->Edit([](EditController& Editor) { return Editor.setPaintFinish("wood-oak"); }).ok());
	TestTrue(TEXT("paint the floor"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(2000.0, 1500.0)); }).ok());
	const FString Room = Document->GetRooms().Num() == 1 ? Document->GetRooms()[0].Key : FString();
	const AFloor101SceneNodeActor* Floor = Cast<AFloor101SceneNodeActor>(Document->FindRoomFloorActor(Room));
	if (TestNotNull(TEXT("floor actor"), Floor))
	{
		TestTrue(TEXT("the floor shows the oak"), Floor->GetDisplayMaterial() == Library->GetFinishMaterial(TEXT("wood-oak")));
	}

	// Select the room by its floor; the floor turns orange, and its ceiling finish is set from the selection.
	TestTrue(TEXT("select mode"), Document->Edit([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Select); return f101::Status::success(); }).ok());
	TestTrue(TEXT("click the floor"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(2000.0, 1500.0)); }).ok());
	TestEqual(TEXT("room selected"), FString(UTF8_TO_TCHAR(Document->GetEditController().selectedRoom().c_str())), Room);
	if (Floor)
	{
		TestNotNull(TEXT("selected floor is tinted"), Cast<UMaterialInstanceDynamic>(Floor->GetDisplayMaterial()));
		TestTrue(TEXT("not the oak any more"), Floor->GetDisplayMaterial() != Library->GetFinishMaterial(TEXT("wood-oak")));
	}
	TestTrue(TEXT("ceiling"), Document->Edit([](EditController& Editor) { return Editor.setSelectedRoomCeiling("wood-oak"); }).ok());
	const f101::cad::RoomFinish* Finish = Document->GetDocument().project().findRoomFinish(TCHAR_TO_UTF8(*Room));
	TestTrue(TEXT("floor and ceiling stored by room key"), Finish && Finish->floor == "wood-oak" && Finish->ceiling == "wood-oak");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CarpentryInteractionTest, "Floor101.Interaction.Carpentry", TestFlags)
bool FFloor101CarpentryInteractionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document subsystem"), Document))
	{
		return false;
	}
	// A 6 m wall with a wall joined in a T at 2 m, and a window on it.
	f101::cad::Document& Model = Document->GetMutableDocument();
	const f101::cad::WallId Host = Model.addWall({0, 0}, {6000, 0}, 120, 2800).value();
	(void)Model.addWall({2000, 0}, {2000, 3000}, 100, 2800);
	TestFalse(TEXT("no window across the T (the subsystem checks joints)"),
		Model.addOpening(Host, f101::cad::Opening{f101::cad::OpeningId{}, f101::cad::OpeningKind::Window, 1500, 1200, 1200, 900}).ok());
	const f101::cad::OpeningId Window =
		Model.addOpening(Host, f101::cad::Opening{f101::cad::OpeningId{}, f101::cad::OpeningKind::Window, 3000, 1200, 1200, 900}).value();
	const FString NodeId = UTF8_TO_TCHAR((f101::cad::toString(Host) + "/opening/" + f101::cad::toString(Window)).c_str());
	TestNotNull(TEXT("its carpentry has an actor"), FindNode(TestWorld.World, NodeId));

	// Drag it 830 mm east through the subsystem, as the viewport does: one change, on the 50 mm grid.
	TestTrue(TEXT("select it"), Document->Edit([Host, Window](EditController& Editor) { return Editor.selectOpening(Host, Window); }).ok());
	TestTrue(TEXT("grab the hole"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(3600.0, 30.0)); }).ok());
	Document->UpdatePointer(Down(4430.0, 30.0));
	TestTrue(TEXT("release"), Document->Edit([](EditController& Editor) { return Editor.pointerReleased(Down(4430.0, 30.0)); }).ok());
	TestEqual(TEXT("moved on the grid"), Document->GetDocument().project().findWall(Host)->findOpening(Window)->offsetMm, int64(3850));
	const AFloor101SceneNodeActor* Carpentry = FindNode(TestWorld.World, NodeId);
	if (TestNotNull(TEXT("same carpentry actor"), Carpentry))
	{
		TestEqual(TEXT("its frame followed (385 cm)"), Carpentry->GetMeshBounds().Min.X, 385.0, 1e-6);
	}

	// Esc (the Cancel key command) in the middle of a drag: the release that follows changes nothing.
	TestTrue(TEXT("grab it again"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(4400.0, 30.0)); }).ok());
	Document->UpdatePointer(Down(5000.0, 30.0));
	(void)Document->Edit([](EditController& Editor) { Editor.cancel(); return f101::Status::success(); });
	TestFalse(TEXT("the drag is dropped"), Document->GetEditController().isDraggingOpening());
	(void)Document->Edit([](EditController& Editor) { return Editor.pointerReleased(Down(5000.0, 30.0)); });
	TestEqual(TEXT("not moved"), Document->GetDocument().project().findWall(Host)->findOpening(Window)->offsetMm, int64(3850));
	TestTrue(TEXT("still selected"), Document->GetEditController().selectedOpening().has_value());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101OpeningTypesInteractionTest, "Floor101.Interaction.OpeningTypes", TestFlags)
bool FFloor101OpeningTypesInteractionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document subsystem"), Document))
	{
		return false;
	}
	const f101::cad::WallId Host = Document->GetMutableDocument().addWall({0, 0}, {6000, 0}, 120, 2800).value();

	// The door tool, pointing at the south half of the wall: the door opens south; Space puts its hinges at its end.
	(void)Document->Edit([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Door); return f101::Status::success(); });
	Document->UpdatePointer(Down(3000.0, -40.0));
	TestTrue(TEXT("Space"), Document->Edit([](EditController& Editor) { return Editor.flip(); }).ok());
	TestTrue(TEXT("click places it"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(3000.0, -40.0)); }).ok());
	const f101::cad::Opening Placed = Document->GetDocument().project().findWall(Host)->openings.front();
	TestTrue(TEXT("opens south, hinged at its end"), Placed.opensTo == f101::cad::WallSide::Right && Placed.hinge == f101::cad::OpeningEnd::End);

	// Selected: shown 100 % open, its leaf stands south of the wall (Unreal +Y) in the same actor; its swing is drawn.
	(void)Document->Edit([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Select); return f101::Status::success(); });
	TestTrue(TEXT("select it"), Document->Edit([Host, &Placed](EditController& Editor) { return Editor.selectOpening(Host, Placed.id); }).ok());
	const FString NodeId = UTF8_TO_TCHAR((f101::cad::toString(Host) + "/opening/" + f101::cad::toString(Placed.id)).c_str());
	const AFloor101SceneNodeActor* Carpentry = FindNode(TestWorld.World, NodeId);
	if (!TestNotNull(TEXT("carpentry actor"), Carpentry) || !TestNotNull(TEXT("swing symbol"), FindNode(TestWorld.World, NodeId + TEXT("/swing"))))
	{
		return false;
	}
	TestEqual(TEXT("closed: inside the wall"), Carpentry->GetMeshBounds().Max.Y, 5.0, 1e-6);
	TestTrue(TEXT("100 % open"), Document->Edit([](EditController& Editor) { return Editor.setOpeningOpenPercent("100"); }).ok());
	TestTrue(TEXT("same actor"), FindNode(TestWorld.World, NodeId) == Carpentry);
	TestEqual(TEXT("open south: 81.4 cm out"), Carpentry->GetMeshBounds().Max.Y, 81.4, 1e-6);

	// A sliding door leaves no swing; a window cannot become a door's type it does not have, and a door cannot be fixed.
	TestTrue(TEXT("sliding"), Document->Edit([](EditController& Editor) { return Editor.setOpeningOperation(f101::cad::OpeningOperation::Sliding); }).ok());
	TestNull(TEXT("no swing for a sliding door"), FindNode(TestWorld.World, NodeId + TEXT("/swing")));
	AddExpectedMessagePlain(TEXT("A door cannot be fixed"), ELogVerbosity::Warning);
	TestFalse(TEXT("fixed door refused"), Document->Edit([](EditController& Editor) { return Editor.setOpeningOperation(f101::cad::OpeningOperation::Fixed); }).ok());
	TestTrue(TEXT("the refusal reaches the status bar"), Document->IsStatusError());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101PlanViewTest, "Floor101.Interaction.PlanView", TestFlags)
bool FFloor101PlanViewTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document subsystem"), Document))
	{
		return false;
	}
	TArray<f101::cad::WallId> Walls;
	const int64 Corners[][2] = {{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}};
	for (int32 i = 0; i < 4; ++i)
	{
		Walls.Add(Document->GetMutableDocument().addWall({Corners[i][0], Corners[i][1]}, {Corners[(i + 1) % 4][0], Corners[(i + 1) % 4][1]}, 120, 2800).value());
	}

	// Shown: every refresh draws the edited level, with the room in Spanish and the editing marks.
	TestTrue(TEXT("no drawing while hidden"), Document->GetPlanDrawing().empty());
	Document->SetPlanView(true);
	const f101::drawing::PlanDrawing& Plan = Document->GetPlanDrawing();
	TestEqual(TEXT("four cut walls"), static_cast<int32>(Plan.fills.size()), 4);
	auto HasLabel = [&Plan](const char* Text)
	{
		return std::any_of(Plan.labels.begin(), Plan.labels.end(), [Text](const f101::drawing::Label& Label) { return Label.text == Text; });
	};
	TestTrue(TEXT("room name"), HasLabel("Habitación 1"));
	TestTrue(TEXT("its area with a decimal comma"), HasLabel("11,17 m²"));
	TestTrue(TEXT("a length in metres"), HasLabel("4,00"));
	TestTrue(TEXT("select the south wall"), Document->Edit([&Walls](EditController& Editor) { return Editor.select(Walls[0]); }).ok());
	TestTrue(TEXT("tinted as selected"), std::any_of(Plan.fills.begin(), Plan.fills.end(),
		[](const f101::drawing::Fill& Fill) { return Fill.ink == f101::drawing::Ink::Selected; }));
	TestTrue(TEXT("with its handles"), std::any_of(Plan.fills.begin(), Plan.fills.end(),
		[](const f101::drawing::Fill& Fill) { return Fill.ink == f101::drawing::Ink::Handle; }));
	Document->SetPlanView(false);
	TestTrue(TEXT("hidden: nothing drawn"), Document->GetPlanDrawing().empty());

	// The camera of the plan: orthographic, straight down, north up, as wide as the view was.
	AFloor101EditingPawn* Pawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	if (TestNotNull(TEXT("pawn"), Pawn))
	{
		Pawn->SetPlanView(true);
		TestTrue(TEXT("orthographic"), Pawn->GetCamera()->ProjectionMode == ECameraProjectionMode::Orthographic);
		TestEqual(TEXT("straight down"), Pawn->GetActorRotation().Pitch, -90.0, 1e-3);
		TestEqual(TEXT("ortho width = view width"), static_cast<double>(Pawn->GetCamera()->OrthoWidth), Pawn->GetViewWidth(), 1e-2);
		Pawn->Frame(FBox(FVector(0.0, -300.0, 0.0), FVector(400.0, 0.0, 280.0)));
		TestTrue(TEXT("framed: the plan and its dimensions fit"), Pawn->GetViewWidth() > 400.0 && Pawn->GetViewWidth() < 1500.0);
		TestTrue(TEXT("far above the walls"), Pawn->GetActorLocation().Z > 10000.0);
		Pawn->Orbit(FVector2D(100.0, 50.0));
		TestEqual(TEXT("no orbit in the plan"), Pawn->GetActorRotation().Pitch, -90.0, 1e-3);
		Pawn->SetPlanView(false);
		TestTrue(TEXT("back to perspective"), Pawn->GetCamera()->ProjectionMode == ECameraProjectionMode::Perspective);
	}

	// Exports: SVG always; PNG where there is a renderer. Default paths sit next to the project file.
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101"), TEXT("Plano ñ")));
	const FString SvgPath = FPaths::Combine(Directory, TEXT("casa-planta-baja.svg"));
	IFileManager::Get().Delete(*SvgPath, false, true, true);
	TestTrue(TEXT("SVG exported"), Document->ExportPlan(SvgPath, f101::cad::LevelId()).ok());
	FString Svg;
	TestTrue(TEXT("SVG on disk"), FFileHelper::LoadFileToString(Svg, *SvgPath) && Svg.Contains(TEXT("Habitación 1")));
	TestTrue(TEXT("no editing marks in exports"), !Svg.Contains(TEXT("#ff6b14")));
	TestTrue(TEXT("status tells where"), Document->GetStatusMessage().ToString().Contains(SvgPath));
	TestTrue(TEXT("save the project"), Document->SaveProject(FPaths::Combine(Directory, TEXT("casa.json"))).ok());
	TestEqual(TEXT("default plan path"), Document->GetDefaultPlanPath(TEXT("png")), FPaths::Combine(Directory, TEXT("casa-planta-baja.png")));
	const FString PngPath = FPaths::Combine(Directory, TEXT("casa-planta-baja.png"));
	if (GUsingNullRHI)
	{
		AddExpectedMessagePlain(TEXT("needs a renderer"), ELogVerbosity::Warning);
		TestFalse(TEXT("PNG needs a renderer"), Document->ExportPlan(PngPath, f101::cad::LevelId()).ok());
	}
	else
	{
		TestTrue(TEXT("PNG exported"), Document->ExportPlan(PngPath, f101::cad::LevelId()).ok());
		TestTrue(TEXT("PNG on disk"), IFileManager::Get().FileSize(*PngPath) > 1000);
		// The cut walls keep their ink: the darkest common colour is the cut's #262626, not paler or darker.
		FImage Image;
		if (TestTrue(TEXT("PNG readable"), FImageUtils::LoadImage(*PngPath, Image)))
		{
			Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			const FImageView View = Image;
			int32 Cut = 0;
			for (int64 Index = 0; Index < View.GetNumPixels(); ++Index)
			{
				const FColor Pixel = View.AsBGRA8()[Index];
				Cut += FMath::Abs(Pixel.R - 38) <= 3 && FMath::Abs(Pixel.G - 38) <= 3 && FMath::Abs(Pixel.B - 38) <= 3 ? 1 : 0;
			}
			TestTrue(TEXT("walls drawn in the cut ink (#262626)"), Cut > 1000);
		}
	}
	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
