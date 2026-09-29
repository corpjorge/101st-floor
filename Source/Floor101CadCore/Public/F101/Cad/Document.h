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

	// Removes a level and every wall on it; the levels above move down. The last level cannot be removed.
	struct DeleteLevelCommand
	{
		LevelId level;
	};

	using Command = std::variant<CreateWallCommand, DeleteWallCommand, MoveWallEndpointCommand, MoveJointCommand,
		ChangeWallLengthCommand, ChangeWallThicknessCommand, ChangeWallHeightCommand, AddOpeningCommand,
		ChangeOpeningCommand, DeleteOpeningCommand, AddLevelCommand, ChangeLevelCommand, DeleteLevelCommand>;

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

	// A validated, reversible edit (ADR-0006): undo applies the `before` states, redo the `after` states.
	struct Transaction
	{
		std::string label;
		std::vector<WallChange> walls;
		std::vector<LevelChange> levels;

		bool empty() const { return walls.empty() && levels.empty(); }
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

		// Returns a handle for unsubscribe().
		FLOOR101CADCORE_API std::size_t subscribe(ChangeListener listener);
		FLOOR101CADCORE_API void unsubscribe(std::size_t handle);

	private:
		Result<Transaction> buildTransaction(const Command& command);
		void apply(const Transaction& transaction, bool forward);
		void notify(const ChangeSet& changes) const;

		Project model;
		UuidSource newUuid;
		std::vector<Transaction> undoStack;
		std::vector<Transaction> redoStack;
		std::vector<std::pair<std::size_t, ChangeListener>> listeners;
		std::size_t nextListenerHandle = 1;
	};

	// Checks a whole project: at least one level, every level and wall valid, every id unique, every wall on an existing
	// level, and the top of the building inside the model range.
	FLOOR101CADCORE_API Status validateProject(const Project& project);
}
