#include "Floor101DocumentToolset.h"

#include "Editor.h"
#include "F101/Assets/Library.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Scene/SceneCompiler.h"
#include "Floor101DocumentSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"

// The toolset's operations mirror the core's, value for value.
static_assert(static_cast<int>(EFloor101OpeningOperation::Hinged) == static_cast<int>(f101::cad::OpeningOperation::Hinged) &&
	static_cast<int>(EFloor101OpeningOperation::DoubleHinged) == static_cast<int>(f101::cad::OpeningOperation::DoubleHinged) &&
	static_cast<int>(EFloor101OpeningOperation::Sliding) == static_cast<int>(f101::cad::OpeningOperation::Sliding) &&
	static_cast<int>(EFloor101OpeningOperation::Fixed) == static_cast<int>(f101::cad::OpeningOperation::Fixed));

namespace
{
	void RaiseError(const FString& Message)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("Floor101DocumentToolset: %s"), *Message));
	}

	FString ToFString(std::string_view Text)
	{
		return UTF8_TO_TCHAR(std::string(Text).c_str());
	}

	// The play session's document while Play-In-Editor runs (what the user is editing), else the editor world's.
	UFloor101DocumentSubsystem* GetSubsystem()
	{
		UWorld* World = GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
		UFloor101DocumentSubsystem* Subsystem = World ? World->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
		if (!Subsystem)
		{
			RaiseError(TEXT("the target world has no 101st Floor document."));
		}
		return Subsystem;
	}

	bool ParseWallId(const FString& Text, f101::cad::WallId& OutId)
	{
		const f101::Result<f101::cad::Uuid> Parsed = f101::cad::parseUuid(TCHAR_TO_UTF8(*Text));
		if (!Parsed)
		{
			RaiseError(FString::Printf(TEXT("'%s' is not a wall id: %s"), *Text, *ToFString(Parsed.error().message)));
			return false;
		}
		OutId = f101::cad::WallId{Parsed.value()};
		return true;
	}

	// Empty text is the nil id (the lowest level, for the commands that accept it).
	bool ParseLevelId(const FString& Text, f101::cad::LevelId& OutId)
	{
		if (Text.IsEmpty())
		{
			OutId = f101::cad::LevelId();
			return true;
		}
		const f101::Result<f101::cad::Uuid> Parsed = f101::cad::parseUuid(TCHAR_TO_UTF8(*Text));
		if (!Parsed)
		{
			RaiseError(FString::Printf(TEXT("'%s' is not a level id: %s"), *Text, *ToFString(Parsed.error().message)));
			return false;
		}
		OutId = f101::cad::LevelId{Parsed.value()};
		return true;
	}

	FFloor101LevelInfo ToInfo(const f101::cad::Project& Project, const f101::cad::Level& Level)
	{
		FFloor101LevelInfo Info;
		Info.Id = ToFString(f101::cad::toString(Level.id));
		Info.Name = ToFString(Level.name);
		Info.Index = static_cast<int32>(Project.levelIndex(Level.id).value_or(0));
		Info.ElevationMm = f101::cad::levelElevationMm(Project, Level.id);
		Info.HeightMm = Level.heightMm;
		Info.SlabThicknessMm = Level.slabThicknessMm;
		return Info;
	}

	bool ParseOpeningId(const FString& Text, f101::cad::OpeningId& OutId)
	{
		const f101::Result<f101::cad::Uuid> Parsed = f101::cad::parseUuid(TCHAR_TO_UTF8(*Text));
		if (!Parsed)
		{
			RaiseError(FString::Printf(TEXT("'%s' is not an opening id: %s"), *Text, *ToFString(Parsed.error().message)));
			return false;
		}
		OutId = f101::cad::OpeningId{Parsed.value()};
		return true;
	}

	FFloor101OpeningInfo ToInfo(const f101::cad::Opening& Opening)
	{
		FFloor101OpeningInfo Info;
		Info.Id = ToFString(f101::cad::toString(Opening.id));
		Info.Kind = Opening.kind == f101::cad::OpeningKind::Door ? EFloor101OpeningKind::Door : EFloor101OpeningKind::Window;
		Info.OffsetMm = Opening.offsetMm;
		Info.WidthMm = Opening.widthMm;
		Info.HeightMm = Opening.heightMm;
		Info.SillMm = Opening.sillMm;
		Info.Finish = ToFString(Opening.finish);
		Info.Operation = static_cast<EFloor101OpeningOperation>(Opening.operation); // same order
		Info.Hinge = Opening.hinge == f101::cad::OpeningEnd::Start ? EFloor101OpeningEnd::Start : EFloor101OpeningEnd::End;
		Info.OpensTo = Opening.opensTo == f101::cad::WallSide::Left ? EFloor101WallSide::Left : EFloor101WallSide::Right;
		Info.OpenPercent = Opening.openPercent;
		return Info;
	}

	// Applies `Edit` to a copy of an opening and changes it (one undoable step); raises and returns empty on failure.
	FFloor101OpeningInfo ChangeOpening(const FString& WallId, const FString& OpeningId, TFunctionRef<void(f101::cad::Opening&)> Edit)
	{
		UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
		f101::cad::WallId Wall;
		f101::cad::OpeningId Id;
		if (!Subsystem || !ParseWallId(WallId, Wall) || !ParseOpeningId(OpeningId, Id))
		{
			return {};
		}
		const f101::cad::Wall* Host = Subsystem->GetDocument().project().findWall(Wall);
		const f101::cad::Opening* Current = Host ? Host->findOpening(Id) : nullptr;
		if (!Current)
		{
			RaiseError(FString::Printf(TEXT("cannot change opening %s: no such opening in wall %s"), *OpeningId, *WallId));
			return {};
		}
		f101::cad::Opening Changed = *Current;
		Edit(Changed);
		const f101::Result<f101::cad::Transaction> Applied = Subsystem->GetMutableDocument().execute(f101::cad::ChangeOpeningCommand{Wall, Changed});
		if (!Applied)
		{
			RaiseError(FString::Printf(TEXT("cannot change opening %s: %s"), *OpeningId, *ToFString(Applied.error().message)));
			return {};
		}
		return ToInfo(*Subsystem->GetDocument().project().findWall(Wall)->findOpening(Id));
	}

	FFloor101WallInfo ToInfo(const f101::cad::Wall& Wall, const UFloor101DocumentSubsystem& Subsystem)
	{
		FFloor101WallInfo Info;
		for (const f101::cad::Opening& Opening : Wall.openings)
		{
			Info.Openings.Add(ToInfo(Opening));
		}
		Info.Id = ToFString(f101::cad::toString(Wall.id));
		Info.LevelId = ToFString(f101::cad::toString(Subsystem.GetDocument().project().levelOf(Wall)));
		Info.LeftFinish = ToFString(Wall.finishLeft);
		Info.RightFinish = ToFString(Wall.finishRight);
		Info.StartXMm = Wall.start.x_mm;
		Info.StartYMm = Wall.start.y_mm;
		Info.EndXMm = Wall.end.x_mm;
		Info.EndYMm = Wall.end.y_mm;
		Info.ThicknessMm = Wall.thicknessMm;
		Info.HeightMm = Wall.heightMm;
		Info.LengthMm = Wall.lengthMm();
		Info.Actor = Subsystem.FindWallActor(Wall.id);
		return Info;
	}

	TArray<FFloor101WallInfo> AllWalls(const UFloor101DocumentSubsystem& Subsystem)
	{
		TArray<FFloor101WallInfo> Walls;
		for (const f101::cad::Wall& Wall : Subsystem.GetDocument().project().walls)
		{
			Walls.Add(ToInfo(Wall, Subsystem));
		}
		return Walls;
	}

	// Applies a command built for the given wall id; raises "cannot <Action> wall <id>: <why>" if it is rejected.
	FFloor101WallInfo ChangeWall(const FString& WallId, const TCHAR* Action, TFunctionRef<f101::cad::Command(const f101::cad::WallId&)> MakeCommand)
	{
		UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
		f101::cad::WallId Id;
		if (!Subsystem || !ParseWallId(WallId, Id))
		{
			return {};
		}
		const f101::Result<f101::cad::Transaction> Applied = Subsystem->GetMutableDocument().execute(MakeCommand(Id));
		if (!Applied)
		{
			RaiseError(FString::Printf(TEXT("cannot %s wall %s: %s"), Action, *WallId, *ToFString(Applied.error().message)));
			return {};
		}
		const f101::cad::Wall* Changed = Subsystem->GetDocument().project().findWall(Id);
		return Changed ? ToInfo(*Changed, *Subsystem) : FFloor101WallInfo(); // deleted walls have no info
	}

	bool ParseFurnitureId(const FString& Text, f101::cad::FurnitureId& OutId)
	{
		const f101::Result<f101::cad::Uuid> Parsed = f101::cad::parseUuid(TCHAR_TO_UTF8(*Text));
		if (!Parsed)
		{
			RaiseError(FString::Printf(TEXT("'%s' is not a furniture id: %s"), *Text, *ToFString(Parsed.error().message)));
			return false;
		}
		OutId = f101::cad::FurnitureId{Parsed.value()};
		return true;
	}

	FFloor101FurnitureInfo ToInfo(const f101::cad::Furniture& Piece, const UFloor101DocumentSubsystem& Subsystem)
	{
		FFloor101FurnitureInfo Info;
		Info.Id = ToFString(f101::cad::toString(Piece.id));
		Info.Type = ToFString(Piece.type);
		Info.LevelId = ToFString(f101::cad::toString(Piece.level));
		Info.XMm = Piece.position.x_mm;
		Info.YMm = Piece.position.y_mm;
		Info.RotationCentidegrees = static_cast<int32>(Piece.rotationCentidegrees);
		Info.WidthMm = Piece.widthMm;
		Info.DepthMm = Piece.depthMm;
		Info.HeightMm = Piece.heightMm;
		Info.Finish = ToFString(Piece.finish);
		Info.Actor = Subsystem.FindFurnitureActor(Piece.id);
		return Info;
	}

	// Applies `Edit` to a copy of a piece and changes it (one undoable step); raises and returns empty on failure.
	FFloor101FurnitureInfo ChangeFurniture(const FString& FurnitureId, TFunctionRef<void(f101::cad::Furniture&)> Edit)
	{
		UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
		f101::cad::FurnitureId Id;
		if (!Subsystem || !ParseFurnitureId(FurnitureId, Id))
		{
			return {};
		}
		const f101::cad::Furniture* Current = Subsystem->GetDocument().project().findFurniture(Id);
		if (!Current)
		{
			RaiseError(FString::Printf(TEXT("cannot change furniture %s: no such piece"), *FurnitureId));
			return {};
		}
		f101::cad::Furniture Changed = *Current;
		Edit(Changed);
		const f101::Result<f101::cad::Transaction> Applied =
			Subsystem->GetMutableDocument().execute(f101::cad::ChangeFurnitureCommand{Changed});
		if (!Applied)
		{
			RaiseError(FString::Printf(TEXT("cannot change furniture %s: %s"), *FurnitureId, *ToFString(Applied.error().message)));
			return {};
		}
		return ToInfo(*Subsystem->GetDocument().project().findFurniture(Id), *Subsystem);
	}
}

