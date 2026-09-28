#include "Floor101DocumentToolset.h"

#include "Editor.h"
#include "F101/Cad/Document.h"
#include "Floor101DocumentSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	void RaiseError(const FString& Message)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("Floor101DocumentToolset: %s"), *Message));
	}

	FString ToFString(const std::string& Text)
	{
		return UTF8_TO_TCHAR(Text.c_str());
	}

	UFloor101DocumentSubsystem* GetSubsystem()
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		UFloor101DocumentSubsystem* Subsystem = World ? World->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
		if (!Subsystem)
		{
			RaiseError(TEXT("the editor world has no 101st Floor document."));
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
}

TArray<FFloor101WallInfo> UFloor101DocumentToolset::GetWalls()
{
	TArray<FFloor101WallInfo> Walls;
	if (const UFloor101DocumentSubsystem* Subsystem = GetSubsystem())
	{
		for (const f101::cad::Wall& Wall : Subsystem->GetDocument().project().walls)
		{
			Walls.Add(ToInfo(Wall, *Subsystem));
		}
	}
	return Walls;
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
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::WallId Id;
	if (!Subsystem || !ParseWallId(WallId, Id))
	{
		return {};
	}
	const f101::Status Changed = Subsystem->GetMutableDocument().setWallEnd(Id, {EndXMm, EndYMm});
	if (!Changed)
	{
		RaiseError(FString::Printf(TEXT("cannot move the end of wall %s: %s"), *WallId, *ToFString(Changed.error().message)));
		return {};
	}
	return ToInfo(*Subsystem->GetDocument().project().findWall(Id), *Subsystem);
}

void UFloor101DocumentToolset::RemoveWall(const FString& WallId)
{
	UFloor101DocumentSubsystem* Subsystem = GetSubsystem();
	f101::cad::WallId Id;
	if (!Subsystem || !ParseWallId(WallId, Id))
	{
		return;
	}
	const f101::Status Removed = Subsystem->GetMutableDocument().removeWall(Id);
	if (!Removed)
	{
		RaiseError(FString::Printf(TEXT("cannot remove wall %s: %s"), *WallId, *ToFString(Removed.error().message)));
	}
}
