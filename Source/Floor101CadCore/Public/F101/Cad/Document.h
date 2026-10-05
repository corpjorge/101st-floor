#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Project.h"
#include "F101/Cad/Result.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace f101::cad
{
	enum class WallEndpoint
	{
		Start,
		End,
	};

	// Editing commands (master prompt §22). Each one is validated and applied as one undoable transaction.
	// Commands that name a level accept a nil level for the first one (the ground floor).
	struct CreateWallCommand
	{
		Point2D start;
		Point2D end;
		std::int64_t thicknessMm = 0;
		std::int64_t heightMm = 0;
		LevelId level;
	};

	struct DeleteWallCommand
	{
		WallId wall;
	};

	struct MoveWallEndpointCommand
	{
		WallId wall;
		WallEndpoint endpoint = WallEndpoint::End;
		Point2D position;
	};

	// Moves every wall end at `from` on `level` to `to`, so a corner shared by several walls moves as one (walls stacked on
	// other levels stay). Rejected if no wall of the level ends at `from` or if a moved wall would become invalid.
	struct MoveJointCommand
	{
		Point2D from;
		Point2D to;
		LevelId level;
	};

	// Keeps the start point and the direction; the new end is quantized to integer millimetres, so on a diagonal wall
	// the resulting length can differ from the requested one by less than 0.5 mm (see pointAtDistance).
	struct ChangeWallLengthCommand
	{
		WallId wall;
		std::int64_t lengthMm = 0;
	};

	struct ChangeWallThicknessCommand
	{
		WallId wall;
		std::int64_t thicknessMm = 0;
	};

	struct ChangeWallHeightCommand
	{
		WallId wall;
		std::int64_t heightMm = 0;
	};

	// Adds a door or window to a wall; a new id is assigned (the given one is ignored).
	struct AddOpeningCommand
	{
		WallId wall;
		Opening opening;
	};

	// Replaces the opening with the same id: kind, position and size.
	struct ChangeOpeningCommand
	{
		WallId wall;
		Opening opening;
	};

	struct DeleteOpeningCommand
	{
		WallId wall;
		OpeningId opening;
	};

	// Sets the finish of one face of a wall (ADR-0012); empty restores the default.
	struct SetWallFinishCommand
	{
		WallId wall;
		WallSide side = WallSide::Left;
		FinishId finish;
	};

	// Sets the floor and ceiling finishes of a room, by room key; both empty removes the room's own finishes.
	struct SetRoomFinishCommand
	{
		std::string room;
		FinishId floor;
		FinishId ceiling;
	};

	// Empty restores the automatic display name.
	struct SetRoomNameCommand
	{
		std::string room;
		std::string name;
	};

	struct SetRoomBaseboardCommand
	{
		RoomBaseboard baseboard;
	};

	// Nil id adds a manual dimension with a generated UUID; existing id replaces an annotation/automatic override.
	struct SetDimensionCommand { Dimension dimension; };
	// Hide the derived original and add a resized manual copy in one transaction.
	struct MakeDimensionManualCommand { Dimension dimension; };
	struct DeleteDimensionCommand { std::string id; };
	// Clears automatic presentation overrides on one level; manual measurements stay.
	struct ResetDimensionsCommand { LevelId level; };

	// Adds a level on top of the others (a new id is assigned).
	struct AddLevelCommand
	{
		std::int64_t heightMm = DefaultLevelHeightMm;
		std::int64_t slabThicknessMm = DefaultSlabThicknessMm;
		std::string name;
	};

	// Replaces the level with the same id: name, height, slab. The levels above move with its height, and the walls of the
	// level that were as tall as it follow the new height (the others keep theirs), in the same step.
	struct ChangeLevelCommand
	{
		Level level;
	};

	// Removes a level with every wall, piece of furniture, column and service run on it; the levels above move down. The last level cannot be
	// removed.
	struct DeleteLevelCommand
	{
		LevelId level;
	};

	// Adds a piece of furniture (ADR-0016); a new id is assigned (the given one is ignored), and a nil level is the first
	// one.
	struct AddFurnitureCommand
	{
		Furniture furniture;
	};

	// Replaces the piece of furniture with the same id: type, level, place, size and finish.
	struct ChangeFurnitureCommand
	{
		Furniture furniture;
	};

	struct DeleteFurnitureCommand
	{
		FurnitureId furniture;
	};

	// Adds an imported asset (ADR-0017) with its own id, since its files are already in the folder named after it: a
	// non-nil id no other asset has.
	struct AddAssetCommand
	{
		Asset asset;
	};

	// Replaces the asset with the same id: its name and settings (its kind and file stay). A texture cannot stop allowing
	// a surface it is used on.
	struct ChangeAssetCommand
	{
		Asset asset;
	};

	// Removes an asset nothing uses (a surface finish or a piece of furniture). Its files stay on disk.
	struct DeleteAssetCommand
	{
		AssetId asset;
	};

	// Adds a column (ADR-0021); a new id is assigned (the given one is ignored), and a nil level is the first one.
	struct AddColumnCommand
	{
		Column column;
	};

	// Replaces the column with the same id: level, place, shape, size, rotation and finish.
	struct ChangeColumnCommand
	{
		Column column;
	};

	struct DeleteColumnCommand
	{
		ColumnId column;
	};

	// Adds a run of a building service (ADR-0021); a new id is assigned, and a nil level is the first one.
	struct AddServiceRunCommand
	{
		ServiceRun run;
	};

	// Replaces the run with the same id: level, kind and points.
	struct ChangeServiceRunCommand
	{
		ServiceRun run;
	};

	struct DeleteServiceRunCommand
	{
		ServiceRunId run;
	};

	using Command = std::variant<CreateWallCommand, DeleteWallCommand, MoveWallEndpointCommand, MoveJointCommand,
		ChangeWallLengthCommand, ChangeWallThicknessCommand, ChangeWallHeightCommand, AddOpeningCommand,
		ChangeOpeningCommand, DeleteOpeningCommand, AddLevelCommand, ChangeLevelCommand, DeleteLevelCommand, SetWallFinishCommand,
		SetRoomFinishCommand, SetRoomNameCommand, SetRoomBaseboardCommand, SetDimensionCommand, MakeDimensionManualCommand, DeleteDimensionCommand, ResetDimensionsCommand, AddFurnitureCommand, ChangeFurnitureCommand, DeleteFurnitureCommand, AddAssetCommand,
		ChangeAssetCommand, DeleteAssetCommand, AddColumnCommand, ChangeColumnCommand, DeleteColumnCommand,
		AddServiceRunCommand, ChangeServiceRunCommand, DeleteServiceRunCommand>;

	// Short name for undo/redo UI, e.g. "Create wall".
	FLOOR101CADCORE_API std::string_view commandLabel(const Command& command);

	// State of one wall before and after a change; nullopt means the wall does not exist in that state.
	// `index` is the wall's position in Project::walls, so undoing a deletion restores the original order.
	struct WallChange
	{
		WallId id;
		std::size_t index = 0;
		std::optional<Wall> before;
		std::optional<Wall> after;
	};

	// Same for a level; `index` is its position in Project::levels (its place in the stack).
	struct LevelChange
	{
		LevelId id;
		std::size_t index = 0;
		std::optional<Level> before;
		std::optional<Level> after;
	};

	// Same for the finishes of a room, keyed by room key.
	struct RoomFinishChange
	{
		std::string id; // room key
		std::size_t index = 0;
		std::optional<RoomFinish> before;
		std::optional<RoomFinish> after;
	};

	struct RoomBaseboardChange
	{
		std::string id;
		std::size_t index = 0;
		std::optional<RoomBaseboard> before;
		std::optional<RoomBaseboard> after;
	};

	struct DimensionChange
	{
		std::string id;
		std::size_t index = 0;
		std::optional<Dimension> before;
		std::optional<Dimension> after;
	};

	struct RoomNameChange
	{
		std::string id;
		std::size_t index = 0;
		std::optional<RoomName> before;
		std::optional<RoomName> after;
	};

	// Same for a piece of furniture; `index` is its position in Project::furniture.
	struct FurnitureChange
	{
		FurnitureId id;
		std::size_t index = 0;
		std::optional<Furniture> before;
		std::optional<Furniture> after;
	};

	// Same for an imported asset; `index` is its position in Project::assets.
	struct AssetChange
	{
		AssetId id;
		std::size_t index = 0;
		std::optional<Asset> before;
		std::optional<Asset> after;
	};

	// Same for a column and for a run of a building service (ADR-0021).
	struct ColumnChange
	{
		ColumnId id;
		std::size_t index = 0;
		std::optional<Column> before;
		std::optional<Column> after;
	};

	struct ServiceRunChange
	{
		ServiceRunId id;
		std::size_t index = 0;
		std::optional<ServiceRun> before;
		std::optional<ServiceRun> after;
	};

	// A validated, reversible edit (ADR-0006): undo applies the `before` states, redo the `after` states.
	struct Transaction
	{
		std::string label;
		std::vector<WallChange> walls;
		std::vector<LevelChange> levels;
		std::vector<RoomFinishChange> roomFinishes;
		std::vector<FurnitureChange> furniture;
		std::vector<AssetChange> assets;
		std::vector<ColumnChange> columns;
		std::vector<ServiceRunChange> services;

		std::vector<RoomNameChange> roomNames;
		std::vector<RoomBaseboardChange> roomBaseboards;
		std::vector<DimensionChange> dimensions;

		bool empty() const
		{
			return walls.empty() && levels.empty() && roomFinishes.empty() && furniture.empty() && assets.empty() && columns.empty() &&
				services.empty() && roomNames.empty() && roomBaseboards.empty() && dimensions.empty();
		}
	};

	// Entities affected by one applied transaction (forward, undo or redo).
	struct ChangeSet
	{
		std::vector<WallId> addedWalls;
		std::vector<WallId> modifiedWalls;
		std::vector<WallId> removedWalls;
		std::vector<LevelId> addedLevels;
		std::vector<LevelId> modifiedLevels;
		std::vector<LevelId> removedLevels;
		std::vector<std::string> changedRoomFinishes; // room keys
		std::vector<FurnitureId> addedFurniture;
		std::vector<FurnitureId> modifiedFurniture;
		std::vector<FurnitureId> removedFurniture;
		std::vector<AssetId> addedAssets;
		std::vector<AssetId> modifiedAssets;
		std::vector<AssetId> removedAssets;
		std::vector<ColumnId> addedColumns;
		std::vector<ColumnId> modifiedColumns;
		std::vector<ColumnId> removedColumns;
		std::vector<ServiceRunId> addedServiceRuns;
		std::vector<ServiceRunId> modifiedServiceRuns;
		std::vector<ServiceRunId> removedServiceRuns;
		std::vector<std::string> changedRoomNames;
		std::vector<std::string> changedRoomBaseboards;
		std::vector<std::string> changedDimensions;
	};

	using ChangeListener = std::function<void(const ChangeSet&)>;

	// A project being edited, with its undo/redo history. Every change goes through a validated transaction, and a
	// rejected command leaves the project and the history untouched. Listeners run synchronously after each applied
	// transaction (scene regeneration hooks in here).
	class Document
	{
	public:
		// A new document has one level, the ground floor, with the default dimensions.
		FLOOR101CADCORE_API explicit Document(UuidSource newUuid);

		const Project& project() const { return model; }

		// Validates and applies a command. A command that changes nothing succeeds with an empty transaction and is not
		// recorded in the history.
		FLOOR101CADCORE_API Result<Transaction> execute(const Command& command);

		FLOOR101CADCORE_API Status undo();
		FLOOR101CADCORE_API Status redo();
		bool canUndo() const { return !undoStack.empty(); }
		bool canRedo() const { return !redoStack.empty(); }
		// Label of the transaction undo()/redo() would apply; empty if none.
		FLOOR101CADCORE_API std::string_view undoLabel() const;
		FLOOR101CADCORE_API std::string_view redoLabel() const;

		// Replaces the whole project (e.g. a loaded file) after validating it. Clears the history.
		FLOOR101CADCORE_API Status replaceProject(Project replacement);

		// Convenience wrappers; each one is an undoable command.
		FLOOR101CADCORE_API Result<WallId> addWall(Point2D start, Point2D end, std::int64_t thicknessMm, std::int64_t heightMm,
			const LevelId& level = {});
		FLOOR101CADCORE_API Status setWallEnd(const WallId& id, Point2D end);
		FLOOR101CADCORE_API Status removeWall(const WallId& id);
		FLOOR101CADCORE_API Result<OpeningId> addOpening(const WallId& wall, const Opening& opening);
		FLOOR101CADCORE_API Result<LevelId> addLevel(std::int64_t heightMm = DefaultLevelHeightMm,
			std::int64_t slabThicknessMm = DefaultSlabThicknessMm);
		FLOOR101CADCORE_API Result<FurnitureId> addFurniture(const Furniture& furniture);
		FLOOR101CADCORE_API Result<ColumnId> addColumn(const Column& column);
		FLOOR101CADCORE_API Result<ServiceRunId> addServiceRun(const ServiceRun& run);

		// Openings added or changed must stay clear of the walls joined to their host (ADR-0013), pockets of sliding doors
		// included (ADR-0014). The Geometry Engine provides the spans (geometry::makeWallObstructions); without it only the
		// wall itself is checked.
		void setWallObstructions(WallObstructions provider) { obstructions = std::move(provider); }
		// Geometry supplies inherited room finishes when splitting changes derived room keys (ADR-0025).
		using RoomFinishTransfer = std::function<std::vector<RoomFinish>(const Project&, const Project&)>;
		void setRoomFinishTransfer(RoomFinishTransfer provider) { roomFinishTransfer = std::move(provider); }
		using RoomBaseboardTransfer = std::function<std::vector<RoomBaseboard>(const Project&, const Project&)>;
		void setRoomBaseboardTransfer(RoomBaseboardTransfer provider) { roomBaseboardTransfer = std::move(provider); }
		using RoomNameTransfer = std::function<std::vector<RoomName>(const Project&, const Project&)>;
		void setRoomNameTransfer(RoomNameTransfer provider) { roomNameTransfer = std::move(provider); }
		// Where `wall` of `levelWalls` must not have an opening; empty without a provider.
		FLOOR101CADCORE_API std::vector<AxisSpan> openingObstructions(const Project& levelWalls, const Wall& wall) const;

		// Returns a handle for unsubscribe().
		FLOOR101CADCORE_API std::size_t subscribe(ChangeListener listener);
		FLOOR101CADCORE_API void unsubscribe(std::size_t handle);

	private:
		Result<Transaction> buildTransaction(const Command& command);
		void apply(const Transaction& transaction, bool forward);
		void notify(const ChangeSet& changes) const;
		// The opening of the (single-wall) transaction clear of the walls joined to its host.
		Status checkOpeningClear(const Transaction& transaction, const OpeningId& opening) const;

		Project model;
		UuidSource newUuid;
		std::vector<Transaction> undoStack;
		std::vector<Transaction> redoStack;
		std::vector<std::pair<std::size_t, ChangeListener>> listeners;
		std::size_t nextListenerHandle = 1;
		WallObstructions obstructions;
		RoomFinishTransfer roomFinishTransfer;
		RoomNameTransfer roomNameTransfer;
		RoomBaseboardTransfer roomBaseboardTransfer;
	};

	// Checks a whole project: at least one level, every level, wall, piece of furniture, asset, column and service run valid,
	// every id unique, every wall, piece of furniture, column and run on an existing level, every imported texture and model they use in the project
	// (and allowing that surface), and the top of the building inside the model range.
	FLOOR101CADCORE_API Status validateProject(const Project& project);
}
