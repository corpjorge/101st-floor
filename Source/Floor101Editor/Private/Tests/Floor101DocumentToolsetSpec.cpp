#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Floor101DocumentToolset.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
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

	Describe("furniture", [this]()
	{
		It("lists the catalogue, adds a sofa with its type's size, moves, turns and finishes it in the same actor, and removes it", [this]()
		{
			const TArray<FFloor101FurnitureTypeInfo> Types = UFloor101DocumentToolset::GetFurnitureTypes();
			TestEqual(TEXT("twelve types"), Types.Num(), 12);
			const FFloor101FurnitureTypeInfo* SofaType = Types.FindByPredicate([](const FFloor101FurnitureTypeInfo& T) { return T.Id == TEXT("sofa"); });
			if (TestNotNull(TEXT("sofa in the catalogue"), SofaType))
			{
				TestTrue(TEXT("against a wall"), SofaType->bAgainstWall);
				TestEqual(TEXT("default width"), SofaType->WidthMm, int64(2000));
				TestTrue(TEXT("width range"), SofaType->MinWidthMm < SofaType->WidthMm && SofaType->WidthMm < SofaType->MaxWidthMm);
				TestEqual(TEXT("body finish"), SofaType->BodyFinish, FString(TEXT("fabric-grey")));
			}
			const TArray<FFloor101FinishInfo> Finishes = UFloor101DocumentToolset::GetFinishes();
			const FFloor101FinishInfo* Fabric = Finishes.FindByPredicate([](const FFloor101FinishInfo& F) { return F.Id == TEXT("fabric-sand"); });
			TestTrue(TEXT("fabric only for furniture"), Fabric && Fabric->bForFurniture && !Fabric->bForWalls && !Fabric->bForCarpentry);

			const FFloor101FurnitureInfo Sofa = UFloor101DocumentToolset::AddFurniture(TEXT(""), TEXT("sofa"), 120000, 0, 18000, 0, 0, 0);
			TestFalse(TEXT("id"), Sofa.Id.IsEmpty());
			TestEqual(TEXT("type"), Sofa.Type, FString(TEXT("sofa")));
			TestEqual(TEXT("default depth"), Sofa.DepthMm, int64(900));
			TestEqual(TEXT("rotation"), Sofa.RotationCentidegrees, 18000);
			TestEqual(TEXT("on the lowest level"), Sofa.LevelId, UFloor101DocumentToolset::GetLevels()[0].Id);
			TestTrue(TEXT("listed"), UFloor101DocumentToolset::GetFurniture().ContainsByPredicate(
				[&Sofa](const FFloor101FurnitureInfo& Piece) { return Piece.Id == Sofa.Id; }));
			if (!TestNotNull(TEXT("actor"), Sofa.Actor.Get()))
			{
				return;
			}
			FVector Origin;
			FVector Extent;
			Sofa.Actor->GetActorBounds(false, Origin, Extent);
			TestEqual(TEXT("actor 200 x 90 x 85 cm"), Extent * 2.0, FVector(200.0, 90.0, 85.0), 1e-3f);

			const FFloor101FurnitureInfo Turned = UFloor101DocumentToolset::SetFurniture(Sofa.Id, 121000, 500, 9000, 2400, 950, 800);
			TestTrue(TEXT("same actor"), Turned.Actor == Sofa.Actor);
			TestEqual(TEXT("moved"), Turned.XMm, int64(121000));
			TestEqual(TEXT("wider"), Turned.WidthMm, int64(2400));
			Turned.Actor->GetActorBounds(false, Origin, Extent);
			TestEqual(TEXT("turned: 95 x 240 x 80 cm"), Extent * 2.0, FVector(95.0, 240.0, 80.0), 1e-3f);

			const FFloor101FurnitureInfo Finished = UFloor101DocumentToolset::SetFurnitureFinish(Sofa.Id, TEXT("fabric-sand"));
			TestEqual(TEXT("finish"), Finished.Finish, FString(TEXT("fabric-sand")));
			TestEqual(TEXT("undo"), UFloor101DocumentToolset::Undo(), FString(TEXT("Change furniture")));

			UFloor101DocumentToolset::RemoveFurniture(Sofa.Id);
			TestFalse(TEXT("removed"), UFloor101DocumentToolset::GetFurniture().ContainsByPredicate(
				[&Sofa](const FFloor101FurnitureInfo& Piece) { return Piece.Id == Sofa.Id; }));
			TestEqual(TEXT("undo the removal"), UFloor101DocumentToolset::Undo(), FString(TEXT("Delete furniture")));
			TestTrue(TEXT("back"), UFloor101DocumentToolset::GetFurniture().ContainsByPredicate(
				[&Sofa](const FFloor101FurnitureInfo& Piece) { return Piece.Id == Sofa.Id; }));
			UFloor101DocumentToolset::RemoveFurniture(Sofa.Id);
		});

		It("raises on unknown types, sizes out of range, bad rotations, bad finishes and unknown pieces", [this]()
		{
			ExpectRaises(TEXT("Unknown furniture type 'spaceship'"),
				[] { UFloor101DocumentToolset::AddFurniture(TEXT(""), TEXT("spaceship"), 0, 0, 0, 0, 0, 0); });
			ExpectRaises(TEXT("Its width goes from 700 to 3000 mm"),
				[] { UFloor101DocumentToolset::AddFurniture(TEXT(""), TEXT("table"), 0, 0, 0, 5000, 0, 0); });
			ExpectRaises(TEXT("No level with id"),
				[] { UFloor101DocumentToolset::AddFurniture(TEXT("00000000-0000-0000-0000-0000000000ff"), TEXT("table"), 0, 0, 0, 0, 0, 0); });
			const FFloor101FurnitureInfo Table = UFloor101DocumentToolset::AddFurniture(TEXT(""), TEXT("table"), 125000, 0, 0, 0, 0, 0);
			ExpectRaises(TEXT("A rotation goes from 0 to 359.99 degrees"),
				[&Table] { UFloor101DocumentToolset::SetFurniture(Table.Id, 0, 0, 36000, 1600, 900, 750); });
			ExpectRaises(TEXT("is not for furniture"), [&Table] { UFloor101DocumentToolset::SetFurnitureFinish(Table.Id, TEXT("brick-red")); });
			ExpectRaises(TEXT("is not a furniture id"), [] { UFloor101DocumentToolset::SetFurnitureFinish(TEXT("table"), TEXT("")); });
			ExpectRaises(TEXT("no such piece"),
				[] { UFloor101DocumentToolset::SetFurniture(TEXT("00000000-0000-0000-0000-0000000000ff"), 0, 0, 0, 1600, 900, 750); });
			ExpectRaises(TEXT("cannot remove furniture"),
				[] { UFloor101DocumentToolset::RemoveFurniture(TEXT("00000000-0000-0000-0000-0000000000ff")); });
			UFloor101DocumentToolset::RemoveFurniture(Table.Id);
		});
	});

	Describe("assets", [this]()
	{
		It("imports a texture and a model into the project's folder, uses them, changes them and removes them", [this]()
		{
			const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101ToolsetAssets")));
			IFileManager::Get().DeleteDirectory(*Folder, false, true);
			UFloor101DocumentToolset::SaveProject(FPaths::Combine(Folder, TEXT("project.json"))); // the assets go next to it
			const FString Data = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Data/Assets")));

			const FFloor101AssetInfo Texture = UFloor101DocumentToolset::ImportAsset(FPaths::Combine(Data, TEXT("parquet.png")));
			TestTrue(TEXT("a texture"), Texture.Kind == EFloor101AssetKind::Texture);
			TestEqual(TEXT("its pixels"), Texture.PixelWidth, 256);
			TestEqual(TEXT("1 m wide"), Texture.TextureWidthMm, int64(1000));
			TestTrue(TEXT("copied into the project"), FPaths::FileExists(FPaths::Combine(Folder, Texture.File)));
			TestEqual(TEXT("its reference"), Texture.Ref, TEXT("asset:") + Texture.Id);
			const FFloor101AssetInfo Model = UFloor101DocumentToolset::ImportAsset(FPaths::Combine(Data, TEXT("armchair.glb")));
			TestTrue(TEXT("a model"), Model.Kind == EFloor101AssetKind::Model);
			TestEqual(TEXT("80 cm wide"), Model.WidthMm, int64(800));
			TestEqual(TEXT("90 cm tall"), Model.HeightMm, int64(900));
			TestTrue(TEXT("readable"), Model.Problem.IsEmpty());
			TestEqual(TEXT("both listed"), UFloor101DocumentToolset::GetAssets().Num(), 2);

			const FFloor101WallInfo Wall = UFloor101DocumentToolset::AddWall(TEXT(""), 130000, 0, 134000, 0, 120, 2800);
			const FFloor101WallInfo Painted = UFloor101DocumentToolset::SetWallFinish(Wall.Id, EFloor101WallSide::Left, Texture.Ref);
			TestEqual(TEXT("the wall shows the texture"), Painted.LeftFinish, Texture.Ref);
			const FFloor101FurnitureInfo Chair = UFloor101DocumentToolset::AddFurniture(TEXT(""), Model.Ref, 132000, 1000, 0, 0, 0, 0);
			TestEqual(TEXT("placed at the model's size"), Chair.WidthMm, int64(800));
			if (TestNotNull(TEXT("its actor"), Chair.Actor.Get()))
			{
				FVector Origin;
				FVector Extent;
				Chair.Actor->GetActorBounds(false, Origin, Extent);
				TestEqual(TEXT("80 x 75 x 90 cm"), Extent * 2.0, FVector(80.0, 75.0, 90.0), 0.05f);
			}

			const FFloor101AssetInfo Renamed = UFloor101DocumentToolset::SetTextureAsset(Texture.Id, TEXT("Roble"), 600, 300, 40);
			TestEqual(TEXT("renamed"), Renamed.Name, FString(TEXT("Roble")));
			TestEqual(TEXT("60 cm a repeat"), Renamed.TextureWidthMm, int64(600));
			const FFloor101AssetInfo Centimetres = UFloor101DocumentToolset::SetModelAsset(Model.Id, TEXT("Sillón"), EFloor101ModelUnit::Centimeter,
				false, 0, true);
			TestEqual(TEXT("read in centimetres: a hundredth"), Centimetres.WidthMm, int64(8));
			TestTrue(TEXT("against walls"), Centimetres.bAgainstWall);
			TestEqual(TEXT("undo"), UFloor101DocumentToolset::Undo(), FString(TEXT("Change asset")));

			AddExpectedMessagePlain(TEXT("F101: 'Roble' is in use"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("is in use (1 surface or piece of furniture)"), [&Texture] { UFloor101DocumentToolset::RemoveAsset(Texture.Id); });
			UFloor101DocumentToolset::RemoveFurniture(Chair.Id);
			UFloor101DocumentToolset::RemoveWall(Wall.Id);
			UFloor101DocumentToolset::RemoveAsset(Texture.Id);
			UFloor101DocumentToolset::RemoveAsset(Model.Id);
			TestEqual(TEXT("none left"), UFloor101DocumentToolset::GetAssets().Num(), 0);
		});

		It("raises on files it cannot import and on unknown assets", [this]()
		{
			const FString Data = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Data/Assets")));
			AddExpectedMessagePlain(TEXT("F101: Import PNG or JPEG textures"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("Import PNG or JPEG textures, or glTF (.gltf, .glb) or OBJ models"),
				[&Data] { UFloor101DocumentToolset::ImportAsset(FPaths::Combine(Data, TEXT("make_samples.py"))); });
			AddExpectedMessagePlain(TEXT("F101: Cannot open"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("Cannot open"), [&Data] { UFloor101DocumentToolset::ImportAsset(FPaths::Combine(Data, TEXT("missing.png"))); });
			ExpectRaises(TEXT("is not an asset id"), [] { UFloor101DocumentToolset::SetTextureAsset(TEXT("parquet"), TEXT("x"), 1000, 1000, 50); });
			ExpectRaises(TEXT("no such asset"),
				[] { UFloor101DocumentToolset::SetTextureAsset(TEXT("00000000-0000-0000-0000-0000000000ff"), TEXT("x"), 1000, 1000, 50); });
			AddExpectedMessagePlain(TEXT("F101: No asset with id"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("No asset with id"), [] { UFloor101DocumentToolset::RemoveAsset(TEXT("00000000-0000-0000-0000-0000000000ff")); });
		});
	});

	Describe("library", [this]()
	{
		It("lists the built-in library by category and adds an item to the project once, used like any asset", [this]()
		{
			const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101ToolsetLibrary")));
			IFileManager::Get().DeleteDirectory(*Folder, false, true);
			UFloor101DocumentToolset::SaveProject(FPaths::Combine(Folder, TEXT("project.json"))); // the assets go next to it

			const TArray<FFloor101LibraryItem> Items = UFloor101DocumentToolset::GetLibrary();
			const int32 Models = Items.FilterByPredicate([](const FFloor101LibraryItem& Item) { return Item.Kind == EFloor101AssetKind::Model; }).Num();
			TestTrue(TEXT("50 models or more"), Models >= 50);
			TestTrue(TEXT("14 textures or more"), Items.Num() - Models >= 14);
			const FFloor101LibraryItem* Windsor = Items.FindByPredicate([](const FFloor101LibraryItem& Item) { return Item.Id == TEXT("chair-windsor"); });
			if (!TestNotNull(TEXT("the Windsor chair"), Windsor))
			{
				return;
			}
			TestEqual(TEXT("a chair"), Windsor->Category, FString(TEXT("seating")));
			TestTrue(TEXT("the catalogue's chair too"), UFloor101DocumentToolset::GetFurnitureTypes().ContainsByPredicate(
				[](const FFloor101FurnitureTypeInfo& Type) { return Type.Id == TEXT("chair") && Type.Category == TEXT("seating"); }));
			TestTrue(TEXT("finishes by category"), UFloor101DocumentToolset::GetFinishes().ContainsByPredicate(
				[](const FFloor101FinishInfo& Finish) { return Finish.Id == TEXT("tile-grey") && Finish.Category == TEXT("tiles"); }));

			const FFloor101AssetInfo Chair = UFloor101DocumentToolset::AddFromLibrary(TEXT("chair-windsor"));
			TestTrue(TEXT("a model"), Chair.Kind == EFloor101AssetKind::Model);
			TestEqual(TEXT("from the library"), Chair.Library, FString(TEXT("chair-windsor")));
			TestEqual(TEXT("named as in the library"), Chair.Name, Windsor->Name);
			TestTrue(TEXT("copied into the project"), FPaths::FileExists(FPaths::Combine(Folder, Chair.File)));
			TestTrue(TEXT("readable"), Chair.Problem.IsEmpty());
			TestEqual(TEXT("added once"), UFloor101DocumentToolset::AddFromLibrary(TEXT("chair-windsor")).Id, Chair.Id);
			const FFloor101AssetInfo Oak = UFloor101DocumentToolset::AddFromLibrary(TEXT("photo-laminate-oak"));
			TestTrue(TEXT("a texture"), Oak.Kind == EFloor101AssetKind::Texture);
			TestEqual(TEXT("its real size"), Oak.TextureWidthMm, int64(1700));
			TestEqual(TEXT("both in the project"), UFloor101DocumentToolset::GetAssets().Num(), 2);

			const FFloor101FurnitureInfo Placed = UFloor101DocumentToolset::AddFurniture(TEXT(""), Chair.Ref, 140000, 0, 0, 0, 0, 0);
			TestEqual(TEXT("placed at the model's size"), Placed.WidthMm, Chair.WidthMm);
			TestNotNull(TEXT("its actor"), Placed.Actor.Get());
			UFloor101DocumentToolset::RemoveFurniture(Placed.Id);
			UFloor101DocumentToolset::RemoveAsset(Oak.Id);
			UFloor101DocumentToolset::RemoveAsset(Chair.Id);
			TestEqual(TEXT("none left"), UFloor101DocumentToolset::GetAssets().Num(), 0);
		});

		It("raises on an item the library does not have", [this]()
		{
			AddExpectedMessagePlain(TEXT("F101: The library has no item"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("The library has no item 'sofa-flying'"), [] { UFloor101DocumentToolset::AddFromLibrary(TEXT("sofa-flying")); });
		});
	});

	Describe("export for Twinmotion", [this]()
	{
		It("writes the whole project as a .glb next to it, with its objects, materials and images", [this]()
		{
			const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101ToolsetGltf")));
			IFileManager::Get().DeleteDirectory(*Folder, false, true);
			UFloor101DocumentToolset::SaveProject(FPaths::Combine(Folder, TEXT("casa.json")));
			TArray<FString> Walls;
			const int64 Corners[][2] = {{150000, 0}, {154000, 0}, {154000, 3000}, {150000, 3000}};
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const int64* From = Corners[Corner];
				const int64* To = Corners[(Corner + 1) % 4];
				Walls.Add(UFloor101DocumentToolset::AddWall(TEXT(""), From[0], From[1], To[0], To[1], 120, 2800).Id);
			}
			UFloor101DocumentToolset::SetWallFinish(Walls[0], EFloor101WallSide::Left, TEXT("brick-red"));
			const FFloor101FurnitureInfo Sofa = UFloor101DocumentToolset::AddFurniture(TEXT(""), TEXT("sofa"), 152000, 1500, 0, 0, 0, 0);

			const FFloor101GltfExportInfo Info = UFloor101DocumentToolset::ExportGltf(TEXT(""));
			TestEqual(TEXT("next to the project"), Info.Path, FPaths::Combine(Folder, TEXT("casa.glb")));
			TArray<uint8> Bytes;
			if (TestTrue(TEXT("written"), FFileHelper::LoadFileToArray(Bytes, *Info.Path)))
			{
				TestEqual(TEXT("its size"), static_cast<int64>(Bytes.Num()), Info.Bytes);
				TestTrue(TEXT("a binary glTF"), Bytes.Num() > 12 && FMemory::Memcmp(Bytes.GetData(), "glTF", 4) == 0);
			}
			TestTrue(TEXT("walls, floor, slab and sofa"), Info.Objects >= 7);
			TestTrue(TEXT("the bricks' image"), Info.Images >= 1);
			TestTrue(TEXT("materials"), Info.Materials >= 3);
			TestTrue(TEXT("triangles"), Info.Triangles > 100);
			TestTrue(TEXT("no warnings"), Info.Warnings.IsEmpty());

			UFloor101DocumentToolset::RemoveFurniture(Sofa.Id);
			for (const FString& Id : Walls)
			{
				UFloor101DocumentToolset::RemoveWall(Id);
			}
			IFileManager::Get().DeleteDirectory(*Folder, false, true);
		});

		It("raises on a path that is not .glb", [this]()
		{
			AddExpectedMessagePlain(TEXT("F101: Export the model as .glb"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("Export the model as .glb"), []
			{
				UFloor101DocumentToolset::ExportGltf(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("casa.fbx")));
			});
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

		It("SetOpeningOperation sets how it opens; raises for a fixed door, a pocket over a joint and an unknown opening", [this]()
		{
			const FFloor101WallInfo Host = UFloor101DocumentToolset::AddWall(TEXT(""), 60000, 0, 66000, 0, 120, 2800);
			const FFloor101WallInfo Tee = UFloor101DocumentToolset::AddWall(TEXT(""), 62000, 0, 62000, 3000, 100, 2800);
			const FFloor101OpeningInfo Door = UFloor101DocumentToolset::AddOpening(Host.Id, EFloor101OpeningKind::Door, 2300, 800, 2100, 0);
			TestTrue(TEXT("added closed, hinged at its start"), Door.Operation == EFloor101OpeningOperation::Hinged &&
				Door.Hinge == EFloor101OpeningEnd::Start && Door.OpenPercent == 0);
			const FFloor101OpeningInfo Open = UFloor101DocumentToolset::SetOpeningOperation(Host.Id, Door.Id,
				EFloor101OpeningOperation::DoubleHinged, EFloor101OpeningEnd::Start, EFloor101WallSide::Right, 45);
			TestTrue(TEXT("double, towards the right face, 45 % open"), Open.Operation == EFloor101OpeningOperation::DoubleHinged &&
				Open.OpensTo == EFloor101WallSide::Right && Open.OpenPercent == 45);
			TestEqual(TEXT("one undoable step"), UFloor101DocumentToolset::Undo(), FString(TEXT("Change opening")));
			ExpectRaises(TEXT("A door cannot be fixed"), [&Host, &Door]
			{
				UFloor101DocumentToolset::SetOpeningOperation(Host.Id, Door.Id, EFloor101OpeningOperation::Fixed, EFloor101OpeningEnd::Start,
					EFloor101WallSide::Left, 0);
			});
			ExpectRaises(TEXT("pocket runs into a wall joined to this one"), [&Host, &Door]
			{
				UFloor101DocumentToolset::SetOpeningOperation(Host.Id, Door.Id, EFloor101OpeningOperation::Sliding, EFloor101OpeningEnd::Start,
					EFloor101WallSide::Left, 0);
			});
			const FFloor101OpeningInfo Sliding = UFloor101DocumentToolset::SetOpeningOperation(Host.Id, Door.Id,
				EFloor101OpeningOperation::Sliding, EFloor101OpeningEnd::End, EFloor101WallSide::Left, 100);
			TestTrue(TEXT("sliding into a pocket at its end"), Sliding.Operation == EFloor101OpeningOperation::Sliding && Sliding.Hinge == EFloor101OpeningEnd::End);
			ExpectRaises(TEXT("no such opening"), [&Host]
			{
				UFloor101DocumentToolset::SetOpeningOperation(Host.Id, TEXT("00000000-0000-0000-0000-0000000000ff"),
					EFloor101OpeningOperation::Hinged, EFloor101OpeningEnd::Start, EFloor101WallSide::Left, 0);
			});
			UFloor101DocumentToolset::RemoveWall(Tee.Id);
			UFloor101DocumentToolset::RemoveWall(Host.Id);
		});

		It("ExportPlan writes the plan as SVG; raises on another extension and on an unknown level", [this]()
		{
			TArray<FString> Ids;
			const int64 Corners[][2] = {{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}};
			for (int32 i = 0; i < 4; ++i)
			{
				const int64* A = Corners[i];
				const int64* B = Corners[(i + 1) % 4];
				Ids.Add(UFloor101DocumentToolset::AddWall(TEXT(""), A[0] + 80000, A[1], B[0] + 80000, B[1], 120, 2800).Id);
			}
			const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Toolset")));
			const FString Path = FPaths::Combine(Directory, TEXT("plan.svg"));
			IFileManager::Get().Delete(*Path, false, true, true);
			UFloor101DocumentToolset::ExportPlan(Path, TEXT(""));
			FString Svg;
			TestTrue(TEXT("written"), FFileHelper::LoadFileToString(Svg, *Path));
			TestTrue(TEXT("an SVG sheet"), Svg.StartsWith(TEXT("<?xml")) && Svg.Contains(TEXT("<svg xmlns=\"http://www.w3.org/2000/svg\"")));
			TestTrue(TEXT("with the cut walls and the room's area"), Svg.Contains(TEXT("<polygon")) && Svg.Contains(TEXT("11,17 m²")));
			AddExpectedMessagePlain(TEXT("F101: Export the plan as .svg or .png"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("Export the plan as .svg or .png"),
				[&Directory] { UFloor101DocumentToolset::ExportPlan(FPaths::Combine(Directory, TEXT("plan.pdf")), TEXT("")); });
			AddExpectedMessagePlain(TEXT("F101: Level not found"), ELogVerbosity::Warning);
			ExpectRaises(TEXT("Level not found"),
				[&Path] { UFloor101DocumentToolset::ExportPlan(Path, TEXT("00000000-0000-0000-0000-0000000000ff")); });
			IFileManager::Get().Delete(*Path, false, true, true);
			for (const FString& Id : Ids)
			{
				UFloor101DocumentToolset::RemoveWall(Id);
			}
		});

		It("RemoveWall raises on an unknown id", [this]()
		{
			ExpectRaises(TEXT("cannot remove wall"),
				[] { UFloor101DocumentToolset::RemoveWall(TEXT("00000000-0000-0000-0000-0000000000ff")); });
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
