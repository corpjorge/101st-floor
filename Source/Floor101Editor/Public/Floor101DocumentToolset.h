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

/** How a door or window opens. */
UENUM(BlueprintType)
enum class EFloor101OpeningOperation : uint8
{
	/** One leaf or sash on hinges at one jamb. */
	Hinged,
	/** Two leaves or sashes hinged at both jambs. */
	DoubleHinged,
	/** Doors: a leaf sliding into a pocket in the wall beside it, as long as the door is wide. Windows: two sashes on two tracks. */
	Sliding,
	/** Windows only: does not open. */
	Fixed,
};

/** An end of an opening along its wall. */
UENUM(BlueprintType)
enum class EFloor101OpeningEnd : uint8
{
	/** Towards the wall start. */
	Start,
	End,
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

	/** Palette group: "paint", "tiles", "wood", "stone", "textile", "joinery", "fabric", "glass" or "services". */
	UPROPERTY()
	FString Category;

	UPROPERTY()
	bool bForWalls = false;

	UPROPERTY()
	bool bForFloors = false;

	UPROPERTY()
	bool bForCeilings = false;

	/** Frames and leaves of doors and windows (SetOpeningFinish). */
	UPROPERTY()
	bool bForCarpentry = false;

	/** The main parts of furniture (SetFurnitureFinish). */
	UPROPERTY()
	bool bForFurniture = false;

	/** The colour of a kind of building-services run (not chosen: each kind has its own). */
	UPROPERTY()
	bool bForServices = false;

	/** Base colour (sRGB). */
	UPROPERTY()
	FColor Color = FColor::White;

	/** 0 mirror .. 1 matte. */
	UPROPERTY()
	float Roughness = 0.0f;

	/** 1 opaque; below 1 see-through (glass). */
	UPROPERTY()
	float Opacity = 1.0f;
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

	/** Finish id of its frame and leaf; empty: the default. */
	UPROPERTY()
	FString Finish;

	UPROPERTY()
	EFloor101OpeningOperation Operation = EFloor101OpeningOperation::Hinged;

	/** Hinged: the jamb with the hinges. Sliding: the end the moving leaf goes to (a door's pocket is there). */
	UPROPERTY()
	EFloor101OpeningEnd Hinge = EFloor101OpeningEnd::Start;

	/** Hinged and double: the wall face the leaves swing towards. Sliding window: the face of the moving sash. */
	UPROPERTY()
	EFloor101WallSide OpensTo = EFloor101WallSide::Left;

	/** How open it is shown: 100 = hinged at 90 degrees, sliding fully open; always 0 when fixed. */
	UPROPERTY(meta = (ClampMin = "0", ClampMax = "100"))
	int32 OpenPercent = 0;
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
 * A furniture type of the catalogue: its default size and the sizes it allows. Millimetres.
 */
USTRUCT(BlueprintType)
struct FFloor101FurnitureTypeInfo
{
	GENERATED_BODY()

	/** Catalogue id, e.g. "sofa". */
	UPROPERTY()
	FString Id;

	/** Palette group: "beds", "seating", "sofas", "tables", "storage", "kitchen", "bathroom", "lighting" or "decor". */
	UPROPERTY()
	FString Category;

	/** Along its front. */
	UPROPERTY()
	int64 WidthMm = 0;

	/** Front to back. */
	UPROPERTY()
	int64 DepthMm = 0;

	UPROPERTY()
	int64 HeightMm = 0;

	UPROPERTY()
	int64 MinWidthMm = 0;

	UPROPERTY()
	int64 MaxWidthMm = 0;

	UPROPERTY()
	int64 MinDepthMm = 0;

	UPROPERTY()
	int64 MaxDepthMm = 0;

	UPROPERTY()
	int64 MinHeightMm = 0;

	UPROPERTY()
	int64 MaxHeightMm = 0;

	/** Placed with the mouse near a wall, its back goes against the wall. */
	UPROPERTY()
	bool bAgainstWall = false;

	/** Finish id of its main parts unless the piece has its own. */
	UPROPERTY()
	FString BodyFinish;
};

/**
 * A piece of furniture: a width x depth footprint centred on (XMm, YMm), relative to its level's floor.
 */
USTRUCT(BlueprintType)
struct FFloor101FurnitureInfo
{
	GENERATED_BODY()

