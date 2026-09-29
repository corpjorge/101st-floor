#include "Floor101DocumentToolset.h"

#include "Editor.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Scene/SceneCompiler.h"
#include "Floor101DocumentSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"

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
		return Info;
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
	Changed.offsetMm = OffsetMm;
	Changed.widthMm = WidthMm;
	Changed.heightMm = HeightMm;
	Changed.sillMm = SillMm;
	const f101::Result<f101::cad::Transaction> Applied = Subsystem->GetMutableDocument().execute(f101::cad::ChangeOpeningCommand{Wall, Changed});
	if (!Applied)
	{
		RaiseError(FString::Printf(TEXT("cannot change opening %s: %s"), *OpeningId, *ToFString(Applied.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findWall(Wall)->findOpening(Id));
}

FFloor101OpeningInfo UFloor101DocumentToolset::SetOpeningFinish(const FString& WallId, const FString& OpeningId, const FString& FinishId)
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
	Changed.finish = TCHAR_TO_UTF8(*FinishId);
	const f101::Result<f101::cad::Transaction> Applied = Subsystem->GetMutableDocument().execute(f101::cad::ChangeOpeningCommand{Wall, Changed});
	if (!Applied)
	{
		RaiseError(FString::Printf(TEXT("cannot change opening %s: %s"), *OpeningId, *ToFString(Applied.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findWall(Wall)->findOpening(Id));
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
		Info.bForWalls = Finish.allows(f101::cad::Surface::Wall);
		Info.bForFloors = Finish.allows(f101::cad::Surface::Floor);
		Info.bForCeilings = Finish.allows(f101::cad::Surface::Ceiling);
		Info.bForCarpentry = Finish.allows(f101::cad::Surface::Carpentry);
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