TArray<FFloor101LevelInfo> UFloor101DocumentToolset::GetLevels()
{
	const UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	TArray<FFloor101LevelInfo> Levels;
	if (Subsystem)
	{
		const f101::cad::Project& Project = Subsystem->GetDocument().project();
		for (const f101::cad::Level& Level : Project.levels)
		{
			Levels.Add(ToInfo(Project, Level));
		}
	}
	return Levels;
}

FFloor101LevelInfo UFloor101DocumentToolset::AddLevel(const FString& Name, int64 HeightMm, int64 SlabThicknessMm)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	const f101::Result<f101::cad::Transaction> Added = Subsystem->GetMutableDocument().execute(
		f101::cad::AddLevelCommand{HeightMm, SlabThicknessMm, TCHAR_TO_UTF8(*Name)});
	if (!Added)
	{
		RaiseError(FString::Printf(TEXT("level rejected: %s"), *ToFString(Added.error().message)));
		return {};
	}
	const f101::cad::Project& Project = Subsystem->GetDocument().project();
	return ToInfo(Project, *Project.findLevel(Added.value().levels.front().id));
}

FFloor101LevelInfo UFloor101DocumentToolset::SetLevel(const FString& LevelId, const FString& Name, int64 HeightMm, int64 SlabThicknessMm)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Id;
	if (!Subsystem || !ParseLevelId(LevelId, Id))
	{
		return {};
	}
	const f101::Result<f101::cad::Transaction> Changed = Subsystem->GetMutableDocument().execute(
		f101::cad::ChangeLevelCommand{f101::cad::Level{Id, TCHAR_TO_UTF8(*Name), HeightMm, SlabThicknessMm}});
	if (!Changed)
	{
		RaiseError(FString::Printf(TEXT("cannot change level %s: %s"), *LevelId, *ToFString(Changed.error().message)));
		return {};
	}
	const f101::cad::Project& Project = Subsystem->GetDocument().project();
	return ToInfo(Project, *Project.findLevel(Id));
}