	/** Persistent id (UUID). */
	UPROPERTY()
	FString Id;

	/** Furniture type id (GetFurnitureTypes). */
	UPROPERTY()
	FString Type;

	UPROPERTY()
	FString LevelId;

	UPROPERTY()
	int64 XMm = 0;

	UPROPERTY()
	int64 YMm = 0;

	/** Counter-clockwise, in hundredths of a degree. 0: its front faces south (-Y) and its back north. */
	UPROPERTY(meta = (ClampMin = "0", ClampMax = "35999"))
	int32 RotationCentidegrees = 0;

	UPROPERTY()
	int64 WidthMm = 0;

	UPROPERTY()
	int64 DepthMm = 0;

	UPROPERTY()
	int64 HeightMm = 0;

	/** Finish id of its main parts; empty: its type's. */
	UPROPERTY()
	FString Finish;

	/** Transient actor rendering it in the target world, or null while its level is hidden. */
	UPROPERTY()
	TObjectPtr<AActor> Actor;
};

/** A column's cross-section. */
UENUM(BlueprintType)
enum class EFloor101ColumnShape : uint8
{
	Rectangle,
	/** A circle: its width is its diameter. */
	Round,
};

/**
 * A column: a structural post standing on its level's floor, as tall as the level, centred on (XMm, YMm).
 */
USTRUCT(BlueprintType)
struct FFloor101ColumnInfo
{
	GENERATED_BODY()

	/** Persistent id (UUID). */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString LevelId;

	UPROPERTY()
	int64 XMm = 0;

	UPROPERTY()
	int64 YMm = 0;

	UPROPERTY()
	EFloor101ColumnShape Shape = EFloor101ColumnShape::Rectangle;

	/** Along its own X before turning; a round column: its diameter. */
	UPROPERTY(meta = (ClampMin = "100", ClampMax = "3000"))
	int64 WidthMm = 0;

	/** A round column: its diameter too. */
	UPROPERTY(meta = (ClampMin = "100", ClampMax = "3000"))
	int64 DepthMm = 0;

	/** Counter-clockwise, in hundredths of a degree; a round column: 0. */
	UPROPERTY(meta = (ClampMin = "0", ClampMax = "35999"))
	int32 RotationCentidegrees = 0;

	/** Finish id (one that allows walls); empty: exposed concrete. */
	UPROPERTY()
	FString Finish;

	/** Transient actor rendering it in the target world, or null while its level is hidden. */
	UPROPERTY()
	TObjectPtr<AActor> Actor;
};

/** A kind of building-services run: its colour and pipe diameter come with it. */
UENUM(BlueprintType)
enum class EFloor101ServiceKind : uint8
{
	/** Electrical conduit: a thin green tube, 25 mm. */
	Electrical,
	/** Clean water: a thin white pipe, 25 mm. */
	Water,
	/** Sewage: a thick yellow pipe, 110 mm. */
	Sewage,
	/** Gas: a thin gold pipe, 22 mm. */
	Gas,
};

/** A point of a run: its plan position, and the height of the pipe's axis over its level's floor (negative: under it). */
USTRUCT(BlueprintType)
struct FFloor101ServicePoint
{
	GENERATED_BODY()

	UPROPERTY()
	int64 XMm = 0;

	UPROPERTY()
	int64 YMm = 0;

	UPROPERTY(meta = (ClampMin = "-5000", ClampMax = "50000"))
	int64 ZMm = 0;
};

/**
 * A run of a building service on one level: a pipe through its points.
 */
USTRUCT(BlueprintType)
struct FFloor101ServiceRunInfo
{
	GENERATED_BODY()

	/** Persistent id (UUID). */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString LevelId;

	UPROPERTY()
	EFloor101ServiceKind Kind = EFloor101ServiceKind::Electrical;

	/** Two or more; consecutive points differ. The same plan position twice in a row rises or drops on the spot. */
	UPROPERTY()
	TArray<FFloor101ServicePoint> Points;

	/** Along every segment, vertical ones included. */
	UPROPERTY()
	double LengthMm = 0.0;

	UPROPERTY()
	int64 DiameterMm = 0;

