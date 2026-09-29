#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Floor101DocumentToolset.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"

BEGIN_DEFINE_SPEC(FFloor101DocumentToolsetSpec, "Floor101.Editor.DocumentToolset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	static FVector ActorSizeCm(const FFloor101WallInfo& Wall)
	{
		FVector Origin;
		FVector Extent;
		Wall.Actor->GetActorBounds(false, Origin, Extent);
		return Extent * 2.0;
	}

	static bool HasWall(const FString& Id)
	{
		return UFloor101DocumentToolset::GetWalls().ContainsByPredicate([&Id](const FFloor101WallInfo& Wall) { return Wall.Id == Id; });
	}

	// Runs a tool call the way the ToolsetRegistry does (inside a script frame, where RaiseScriptError reports) and
	// checks that it raised an error containing Fragment.
	void ExpectRaises(const TCHAR* Fragment, TFunction<void()>&& Call)
	{
		AddExpectedErrorPlain(Fragment);
		UE::ToolsetRegistry::FToolCallExceptionHandler ExceptionHandler;
		ExceptionHandler.CaptureErrorsIn(MoveTemp(Call));
		TestTrue(FString::Printf(TEXT("raised '%s' (got '%s')"), Fragment, *ExceptionHandler.GetException()),
			ExceptionHandler.GetException().Contains(Fragment));
	}

END_DEFINE_SPEC(FFloor101DocumentToolsetSpec)

void FFloor101DocumentToolsetSpec::Define()
{
	Describe("wall lifecycle", [this]()
	{
		It("adds the reference wall, regenerates it from 3550 to 4200 mm in the same actor, and removes it", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 3550, 0, 120, 2400);
			TestEqual(TEXT("length"), Added.LengthMm, 3550.0);
			TestEqual(TEXT("thickness"), Added.ThicknessMm, int64(120));
			TestEqual(TEXT("height"), Added.HeightMm, int64(2400));
			TestTrue(TEXT("listed"), HasWall(Added.Id));
			if (!TestNotNull(TEXT("actor"), Added.Actor.Get()))
			{
				return;
			}
			TestEqual(TEXT("actor size 355 x 12 x 240 cm"), ActorSizeCm(Added), FVector(355.0, 12.0, 240.0), 1e-3f);

			const FFloor101WallInfo Moved = UFloor101DocumentToolset::SetWallEnd(Added.Id, 4200, 0);
			TestEqual(TEXT("same id"), Moved.Id, Added.Id);
			TestEqual(TEXT("new length"), Moved.LengthMm, 4200.0);
			TestTrue(TEXT("same actor"), Moved.Actor == Added.Actor);
			TestEqual(TEXT("actor size 420 x 12 x 240 cm"), ActorSizeCm(Moved), FVector(420.0, 12.0, 240.0), 1e-3f);

			UFloor101DocumentToolset::RemoveWall(Added.Id);
			TestFalse(TEXT("removed"), HasWall(Added.Id));
		});
	});

	Describe("dimensions", [this]()
	{
		It("changes length, thickness and height, each regenerating the same actor", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 3550, 0, 120, 2400);
			const FFloor101WallInfo Longer = UFloor101DocumentToolset::SetWallLength(Added.Id, 4200);
			TestEqual(TEXT("end moved along the axis"), Longer.EndXMm, int64(4200));
			const FFloor101WallInfo Thicker = UFloor101DocumentToolset::SetWallThickness(Added.Id, 150);
			TestEqual(TEXT("thickness"), Thicker.ThicknessMm, int64(150));
			const FFloor101WallInfo Taller = UFloor101DocumentToolset::SetWallHeight(Added.Id, 2700);
			TestEqual(TEXT("height"), Taller.HeightMm, int64(2700));
			if (TestNotNull(TEXT("actor"), Taller.Actor.Get()))
			{
				TestTrue(TEXT("same actor"), Taller.Actor == Added.Actor);
				TestEqual(TEXT("actor size 420 x 15 x 270 cm"), ActorSizeCm(Taller), FVector(420.0, 15.0, 270.0), 1e-3f);
			}
			UFloor101DocumentToolset::RemoveWall(Added.Id);
		});
	});

	Describe("corners and rooms", [this]()
	{
		It("closes a room, reports its net area and floor, and moves a shared corner as one step", [this]()
		{
			TArray<FString> Ids;
			const int64 Corners[][2] = {{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}};
			for (int32 i = 0; i < 4; ++i)
			{
				const int64* A = Corners[i];
				const int64* B = Corners[(i + 1) % 4];
				Ids.Add(UFloor101DocumentToolset::AddWall(TEXT(""), A[0] + 20000, A[1], B[0] + 20000, B[1], 120, 2400).Id);
			}
			const TArray<FFloor101RoomInfo> Rooms = UFloor101DocumentToolset::GetRooms();
			const FFloor101RoomInfo* Room = Rooms.FindByPredicate([&Ids](const FFloor101RoomInfo& R) { return R.WallIds.Contains(Ids[0]); });
			if (TestNotNull(TEXT("room found"), Room))
			{
				TestEqual(TEXT("net area 3.88 x 2.88 m"), Room->AreaM2, 3.88 * 2.88, 1e-9);
				TestEqual(TEXT("four walls"), Room->WallIds.Num(), 4);
				TestNotNull(TEXT("floor actor"), Room->FloorActor.Get());
			}

			const TArray<FFloor101WallInfo> Moved = UFloor101DocumentToolset::MoveCorner(TEXT(""), 24000, 0, 24500, 0);
			TestEqual(TEXT("both walls at the corner moved"), Moved.Num(), 2);
			for (const FFloor101WallInfo& Wall : Moved)
			{
				TestTrue(TEXT("an end at the new corner"), (Wall.EndXMm == 24500 && Wall.EndYMm == 0) || (Wall.StartXMm == 24500 && Wall.StartYMm == 0));
			}
			TestEqual(TEXT("one undo step"), UFloor101DocumentToolset::Undo(), FString(TEXT("Move corner")));
			for (const FString& Id : Ids)
			{
				UFloor101DocumentToolset::RemoveWall(Id);
			}
		});
	});

	Describe("levels", [this]()
	{
		It("stacks a level, lifts its walls, solves its rooms apart, and removes it with its walls", [this]()
		{
			const TArray<FFloor101LevelInfo> Before = UFloor101DocumentToolset::GetLevels();
			if (!TestEqual(TEXT("one level to start with"), Before.Num(), 1))
			{
				return;
			}
			const FFloor101LevelInfo Ground = Before[0];
			TestEqual(TEXT("ground at 0"), Ground.ElevationMm, int64(0));

			const FFloor101LevelInfo Upper = UFloor101DocumentToolset::AddLevel(TEXT("Primera"), 3000, 300);
			TestEqual(TEXT("on top"), Upper.Index, 1);
			TestEqual(TEXT("elevation = ground height"), Upper.ElevationMm, Ground.HeightMm);
			TestEqual(TEXT("name"), Upper.Name, FString(TEXT("Primera")));

			// The same 4 x 3 m room on both levels: two rooms, one per level.
			TArray<FString> Ids;
			const int64 Corners[][2] = {{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}};
			for (const FString& Level : {Ground.Id, Upper.Id})
			{
				for (int32 i = 0; i < 4; ++i)
				{
					const int64* A = Corners[i];
					const int64* B = Corners[(i + 1) % 4];
					Ids.Add(UFloor101DocumentToolset::AddWall(Level, A[0] + 30000, A[1], B[0] + 30000, B[1], 120, 2800).Id);
				}
			}
			const TArray<FFloor101RoomInfo> Rooms = UFloor101DocumentToolset::GetRooms().FilterByPredicate(
				[&Ids](const FFloor101RoomInfo& Room) { return Room.WallIds.Contains(Ids[0]) || Room.WallIds.Contains(Ids[4]); });
			if (TestEqual(TEXT("a room per level"), Rooms.Num(), 2))
			{
				TestNotEqual(TEXT("on different levels"), Rooms[0].LevelId, Rooms[1].LevelId);
			}

			// Upper walls only have actors when the view shows their level.
			const TArray<FFloor101WallInfo> Hidden = UFloor101DocumentToolset::GetWalls();
			const FFloor101WallInfo* UpperWall = Hidden.FindByPredicate([&Ids](const FFloor101WallInfo& Wall) { return Wall.Id == Ids[4]; });
			if (TestNotNull(TEXT("upper wall"), UpperWall))
			{
				TestEqual(TEXT("on the upper level"), UpperWall->LevelId, Upper.Id);
				TestNull(TEXT("hidden above the edited level"), UpperWall->Actor.Get());
			}
			UFloor101DocumentToolset::SetLevelView(Upper.Id, false);
			const TArray<FFloor101WallInfo> Walls = UFloor101DocumentToolset::GetWalls();
			const FFloor101WallInfo* Shown = Walls.FindByPredicate([&Ids](const FFloor101WallInfo& Wall) { return Wall.Id == Ids[4]; });
			if (TestNotNull(TEXT("upper wall"), Shown) && TestNotNull(TEXT("actor once its level is shown"), Shown->Actor.Get()))
			{
				FVector Origin;
				FVector Extent;
				Shown->Actor->GetActorBounds(false, Origin, Extent);
				TestEqual(TEXT("stands on the upper level (cm)"), Origin.Z - Extent.Z, static_cast<double>(Ground.HeightMm) / 10.0, 0.01);
			}

			// A taller ground floor lifts the level above; removing the upper level removes its walls, undo restores them.
			const FFloor101LevelInfo Taller = UFloor101DocumentToolset::SetLevel(Ground.Id, TEXT(""), 3200, 250);
			TestEqual(TEXT("taller"), Taller.HeightMm, int64(3200));
			TestEqual(TEXT("upper lifted"), UFloor101DocumentToolset::GetLevels()[1].ElevationMm, int64(3200));
			UFloor101DocumentToolset::RemoveLevel(Upper.Id);
			TestEqual(TEXT("one level left"), UFloor101DocumentToolset::GetLevels().Num(), 1);
			TestFalse(TEXT("its walls went with it"), HasWall(Ids[4]));
			TestEqual(TEXT("one undo step"), UFloor101DocumentToolset::Undo(), FString(TEXT("Delete level")));
			TestTrue(TEXT("walls back"), HasWall(Ids[4]));

			UFloor101DocumentToolset::SetLevelView(TEXT(""), false);
			UFloor101DocumentToolset::RemoveLevel(Upper.Id);
			UFloor101DocumentToolset::SetLevel(Ground.Id, Ground.Name, Ground.HeightMm, Ground.SlabThicknessMm);
			for (int32 i = 0; i < 4; ++i)
			{
				UFloor101DocumentToolset::RemoveWall(Ids[i]);
			}
		});
	});

	Describe("finishes", [this]()
	{
		It("lists the catalogue, paints a wall face and a room floor, and undoes it", [this]()
		{
			const TArray<FFloor101FinishInfo> Finishes = UFloor101DocumentToolset::GetFinishes();
			const FFloor101FinishInfo* Brick = Finishes.FindByPredicate([](const FFloor101FinishInfo& F) { return F.Id == TEXT("brick-red"); });
			if (TestNotNull(TEXT("brick in the catalogue"), Brick))
			{
				TestTrue(TEXT("for walls"), Brick->bForWalls);
				TestFalse(TEXT("not for floors"), Brick->bForFloors);
				TestFalse(TEXT("not for carpentry"), Brick->bForCarpentry);
			}
			const FFloor101FinishInfo* Walnut = Finishes.FindByPredicate([](const FFloor101FinishInfo& F) { return F.Id == TEXT("wood-walnut"); });
			TestTrue(TEXT("walnut floors and carpentry"), Walnut && Walnut->bForFloors && Walnut->bForCarpentry && Walnut->Opacity == 1.0f);
			const FFloor101FinishInfo* Glass = Finishes.FindByPredicate([](const FFloor101FinishInfo& F) { return F.Id == TEXT("glass-clear"); });
			TestTrue(TEXT("glass is see-through and for no wall, floor or carpentry"),
				Glass && Glass->Opacity < 1.0f && !Glass->bForWalls && !Glass->bForFloors && !Glass->bForCarpentry);

			TArray<FString> Ids;
			const int64 Corners[][2] = {{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}};
			for (int32 i = 0; i < 4; ++i)
			{
				const int64* A = Corners[i];
				const int64* B = Corners[(i + 1) % 4];
				Ids.Add(UFloor101DocumentToolset::AddWall(TEXT(""), A[0] + 40000, A[1], B[0] + 40000, B[1], 120, 2800).Id);
			}
			const FFloor101WallInfo Painted = UFloor101DocumentToolset::SetWallFinish(Ids[0], EFloor101WallSide::Left, TEXT("brick-red"));
			TestEqual(TEXT("left face"), Painted.LeftFinish, FString(TEXT("brick-red")));
			TestTrue(TEXT("right face default"), Painted.RightFinish.IsEmpty());

			const TArray<FFloor101RoomInfo> Rooms = UFloor101DocumentToolset::GetRooms();
			const FFloor101RoomInfo* Room = Rooms.FindByPredicate([&Ids](const FFloor101RoomInfo& R) { return R.WallIds.Contains(Ids[0]); });
			if (TestNotNull(TEXT("room"), Room))
			{
				UFloor101DocumentToolset::SetRoomFinish(Room->Key, TEXT("wood-oak"), TEXT(""));
				const TArray<FFloor101RoomInfo> After = UFloor101DocumentToolset::GetRooms();
				const FFloor101RoomInfo* Same = After.FindByPredicate([Room](const FFloor101RoomInfo& R) { return R.Key == Room->Key; });
				if (TestNotNull(TEXT("same room"), Same))
				{
					TestEqual(TEXT("oak floor"), Same->FloorFinish, FString(TEXT("wood-oak")));
					TestTrue(TEXT("default ceiling"), Same->CeilingFinish.IsEmpty());
				}
				TestEqual(TEXT("undo"), UFloor101DocumentToolset::Undo(), FString(TEXT("Change room finish")));
			}
			TestEqual(TEXT("undo the wall"), UFloor101DocumentToolset::Undo(), FString(TEXT("Change finish")));
			for (const FString& Id : Ids)
			{
				UFloor101DocumentToolset::RemoveWall(Id);
			}
		});
	});

	Describe("openings", [this]()
	{
		It("adds a door and a window, moves the window, and removes them", [this]()
		{
			const FFloor101WallInfo Wall = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 4000, 0, 120, 2400);
			const FFloor101OpeningInfo Door = UFloor101DocumentToolset::AddOpening(Wall.Id, EFloor101OpeningKind::Door, 300, 900, 2100, 0);
			TestFalse(TEXT("door id"), Door.Id.IsEmpty());
			const FFloor101OpeningInfo Window = UFloor101DocumentToolset::AddOpening(Wall.Id, EFloor101OpeningKind::Window, 2000, 1200, 1200, 900);
			TestTrue(TEXT("window kind"), Window.Kind == EFloor101OpeningKind::Window);
			const FFloor101OpeningInfo Moved = UFloor101DocumentToolset::SetOpening(Wall.Id, Window.Id, 2500, 1200, 1000, 1000);
			TestEqual(TEXT("moved"), Moved.OffsetMm, int64(2500));
			TestEqual(TEXT("sill"), Moved.SillMm, int64(1000));
			const TArray<FFloor101WallInfo> Walls = UFloor101DocumentToolset::GetWalls();
			const FFloor101WallInfo* Host = Walls.FindByPredicate([&Wall](const FFloor101WallInfo& W) { return W.Id == Wall.Id; });
			if (TestNotNull(TEXT("host listed"), Host))
			{
				TestEqual(TEXT("two openings, by offset"), Host->Openings.Num(), 2);
				TestEqual(TEXT("door first"), Host->Openings[0].Id, Door.Id);
			}
			UFloor101DocumentToolset::RemoveOpening(Wall.Id, Door.Id);
			UFloor101DocumentToolset::RemoveOpening(Wall.Id, Window.Id);
			TestEqual(TEXT("undo label"), UFloor101DocumentToolset::Undo(), FString(TEXT("Delete opening")));
			UFloor101DocumentToolset::RemoveWall(Wall.Id);
		});
	});

	Describe("history and files", [this]()
	{
		It("undoes and redoes a change, returning what it was", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 3550, 0, 120, 2400);
			TestEqual(TEXT("undo label"), UFloor101DocumentToolset::Undo(), FString(TEXT("Create wall")));
			TestFalse(TEXT("undone"), HasWall(Added.Id));
			TestEqual(TEXT("redo label"), UFloor101DocumentToolset::Redo(), FString(TEXT("Create wall")));
			TestTrue(TEXT("redone with the same id"), HasWall(Added.Id));
			UFloor101DocumentToolset::RemoveWall(Added.Id);
			ExpectRaises(TEXT("nothing to redo"), [] { UFloor101DocumentToolset::Redo(); });
		});

		It("saves and opens project.json; opening clears the history", [this]()
		{
			const FString Path = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Toolset"), TEXT("project.json")));
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 3550, 0, 120, 2400);
			UFloor101DocumentToolset::SaveProject(Path);
			TestTrue(TEXT("file written"), IFileManager::Get().FileExists(*Path));
			UFloor101DocumentToolset::RemoveWall(Added.Id);
			const TArray<FFloor101WallInfo> Opened = UFloor101DocumentToolset::OpenProject(Path);
			TestTrue(TEXT("the saved wall is back"), Opened.ContainsByPredicate([&Added](const FFloor101WallInfo& Wall) { return Wall.Id == Added.Id; }));
			ExpectRaises(TEXT("nothing to undo"), [] { UFloor101DocumentToolset::Undo(); });
			UFloor101DocumentToolset::RemoveWall(Added.Id);
			IFileManager::Get().Delete(*Path, false, true, true);
		});
	});

	Describe("errors", [this]()
	{
		It("AddWall raises on a degenerate wall", [this]()
		{
			ExpectRaises(TEXT("wall rejected"), [] { UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 0, 0, 120, 2400); });
		});

		It("SetWallEnd raises on a malformed id", [this]()
		{
			ExpectRaises(TEXT("is not a wall id"), [] { UFloor101DocumentToolset::SetWallEnd(TEXT("not-a-uuid"), 10, 0); });
		});

		It("SetWallEnd raises on an unknown id", [this]()
		{
			ExpectRaises(TEXT("cannot move the end"),
				[] { UFloor101DocumentToolset::SetWallEnd(TEXT("00000000-0000-0000-0000-0000000000ff"), 10, 0); });
		});

		It("SetWallEnd raises when the end would coincide with the start", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 1000, 0, 120, 2400);
			ExpectRaises(TEXT("cannot move the end"), [&Added] { UFloor101DocumentToolset::SetWallEnd(Added.Id, 0, 0); });
			UFloor101DocumentToolset::RemoveWall(Added.Id);
		});

		It("SetWallLength, SetWallThickness and SetWallHeight raise on non-positive values", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 1000, 0, 120, 2400);
			ExpectRaises(TEXT("cannot change the length"), [&Added] { UFloor101DocumentToolset::SetWallLength(Added.Id, 0); });
			ExpectRaises(TEXT("cannot change the thickness"), [&Added] { UFloor101DocumentToolset::SetWallThickness(Added.Id, 0); });
			ExpectRaises(TEXT("cannot change the height"), [&Added] { UFloor101DocumentToolset::SetWallHeight(Added.Id, -1); });
			UFloor101DocumentToolset::RemoveWall(Added.Id);
		});

		It("SaveProject raises when the folder cannot be created", [this]()
		{
			const FString Path = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("bad<>|folder"), TEXT("project.json")));
			AddExpectedMessagePlain(TEXT("F101: Cannot create the folder"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("cannot save"), [&Path] { UFloor101DocumentToolset::SaveProject(Path); });
		});

		It("OpenProject raises on a missing file", [this]()
		{
			const FString Path = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Toolset"), TEXT("missing.json")));
			AddExpectedMessagePlain(TEXT("F101: Cannot open"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("cannot open"), [&Path] { UFloor101DocumentToolset::OpenProject(Path); });
		});

		It("AddOpening, SetOpening and RemoveOpening raise when the opening does not fit or does not exist", [this]()
		{
			const FFloor101WallInfo Wall = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 2000, 0, 120, 2400);
			ExpectRaises(TEXT("cannot add the opening"),
				[&Wall] { UFloor101DocumentToolset::AddOpening(Wall.Id, EFloor101OpeningKind::Door, 1500, 900, 2100, 0); });
			ExpectRaises(TEXT("cannot add the opening"),
				[&Wall] { UFloor101DocumentToolset::AddOpening(Wall.Id, EFloor101OpeningKind::Door, 0, 900, 2100, 300); });
			const FFloor101OpeningInfo Door = UFloor101DocumentToolset::AddOpening(Wall.Id, EFloor101OpeningKind::Door, 0, 900, 2100, 0);
			ExpectRaises(TEXT("cannot change opening"),
				[&Wall, &Door] { UFloor101DocumentToolset::SetOpening(Wall.Id, Door.Id, 0, 900, 2400, 0); });
			ExpectRaises(TEXT("cannot remove opening"),
				[&Wall] { UFloor101DocumentToolset::RemoveOpening(Wall.Id, TEXT("00000000-0000-0000-0000-0000000000ff")); });
			ExpectRaises(TEXT("is not an opening id"), [&Wall] { UFloor101DocumentToolset::RemoveOpening(Wall.Id, TEXT("x")); });
			UFloor101DocumentToolset::RemoveWall(Wall.Id);
		});

		It("MoveCorner raises when no wall ends at the point", [this]()
		{
			ExpectRaises(TEXT("cannot move the corner"), [] { UFloor101DocumentToolset::MoveCorner(TEXT(""), -777, -777, 0, 0); });
		});

		It("the level tools raise on invalid levels, unknown ids and the last level", [this]()
		{
			const FString Ground = UFloor101DocumentToolset::GetLevels()[0].Id;
			ExpectRaises(TEXT("level rejected"), [] { UFloor101DocumentToolset::AddLevel(TEXT(""), 2800, 2800); });
			ExpectRaises(TEXT("cannot change level"),
				[] { UFloor101DocumentToolset::SetLevel(TEXT("00000000-0000-0000-0000-0000000000ff"), TEXT(""), 2800, 250); });
			ExpectRaises(TEXT("cannot remove level"), [&Ground] { UFloor101DocumentToolset::RemoveLevel(Ground); });
			ExpectRaises(TEXT("is not a level id"), [] { UFloor101DocumentToolset::AddWall(TEXT("x"), 0, 0, 1000, 0, 120, 2400); });
			ExpectRaises(TEXT("wall rejected"),
				[] { UFloor101DocumentToolset::AddWall(TEXT("00000000-0000-0000-0000-0000000000ff"), 0, 0, 1000, 0, 120, 2400); });
		});

		It("SetWallFinish and SetRoomFinish raise on finishes that are unknown or not for that surface", [this]()
		{
			const FFloor101WallInfo Wall = UFloor101DocumentToolset::AddWall(TEXT(""), 0, 0, 1000, 0, 120, 2400);
			ExpectRaises(TEXT("cannot change the finish of"),
				[&Wall] { UFloor101DocumentToolset::SetWallFinish(Wall.Id, EFloor101WallSide::Right, TEXT("wood-oak")); });
			ExpectRaises(TEXT("cannot change the finish of"),
				[&Wall] { UFloor101DocumentToolset::SetWallFinish(Wall.Id, EFloor101WallSide::Left, TEXT("gold-leaf")); });
			ExpectRaises(TEXT("cannot change the finishes of room"),
				[] { UFloor101DocumentToolset::SetRoomFinish(TEXT("some-room"), TEXT("brick-red"), TEXT("")); });
			UFloor101DocumentToolset::RemoveWall(Wall.Id);
		});

		It("SetOpeningFinish changes the carpentry; AddOpening raises where another wall joins", [this]()
		{
			const FFloor101WallInfo Host = UFloor101DocumentToolset::AddWall(TEXT(""), 50000, 0, 56000, 0, 120, 2800);
			const FFloor101WallInfo Tee = UFloor101DocumentToolset::AddWall(TEXT(""), 52000, 0, 52000, 3000, 100, 2800);
			ExpectRaises(TEXT("runs into a wall joined to this one"),
				[&Host] { UFloor101DocumentToolset::AddOpening(Host.Id, EFloor101OpeningKind::Door, 1500, 900, 2100, 0); });
			const FFloor101OpeningInfo Door = UFloor101DocumentToolset::AddOpening(Host.Id, EFloor101OpeningKind::Door, 500, 900, 2100, 0);
			const FFloor101OpeningInfo Oak = UFloor101DocumentToolset::SetOpeningFinish(Host.Id, Door.Id, TEXT("wood-oak"));
			TestEqual(TEXT("oak door"), Oak.Finish, FString(TEXT("wood-oak")));
			ExpectRaises(TEXT("cannot change opening"),
				[&Host, &Door] { UFloor101DocumentToolset::SetOpeningFinish(Host.Id, Door.Id, TEXT("brick-red")); });
			UFloor101DocumentToolset::RemoveWall(Tee.Id);
			UFloor101DocumentToolset::RemoveWall(Host.Id);
		});

		It("RemoveWall raises on an unknown id", [this]()
		{
			ExpectRaises(TEXT("cannot remove wall"),
				[] { UFloor101DocumentToolset::RemoveWall(TEXT("00000000-0000-0000-0000-0000000000ff")); });
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