void UFloor101DocumentToolset::RemoveLevel(const FString& LevelId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Id;
	if (!Subsystem || !ParseLevelId(LevelId, Id))
	{
		return;
	}
	const f101::Result<f101::cad::Transaction> Removed = Subsystem->GetMutableDocument().execute(f101::cad::DeleteLevelCommand{Id});
	if (!Removed)
	{
		RaiseError(FString::Printf(TEXT("cannot remove level %s: %s"), *LevelId, *ToFString(Removed.error().message)));
	}
}

void UFloor101DocumentToolset::SetLevelView(const FString& LevelId, bool bShowAllLevels)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Id;
	if (!Subsystem || !ParseLevelId(LevelId, Id))
	{
		return;
	}
	if (Id.isNil())
	{
		Id = Subsystem->GetDocument().project().levels.front().id;
	}
	const f101::Status Shown = Subsystem->Edit([&Id, bShowAllLevels](f101::cad::EditController& Editor)
	{
		Editor.setShowAllLevels(bShowAllLevels);
		return Editor.setActiveLevel(Id);
	});
	if (!Shown)
	{
		RaiseError(FString::Printf(TEXT("cannot show level %s: %s"), *LevelId, *ToFString(Shown.error().message)));
	}
}

TArray<FFloor101WallInfo> UFloor101DocumentToolset::GetWalls()
{
	const UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	return Subsystem ? AllWalls(*Subsystem) : TArray<FFloor101WallInfo>();
}

