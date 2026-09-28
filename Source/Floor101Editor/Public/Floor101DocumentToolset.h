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

	/** Transient actor rendering the wall in the editor world (Unreal space: cm, Y mirrored). */
	UPROPERTY()
	TObjectPtr<AActor> Actor;
};

/**
 * Edits the walls of the 101st Floor document in the editor world, the project's source of truth. Changes regenerate
 * the Unreal representation (transient actors) automatically. Dimensions are integer millimetres.
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
	 * Removes a wall and its actor.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void RemoveWall(const FString& WallId);
};