	/** Transient actor rendering it in the target world; null while the services layer or its level is hidden. */
	UPROPERTY()
	TObjectPtr<AActor> Actor;
};

UENUM(BlueprintType)
enum class EFloor101AssetKind : uint8
{
	/** A PNG or JPEG image, used as a finish ("asset:<uuid>"). */
	Texture,
	/** A glTF 2.0 (.gltf, .glb) or OBJ model, used as a furniture type ("asset:<uuid>"). */
	Model,
};

/** The unit a model's coordinates are in. */
UENUM(BlueprintType)
enum class EFloor101ModelUnit : uint8
{
	Meter,
	Centimeter,
	Millimeter,
	Inch,
};

/**
 * An imported asset of the project: its files live in the project's folder under assets/<uuid>/.
 */
USTRUCT(BlueprintType)
struct FFloor101AssetInfo
{
	GENERATED_BODY()

	/** Persistent id (UUID). */
	UPROPERTY()
	FString Id;

	/** "asset:<uuid>": the finish id of a texture (SetWallFinish, SetRoomFinish...) or the furniture type of a model. */
	UPROPERTY()
	FString Ref;

	UPROPERTY()
	EFloor101AssetKind Kind = EFloor101AssetKind::Texture;

	UPROPERTY()
	FString Name;

	/** Relative to the project's folder. */
	UPROPERTY()
	FString File;

	/** The library item it was added from (AddFromLibrary); empty if it was imported. */
	UPROPERTY()
	FString Library;

	/** Textures: the length of wall or floor one image covers, along and across. */
	UPROPERTY()
	int64 TextureWidthMm = 0;

	UPROPERTY()
	int64 TextureHeightMm = 0;

	UPROPERTY(meta = (ClampMin = "0", ClampMax = "100"))
	int32 RoughnessPercent = 0;

	UPROPERTY()
	int32 PixelWidth = 0;

	UPROPERTY()
	int32 PixelHeight = 0;

	/** Models: how the file is read. */
	UPROPERTY()
	EFloor101ModelUnit Unit = EFloor101ModelUnit::Meter;

	/** True: Z up; false: Y up (glTF and most OBJ files). */
	UPROPERTY()
	bool bZUp = false;

	/** Quarter turns (counter-clockwise from above) that put the model's front towards -Y. */
	UPROPERTY(meta = (ClampMin = "0", ClampMax = "3"))
	int32 FrontQuarterTurns = 0;

	/** Placed with the mouse near a wall, its back goes against it. */
	UPROPERTY()
	bool bAgainstWall = false;

	/** The model's size read with these settings, in millimetres (width along its front, depth, height). */
	UPROPERTY()
	int64 WidthMm = 0;

	UPROPERTY()
	int64 DepthMm = 0;

	UPROPERTY()
	int64 HeightMm = 0;

	UPROPERTY()
	int64 Triangles = 0;

	/** Why the model cannot be shown (its pieces then show as boxes); empty if it can. */
	UPROPERTY()
	FString Problem;
};

/**
 * What an export for Twinmotion (ExportGltf) wrote.
 */
USTRUCT(BlueprintType)
struct FFloor101GltfExportInfo
{
	GENERATED_BODY()

	/** Absolute path of the .glb file. */
	UPROPERTY()
	FString Path;

	/** Walls, floors, slabs, doors and windows, and pieces of furniture. */
	UPROPERTY()
	int32 Objects = 0;

	UPROPERTY()
	int32 Materials = 0;

	/** Images embedded in the file (textures of the finishes and of the models). */
	UPROPERTY()
	int32 Images = 0;

	UPROPERTY()
	int64 Triangles = 0;

	/** Size of the file. */
	UPROPERTY()
	int64 Bytes = 0;

	/** E.g. an image that could not be read: its material keeps its colour. */
	UPROPERTY()
	TArray<FString> Warnings;
};

/**
 * An item of the built-in library shipped with 101st Floor: a furniture model or a photographic texture, added to a
 * project as an asset when used.
 */
USTRUCT(BlueprintType)
struct FFloor101LibraryItem
{
	GENERATED_BODY()

	/** Library id, e.g. "chair-windsor". */
	UPROPERTY()
	FString Id;

	UPROPERTY()
	EFloor101AssetKind Kind = EFloor101AssetKind::Model;

	UPROPERTY()
	FString Name;

	/** Models: a furniture category (FFloor101FurnitureTypeInfo::Category). Textures: a finish category (FFloor101FinishInfo::Category). */
	UPROPERTY()
	FString Category;

