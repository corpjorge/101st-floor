#include "Floor101DocumentToolset.h"

#include "Editor.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Geometry/Rooms.h"
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

	FFloor101WallInfo ToInfo(const f101::cad::Wall& Wall, const UFloor101DocumentSubsystem& Subsystem)
	{
		FFloor101WallInfo Info;
		Info.Id = ToFString(f101::cad::toString(Wall.id));
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

TArray<FFloor101WallInfo> UFloor101DocumentToolset::GetWalls()
{
	const UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	return Subsystem ? AllWalls(*Subsystem) : TArray<FFloor101WallInfo>();
}

FFloor101WallInfo UFloor101DocumentToolset::AddWall(int64 StartXMm, int64 StartYMm, int64 EndXMm, int64 EndYMm, int64 ThicknessMm, int64 HeightMm)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	const f101::Result<f101::cad::WallId> Added = Subsystem->GetMutableDocument().addWall(
		{StartXMm, StartYMm}, {EndXMm, EndYMm}, ThicknessMm, HeightMm);
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

TArray<FFloor101WallInfo> UFloor101DocumentToolset::MoveCorner(int64 FromXMm, int64 FromYMm, int64 ToXMm, int64 ToYMm)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return {};
	}
	const f101::Result<f101::cad::Transaction> Moved =
		Subsystem->GetMutableDocument().execute(f101::cad::MoveJointCommand{{FromXMm, FromYMm}, {ToXMm, ToYMm}});
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
	for (const f101::geometry::Room& Room : f101::geometry::detectRooms(Subsystem->GetDocument().project()))
	{
		FFloor101RoomInfo Info;
		Info.Key = ToFString(Room.key);
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
