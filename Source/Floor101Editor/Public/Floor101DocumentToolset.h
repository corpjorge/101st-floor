#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "Floor101DocumentToolset.generated.h"

/**
 * A wall of the 101st Floor document. Plan coordinates in integer millimetres (X east, Y north).
 */
USTRUCT(BlueprintType)
struct FFloor101WallInfo
{
	GENERATED_BODY()

	/** Persistent wall id (UUID). */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	int64 StartXMm = 0;

	UPROPERTY()
	int64 StartYMm = 0;

	UPROPERTY()
	int64 EndXMm = 0;

	UPROPERTY()
	int64 EndYMm = 0;

	/** Centred on the axis. */
	UPROPERTY()
	int64 ThicknessMm = 0;

	UPROPERTY()
	int64 HeightMm = 0;

	/** Axis length, derived from the end points. */
	UPROPERTY()
	double LengthMm = 0.0;

	/** Transient actor rendering the wall in the target world (Unreal space: cm, Y mirrored). */
	UPROPERTY()
	TObjectPtr<AActor> Actor;
};

/**
 * A room: a region closed by walls. Derived from the walls, never stored.
 */
USTRUCT(BlueprintType)
struct FFloor101RoomInfo
{
	GENERATED_BODY()

	/** Stable while the room keeps its lowest-id wall: that wall's id, the side of it and which segment of it. */
	UPROPERTY()
	FString Key;

	/** Net floor area inside the walls, in square metres. */
	UPROPERTY()
	double AreaM2 = 0.0;

	/** Ids of the walls around it. */
	UPROPERTY()
	TArray<FString> WallIds;

	/** Transient actor of its floor slab in the target world. */
	UPROPERTY()
	TObjectPtr<AActor> FloorActor;
};

/**
 * Edits the walls of the 101st Floor document and reads the rooms they enclose: the play session's document while
 * Play-In-Editor runs, otherwise the editor world's. Every change is one undoable step and regenerates the Unreal
 * representation (transient actors). Also saves and opens project.json files. Dimensions are integer millimetres.
 */
UCLASS(BlueprintType, MinimalAPI)
class UFloor101DocumentToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Returns the document's walls in creation order.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101WallInfo> GetWalls();

	/**
	 * Adds a straight wall from start to end.
	 * @return The new wall.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101WallInfo AddWall(int64 StartXMm, int64 StartYMm, int64 EndXMm, int64 EndYMm, int64 ThicknessMm, int64 HeightMm);

	/**
	 * Moves the end point of a wall; its geometry is regenerated, never scaled.
	 * @return The updated wall.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101WallInfo SetWallEnd(const FString& WallId, int64 EndXMm, int64 EndYMm);

	/**
	 * Changes a wall's length, keeping its start and direction. Off the axes the end is rounded to whole millimetres.
	 * @return The updated wall.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101WallInfo SetWallLength(const FString& WallId, int64 LengthMm);

	/**
	 * @return The updated wall.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101WallInfo SetWallThickness(const FString& WallId, int64 ThicknessMm);

	/**
	 * @return The updated wall.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101WallInfo SetWallHeight(const FString& WallId, int64 HeightMm);

	/**
	 * Moves a corner: every wall end at (FromXMm, FromYMm) goes to (ToXMm, ToYMm), as one undoable step.
	 * @return The moved walls.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101WallInfo> MoveCorner(int64 FromXMm, int64 FromYMm, int64 ToXMm, int64 ToYMm);

	/**
	 * Returns the rooms the walls enclose, with their net floor areas.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101RoomInfo> GetRooms();

	/**
	 * Removes a wall and its actor.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void RemoveWall(const FString& WallId);

	/**
	 * Undoes the last change.
	 * @return What was undone, e.g. "Create wall".
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FString Undo();

	/**
	 * Redoes the last undone change.
	 * @return What was redone, e.g. "Create wall".
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FString Redo();

	/**
	 * Saves the document as project.json, creating missing folders.
	 * @param Path Absolute file path.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void SaveProject(const FString& Path);

	/**
	 * Replaces the document with a project.json file. Clears the undo history.
	 * @param Path Absolute file path.
	 * @return The opened walls.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101WallInfo> OpenProject(const FString& Path);
};