	/** Textures: the length of wall or floor one image covers, along and across. */
	UPROPERTY()
	int64 TextureWidthMm = 0;

	UPROPERTY()
	int64 TextureHeightMm = 0;

	UPROPERTY()
	bool bForWalls = false;

	UPROPERTY()
	bool bForFloors = false;

	UPROPERTY()
	bool bForCeilings = false;

	/** Author and licence. */
	UPROPERTY()
	FString Credit;
};

/**
 * Edits the levels, walls, doors, windows, furniture, columns, building-services runs and finishes of the 101st Floor
 * document and reads the rooms the walls enclose: the play session's document while Play-In-Editor runs, otherwise the
 * editor world's. Every change is one undoable step and regenerates the Unreal representation (transient actors). Also
 * saves and opens project.json files and exports plans. Dimensions are integer millimetres; plan coordinates are
 * relative to their level's floor.
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
	 * Removes a level with all its walls and furniture; the levels above move down. The last level cannot be removed.
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
	 * Adds a door or window to a wall. It must fit the wall, keep clear of the walls joined to it and not overlap another
	 * opening; doors have sill 0. It is hinged at its start, opens towards the left face and is shown closed.
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

	/**
	 * Sets the finish of a door or window's carpentry (frame and leaf).
	 * @param FinishId A catalogue id that allows carpentry; empty restores the default.
	 * @return The updated opening.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101OpeningInfo SetOpeningFinish(const FString& WallId, const FString& OpeningId, const FString& FinishId);

	/**
	 * Sets how a door or window opens. Doors cannot be Fixed; a fixed window must be 0 % open; a sliding door needs its
	 * pocket clear inside the wall.
	 * @param Hinge See FFloor101OpeningInfo::Hinge.
	 * @param OpensTo See FFloor101OpeningInfo::OpensTo.
	 * @param OpenPercent How open it is shown, 0 to 100.
	 * @return The updated opening.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101OpeningInfo SetOpeningOperation(const FString& WallId, const FString& OpeningId, EFloor101OpeningOperation Operation,
		EFloor101OpeningEnd Hinge, EFloor101WallSide OpensTo, int32 OpenPercent);

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
	 * Returns the furniture catalogue.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101FurnitureTypeInfo> GetFurnitureTypes();

	/**
	 * Returns every piece of furniture in creation order.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101FurnitureInfo> GetFurniture();

	/**
	 * Adds a piece of furniture exactly where given (no snapping).
	 * @param LevelId Empty for the lowest level.
	 * @param RotationCentidegrees See FFloor101FurnitureInfo::RotationCentidegrees.
	 * @param WidthMm 0 for the type's default, like DepthMm and HeightMm.
	 * @return The new piece.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101FurnitureInfo AddFurniture(const FString& LevelId, const FString& Type, int64 XMm, int64 YMm,
		int32 RotationCentidegrees, int64 WidthMm, int64 DepthMm, int64 HeightMm);

	/**
	 * Moves, turns or resizes a piece of furniture (its type, level and finish stay).
	 * @return The updated piece.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101FurnitureInfo SetFurniture(const FString& FurnitureId, int64 XMm, int64 YMm, int32 RotationCentidegrees,
		int64 WidthMm, int64 DepthMm, int64 HeightMm);

	/**
	 * Sets the finish of a piece's main parts.
	 * @param FinishId A catalogue id that allows furniture; empty restores its type's.
	 * @return The updated piece.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101FurnitureInfo SetFurnitureFinish(const FString& FurnitureId, const FString& FinishId);

	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void RemoveFurniture(const FString& FurnitureId);

	/**
	 * Returns every column in creation order.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101ColumnInfo> GetColumns();

	/**
	 * Adds a column exactly where given (no snapping), in exposed concrete.
	 * @param LevelId Empty for the lowest level.
	 * @param WidthMm 0 for 300 mm.
	 * @param DepthMm 0 for the width. Ignored for a round column.
	 * @param RotationCentidegrees Ignored for a round column.
	 * @return The new column.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101ColumnInfo AddColumn(const FString& LevelId, EFloor101ColumnShape Shape, int64 XMm, int64 YMm, int64 WidthMm,
		int64 DepthMm, int32 RotationCentidegrees);

	/**
	 * Moves, reshapes, resizes or turns a column (its level and finish stay).
	 * @param DepthMm Ignored for a round column.
	 * @param RotationCentidegrees Ignored for a round column.
	 * @return The updated column.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101ColumnInfo SetColumn(const FString& ColumnId, EFloor101ColumnShape Shape, int64 XMm, int64 YMm, int64 WidthMm,
		int64 DepthMm, int32 RotationCentidegrees);

	/**
	 * Sets a column's finish.
	 * @param FinishId A catalogue id that allows walls, or an imported texture; empty restores concrete.
	 * @return The updated column.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101ColumnInfo SetColumnFinish(const FString& ColumnId, const FString& FinishId);

	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void RemoveColumn(const FString& ColumnId);

	/**
	 * Returns every run of the building services in creation order.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101ServiceRunInfo> GetServiceRuns();

	/**
	 * Adds a run of a building service through the given points, exactly as given.
	 * @param LevelId Empty for the lowest level.
	 * @return The new run.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101ServiceRunInfo AddServiceRun(const FString& LevelId, EFloor101ServiceKind Kind, const TArray<FFloor101ServicePoint>& Points);

	/**
	 * Changes a run's kind and points (its level stays).
	 * @return The updated run.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101ServiceRunInfo SetServiceRun(const FString& RunId, EFloor101ServiceKind Kind, const TArray<FFloor101ServicePoint>& Points);

	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void RemoveServiceRun(const FString& RunId);

	/**
	 * Shows or hides the building-services layer of the view: shown, the runs appear and the walls, floors, slabs, doors
	 * and windows turn see-through; hidden, the runs are left out. Columns always show.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void SetServicesView(bool bShown);

	/**
	 * Returns the project's imported assets (textures and models) in import order.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101AssetInfo> GetAssets();

	/**
	 * Imports a texture (.png, .jpg) or a model (.glb, .gltf, .obj with its .mtl and textures) into the project: its
	 * files are copied into the project's folder. Use its Ref as a finish or a furniture type.
	 * @param Path Absolute path of the file to import.
	 * @return The new asset.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101AssetInfo ImportAsset(const FString& Path);

	/**
	 * Renames a texture and sets the length one image covers and its roughness.
	 * @return The updated asset.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101AssetInfo SetTextureAsset(const FString& AssetId, const FString& Name, int64 WidthMm, int64 HeightMm,
		int32 RoughnessPercent);

	/**
	 * Renames a model and sets how its file is read and placed; its size is measured again.
	 * @param bZUp True for Z up, false for Y up.
	 * @param FrontQuarterTurns 0 to 3.
	 * @return The updated asset.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101AssetInfo SetModelAsset(const FString& AssetId, const FString& Name, EFloor101ModelUnit Unit, bool bZUp,
		int32 FrontQuarterTurns, bool bAgainstWall);

	/**
	 * Removes an asset no surface or piece of furniture uses. Its files stay on disk.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void RemoveAsset(const FString& AssetId);

	/**
	 * Returns the built-in library: furniture models, then textures.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static TArray<FFloor101LibraryItem> GetLibrary();

	/**
	 * Adds a library item to the project as an asset (its files copied into the project's folder), or returns the one
	 * added before. Use its Ref as a furniture type or a finish.
	 * @param ItemId A GetLibrary id.
	 * @return The project's asset.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101AssetInfo AddFromLibrary(const FString& ItemId);

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

	/**
	 * Exports the floor plan of a level: cut walls and columns, doors and windows, dimensions in metres, room names and
	 * areas, and the building-services runs while their layer is shown (SetServicesView).
	 * @param Path Absolute path ending in .svg or .png (PNG needs a renderer).
	 * @param LevelId Empty: the level being edited.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static void ExportPlan(const FString& Path, const FString& LevelId);

	/**
	 * Exports the whole project for Twinmotion as binary glTF: every level with its slabs, doors and windows, furniture,
	 * columns and building-services runs, the materials and their images; metres, Y up.
	 * @param Path Absolute path ending in .glb; empty for "<project file name>.glb" next to the project.
	 * @return What was written.
	 */
	UFUNCTION(meta = (AICallable), Category = "Floor101DocumentToolset")
	static FFloor101GltfExportInfo ExportGltf(const FString& Path);
};