FFloor101WallInfo UFloor101DocumentToolset::AddWall(const FString& LevelId, int64 StartXMm, int64 StartYMm, int64 EndXMm, int64 EndYMm,
	int64 ThicknessMm, int64 HeightMm)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Level;
	if (!Subsystem || !ParseLevelId(LevelId, Level))
	{
		return {};
	}
	const f101::Result<f101::cad::WallId> Added = Subsystem->GetMutableDocument().addWall(
		{StartXMm, StartYMm}, {EndXMm, EndYMm}, ThicknessMm, HeightMm, Level);
	if (!Added)
	{
		RaiseError(FString::Printf(TEXT("wall rejected: %s"), *ToFString(Added.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findWall(Added.value()), *Subsystem);
}

FFloor101WallInfo UFloor101DocumentToolset::SetWallEnd(const FString& WallId, int64 EndXMm, int64 EndYMm)
{
	return ChangeWall(WallId, TEXT("move the end of"), [EndXMm, EndYMm](const f101::cad::WallId& Id) -> f101::cad::Command
	{
		return f101::cad::MoveWallEndpointCommand{Id, f101::cad::WallEndpoint::End, {EndXMm, EndYMm}};
	});
}

FFloor101WallInfo UFloor101DocumentToolset::SetWallLength(const FString& WallId, int64 LengthMm)
{
	return ChangeWall(WallId, TEXT("change the length of"), [LengthMm](const f101::cad::WallId& Id) -> f101::cad::Command
	{
		return f101::cad::ChangeWallLengthCommand{Id, LengthMm};
	});
}

FFloor101WallInfo UFloor101DocumentToolset::SetWallThickness(const FString& WallId, int64 ThicknessMm)
{
	return ChangeWall(WallId, TEXT("change the thickness of"), [ThicknessMm](const f101::cad::WallId& Id) -> f101::cad::Command
	{
		return f101::cad::ChangeWallThicknessCommand{Id, ThicknessMm};
	});
}

FFloor101WallInfo UFloor101DocumentToolset::SetWallHeight(const FString& WallId, int64 HeightMm)
{
	return ChangeWall(WallId, TEXT("change the height of"), [HeightMm](const f101::cad::WallId& Id) -> f101::cad::Command
	{
		return f101::cad::ChangeWallHeightCommand{Id, HeightMm};
	});
}

TArray<FFloor101WallInfo> UFloor101DocumentToolset::MoveCorner(const FString& LevelId, int64 FromXMm, int64 FromYMm, int64 ToXMm, int64 ToYMm)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Level;
	if (!Subsystem || !ParseLevelId(LevelId, Level))
	{
		return {};
	}
	const f101::Result<f101::cad::Transaction> Moved =
		Subsystem->GetMutableDocument().execute(f101::cad::MoveJointCommand{{FromXMm, FromYMm}, {ToXMm, ToYMm}, Level});
	if (!Moved)
	{
		RaiseError(FString::Printf(TEXT("cannot move the corner at (%lld, %lld): %s"), FromXMm, FromYMm, *ToFString(Moved.error().message)));
		return {};
	}
	TArray<FFloor101WallInfo> Walls;
	for (const f101::cad::WallChange& Change : Moved.value().walls)
	{
		Walls.Add(ToInfo(*Subsystem->GetDocument().project().findWall(Change.id), *Subsystem));
	}
	return Walls;
}

TArray<FFloor101RoomInfo> UFloor101DocumentToolset::GetRooms()
{
	const UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	TArray<FFloor101RoomInfo> Rooms;
	// Rooms are solved per level, as in the scene.
	for (const f101::scene::SceneRoom& Entry : f101::scene::compileScene(Subsystem->GetDocument().project()).rooms)
	{
		const f101::geometry::Room& Room = Entry.room;
		FFloor101RoomInfo Info;
		Info.Key = ToFString(Room.key);
		Info.LevelId = ToFString(f101::cad::toString(Entry.level));
		if (const f101::cad::RoomFinish* Finish = Subsystem->GetDocument().project().findRoomFinish(Room.key))
		{
			Info.FloorFinish = ToFString(Finish->floor);
			Info.CeilingFinish = ToFString(Finish->ceiling);
		}
		Info.AreaM2 = Room.areaMm2 / 1.0e6;
		for (const f101::cad::WallId& Wall : Room.walls)
		{
			Info.WallIds.Add(ToFString(f101::cad::toString(Wall)));
		}
		Info.FloorActor = Subsystem->FindRoomFloorActor(Info.Key);
		Rooms.Add(MoveTemp(Info));
	}
	return Rooms;
}

FFloor101OpeningInfo UFloor101DocumentToolset::AddOpening(const FString& WallId, EFloor101OpeningKind Kind, int64 OffsetMm,
	int64 WidthMm, int64 HeightMm, int64 SillMm)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::WallId Wall;
	if (!Subsystem || !ParseWallId(WallId, Wall))
	{
		return {};
	}
	const f101::cad::OpeningKind CadKind = Kind == EFloor101OpeningKind::Door ? f101::cad::OpeningKind::Door : f101::cad::OpeningKind::Window;
	const f101::Result<f101::cad::OpeningId> Added = Subsystem->GetMutableDocument().addOpening(
		Wall, f101::cad::Opening{f101::cad::OpeningId{}, CadKind, OffsetMm, WidthMm, HeightMm, SillMm});
	if (!Added)
	{
		RaiseError(FString::Printf(TEXT("cannot add the opening to wall %s: %s"), *WallId, *ToFString(Added.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findWall(Wall)->findOpening(Added.value()));
}

FFloor101OpeningInfo UFloor101DocumentToolset::SetOpening(const FString& WallId, const FString& OpeningId, int64 OffsetMm,
	int64 WidthMm, int64 HeightMm, int64 SillMm)
{
	return ChangeOpening(WallId, OpeningId, [&](f101::cad::Opening& Opening)
	{
		Opening.offsetMm = OffsetMm;
		Opening.widthMm = WidthMm;
		Opening.heightMm = HeightMm;
		Opening.sillMm = SillMm;
	});
}

FFloor101OpeningInfo UFloor101DocumentToolset::SetOpeningFinish(const FString& WallId, const FString& OpeningId, const FString& FinishId)
{
	return ChangeOpening(WallId, OpeningId, [&FinishId](f101::cad::Opening& Opening) { Opening.finish = TCHAR_TO_UTF8(*FinishId); });
}

FFloor101OpeningInfo UFloor101DocumentToolset::SetOpeningOperation(const FString& WallId, const FString& OpeningId,
	EFloor101OpeningOperation Operation, EFloor101OpeningEnd Hinge, EFloor101WallSide OpensTo, int32 OpenPercent)
{
	return ChangeOpening(WallId, OpeningId, [&](f101::cad::Opening& Opening)
	{
		Opening.operation = static_cast<f101::cad::OpeningOperation>(Operation); // same order
		Opening.hinge = Hinge == EFloor101OpeningEnd::Start ? f101::cad::OpeningEnd::Start : f101::cad::OpeningEnd::End;
		Opening.opensTo = OpensTo == EFloor101WallSide::Left ? f101::cad::WallSide::Left : f101::cad::WallSide::Right;
		Opening.openPercent = OpenPercent;
	});
}

void UFloor101DocumentToolset::RemoveOpening(const FString& WallId, const FString& OpeningId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::WallId Wall;
	f101::cad::OpeningId Id;
	if (!Subsystem || !ParseWallId(WallId, Wall) || !ParseOpeningId(OpeningId, Id))
	{
		return;
	}
	const f101::Result<f101::cad::Transaction> Removed = Subsystem->GetMutableDocument().execute(f101::cad::DeleteOpeningCommand{Wall, Id});
	if (!Removed)
	{
		RaiseError(FString::Printf(TEXT("cannot remove opening %s: %s"), *OpeningId, *ToFString(Removed.error().message)));
	}
}

TArray<FFloor101FinishInfo> UFloor101DocumentToolset::GetFinishes()
{
	TArray<FFloor101FinishInfo> Finishes;
	for (const f101::cad::FinishInfo& Finish : f101::cad::finishCatalogue())
	{
		FFloor101FinishInfo Info;
		Info.Id = ToFString(Finish.id);
		Info.Category = ToFString(f101::cad::finishCategoryId(Finish.category));
		Info.bForWalls = Finish.allows(f101::cad::Surface::Wall);
		Info.bForFloors = Finish.allows(f101::cad::Surface::Floor);
		Info.bForCeilings = Finish.allows(f101::cad::Surface::Ceiling);
		Info.bForCarpentry = Finish.allows(f101::cad::Surface::Carpentry);
		Info.bForFurniture = Finish.allows(f101::cad::Surface::Furniture);
		Info.bForServices = Finish.allows(f101::cad::Surface::Services);
		Info.Color = FColor(Finish.color.r, Finish.color.g, Finish.color.b);
		Info.Roughness = static_cast<float>(Finish.roughness);
		Info.Opacity = static_cast<float>(Finish.opacity);
		Finishes.Add(Info);
	}
	return Finishes;
}

FFloor101WallInfo UFloor101DocumentToolset::SetWallFinish(const FString& WallId, EFloor101WallSide Side, const FString& FinishId)
{
	const f101::cad::WallSide CadSide = Side == EFloor101WallSide::Left ? f101::cad::WallSide::Left : f101::cad::WallSide::Right;
	const std::string Finish = TCHAR_TO_UTF8(*FinishId);
	return ChangeWall(WallId, TEXT("change the finish of"), [CadSide, &Finish](const f101::cad::WallId& Id) -> f101::cad::Command
	{
		return f101::cad::SetWallFinishCommand{Id, CadSide, Finish};
	});
}

void UFloor101DocumentToolset::SetRoomFinish(const FString& RoomKey, const FString& FloorFinishId, const FString& CeilingFinishId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const f101::Result<f101::cad::Transaction> Changed = Subsystem->GetMutableDocument().execute(f101::cad::SetRoomFinishCommand{
		TCHAR_TO_UTF8(*RoomKey), TCHAR_TO_UTF8(*FloorFinishId), TCHAR_TO_UTF8(*CeilingFinishId)});
	if (!Changed)
	{
		RaiseError(FString::Printf(TEXT("cannot change the finishes of room %s: %s"), *RoomKey, *ToFString(Changed.error().message)));
	}
}

void UFloor101DocumentToolset::RemoveWall(const FString& WallId)
{
	ChangeWall(WallId, TEXT("remove"), [](const f101::cad::WallId& Id) -> f101::cad::Command
	{
		return f101::cad::DeleteWallCommand{Id};
	});
}

TArray<FFloor101FurnitureTypeInfo> UFloor101DocumentToolset::GetFurnitureTypes()
{
	TArray<FFloor101FurnitureTypeInfo> Types;
	for (const f101::cad::FurnitureType& Type : f101::cad::furnitureCatalogue())
	{
		FFloor101FurnitureTypeInfo Info;
		Info.Id = ToFString(Type.id);
		Info.Category = ToFString(f101::cad::furnitureCategoryId(Type.category));
		Info.WidthMm = Type.widthMm;
		Info.DepthMm = Type.depthMm;
		Info.HeightMm = Type.heightMm;
		Info.MinWidthMm = Type.minWidthMm;
		Info.MaxWidthMm = Type.maxWidthMm;
		Info.MinDepthMm = Type.minDepthMm;
		Info.MaxDepthMm = Type.maxDepthMm;
		Info.MinHeightMm = Type.minHeightMm;
		Info.MaxHeightMm = Type.maxHeightMm;
		Info.bAgainstWall = Type.againstWall;
		Info.BodyFinish = ToFString(Type.bodyFinish);
		Types.Add(Info);
	}
	return Types;
}

TArray<FFloor101FurnitureInfo> UFloor101DocumentToolset::GetFurniture()
{
	const UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	TArray<FFloor101FurnitureInfo> Pieces;
	if (Subsystem)
	{
		for (const f101::cad::Furniture& Piece : Subsystem->GetDocument().project().furniture)
		{
			Pieces.Add(ToInfo(Piece, *Subsystem));
		}
	}
	return Pieces;
}

FFloor101FurnitureInfo UFloor101DocumentToolset::AddFurniture(const FString& LevelId, const FString& Type, int64 XMm, int64 YMm,
	int32 RotationCentidegrees, int64 WidthMm, int64 DepthMm, int64 HeightMm)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Level;
	if (!Subsystem || !ParseLevelId(LevelId, Level))
	{
		return {};
	}
	// Catalogue types and imported models ("asset:<uuid>", ADR-0017) alike: the default size of the type.
	const std::string TypeId = TCHAR_TO_UTF8(*Type);
	const std::optional<f101::cad::FurnitureType> Spec = f101::cad::furnitureSpec(Subsystem->GetDocument().project(), TypeId);
	f101::cad::Furniture Piece = Spec ? f101::cad::makeFurniture(*Spec, {XMm, YMm}, RotationCentidegrees)
		: f101::cad::makeFurniture(TypeId, {XMm, YMm}, RotationCentidegrees);
	Piece.level = Level;
	for (const auto& [Given, Target] : {std::pair{WidthMm, &Piece.widthMm}, {DepthMm, &Piece.depthMm}, {HeightMm, &Piece.heightMm}})
	{
		if (Given != 0)
		{
			*Target = Given;
		}
	}
	const f101::Result<f101::cad::FurnitureId> Added = Subsystem->GetMutableDocument().addFurniture(Piece);
	if (!Added)
	{
		RaiseError(FString::Printf(TEXT("furniture rejected: %s"), *ToFString(Added.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findFurniture(Added.value()), *Subsystem);
}

FFloor101FurnitureInfo UFloor101DocumentToolset::SetFurniture(const FString& FurnitureId, int64 XMm, int64 YMm,
	int32 RotationCentidegrees, int64 WidthMm, int64 DepthMm, int64 HeightMm)
{
	return ChangeFurniture(FurnitureId, [&](f101::cad::Furniture& Piece)
	{
		Piece.position = {XMm, YMm};
		Piece.rotationCentidegrees = RotationCentidegrees;
		Piece.widthMm = WidthMm;
		Piece.depthMm = DepthMm;
		Piece.heightMm = HeightMm;
	});
}

FFloor101FurnitureInfo UFloor101DocumentToolset::SetFurnitureFinish(const FString& FurnitureId, const FString& FinishId)
{
	return ChangeFurniture(FurnitureId, [&FinishId](f101::cad::Furniture& Piece) { Piece.finish = TCHAR_TO_UTF8(*FinishId); });
}

void UFloor101DocumentToolset::RemoveFurniture(const FString& FurnitureId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::FurnitureId Id;
	if (!Subsystem || !ParseFurnitureId(FurnitureId, Id))
	{
		return;
	}
	const f101::Result<f101::cad::Transaction> Removed = Subsystem->GetMutableDocument().execute(f101::cad::DeleteFurnitureCommand{Id});
	if (!Removed)
	{
		RaiseError(FString::Printf(TEXT("cannot remove furniture %s: %s"), *FurnitureId, *ToFString(Removed.error().message)));
	}
}

// Columns and building services (ADR-0021).
static_assert(static_cast<int>(EFloor101ColumnShape::Rectangle) == static_cast<int>(f101::cad::ColumnShape::Rectangle) &&
	static_cast<int>(EFloor101ColumnShape::Round) == static_cast<int>(f101::cad::ColumnShape::Round));
static_assert(static_cast<int>(EFloor101ServiceKind::Electrical) == static_cast<int>(f101::cad::ServiceKind::Electrical) &&
	static_cast<int>(EFloor101ServiceKind::Water) == static_cast<int>(f101::cad::ServiceKind::Water) &&
	static_cast<int>(EFloor101ServiceKind::Sewage) == static_cast<int>(f101::cad::ServiceKind::Sewage) &&
	static_cast<int>(EFloor101ServiceKind::Gas) == static_cast<int>(f101::cad::ServiceKind::Gas));

namespace
{
	// A UUID of some kind of entity; raises and returns false if the text is not one.
	template <typename IdType>
	bool ParseEntityId(const FString& Text, const TCHAR* What, IdType& OutId)
	{
		const f101::Result<f101::cad::Uuid> Parsed = f101::cad::parseUuid(TCHAR_TO_UTF8(*Text));
		if (!Parsed)
		{
			RaiseError(FString::Printf(TEXT("'%s' is not a %s id: %s"), *Text, What, *ToFString(Parsed.error().message)));
			return false;
		}
		OutId = IdType{Parsed.value()};
		return true;
	}

	FFloor101ColumnInfo ToInfo(const f101::cad::Column& Column, const UFloor101DocumentSubsystem& Subsystem)
	{
		FFloor101ColumnInfo Info;
		Info.Id = ToFString(f101::cad::toString(Column.id));
		Info.LevelId = ToFString(f101::cad::toString(Column.level));
		Info.XMm = Column.position.x_mm;
		Info.YMm = Column.position.y_mm;
		Info.Shape = static_cast<EFloor101ColumnShape>(Column.shape);
		Info.WidthMm = Column.widthMm;
		Info.DepthMm = Column.depthMm;
		Info.RotationCentidegrees = static_cast<int32>(Column.rotationCentidegrees);
		Info.Finish = ToFString(Column.finish);
		Info.Actor = Subsystem.FindColumnActor(Column.id);
		return Info;
	}

	// A column of that shape and size: 0 width is the default, 0 depth the width; a round one is as deep as wide, unturned.
	void Reshape(f101::cad::Column& Column, EFloor101ColumnShape Shape, int64 WidthMm, int64 DepthMm, int32 RotationCentidegrees)
	{
		Column.shape = static_cast<f101::cad::ColumnShape>(Shape);
		Column.widthMm = WidthMm != 0 ? WidthMm : f101::cad::DefaultColumnSizeMm;
		Column.depthMm = Column.shape == f101::cad::ColumnShape::Round || DepthMm == 0 ? Column.widthMm : DepthMm;
		Column.rotationCentidegrees = Column.shape == f101::cad::ColumnShape::Round ? 0 : RotationCentidegrees;
	}

	FFloor101ColumnInfo ChangeColumn(const FString& ColumnId, TFunctionRef<void(f101::cad::Column&)> Edit)
	{
		UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
		f101::cad::ColumnId Id;
		if (!Subsystem || !ParseEntityId(ColumnId, TEXT("column"), Id))
		{
			return {};
		}
		const f101::cad::Column* Current = Subsystem->GetDocument().project().findColumn(Id);
		if (!Current)
		{
			RaiseError(FString::Printf(TEXT("cannot change column %s: no such column"), *ColumnId));
			return {};
		}
		f101::cad::Column Changed = *Current;
		Edit(Changed);
		const f101::Result<f101::cad::Transaction> Applied = Subsystem->GetMutableDocument().execute(f101::cad::ChangeColumnCommand{Changed});
		if (!Applied)
		{
			RaiseError(FString::Printf(TEXT("cannot change column %s: %s"), *ColumnId, *ToFString(Applied.error().message)));
			return {};
		}
		return ToInfo(*Subsystem->GetDocument().project().findColumn(Id), *Subsystem);
	}

	FFloor101ServiceRunInfo ToInfo(const f101::cad::ServiceRun& Run, const UFloor101DocumentSubsystem& Subsystem)
	{
		FFloor101ServiceRunInfo Info;
		Info.Id = ToFString(f101::cad::toString(Run.id));
		Info.LevelId = ToFString(f101::cad::toString(Run.level));
		Info.Kind = static_cast<EFloor101ServiceKind>(Run.kind);
		for (const f101::cad::ServicePoint& Point : Run.points)
		{
			Info.Points.Add(FFloor101ServicePoint{Point.x_mm, Point.y_mm, Point.z_mm});
		}
		Info.LengthMm = f101::cad::serviceRunLengthMm(Run);
		Info.DiameterMm = f101::cad::serviceKindInfo(Run.kind).diameterMm;
		Info.Actor = Subsystem.FindServiceRunActor(Run.id);
		return Info;
	}

	std::vector<f101::cad::ServicePoint> ToPoints(const TArray<FFloor101ServicePoint>& Points)
	{
		std::vector<f101::cad::ServicePoint> Converted;
		for (const FFloor101ServicePoint& Point : Points)
		{
			Converted.push_back(f101::cad::ServicePoint{Point.XMm, Point.YMm, Point.ZMm});
		}
		return Converted;
	}
}

TArray<FFloor101ColumnInfo> UFloor101DocumentToolset::GetColumns()
{
	const UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	TArray<FFloor101ColumnInfo> Columns;
	if (Subsystem)
	{
		for (const f101::cad::Column& Column : Subsystem->GetDocument().project().columns)
		{
			Columns.Add(ToInfo(Column, *Subsystem));
		}
	}
	return Columns;
}

FFloor101ColumnInfo UFloor101DocumentToolset::AddColumn(const FString& LevelId, EFloor101ColumnShape Shape, int64 XMm, int64 YMm,
	int64 WidthMm, int64 DepthMm, int32 RotationCentidegrees)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Level;
	if (!Subsystem || !ParseLevelId(LevelId, Level))
	{
		return {};
	}
	f101::cad::Column Column;
	Column.level = Level;
	Column.position = {XMm, YMm};
	Reshape(Column, Shape, WidthMm, DepthMm, RotationCentidegrees);
	const f101::Result<f101::cad::ColumnId> Added = Subsystem->GetMutableDocument().addColumn(Column);
	if (!Added)
	{
		RaiseError(FString::Printf(TEXT("column rejected: %s"), *ToFString(Added.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findColumn(Added.value()), *Subsystem);
}

FFloor101ColumnInfo UFloor101DocumentToolset::SetColumn(const FString& ColumnId, EFloor101ColumnShape Shape, int64 XMm, int64 YMm,
	int64 WidthMm, int64 DepthMm, int32 RotationCentidegrees)
{
	return ChangeColumn(ColumnId, [&](f101::cad::Column& Column)
	{
		Column.position = {XMm, YMm};
		Reshape(Column, Shape, WidthMm, DepthMm, RotationCentidegrees);
	});
}

FFloor101ColumnInfo UFloor101DocumentToolset::SetColumnFinish(const FString& ColumnId, const FString& FinishId)
{
	return ChangeColumn(ColumnId, [&FinishId](f101::cad::Column& Column) { Column.finish = TCHAR_TO_UTF8(*FinishId); });
}

void UFloor101DocumentToolset::RemoveColumn(const FString& ColumnId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::ColumnId Id;
	if (!Subsystem || !ParseEntityId(ColumnId, TEXT("column"), Id))
	{
		return;
	}
	const f101::Result<f101::cad::Transaction> Removed = Subsystem->GetMutableDocument().execute(f101::cad::DeleteColumnCommand{Id});
	if (!Removed)
	{
		RaiseError(FString::Printf(TEXT("cannot remove column %s: %s"), *ColumnId, *ToFString(Removed.error().message)));
	}
}

TArray<FFloor101ServiceRunInfo> UFloor101DocumentToolset::GetServiceRuns()
{
	const UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	TArray<FFloor101ServiceRunInfo> Runs;
	if (Subsystem)
	{
		for (const f101::cad::ServiceRun& Run : Subsystem->GetDocument().project().services)
		{
			Runs.Add(ToInfo(Run, *Subsystem));
		}
	}
	return Runs;
}

FFloor101ServiceRunInfo UFloor101DocumentToolset::AddServiceRun(const FString& LevelId, EFloor101ServiceKind Kind,
	const TArray<FFloor101ServicePoint>& Points)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Level;
	if (!Subsystem || !ParseLevelId(LevelId, Level))
	{
		return {};
	}
	f101::cad::ServiceRun Run;
	Run.level = Level;
	Run.kind = static_cast<f101::cad::ServiceKind>(Kind);
	Run.points = ToPoints(Points);
	const f101::Result<f101::cad::ServiceRunId> Added = Subsystem->GetMutableDocument().addServiceRun(Run);
	if (!Added)
	{
		RaiseError(FString::Printf(TEXT("run rejected: %s"), *ToFString(Added.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findServiceRun(Added.value()), *Subsystem);
}

FFloor101ServiceRunInfo UFloor101DocumentToolset::SetServiceRun(const FString& RunId, EFloor101ServiceKind Kind,
	const TArray<FFloor101ServicePoint>& Points)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::ServiceRunId Id;
	if (!Subsystem || !ParseEntityId(RunId, TEXT("run"), Id))
	{
		return {};
	}
	const f101::cad::ServiceRun* Current = Subsystem->GetDocument().project().findServiceRun(Id);
	if (!Current)
	{
		RaiseError(FString::Printf(TEXT("cannot change run %s: no such run"), *RunId));
		return {};
	}
	f101::cad::ServiceRun Changed = *Current;
	Changed.kind = static_cast<f101::cad::ServiceKind>(Kind);
	Changed.points = ToPoints(Points);
	const f101::Result<f101::cad::Transaction> Applied = Subsystem->GetMutableDocument().execute(f101::cad::ChangeServiceRunCommand{Changed});
	if (!Applied)
	{
		RaiseError(FString::Printf(TEXT("cannot change run %s: %s"), *RunId, *ToFString(Applied.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findServiceRun(Id), *Subsystem);
}

void UFloor101DocumentToolset::RemoveServiceRun(const FString& RunId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::ServiceRunId Id;
	if (!Subsystem || !ParseEntityId(RunId, TEXT("run"), Id))
	{
		return;
	}
	const f101::Result<f101::cad::Transaction> Removed = Subsystem->GetMutableDocument().execute(f101::cad::DeleteServiceRunCommand{Id});
	if (!Removed)
	{
		RaiseError(FString::Printf(TEXT("cannot remove run %s: %s"), *RunId, *ToFString(Removed.error().message)));
	}
}

void UFloor101DocumentToolset::SetServicesView(bool bShown)
{
	if (UFloor101DocumentSubsystem* Subsystem = GetSubsystem())
	{
		(void)Subsystem->Edit([bShown](f101::cad::EditController& Editor)
		{
			Editor.setShowServices(bShown);
			return f101::Status::success();
		});
	}
}

namespace
{
	FFloor101AssetInfo ToInfo(const f101::cad::Asset& Asset, const UFloor101DocumentSubsystem& Subsystem)
	{
		FFloor101AssetInfo Info;
		Info.Id = ToFString(f101::cad::toString(Asset.id));
		Info.Ref = ToFString(f101::cad::assetRef(Asset.id));
		Info.Kind = Asset.kind == f101::cad::AssetKind::Texture ? EFloor101AssetKind::Texture : EFloor101AssetKind::Model;
		Info.Name = ToFString(Asset.name);
		Info.File = ToFString(Asset.file);
		Info.Library = ToFString(Asset.library);
		Info.TextureWidthMm = Asset.texture.widthMm;
		Info.TextureHeightMm = Asset.texture.heightMm;
		Info.RoughnessPercent = Asset.texture.roughnessPercent;
		Info.PixelWidth = Asset.texture.pixelWidth;
		Info.PixelHeight = Asset.texture.pixelHeight;
		Info.Unit = static_cast<EFloor101ModelUnit>(Asset.model.unit); // same order
		Info.bZUp = Asset.model.up == f101::cad::ModelUpAxis::Z;
		Info.FrontQuarterTurns = Asset.model.frontQuarterTurns;
		Info.bAgainstWall = Asset.model.againstWall;
		Info.WidthMm = Asset.model.widthMm;
		Info.DepthMm = Asset.model.depthMm;
		Info.HeightMm = Asset.model.heightMm;
		Info.Triangles = Asset.model.triangles;
		Info.Problem = Asset.kind == f101::cad::AssetKind::Model ? Subsystem.GetAssetProblem(Asset.id) : FString();
		return Info;
	}

	bool ParseAssetId(const FString& Text, f101::cad::AssetId& OutId)
	{
		const f101::Result<f101::cad::Uuid> Parsed = f101::cad::parseUuid(TCHAR_TO_UTF8(*Text));
		if (!Parsed)
		{
			RaiseError(FString::Printf(TEXT("'%s' is not an asset id: %s"), *Text, *ToFString(Parsed.error().message)));
			return false;
		}
		OutId = f101::cad::AssetId{Parsed.value()};
		return true;
	}

	// Applies `Edit` to a copy of an asset and changes it through the subsystem (a model is measured again).
	FFloor101AssetInfo ChangeAssetWith(const FString& AssetId, TFunctionRef<void(f101::cad::Asset&)> Edit)
	{
		UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
		f101::cad::AssetId Id;
		if (!Subsystem || !ParseAssetId(AssetId, Id))
		{
			return {};
		}
		const f101::cad::Asset* Current = Subsystem->GetDocument().project().findAsset(Id);
		if (!Current)
		{
			RaiseError(FString::Printf(TEXT("cannot change asset %s: no such asset"), *AssetId));
			return {};
		}
		f101::cad::Asset Changed = *Current;
		Edit(Changed);
		const f101::Status Applied = Subsystem->ChangeAsset(Changed);
		if (!Applied)
		{
			RaiseError(FString::Printf(TEXT("cannot change asset %s: %s"), *AssetId, *ToFString(Applied.error().message)));
			return {};
		}
		return ToInfo(*Subsystem->GetDocument().project().findAsset(Id), *Subsystem);
	}
}

// The toolset's model units mirror the core's, value for value.
static_assert(static_cast<int>(EFloor101ModelUnit::Meter) == static_cast<int>(f101::cad::ModelUnit::Meter) &&
	static_cast<int>(EFloor101ModelUnit::Centimeter) == static_cast<int>(f101::cad::ModelUnit::Centimeter) &&
	static_cast<int>(EFloor101ModelUnit::Millimeter) == static_cast<int>(f101::cad::ModelUnit::Millimeter) &&
	static_cast<int>(EFloor101ModelUnit::Inch) == static_cast<int>(f101::cad::ModelUnit::Inch));

TArray<FFloor101AssetInfo> UFloor101DocumentToolset::GetAssets()
{
	const UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	TArray<FFloor101AssetInfo> Assets;
	if (Subsystem)
	{
		for (const f101::cad::Asset& Asset : Subsystem->GetDocument().project().assets)
		{
			Assets.Add(ToInfo(Asset, *Subsystem));
		}
	}
	return Assets;
}

FFloor101AssetInfo UFloor101DocumentToolset::ImportAsset(const FString& Path)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	const f101::Result<f101::cad::AssetId> Imported = Subsystem->ImportAsset(Path);
	if (!Imported)
	{
		RaiseError(FString::Printf(TEXT("cannot import %s: %s"), *Path, *ToFString(Imported.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findAsset(Imported.value()), *Subsystem);
}

FFloor101AssetInfo UFloor101DocumentToolset::SetTextureAsset(const FString& AssetId, const FString& Name, int64 WidthMm, int64 HeightMm,
	int32 RoughnessPercent)
{
	return ChangeAssetWith(AssetId, [&](f101::cad::Asset& Asset)
	{
		Asset.name = TCHAR_TO_UTF8(*Name);
		Asset.texture.widthMm = WidthMm;
		Asset.texture.heightMm = HeightMm;
		Asset.texture.roughnessPercent = RoughnessPercent;
	});
}

FFloor101AssetInfo UFloor101DocumentToolset::SetModelAsset(const FString& AssetId, const FString& Name, EFloor101ModelUnit Unit, bool bZUp,
	int32 FrontQuarterTurns, bool bAgainstWall)
{
	return ChangeAssetWith(AssetId, [&](f101::cad::Asset& Asset)
	{
		Asset.name = TCHAR_TO_UTF8(*Name);
		Asset.model.unit = static_cast<f101::cad::ModelUnit>(Unit); // same order
		Asset.model.up = bZUp ? f101::cad::ModelUpAxis::Z : f101::cad::ModelUpAxis::Y;
		Asset.model.frontQuarterTurns = FrontQuarterTurns;
		Asset.model.againstWall = bAgainstWall;
	});
}

void UFloor101DocumentToolset::RemoveAsset(const FString& AssetId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::AssetId Id;
	if (!Subsystem || !ParseAssetId(AssetId, Id))
	{
		return;
	}
	const f101::Status Removed = Subsystem->RemoveAsset(Id);
	if (!Removed)
	{
		RaiseError(FString::Printf(TEXT("cannot remove asset %s: %s"), *AssetId, *ToFString(Removed.error().message)));
	}
}

TArray<FFloor101LibraryItem> UFloor101DocumentToolset::GetLibrary()
{
	const f101::assets::Library& Library = UFloor101DocumentSubsystem::GetLibrary();
	TArray<FFloor101LibraryItem> Items;
	for (const f101::assets::LibraryFurniture& Furniture : Library.furniture)
	{
		FFloor101LibraryItem Item;
		Item.Id = ToFString(Furniture.id);
		Item.Kind = EFloor101AssetKind::Model;
		Item.Name = ToFString(Furniture.name);
		Item.Category = ToFString(f101::cad::furnitureCategoryId(Furniture.category));
		Item.Credit = ToFString(Furniture.credit);
		Items.Add(Item);
	}
	for (const f101::assets::LibraryTexture& Texture : Library.textures)
	{
		FFloor101LibraryItem Item;
		Item.Id = ToFString(Texture.id);
		Item.Kind = EFloor101AssetKind::Texture;
		Item.Name = ToFString(Texture.name);
		Item.Category = ToFString(f101::cad::finishCategoryId(Texture.category));
		Item.TextureWidthMm = Texture.texture.widthMm;
		Item.TextureHeightMm = Texture.texture.heightMm;
		Item.bForWalls = (Texture.texture.surfaces & static_cast<std::uint8_t>(f101::cad::Surface::Wall)) != 0;
		Item.bForFloors = (Texture.texture.surfaces & static_cast<std::uint8_t>(f101::cad::Surface::Floor)) != 0;
		Item.bForCeilings = (Texture.texture.surfaces & static_cast<std::uint8_t>(f101::cad::Surface::Ceiling)) != 0;
		Item.Credit = ToFString(Texture.credit);
		Items.Add(Item);
	}
	return Items;
}

FFloor101AssetInfo UFloor101DocumentToolset::AddFromLibrary(const FString& ItemId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	const f101::Result<f101::cad::AssetId> Added = Subsystem->AddFromLibrary(ItemId);
	if (!Added)
	{
		RaiseError(FString::Printf(TEXT("cannot add %s from the library: %s"), *ItemId, *ToFString(Added.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findAsset(Added.value()), *Subsystem);
}

FString UFloor101DocumentToolset::Undo()
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	if (!Subsystem->GetDocument().canUndo())
	{
		RaiseError(TEXT("nothing to undo."));
		return {};
	}
	// Through the edit controller, like Ctrl+Z: a wall being drawn or an endpoint being dragged is cancelled first.
	const FString Label = ToFString(Subsystem->GetDocument().undoLabel());
	const f101::Status Undone = Subsystem->Edit([](f101::cad::EditController& Editor) { return Editor.undo(); });
	if (!Undone)
	{
		RaiseError(FString::Printf(TEXT("cannot undo: %s"), *ToFString(Undone.error().message)));
		return {};
	}
	return Label;
}

FString UFloor101DocumentToolset::Redo()
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	if (!Subsystem->GetDocument().canRedo())
	{
		RaiseError(TEXT("nothing to redo."));
		return {};
	}
	const FString Label = ToFString(Subsystem->GetDocument().redoLabel());
	const f101::Status Redone = Subsystem->Edit([](f101::cad::EditController& Editor) { return Editor.redo(); });
	if (!Redone)
	{
		RaiseError(FString::Printf(TEXT("cannot redo: %s"), *ToFString(Redone.error().message)));
		return {};
	}
	return Label;
}

void UFloor101DocumentToolset::SaveProject(const FString& Path)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	const f101::Status Saved = Subsystem->SaveProject(Path);
	if (!Saved)
	{
		RaiseError(FString::Printf(TEXT("cannot save %s: %s"), *Path, *ToFString(Saved.error().message)));
	}
}

TArray<FFloor101WallInfo> UFloor101DocumentToolset::OpenProject(const FString& Path)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	const f101::Status Opened = Subsystem->OpenProject(Path);
	if (!Opened)
	{
		RaiseError(FString::Printf(TEXT("cannot open %s: %s"), *Path, *ToFString(Opened.error().message)));
		return {};
	}
	return AllWalls(*Subsystem);
}

void UFloor101DocumentToolset::ExportPlan(const FString& Path, const FString& LevelId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::LevelId Level;
	if (!Subsystem || !ParseLevelId(LevelId, Level))
	{
		return;
	}
	const f101::Status Exported = Subsystem->ExportPlan(Path, Level);
	if (!Exported)
	{
		RaiseError(FString::Printf(TEXT("cannot export the plan to %s: %s"), *Path, *ToFString(Exported.error().message)));
	}
}

FFloor101GltfExportInfo UFloor101DocumentToolset::ExportGltf(const FString& Path)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	const FString Target = Path.IsEmpty() ? Subsystem->GetDefaultGltfPath() : Path;
	const f101::Result<FFloor101GltfSummary> Exported = Subsystem->ExportGltf(Target);
	if (!Exported)
	{
		RaiseError(FString::Printf(TEXT("cannot export to %s: %s"), *Target, *ToFString(Exported.error().message)));
		return {};
	}
	FFloor101GltfExportInfo Info;
	Info.Path = FPaths::ConvertRelativePathToFull(Target);
	Info.Objects = Exported.value().Objects;
	Info.Materials = Exported.value().Materials;
	Info.Images = Exported.value().Images;
	Info.Triangles = Exported.value().Triangles;
	Info.Bytes = Exported.value().Bytes;
	Info.Warnings = Exported.value().Warnings;
	return Info;
}
