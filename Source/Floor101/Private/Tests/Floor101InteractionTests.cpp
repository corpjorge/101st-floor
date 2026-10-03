// Unreal integration tests of the editing layer (ADR-0008): keyboard map, orbit camera, and the document subsystem driving
// the edit controller, the scene and project.json. Run with .\Scripts\Test.ps1 -Integration.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "F101/Assets/Library.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101EditingPawn.h"
#include "Floor101FileActions.h"
#include "Floor101FinishLibrary.h"
#include "Floor101GameViewportClient.h"
#include "Floor101SceneNodeActor.h"
#include "Camera/CameraComponent.h"
#include "DynamicRHI.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Scene/FinishTextures.h"
#include "F101/Scene/SceneCompiler.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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
	TestTrue(TEXT("A: furniture"), Map(EKeys::A) == Command::FurnitureTool);
	TestTrue(TEXT("C: column"), Map(EKeys::C) == Command::ColumnTool);
	TestTrue(TEXT("I: services run"), Map(EKeys::I) == Command::ServicesTool);
	TestTrue(TEXT("X: services layer"), Map(EKeys::X) == Command::ServicesLayer);
	TestTrue(TEXT("C typed in a length is text ('355 cm')"),
		UFloor101GameViewportClient::MapKey(EKeys::C, false, false, false, true) == Command::None);
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
	TestTrue(TEXT("R: photo"), Map(EKeys::R) == Command::PhotoMode);
	TestTrue(TEXT("M typed in a length is text ('3.55 m')"),
		UFloor101GameViewportClient::MapKey(EKeys::M, false, false, false, true) == Command::None);
	TestTrue(TEXT("W still switches while typing"), UFloor101GameViewportClient::MapKey(EKeys::W, false, false, false, true) == Command::WallTool);
	TestTrue(TEXT("Backspace: backspace"), Map(EKeys::BackSpace) == Command::Backspace);
	TestTrue(TEXT("Ctrl+Z: undo"), Map(EKeys::Z, true) == Command::Undo);
	TestTrue(TEXT("Ctrl+Shift+Z: redo"), Map(EKeys::Z, true, true) == Command::Redo);
	TestTrue(TEXT("Ctrl+Y: redo"), Map(EKeys::Y, true) == Command::Redo);
	TestTrue(TEXT("Ctrl+S: save"), Map(EKeys::S, true) == Command::Save);
	TestTrue(TEXT("Ctrl+Shift+S: save as"), Map(EKeys::S, true, true) == Command::SaveAs);
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

	// The window tool places the type chosen for it beforehand: a fixed window, closed, with no swing.
	TestTrue(TEXT("fixed windows chosen"), Document->Edit([](EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Window);
		return Editor.setNewOpeningOperation(f101::cad::OpeningKind::Window, f101::cad::OpeningOperation::Fixed);
	}).ok());
	Document->UpdatePointer(Down(1000.0, 40.0));
	TestTrue(TEXT("click places it"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(1000.0, 40.0)); }).ok());
	const std::vector<f101::cad::Opening>& Openings = Document->GetDocument().project().findWall(Host)->openings;
	const auto Window = std::find_if(Openings.begin(), Openings.end(),
		[](const f101::cad::Opening& Opening) { return Opening.kind == f101::cad::OpeningKind::Window; });
	if (TestTrue(TEXT("a window placed"), Window != Openings.end()))
	{
		TestTrue(TEXT("fixed and closed"), Window->operation == f101::cad::OpeningOperation::Fixed && Window->openPercent == 0);
		const FString WindowNode = UTF8_TO_TCHAR((f101::cad::toString(Host) + "/opening/" + f101::cad::toString(Window->id)).c_str());
		TestNotNull(TEXT("with its carpentry"), FindNode(TestWorld.World, WindowNode));
		TestNull(TEXT("and no swing"), FindNode(TestWorld.World, WindowNode + TEXT("/swing")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101PhotoModeTest, "Floor101.Interaction.PhotoMode", TestFlags)
bool FFloor101PhotoModeTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!TestNotNull(TEXT("document subsystem"), Document) || !TestNotNull(TEXT("finish library"), Library))
	{
		return false;
	}
	const f101::cad::WallId Host = Document->GetMutableDocument().addWall({0, 0}, {4000, 0}, 120, 2800).value();
	const f101::cad::OpeningId Door = Document->GetMutableDocument().addOpening(Host,
		f101::cad::Opening{f101::cad::OpeningId{}, f101::cad::OpeningKind::Door, 1000, 900, 2100, 0}).value();
	TestTrue(TEXT("select the wall"), Document->Edit([Host](EditController& Editor) { return Editor.select(Host); }).ok());
	const FString SwingNode = UTF8_TO_TCHAR((f101::cad::toString(Host) + "/opening/" + f101::cad::toString(Door) + "/swing").c_str());
	const AFloor101SceneNodeActor* Wall = FindNode(TestWorld.World, UTF8_TO_TCHAR(f101::scene::wallBodyNodeId(Host).value.c_str()));
	if (!TestNotNull(TEXT("wall actor"), Wall))
	{
		return false;
	}
	UMaterialInterface* Plaster = Library->GetFinishMaterial(TEXT("plaster-white"));
	TestTrue(TEXT("selected: tinted"), Wall->GetDisplayMaterial() != Plaster);
	TestNotNull(TEXT("the door's swing"), FindNode(TestWorld.World, SwingNode));

	// A photo (ADR-0018) shows the project as it is: no selection tint, no editing symbols.
	Document->SetPhotoMode(true);
	TestTrue(TEXT("photo mode"), Document->IsPhotoMode());
	TestTrue(TEXT("its own finish"), Wall->GetDisplayMaterial() == Plaster);
	TestNull(TEXT("no swing"), FindNode(TestWorld.World, SwingNode));
	TestTrue(TEXT("a PNG next to the project"), Document->GetDefaultPhotoPath().EndsWith(TEXT(".png")) &&
		FPaths::GetCleanFilename(Document->GetDefaultPhotoPath()).Contains(TEXT("-foto-")));
	AddExpectedMessagePlain(TEXT("F101: Save the photo as .png"), ELogVerbosity::Warning);
	TestFalse(TEXT("another format refused"), Document->SavePhoto(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("foto.jpg"))).ok());

	Document->SetPhotoMode(false);
	TestNotNull(TEXT("back to editing: the swing"), FindNode(TestWorld.World, SwingNode));
	TestTrue(TEXT("and the selection"), Wall->GetDisplayMaterial() != Plaster);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101FurnitureInteractionTest, "Floor101.Interaction.Furniture", TestFlags)
