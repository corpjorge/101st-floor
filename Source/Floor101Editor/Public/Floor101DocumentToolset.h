#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "Floor101DocumentToolset.generated.h"

UENUM(BlueprintType)
enum class EFloor101OpeningKind : uint8
{
	Door,
	Window,
};

UENUM(BlueprintType)
enum class EFloor101WallSide : uint8
{
	/** Left of the axis, looking from its start to its end. */
	Left,
	Right,
};

/**
 * A finish of the catalogue: what a wall face, a floor or a ceiling can be covered with.
 */
USTRUCT(BlueprintType)
struct FFloor101FinishInfo
{
	GENERATED_BODY()

	/** Catalogue id, e.g. "brick-red". */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	bool bForWalls = false;

	UPROPERTY()
	bool bForFloors = false;

	UPROPERTY()
	bool bForCeilings = false;

	/** Base colour (sRGB). */
	UPROPERTY()
	FColor Color = FColor::White;

	/** 0 mirror .. 1 matte. */
	UPROPERTY()
	float Roughness = 0.0f;
};

/**
 * A level (floor) of the building, bottom to top. Integer millimetres.
 */
USTRUCT(BlueprintType)
struct FFloor101LevelInfo
{
	GENERATED_BODY()

	/** Persistent level id (UUID). */
	UPROPERTY()
	FString Id;

	/** Empty: the UI shows "Planta baja", "Planta 1"... */
	UPROPERTY()
	FString Name;

	/** 0 for the lowest level. */
	UPROPERTY()
	int32 Index = 0;

	/** Floor height above the lowest level's floor; derived from the heights below. */
	UPROPERTY()
	int64 ElevationMm = 0;

	/** Floor to floor. */
	UPROPERTY()
	int64 HeightMm = 0;

	/** Slab at the top of the level: the ceiling of its rooms and the floor of the next level. */
	UPROPERTY()
	int64 SlabThicknessMm = 0;
};

/**
 * A door or window: a rectangular hole through its wall. Integer millimetres.
 */
USTRUCT(BlueprintType)
struct FFloor101OpeningInfo
{
	GENERATED_BODY()

	/** Persistent opening id (UUID). */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	EFloor101OpeningKind Kind = EFloor101OpeningKind::Door;

	/** From the wall start to the opening's near edge, along the wall. */
	UPROPERTY()
	int64 OffsetMm = 0;

	UPROPERTY()
	int64 WidthMm = 0;

	UPROPERTY()
	int64 HeightMm = 0;

	/** From the floor to the bottom of the opening; 0 for doors. */
	UPROPERTY()
	int64 SillMm = 0;
};

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

	/** Level it stands on; its coordinates are relative to that level's floor. */
	UPROPERTY()
	FString LevelId;

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

	/** Finish ids of its faces (left and right of the axis); empty: the default. */
	UPROPERTY()
	FString LeftFinish;

	UPROPERTY()
	FString RightFinish;

	/** Its doors and windows, by offset. */
	UPROPERTY()
	TArray<FFloor101OpeningInfo> Openings;

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

	/** Level it is on. */
	UPROPERTY()
	FString LevelId;

	/** Net floor area inside the walls, in square metres. */
	UPROPERTY()
	double AreaM2 = 0.0;

	/** Finish ids of its floor and ceiling (underside of its slab); empty: the default. */
	UPROPERTY()
	FString FloorFinish;

	UPROPERTY()
	FString CeilingFinish;

	/** Ids of the walls around it. */
	UPROPERTY()
	TArray<FString> WallIds;

	/** Transient actor of its floor slab in the target world. */
	UPROPERTY()
	TObjectPtr<AActor> FloorActor;
};

/**
 * Edits the levels, walls, doors, windows and finishes of the 101st Floor document and reads the rooms they enclose:
 * the play session's document while Play-In-Editor runs, otherwise the editor world's. Every change is one undoable
 * step and regenerates the Unreal representation (transient actors). Also saves and opens project.json files.
 * Dimensions are integer millimetres; wall coordinates are relative to their level's floor.
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
	 * Returns the levels, bottom to top.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101LevelInfo> GetLevels();

	/**
	 * Adds a level on top of the others.
	 * @param Name Empty for the default name.
	 * @return The new level.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101LevelInfo AddLevel(const FString& Name, int64 HeightMm, int64 SlabThicknessMm);

	/**
	 * Renames or resizes a level; the levels above move with its height. Walls keep their own heights.
	 * @return The updated level.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101LevelInfo SetLevel(const FString& LevelId, const FString& Name, int64 HeightMm, int64 SlabThicknessMm);

	/**
	 * Removes a level and all its walls; the levels above move down. The last level cannot be removed.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void RemoveLevel(const FString& LevelId);

	/**
	 * Chooses what the target world shows, like the level controls of the UI: the edited level and the ones below it
	 * (dimmed, the edited one without its slabs), or every level with its slabs. Walls on hidden levels have no actor.
	 * @param LevelId Level to edit; empty for the lowest level.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void SetLevelView(const FString& LevelId, bool bShowAllLevels);

	/**
	 * Adds a straight wall from start to end.
	 * @param LevelId Level it stands on; empty for the lowest level.
	 * @return The new wall.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101WallInfo AddWall(const FString& LevelId, int64 StartXMm, int64 StartYMm, int64 EndXMm, int64 EndYMm,
		int64 ThicknessMm, int64 HeightMm);

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
	 * Moves a corner: every wall end of the level at (FromXMm, FromYMm) goes to (ToXMm, ToYMm), as one undoable step.
	 * @param LevelId Empty for the lowest level.
	 * @return The moved walls.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101WallInfo> MoveCorner(const FString& LevelId, int64 FromXMm, int64 FromYMm, int64 ToXMm, int64 ToYMm);

	/**
	 * Returns the rooms the walls of each level enclose, with their net floor areas.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101RoomInfo> GetRooms();

	/**
	 * Adds a door or window to a wall. It must fit the wall and not overlap another opening; doors have sill 0.
	 * @return The new opening.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101OpeningInfo AddOpening(const FString& WallId, EFloor101OpeningKind Kind, int64 OffsetMm, int64 WidthMm,
		int64 HeightMm, int64 SillMm);

	/**
	 * Moves or resizes an opening (its kind stays).
	 * @return The updated opening.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101OpeningInfo SetOpening(const FString& WallId, const FString& OpeningId, int64 OffsetMm, int64 WidthMm,
		int64 HeightMm, int64 SillMm);

	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void RemoveOpening(const FString& WallId, const FString& OpeningId);

	/**
	 * Returns the finish catalogue.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101FinishInfo> GetFinishes();

	/**
	 * Sets the finish of one face of a wall.
	 * @param FinishId A catalogue id that allows walls; empty restores the default.
	 * @return The updated wall.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101WallInfo SetWallFinish(const FString& WallId, EFloor101WallSide Side, const FString& FinishId);

	/**
	 * Sets the floor and ceiling finishes of a room (GetRooms Key). They stay with the room while its key does.
	 * @param FloorFinishId Empty for the default.
	 * @param CeilingFinishId Empty for the default.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void SetRoomFinish(const FString& RoomKey, const FString& FloorFinishId, const FString& CeilingFinishId);

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