bool FFloor101FurnitureInteractionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!TestNotNull(TEXT("document subsystem"), Document) || !TestNotNull(TEXT("finish library"), Library))
	{
		return false;
	}
	(void)Document->GetMutableDocument().addWall({0, 0}, {6000, 0}, 120, 2800);

	// Furniture mode (A), a sofa: near the wall its preview turns its back to the wall's face; a click places it.
	(void)Document->Edit([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Furniture); return Editor.setFurnitureType("sofa"); });
	Document->UpdatePointer(Down(3020.0, 700.0));
	TestNotNull(TEXT("previewed"), FindNode(TestWorld.World, TEXT("preview/furniture/0")));
	TestTrue(TEXT("click places it"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(3020.0, 700.0)); }).ok());
	const std::vector<f101::cad::Furniture>& Pieces = Document->GetDocument().project().furniture;
	if (!TestEqual(TEXT("one piece"), static_cast<int32>(Pieces.size()), 1))
	{
		return false;
	}
	const f101::cad::FurnitureId Sofa = Pieces[0].id;
	TestTrue(TEXT("against the wall, facing north"), Pieces[0].position == f101::cad::Point2D{3000, 510} && Pieces[0].rotationCentidegrees == 18000);
	const FString NodeId = UTF8_TO_TCHAR(f101::scene::furnitureNodeId(Sofa).value.c_str());
	const AFloor101SceneNodeActor* Actor = FindNode(TestWorld.World, NodeId);
	if (!TestNotNull(TEXT("its actor"), Actor))
	{
		return false;
	}
	TestTrue(TEXT("looked up by id"), Document->FindFurnitureActor(Sofa) == Actor);
	const FBox Bounds = Actor->GetMeshBounds();
	TestEqual(TEXT("its back on the face (Unreal Y mirrored: -6 cm)"), Bounds.Max.Y, -6.0, 1e-6);
	TestEqual(TEXT("200 cm wide"), Bounds.GetSize().X, 200.0, 1e-6);
	TestEqual(TEXT("on the room floor"), Bounds.Min.Z, 1.0, 1e-6);
	TestEqual(TEXT("85 cm tall"), Bounds.GetSize().Z, 85.0, 1e-6);
	TestTrue(TEXT("its first section (the feet) in brushed steel"), Actor->GetDisplayMaterial() == Library->GetFinishMaterial(TEXT("steel-brushed")));
	TestNotNull(TEXT("its upholstery has a synthesized texture"),
		Library->GetFinishMaterial(TEXT("fabric-grey")) ? Library->FindFinishTexture(TEXT("fabric-grey")) : nullptr);

	// Select it by clicking on it, drag it 1 m east along the wall: one change, and the same actor follows.
	(void)Document->Edit([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Select); return f101::Status::success(); });
	TestTrue(TEXT("click on it"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(3000.0, 500.0)); }).ok());
	TestTrue(TEXT("selected"), Document->GetEditController().selectedFurniture().has_value());
	TestTrue(TEXT("tinted as selected"), Actor->GetDisplayMaterial() != Library->GetFinishMaterial(TEXT("steel-brushed")));
	Document->UpdatePointer(Down(4020.0, 520.0));
	TestTrue(TEXT("release"), Document->Edit([](EditController& Editor) { return Editor.pointerReleased(Down(4020.0, 520.0)); }).ok());
	TestTrue(TEXT("moved along the wall"), Document->GetDocument().project().findFurniture(Sofa)->position == f101::cad::Point2D{4000, 510});
	TestEqual(TEXT("one undoable change"), FString(UTF8_TO_TCHAR(std::string(Document->GetDocument().undoLabel()).c_str())), FString(TEXT("Change furniture")));
	TestTrue(TEXT("same actor"), FindNode(TestWorld.World, NodeId) == Actor);
	TestEqual(TEXT("it followed"), Actor->GetMeshBounds().Min.X, 300.0, 1e-6);

	// Space turns it; Delete removes it; undo brings it back. The plan draws it.
	TestTrue(TEXT("Space"), Document->Edit([](EditController& Editor) { return Editor.flip(); }).ok());
	TestEqual(TEXT("turned 90 degrees"), Document->GetDocument().project().findFurniture(Sofa)->rotationCentidegrees, int64(27000));
	TestTrue(TEXT("Delete"), Document->Edit([](EditController& Editor) { return Editor.deleteSelection(); }).ok());
	TestNull(TEXT("its actor is gone"), FindNode(TestWorld.World, NodeId));
	TestTrue(TEXT("undo"), Document->Edit([](EditController& Editor) { return Editor.undo(); }).ok());
	TestNotNull(TEXT("back"), FindNode(TestWorld.World, NodeId));
	Document->SetPlanView(true);
	const f101::drawing::PlanDrawing& Plan = Document->GetPlanDrawing();
	TestTrue(TEXT("drawn on the plan"), std::any_of(Plan.strokes.begin(), Plan.strokes.end(),
		[](const f101::drawing::Stroke& Stroke) { return Stroke.ink == f101::drawing::Ink::Furniture; }));
	Document->SetPlanView(false);

	// Saved and opened with the project.
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Furniture"), TEXT("project.json")));
	TestTrue(TEXT("saved"), Document->SaveProject(Path).ok());
	FString Json;
	TestTrue(TEXT("schema 10 with the sofa"), FFileHelper::LoadFileToString(Json, *Path) && Json.Contains(TEXT("\"schemaVersion\": 10")) &&
		Json.Contains(TEXT("\"type\": \"sofa\"")));
	TestTrue(TEXT("removed before opening"), Document->GetMutableDocument().execute(f101::cad::DeleteFurnitureCommand{Sofa}).ok());
	TestTrue(TEXT("opened"), Document->OpenProject(Path).ok());
	TestTrue(TEXT("the sofa is back from the file"), Document->GetDocument().project().findFurniture(Sofa) != nullptr);
	TestNotNull(TEXT("with its actor"), FindNode(TestWorld.World, NodeId));
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101ServicesInteractionTest, "Floor101.Interaction.Services", TestFlags)
bool FFloor101ServicesInteractionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!TestNotNull(TEXT("document subsystem"), Document) || !TestNotNull(TEXT("finish library"), Library))
	{
		return false;
	}
	// The map's own ground, like the template's floor.
	AStaticMeshActor* Ground = TestWorld.World->SpawnActor<AStaticMeshActor>();
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* GroundMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (!TestNotNull(TEXT("ground"), Ground) || !TestNotNull(TEXT("plane mesh"), Plane) || !TestNotNull(TEXT("ground material"), GroundMaterial))
	{
		return false;
	}
	Ground->GetStaticMeshComponent()->SetStaticMesh(Plane);
	Ground->GetStaticMeshComponent()->SetMaterial(0, GroundMaterial);
	Document->OnWorldBeginPlay(*TestWorld.World);
	TestFalse(TEXT("the map's meshes cast no shadow (they flood VSM's non-Nanite queue)"), Ground->GetStaticMeshComponent()->CastShadow);
	const f101::cad::WallId Wall = Document->GetMutableDocument().addWall({0, 0}, {4000, 0}, 120, 2800).value();
	const AFloor101SceneNodeActor* WallActor = Cast<AFloor101SceneNodeActor>(Document->FindWallActor(Wall));
	if (!TestNotNull(TEXT("the wall's actor"), WallActor))
	{
		return false;
	}
	UMaterialInterface* const Plaster = WallActor->GetDisplayMaterial();

	// Column mode (C): the preview snaps to the wall's end; a click places a concrete column as tall as the level.
	(void)Document->Edit([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Column); return f101::Status::success(); });
	Document->UpdatePointer(Down(3920.0, 90.0));
	TestNotNull(TEXT("previewed"), FindNode(TestWorld.World, TEXT("preview/column/0")));
	TestTrue(TEXT("click places it"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(3920.0, 90.0)); }).ok());
	const std::vector<f101::cad::Column>& Columns = Document->GetDocument().project().columns;
	if (!TestEqual(TEXT("one column"), static_cast<int32>(Columns.size()), 1))
	{
		return false;
	}
	const f101::cad::ColumnId Column = Columns[0].id;
	TestTrue(TEXT("on the wall's end"), Columns[0].position == f101::cad::Point2D{4000, 0});
	const AFloor101SceneNodeActor* ColumnActor = Cast<AFloor101SceneNodeActor>(Document->FindColumnActor(Column));
	if (!TestNotNull(TEXT("its actor"), ColumnActor))
	{
		return false;
	}
	TestEqual(TEXT("30 cm wide"), ColumnActor->GetMeshBounds().GetSize().X, 30.0, 1e-6);
	TestEqual(TEXT("as tall as the level"), ColumnActor->GetMeshBounds().GetSize().Z, 280.0, 1e-6);
	TestTrue(TEXT("in concrete"), ColumnActor->GetDisplayMaterial() == Library->GetFinishMaterial(TEXT("concrete")));

	// Services mode (I) shows the layer: the walls see-through. A run in three clicks, the last one twice (a double click).
	(void)Document->Edit([](EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Services);
		return Editor.setServiceKind(f101::cad::ServiceKind::Electrical);
	});
	TestTrue(TEXT("the layer is on"), Document->GetEditController().showsServices());
	TestTrue(TEXT("the wall is a ghost"), WallActor->GetDisplayMaterial() == Library->GetGhostMaterial() && Library->GetGhostMaterial() != nullptr);
	TestTrue(TEXT("the column stays solid"), ColumnActor->GetDisplayMaterial() == Library->GetFinishMaterial(TEXT("concrete")));
	TestTrue(TEXT("the map's ground too is a ghost, for the runs under the ground floor"),
		Ground->GetStaticMeshComponent()->GetMaterial(0) == Library->GetGhostMaterial());
	for (const FVector2D& At : {FVector2D(500.0, 600.0), FVector2D(3500.0, 600.0), FVector2D(3500.0, 600.0)})
	{
		Document->UpdatePointer(Down(At.X, At.Y));
		TestTrue(TEXT("click"), Document->Edit([&At](EditController& Editor) { return Editor.pointerPressed(Down(At.X, At.Y)); }).ok());
		TestTrue(TEXT("release"), Document->Edit([&At](EditController& Editor) { return Editor.pointerReleased(Down(At.X, At.Y)); }).ok());
	}
	const std::vector<f101::cad::ServiceRun>& Runs = Document->GetDocument().project().services;
	if (!TestEqual(TEXT("one run"), static_cast<int32>(Runs.size()), 1))
	{
		return false;
	}
	const f101::cad::ServiceRunId Run = Runs[0].id;
	TestTrue(TEXT("two points at 30 cm"), Runs[0].points == std::vector<f101::cad::ServicePoint>{{500, 600, 300}, {3500, 600, 300}});
	TestEqual(TEXT("one undoable step"), FString(UTF8_TO_TCHAR(std::string(Document->GetDocument().undoLabel()).c_str())), FString(TEXT("Add service run")));
	const AFloor101SceneNodeActor* RunActor = Cast<AFloor101SceneNodeActor>(Document->FindServiceRunActor(Run));
	if (!TestNotNull(TEXT("the run's actor"), RunActor))
	{
		return false;
	}
	TestTrue(TEXT("green"), RunActor->GetDisplayMaterial() == Library->GetFinishMaterial(TEXT("service-electrical")));
	TestEqual(TEXT("300 cm long"), RunActor->GetMeshBounds().GetSize().X, 300.0, 1e-6);
	TestEqual(TEXT("at 30 cm"), RunActor->GetMeshBounds().GetCenter().Z, 30.0, 0.01);

	// The plan draws the column and, with the layer, the run; so does the exported SVG.
	Document->SetPlanView(true);
	const f101::drawing::PlanDrawing& Plan = Document->GetPlanDrawing();
	TestTrue(TEXT("the run on the plan"), std::any_of(Plan.strokes.begin(), Plan.strokes.end(),
		[](const f101::drawing::Stroke& Stroke) { return Stroke.ink == f101::drawing::Ink::ServiceElectrical; }));
	Document->SetPlanView(false);
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Services")));
	const FString SvgPath = FPaths::Combine(Folder, TEXT("plano.svg"));
	FString Svg;
	TestTrue(TEXT("plan exported"), Document->ExportPlan(SvgPath, f101::cad::LevelId()).ok() && FFileHelper::LoadFileToString(Svg, *SvgPath));
	TestTrue(TEXT("with the green run"), Svg.Contains(TEXT("#2e9c48")));

	// Each network can be hidden: without electricity, no green run in 3D, in the plan or in the exported plan.
	(void)Document->Edit([](EditController& Editor)
	{
		Editor.setServiceKindShown(f101::cad::ServiceKind::Electrical, false);
		return f101::Status::success();
	});
	TestNull(TEXT("the wiring is hidden"), Document->FindServiceRunActor(Run));
	TestTrue(TEXT("the walls stay see-through"), WallActor->GetDisplayMaterial() == Library->GetGhostMaterial());
	Document->SetPlanView(true);
	TestFalse(TEXT("nor on the plan"), std::any_of(Plan.strokes.begin(), Plan.strokes.end(),
		[](const f101::drawing::Stroke& Stroke) { return Stroke.ink == f101::drawing::Ink::ServiceElectrical; }));
	Document->SetPlanView(false);
	FString SvgWithout;
	TestTrue(TEXT("plan exported without it"), Document->ExportPlan(SvgPath, f101::cad::LevelId()).ok() &&
		FFileHelper::LoadFileToString(SvgWithout, *SvgPath) && !SvgWithout.Contains(TEXT("#2e9c48")));
	(void)Document->Edit([](EditController& Editor)
	{
		Editor.setServiceKindShown(f101::cad::ServiceKind::Electrical, true);
		return f101::Status::success();
	});
	TestNotNull(TEXT("shown again"), Document->FindServiceRunActor(Run));

	// X hides the layer: no run, the wall solid again; selecting the run shows it again.
	(void)Document->Edit([](EditController& Editor) { Editor.setShowServices(false); return f101::Status::success(); });
	TestNull(TEXT("the run is hidden"), Document->FindServiceRunActor(Run));
	TestTrue(TEXT("the wall is solid"), WallActor->GetDisplayMaterial() == Plaster);
	TestTrue(TEXT("the ground has its material back"), Ground->GetStaticMeshComponent()->GetMaterial(0) == GroundMaterial);
	TestTrue(TEXT("select the run"), Document->Edit([Run](EditController& Editor) { return Editor.selectServiceRun(Run); }).ok());
	TestNotNull(TEXT("shown again"), Document->FindServiceRunActor(Run));
	TestTrue(TEXT("tinted as selected"), Cast<AFloor101SceneNodeActor>(Document->FindServiceRunActor(Run))->GetDisplayMaterial() !=
		Library->GetFinishMaterial(TEXT("service-electrical")));
	TestTrue(TEXT("turned to water"), Document->Edit([](EditController& Editor) { return Editor.setServiceRunKind(f101::cad::ServiceKind::Water); }).ok());

	// Saved and opened with the project.
	const FString Path = FPaths::Combine(Folder, TEXT("project.json"));
	TestTrue(TEXT("saved"), Document->SaveProject(Path).ok());
	FString Json;
	TestTrue(TEXT("schema 10 with the column and the run"), FFileHelper::LoadFileToString(Json, *Path) && Json.Contains(TEXT("\"schemaVersion\": 10")) &&
		Json.Contains(TEXT("\"columns\"")) && Json.Contains(TEXT("\"kind\": \"water\"")));
	TestTrue(TEXT("removed before opening"), Document->GetMutableDocument().execute(f101::cad::DeleteColumnCommand{Column}).ok());
	TestTrue(TEXT("opened"), Document->OpenProject(Path).ok());
	TestNotNull(TEXT("the column is back"), Document->FindColumnActor(Column));
	TestTrue(TEXT("the run is back"), Document->GetDocument().project().findServiceRun(Run) != nullptr);
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101FilePathsTest, "Floor101.Interaction.FilePaths", TestFlags)
bool FFloor101FilePathsTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document subsystem"), Document))
	{
		return false;
	}
	// The user's own remembered places are put back at the end.
	const FString UserProject = FFloor101FileActions::GetLastProjectPath();
	const FString UserTwinmotion = FFloor101FileActions::GetLastTwinmotionFolder();
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101FilePaths")));
	const FString Path = FPaths::Combine(Folder, TEXT("mi casa.json"));

	// Remembered between sessions (the user's settings).
	FFloor101FileActions::RememberProjectPath(Path);
	FFloor101FileActions::RememberTwinmotionFolder(Folder);
	TestEqual(TEXT("project remembered"), FFloor101FileActions::GetLastProjectPath(), Path);
	TestEqual(TEXT("Twinmotion folder remembered"), FFloor101FileActions::GetLastTwinmotionFolder(), Folder);

	// "Guardar" on a project that has a file writes it there, without asking, and remembers it.
	(void)Document->GetMutableDocument().addWall({0, 0}, {3000, 0}, 120, 2800);
	TestTrue(TEXT("saved once"), Document->SaveProject(Path).ok());
	(void)Document->GetMutableDocument().addWall({3000, 0}, {3000, 2000}, 120, 2800);
	TestTrue(TEXT("unsaved change"), Document->HasUnsavedChanges());
	FFloor101FileActions::RememberProjectPath(FString());
	FFloor101FileActions::Save(*Document);
	TestFalse(TEXT("saved to its file"), Document->HasUnsavedChanges());
	TestEqual(TEXT("same file"), Document->GetProjectPath(), Path);
	TestEqual(TEXT("and remembered"), FFloor101FileActions::GetLastProjectPath(), Path);
	FString Json;
	TestTrue(TEXT("with both walls"), FFileHelper::LoadFileToString(Json, *Path) && Json.Contains(TEXT("[\n        3000,\n        2000\n      ]")));

	FFloor101FileActions::RememberProjectPath(UserProject);
	FFloor101FileActions::RememberTwinmotionFolder(UserTwinmotion);
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101AssetsInteractionTest, "Floor101.Interaction.Assets", TestFlags)
bool FFloor101AssetsInteractionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!TestNotNull(TEXT("document subsystem"), Document) || !TestNotNull(TEXT("finish library"), Library))
	{
		return false;
	}
	// The project lives in a test folder: its imported assets go next to it (ADR-0017).
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Assets"), TEXT("casa ñ")));
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Folder), false, true);
	TestTrue(TEXT("saved"), Document->SaveProject(FPaths::Combine(Folder, TEXT("project.json"))).ok());
	const FString Data = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Data/Assets")));
	const f101::cad::WallId Wall = Document->GetMutableDocument().addWall({0, 0}, {6000, 0}, 120, 2800).value();

	// A texture: imported, painted on a wall face; the wall shows it through a material with the decoded image.
	const f101::Result<f101::cad::AssetId> Parquet = Document->ImportAsset(FPaths::Combine(Data, TEXT("parquet.png")));
	if (!TestTrue(TEXT("texture imported"), Parquet.ok()))
	{
		return false;
	}
	const f101::cad::Asset* Texture = Document->GetDocument().project().findAsset(Parquet.value());
	const FString TextureFile = Texture ? UTF8_TO_TCHAR(Texture->file.c_str()) : FString(); // the list grows below
	TestTrue(TEXT("its file in the project"), Texture && FPaths::FileExists(Document->GetAssetFilePath(*Texture)));
	TestTrue(TEXT("the status says so"), Document->GetStatusMessage().ToString().Contains(TEXT("Importado: parquet")));
	const std::string ParquetRef = f101::cad::assetRef(Parquet.value());
	TestTrue(TEXT("paint with it"), Document->Edit([&ParquetRef](EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Paint);
		return Editor.setPaintFinish(ParquetRef);
	}).ok());
	TestTrue(TEXT("paint the wall"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(2000.0, 30.0)); }).ok());
	const FString ParquetId = UTF8_TO_TCHAR(ParquetRef.c_str());
	const AFloor101SceneNodeActor* WallActor = Cast<AFloor101SceneNodeActor>(Document->FindWallActor(Wall));
	if (TestNotNull(TEXT("wall actor"), WallActor))
	{
		TestTrue(TEXT("the wall shows the texture"), WallActor->GetDisplayMaterial() == Library->GetFinishMaterial(ParquetId));
	}
	const UTexture2D* ParquetTexture = Library->FindFinishTexture(ParquetId);
	if (TestNotNull(TEXT("the image decoded"), ParquetTexture))
	{
		TestEqual(TEXT("256 px"), ParquetTexture->GetSizeX(), 256);
		TestFalse(TEXT("not missing"), Library->IsAssetImageMissing(ParquetId));
	}

	// A GLB model: placed with the mouse in Furniture mode, at its size, with its own materials.
	const f101::Result<f101::cad::AssetId> Armchair = Document->ImportAsset(FPaths::Combine(Data, TEXT("armchair.glb")));
	if (!TestTrue(TEXT("model imported"), Armchair.ok()))
	{
		return false;
	}
	const std::string ArmchairRef = f101::cad::assetRef(Armchair.value());
	TestTrue(TEXT("furniture mode with it"), Document->Edit([&ArmchairRef](EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Furniture);
		return Editor.setFurnitureType(ArmchairRef);
	}).ok());
	Document->UpdatePointer(Down(3000.0, 2000.0));
	TestNotNull(TEXT("previewed"), FindNode(TestWorld.World, TEXT("preview/furniture/0")));
	TestTrue(TEXT("placed"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(3000.0, 2000.0)); }).ok());
	const f101::cad::Furniture Piece = Document->GetDocument().project().furniture.back(); // a copy: more are added
	TestTrue(TEXT("of the model"), Piece.type == ArmchairRef);
	const AFloor101SceneNodeActor* ChairActor = Cast<AFloor101SceneNodeActor>(Document->FindFurnitureActor(Piece.id));
	if (TestNotNull(TEXT("its actor"), ChairActor))
	{
		const FVector Size = ChairActor->GetMeshBounds().GetSize();
		TestEqual(TEXT("80 x 75 x 90 cm"), Size, FVector(80.0, 75.0, 90.0), 0.05f);
		TestTrue(TEXT("a model material"), ChairActor->GetDisplayMaterial() ==
			Library->GetFinishMaterial(UTF8_TO_TCHAR(f101::cad::modelMaterialFinish(Armchair.value(), 0).c_str())));
	}
	TestTrue(TEXT("its packed texture written out"), FPaths::FileExists(FPaths::Combine(Folder, TEXT("assets"),
		UTF8_TO_TCHAR(f101::cad::toString(Armchair.value()).c_str()), TEXT("embedded_0.png"))) ||
		FPaths::FileExists(FPaths::Combine(Folder, TEXT("assets"), UTF8_TO_TCHAR(f101::cad::toString(Armchair.value()).c_str()), TEXT("embedded_1.png"))));

	// An OBJ with its MTL and texture: every textured material finds its image.
	const f101::Result<f101::cad::AssetId> Lamp = Document->ImportAsset(FPaths::Combine(Data, TEXT("lamp.obj")));
	if (TestTrue(TEXT("OBJ imported"), Lamp.ok()))
	{
		TestTrue(TEXT("its shade texture came along"), FPaths::FileExists(FPaths::Combine(Folder, TEXT("assets"),
			UTF8_TO_TCHAR(f101::cad::toString(Lamp.value()).c_str()), TEXT("lamp_shade.png"))));
		TestTrue(TEXT("place it"), Document->Edit([&Lamp](EditController& Editor)
		{
			return Editor.setFurnitureType(f101::cad::assetRef(Lamp.value()));
		}).ok());
		TestTrue(TEXT("placed"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(1000.0, 2000.0)); }).ok());
		bool bTextured = false;
		for (int32 Material = 0; Material < 2; ++Material)
		{
			const FString Finish = UTF8_TO_TCHAR(f101::cad::modelMaterialFinish(Lamp.value(), static_cast<std::size_t>(Material)).c_str());
			const UTexture2D* Image = Library->GetFinishMaterial(Finish) ? Library->FindFinishTexture(Finish) : nullptr;
			bTextured = bTextured || (Image && Image->GetSizeX() == 128 && !Library->IsAssetImageMissing(Finish));
		}
		TestTrue(TEXT("the shade shows its striped image"), bTextured);
	}

	// Saved somewhere else, the assets go with it; the file refers to them relative to its folder.
	const FString Moved = FPaths::Combine(FPaths::GetPath(Folder), TEXT("copia"));
	TestTrue(TEXT("saved elsewhere"), Document->SaveProject(FPaths::Combine(Moved, TEXT("project.json"))).ok());
	TestTrue(TEXT("the texture went along"), !TextureFile.IsEmpty() && FPaths::FileExists(FPaths::Combine(Moved, TextureFile)));
	FString Json;
	TestTrue(TEXT("schema 10 with its assets"), FFileHelper::LoadFileToString(Json, *FPaths::Combine(Moved, TEXT("project.json"))) &&
		Json.Contains(TEXT("\"schemaVersion\": 10")) && Json.Contains(TEXT("\"file\": \"assets/")));
	TestTrue(TEXT("opened again"), Document->OpenProject(FPaths::Combine(Moved, TEXT("project.json"))).ok());
	TestEqual(TEXT("three assets"), static_cast<int32>(Document->GetDocument().project().assets.size()), 3);
	TestTrue(TEXT("the model still shows"), Document->GetAssetProblem(Armchair.value()).IsEmpty() &&
		FindNode(TestWorld.World, UTF8_TO_TCHAR(f101::scene::furnitureNodeId(Piece.id).value.c_str())) != nullptr);

	// Opened from a project whose folder lacks those files: they are read from the folder of the project opened.
	const FString Empty = FPaths::Combine(FPaths::GetPath(Folder), TEXT("sin assets"));
	IFileManager::Get().MakeDirectory(*Empty, true);
	TestTrue(TEXT("the first save has no assets"), IFileManager::Get().Copy(*FPaths::Combine(Empty, TEXT("project.json")),
		*FPaths::Combine(Folder, TEXT("project.json"))) == COPY_OK);
	TestTrue(TEXT("opened"), Document->OpenProject(FPaths::Combine(Empty, TEXT("project.json"))).ok());
	TestTrue(TEXT("the copy opened"), Document->OpenProject(FPaths::Combine(Moved, TEXT("project.json"))).ok());
	TestFalse(TEXT("its image found"), Library->IsAssetImageMissing(ParquetId));
	const UTexture2D* Swatch = Library->GetAssetFinishImage(ParquetId);
	TestTrue(TEXT("the swatch shows the image"), Swatch && Swatch->GetSizeX() == 256);
	if (const AFloor101SceneNodeActor* Reopened = Cast<AFloor101SceneNodeActor>(Document->FindFurnitureActor(Piece.id)))
	{
		TestTrue(TEXT("the model, not its placeholder"), Reopened->GetDisplayMaterial() ==
			Library->GetFinishMaterial(UTF8_TO_TCHAR(f101::cad::modelMaterialFinish(Armchair.value(), 0).c_str())));
	}
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Folder), false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101LibraryInteractionTest, "Floor101.Interaction.Library", TestFlags)
bool FFloor101LibraryInteractionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!TestNotNull(TEXT("document subsystem"), Document) || !TestNotNull(TEXT("finish library"), Library))
	{
		return false;
	}
	// The built-in library (ADR-0019), read from the project's Library folder: many pieces a category, and photographs.
	const f101::assets::Library& Shipped = UFloor101DocumentSubsystem::GetLibrary();
	TestTrue(TEXT("50 models or more"), Shipped.furniture.size() >= 50);
	TestTrue(TEXT("many chairs"), std::count_if(Shipped.furniture.begin(), Shipped.furniture.end(),
		[](const f101::assets::LibraryFurniture& Item) { return Item.category == f101::cad::FurnitureCategory::Seating; }) >= 5);
	TestTrue(TEXT("14 textures or more"), Shipped.textures.size() >= 14);
	const f101::assets::LibraryFurniture* Windsor = Shipped.findFurniture("chair-windsor");
	if (!TestNotNull(TEXT("the Windsor chair"), Windsor))
	{
		return false;
	}
	const UTexture2D* Picture = Library->GetPicture(FPaths::Combine(UFloor101DocumentSubsystem::GetLibraryFolder(), UTF8_TO_TCHAR(Windsor->thumbnail.c_str())));
	TestTrue(TEXT("its picture for the palette"), Picture && Picture->GetSizeX() == 256);

	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Library")));
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	TestTrue(TEXT("saved"), Document->SaveProject(FPaths::Combine(Folder, TEXT("project.json"))).ok());
	for (const auto& [From, To] : {std::pair<f101::cad::Point2D, f101::cad::Point2D>{{0, 0}, {4000, 0}}, {{4000, 0}, {4000, 4000}},
			 {{4000, 4000}, {0, 4000}}, {{0, 4000}, {0, 0}}})
	{
		(void)Document->GetMutableDocument().addWall(From, To, 120, 2800);
	}

	// A chair of the library: added to the project once (its files copied next to it), then placed like any model.
	const f101::Result<f101::cad::AssetId> Chair = Document->AddFromLibrary(TEXT("chair-windsor"));
	if (!TestTrue(TEXT("added"), Chair.ok()))
	{
		return false;
	}
	TestTrue(TEXT("the status says so"), Document->GetStatusMessage().ToString().Contains(TEXT("Añadido de la biblioteca: Silla Windsor")));
	const f101::cad::Asset ChairAsset = *Document->GetDocument().project().findAsset(Chair.value()); // a copy: more are added
	TestEqual(TEXT("it remembers the item"), FString(UTF8_TO_TCHAR(ChairAsset.library.c_str())), FString(TEXT("chair-windsor")));
	TestTrue(TEXT("its file in the project"), FPaths::FileExists(Document->GetAssetFilePath(ChairAsset)));
	const f101::Result<f101::cad::AssetId> Again = Document->AddFromLibrary(TEXT("chair-windsor"));
	TestTrue(TEXT("added once"), Again.ok() && Again.value() == Chair.value() && Document->GetDocument().project().assets.size() == 1);
	const std::string ChairRef = f101::cad::assetRef(Chair.value());
	TestTrue(TEXT("Amueblar with it"), Document->Edit([&ChairRef](EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Furniture);
		return Editor.setFurnitureType(ChairRef);
	}).ok());
	TestTrue(TEXT("placed"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(2000.0, 2000.0)); }).ok());
	const f101::cad::Furniture Piece = Document->GetDocument().project().furniture.back();
	const AFloor101SceneNodeActor* ChairActor = Cast<AFloor101SceneNodeActor>(Document->FindFurnitureActor(Piece.id));
	if (TestNotNull(TEXT("its actor"), ChairActor))
	{
		TestEqual(TEXT("at the model's width"), ChairActor->GetMeshBounds().GetSize().X, static_cast<double>(ChairAsset.model.widthMm) / 10.0, 0.5);
		TestTrue(TEXT("with its own materials"), Document->GetAssetProblem(Chair.value()).IsEmpty() && ChairActor->GetDisplayMaterial() ==
			Library->GetFinishMaterial(UTF8_TO_TCHAR(f101::cad::modelMaterialFinish(Chair.value(), 0).c_str())));
	}

	// A photographic laminate on the room's floor, at its real size.
	const f101::Result<f101::cad::AssetId> Laminate = Document->AddFromLibrary(TEXT("photo-laminate-oak"));
	if (!TestTrue(TEXT("laminate added"), Laminate.ok()))
	{
		return false;
	}
	const std::string LaminateRef = f101::cad::assetRef(Laminate.value());
	TestTrue(TEXT("Pintar with it"), Document->Edit([&LaminateRef](EditController& Editor)
	{
		Editor.setMode(f101::cad::EditMode::Paint);
		return Editor.setPaintFinish(LaminateRef);
	}).ok());
	TestTrue(TEXT("paint the floor"), Document->Edit([](EditController& Editor) { return Editor.pointerPressed(Down(1000.0, 3000.0)); }).ok());
	if (TestEqual(TEXT("one room"), Document->GetRooms().Num(), 1))
	{
		const AFloor101SceneNodeActor* Floor = Cast<AFloor101SceneNodeActor>(Document->FindRoomFloorActor(Document->GetRooms()[0].Key));
		const FString LaminateId = UTF8_TO_TCHAR(LaminateRef.c_str());
		TestTrue(TEXT("the floor shows the laminate"), Floor && Floor->GetDisplayMaterial() == Library->GetFinishMaterial(LaminateId));
		const UTexture2D* Photo = Library->FindFinishTexture(LaminateId);
		TestTrue(TEXT("its photograph decoded"), Photo && Photo->GetSizeX() == 1024 && !Library->IsAssetImageMissing(LaminateId));
	}

	// The project remembers where each asset came from (schema 9).
	TestTrue(TEXT("saved again"), Document->SaveProject(FPaths::Combine(Folder, TEXT("project.json"))).ok());
	FString Json;
	TestTrue(TEXT("with the library ids"), FFileHelper::LoadFileToString(Json, *FPaths::Combine(Folder, TEXT("project.json"))) &&
		Json.Contains(TEXT("\"library\": \"chair-windsor\"")) && Json.Contains(TEXT("\"library\": \"photo-laminate-oak\"")));
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101ExportInteractionTest, "Floor101.Interaction.Export", TestFlags)
bool FFloor101ExportInteractionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	UFloor101DocumentSubsystem* Document = TestWorld.World->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!TestNotNull(TEXT("document subsystem"), Document))
	{
		return false;
	}
	// The project in a test folder (non-ASCII path): the .glb goes next to it (ADR-0020).
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Export"), TEXT("casa ñ")));
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Folder), false, true);
	TestTrue(TEXT("saved"), Document->SaveProject(FPaths::Combine(Folder, TEXT("casa.json"))).ok());
	std::vector<f101::cad::WallId> Walls;
	for (const auto& [From, To] : {std::pair<f101::cad::Point2D, f101::cad::Point2D>{{0, 0}, {4000, 0}}, {{4000, 0}, {4000, 3000}},
			 {{4000, 3000}, {0, 3000}}, {{0, 3000}, {0, 0}}})
	{
		Walls.push_back(Document->GetMutableDocument().addWall(From, To, 120, 2800).value());
	}
	TestTrue(TEXT("bricks"), Document->GetMutableDocument().execute(f101::cad::SetWallFinishCommand{Walls[0], f101::cad::WallSide::Left, "brick-red"}).ok());
	const f101::cad::OpeningId Door = Document->GetMutableDocument().addOpening(Walls[0],
		f101::cad::Opening{f101::cad::OpeningId{}, f101::cad::OpeningKind::Door, 1500, 900, 2100, 0}).value();
	const f101::cad::FurnitureId Sofa = Document->GetMutableDocument().addFurniture(f101::cad::makeFurniture("sofa", {2000, 1500})).value();

	const FString Path = Document->GetDefaultGltfPath();
	TestEqual(TEXT("next to the project"), Path, FPaths::Combine(Folder, TEXT("casa.glb")));
	const f101::Result<FFloor101GltfSummary> Exported = Document->ExportGltf(Path);
	if (!TestTrue(TEXT("exported"), Exported.ok()))
	{
		return false;
	}
	TestTrue(TEXT("the status says so"), Document->GetStatusMessage().ToString().StartsWith(TEXT("Exportado para Twinmotion: ")));
	TestTrue(TEXT("walls, floor, slab, door and sofa"), Exported.value().Objects >= 8);

	// The file: header, JSON chunk, binary chunk (glTF 2.0, GLB).
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("written"), FFileHelper::LoadFileToArray(Bytes, *Path)) || !TestTrue(TEXT("big enough"), Bytes.Num() > 28))
	{
		return false;
	}
	auto Word = [&Bytes](int32 At) { return static_cast<uint32>(Bytes[At]) | (static_cast<uint32>(Bytes[At + 1]) << 8) |
		(static_cast<uint32>(Bytes[At + 2]) << 16) | (static_cast<uint32>(Bytes[At + 3]) << 24); };
	TestEqual(TEXT("magic"), Word(0), 0x46546C67u);
	TestEqual(TEXT("version 2"), Word(4), 2u);
	TestEqual(TEXT("length"), static_cast<int32>(Word(8)), Bytes.Num());
	const int32 JsonLength = static_cast<int32>(Word(12));
	const int32 BinStart = 20 + JsonLength + 8;
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + 20), JsonLength);
	const FString JsonText(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Gltf;
	if (!TestTrue(TEXT("its JSON reads"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Gltf) && Gltf.IsValid()))
	{
		return false;
	}

	// Named in Spanish, as Twinmotion will list them.
	TSet<FString> Names;
	for (const TSharedPtr<FJsonValue>& Node : Gltf->GetArrayField(TEXT("nodes")))
	{
		Names.Add(Node->AsObject()->GetStringField(TEXT("name")));
	}
	const auto Short = [](const auto& Id) { return FString(UTF8_TO_TCHAR(f101::cad::toString(Id).substr(0, 8).c_str())); };
	TestTrue(TEXT("the level"), Names.Contains(TEXT("Planta baja")));
	TestTrue(TEXT("the walls"), Names.Contains(TEXT("Muros")) && Names.Contains(TEXT("Muro ") + Short(Walls[0])));
	TestTrue(TEXT("the door"), Names.Contains(TEXT("Puerta ") + Short(Door)));
	TestTrue(TEXT("the sofa"), Names.Contains(TEXT("Sofá ") + Short(Sofa)));

	// The bricks' material and its PNG, decoded by Unreal: the very pixels the application shows.
	const TArray<TSharedPtr<FJsonValue>>& Materials = Gltf->GetArrayField(TEXT("materials"));
	const TSharedPtr<FJsonValue>* Bricks = Materials.FindByPredicate([](const TSharedPtr<FJsonValue>& Material)
	{
		return Material->AsObject()->GetObjectField(TEXT("extras"))->GetStringField(TEXT("f101FinishId")) == TEXT("brick-red");
	});
	if (!TestNotNull(TEXT("the bricks' material"), Bricks))
	{
		return false;
	}
	TestEqual(TEXT("its name"), (*Bricks)->AsObject()->GetStringField(TEXT("name")), FString(TEXT("Ladrillo visto")));
	const int32 Texture = static_cast<int32>((*Bricks)->AsObject()->GetObjectField(TEXT("pbrMetallicRoughness"))
		->GetObjectField(TEXT("baseColorTexture"))->GetNumberField(TEXT("index")));
	const int32 Image = static_cast<int32>(Gltf->GetArrayField(TEXT("textures"))[Texture]->AsObject()->GetNumberField(TEXT("source")));
	const int32 View = static_cast<int32>(Gltf->GetArrayField(TEXT("images"))[Image]->AsObject()->GetNumberField(TEXT("bufferView")));
	const TSharedPtr<FJsonObject> ViewObject = Gltf->GetArrayField(TEXT("bufferViews"))[View]->AsObject();
	const int32 Offset = BinStart + static_cast<int32>(ViewObject->GetNumberField(TEXT("byteOffset")));
	const int32 Length = static_cast<int32>(ViewObject->GetNumberField(TEXT("byteLength")));
	FImage Decoded;
	if (TestTrue(TEXT("a PNG Unreal decodes"), Offset + Length <= Bytes.Num() && FImageUtils::DecompressImage(Bytes.GetData() + Offset, Length, Decoded)))
	{
		Decoded.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const f101::scene::FinishLook Look = f101::scene::buildFinishLook("brick-red");
		const f101::scene::TextureImage& Expected = Look.texture.mips.front();
		TestEqual(TEXT("512 px"), static_cast<int32>(Decoded.SizeX), Expected.width);
		int32 Different = 0;
		const uint8* Pixels = Decoded.RawData.GetData();
		for (int32 Pixel = 0; Pixel < Expected.width * Expected.height && Decoded.RawData.Num() >= Expected.width * Expected.height * 4; ++Pixel)
		{
			const uint8* Rgba = Expected.rgba.data() + Pixel * 4;
			Different += Pixels[Pixel * 4 + 2] != Rgba[0] || Pixels[Pixel * 4 + 1] != Rgba[1] || Pixels[Pixel * 4] != Rgba[2] ? 1 : 0;
		}
		TestEqual(TEXT("the same pixels"), Different, 0);
	}

	// Only .glb.
	AddExpectedMessagePlain(TEXT("F101: Export the model as .glb"), ELogVerbosity::Warning);
	TestFalse(TEXT("another format refused"), Document->ExportGltf(FPaths::Combine(Folder, TEXT("casa.fbx"))).ok());
	IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Folder), false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
