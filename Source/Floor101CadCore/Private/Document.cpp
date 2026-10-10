#include "F101/Cad/Document.h"

#include "F101/Cad/Snapping.h"
#include "F101/Cad/Units.h"
#include "WallSplitting.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace f101::cad
{
	namespace
	{
		Error wallNotFound(const WallId& id)
		{
			return Error{ErrorCode::NotFound, "No wall with id " + toString(id)};
		}

		Error openingNotFound(const OpeningId& id)
		{
			return Error{ErrorCode::NotFound, "No opening with id " + toString(id)};
		}

		Error levelNotFound(const LevelId& id)
		{
			return Error{ErrorCode::NotFound, "No level with id " + toString(id)};
		}

		Error furnitureNotFound(const FurnitureId& id)
		{
			return Error{ErrorCode::NotFound, "No furniture with id " + toString(id)};
		}

		Error assetNotFound(const AssetId& id)
		{
			return Error{ErrorCode::NotFound, "No asset with id " + toString(id)};
		}

		Error columnNotFound(const ColumnId& id)
		{
			return Error{ErrorCode::NotFound, "No column with id " + toString(id)};
		}

		Error serviceRunNotFound(const ServiceRunId& id)
		{
			return Error{ErrorCode::NotFound, "No service run with id " + toString(id)};
		}

		// The imported textures a wall uses exist and allow its faces and its openings' carpentry (ADR-0017).
		Status validateWallFinishesIn(const Project& project, const Wall& wall)
		{
			for (const FinishId* face : {&wall.finishLeft, &wall.finishRight})
			{
				if (Status valid = validateFinishIn(project, *face, Surface::Wall); !valid)
				{
					return valid;
				}
			}
			for (const Opening& opening : wall.openings)
			{
				if (Status valid = validateFinishIn(project, opening.finish, Surface::Carpentry); !valid)
				{
					return valid;
				}
			}
			return Status::success();
		}

		// A command's level: the named one, or the first level when nil.
		Result<LevelId> resolveLevel(const Project& project, const LevelId& level)
		{
			if (level.isNil())
			{
				F101_ASSERT(!project.levels.empty());
				return project.levels.front().id;
			}
			if (!project.findLevel(level))
			{
				return levelNotFound(level);
			}
			return level;
		}

		void sortOpenings(Wall& wall)
		{
			std::stable_sort(wall.openings.begin(), wall.openings.end(),
				[](const Opening& a, const Opening& b) { return a.offsetMm < b.offsetMm; });
		}

		// Identity of each kind of entity in a transaction.
		const std::string& keyOf(const Dimension& entry) { return entry.id; }
		const WallId& keyOf(const Wall& wall) { return wall.id; }
		const LevelId& keyOf(const Level& level) { return level.id; }
		const std::string& keyOf(const RoomFinish& finish) { return finish.room; }
		const std::string& keyOf(const RoomName& entry) { return entry.room; }
		const std::string& keyOf(const RoomBaseboard& entry) { return entry.room; }
		const FurnitureId& keyOf(const Furniture& furniture) { return furniture.id; }
		const AssetId& keyOf(const Asset& asset) { return asset.id; }
		const ColumnId& keyOf(const Column& column) { return column.id; }
		const ServiceRunId& keyOf(const ServiceRun& run) { return run.id; }

		template <typename Entity, typename Id>
		std::optional<std::size_t> findIndex(const std::vector<Entity>& entities, const Id& id)
		{
			for (std::size_t index = 0; index < entities.size(); ++index)
			{
				if (keyOf(entities[index]) == id)
				{
					return index;
				}
			}
			return std::nullopt;
		}

		std::optional<std::size_t> findWallIndex(const Project& project, const WallId& id)
		{
			return findIndex(project.walls, id);
		}

		// Transaction that replaces an existing wall with an edited copy (validated).
		Result<Transaction> modifyWall(const Project& project, const WallId& id, std::string label,
			const std::function<Status(Wall&)>& edit)
		{
			const std::optional<std::size_t> index = findWallIndex(project, id);
			if (!index)
			{
				return wallNotFound(id);
			}
			const Wall& before = project.walls[*index];
			Wall after = before;
			if (Status edited = edit(after); !edited)
			{
				return edited.error();
			}
			if (Status valid = validateWall(after); !valid)
			{
				return valid.error();
			}
			Transaction transaction{std::move(label), {}, {}};
			if (!(after == before))
			{
				transaction.walls.push_back(WallChange{id, *index, before, after});
			}
			return transaction;
		}

		template <typename Change, typename Id>
		void sortChanges(const std::vector<Change>& transactionChanges, bool forward, std::vector<Id>& added,
			std::vector<Id>& modified, std::vector<Id>& removed)
		{
			for (const Change& change : transactionChanges)
			{
				const bool existedBefore = forward ? change.before.has_value() : change.after.has_value();
				const bool existsAfter = forward ? change.after.has_value() : change.before.has_value();
				if (!existedBefore && existsAfter)
				{
					added.push_back(change.id);
				}
				else if (existedBefore && !existsAfter)
				{
					removed.push_back(change.id);
				}
				else
				{
					modified.push_back(change.id);
				}
			}
		}

		ChangeSet changeSetOf(const Transaction& transaction, bool forward)
		{
			ChangeSet changes;
			sortChanges(transaction.walls, forward, changes.addedWalls, changes.modifiedWalls, changes.removedWalls);
			sortChanges(transaction.levels, forward, changes.addedLevels, changes.modifiedLevels, changes.removedLevels);
			for (const RoomFinishChange& change : transaction.roomFinishes)
			{
				changes.changedRoomFinishes.push_back(change.id);
			}
			for (const DimensionChange& change : transaction.dimensions) changes.changedDimensions.push_back(change.id);
			for (const RoomBaseboardChange& change : transaction.roomBaseboards) changes.changedRoomBaseboards.push_back(change.id);
			for (const RoomNameChange& change : transaction.roomNames) changes.changedRoomNames.push_back(change.id);
			sortChanges(transaction.furniture, forward, changes.addedFurniture, changes.modifiedFurniture,
				changes.removedFurniture);
			sortChanges(transaction.assets, forward, changes.addedAssets, changes.modifiedAssets, changes.removedAssets);
			sortChanges(transaction.columns, forward, changes.addedColumns, changes.modifiedColumns, changes.removedColumns);
			sortChanges(transaction.services, forward, changes.addedServiceRuns, changes.modifiedServiceRuns, changes.removedServiceRuns);
			return changes;
		}

		// Adds the room settings inherited through a split (ADR-0025) to the project and the transaction, except where a
		// room already has its own.
		template <typename Entry, typename Change>
		void inheritRoomEntries(std::vector<Entry>& entries, std::vector<Change>& changes, const std::vector<Entry>& inherited)
		{
			for (const Entry& entry : inherited)
			{
				if (findIndex(entries, keyOf(entry))) continue;
				changes.push_back(Change{keyOf(entry), entries.size(), std::nullopt, entry});
				entries.push_back(entry);
			}
		}

		// Applies one entity kind of a transaction: removals first (from the back), then insertions/replacements in index
		// order, so indices stay valid.
		template <typename Entity, typename Change>
		void applyChanges(std::vector<Entity>& entities, const std::vector<Change>& changes, bool forward)
		{
			std::vector<const Change*> ordered;
			for (const Change& change : changes)
			{
				ordered.push_back(&change);
			}
			std::sort(ordered.begin(), ordered.end(), [](const Change* a, const Change* b) { return a->index < b->index; });

			for (auto it = ordered.rbegin(); it != ordered.rend(); ++it)
			{
				if (!(forward ? (*it)->after : (*it)->before))
				{
					const std::optional<std::size_t> index = findIndex(entities, (*it)->id);
					F101_ASSERT(index.has_value());
					entities.erase(entities.begin() + static_cast<std::ptrdiff_t>(*index));
				}
			}
			for (const Change* change : ordered)
			{
				const auto& target = forward ? change->after : change->before;
				if (!target)
				{
					continue;
				}
				if (const std::optional<std::size_t> index = findIndex(entities, change->id))
				{
					entities[*index] = *target;
				}
				else
				{
					const std::size_t position = std::min(change->index, entities.size());
					entities.insert(entities.begin() + static_cast<std::ptrdiff_t>(position), *target);
				}
			}
		}

		struct CommandLabeler
		{
			std::string_view operator()(const CreateWallCommand&) const { return "Create wall"; }
			std::string_view operator()(const DeleteWallCommand&) const { return "Delete wall"; }
			std::string_view operator()(const MoveWallEndpointCommand&) const { return "Move wall endpoint"; }
			std::string_view operator()(const MoveJointCommand&) const { return "Move corner"; }
			std::string_view operator()(const TranslateWallCommand&) const { return "Move wall"; }
			std::string_view operator()(const ChangeWallLengthCommand&) const { return "Change wall length"; }
			std::string_view operator()(const ChangeWallThicknessCommand&) const { return "Change wall thickness"; }
			std::string_view operator()(const ChangeWallHeightCommand&) const { return "Change wall height"; }
			std::string_view operator()(const AddOpeningCommand& add) const
			{
				return add.opening.kind == OpeningKind::Door ? "Add door" : "Add window";
			}
			std::string_view operator()(const ChangeOpeningCommand&) const { return "Change opening"; }
			std::string_view operator()(const DeleteOpeningCommand&) const { return "Delete opening"; }
			std::string_view operator()(const AddLevelCommand&) const { return "Add level"; }
			std::string_view operator()(const ChangeLevelCommand&) const { return "Change level"; }
			std::string_view operator()(const DeleteLevelCommand&) const { return "Delete level"; }
			std::string_view operator()(const SetWallFinishCommand&) const { return "Change finish"; }
			std::string_view operator()(const SetRoomFinishCommand&) const { return "Change room finish"; }
			std::string_view operator()(const SetRoomNameCommand&) const { return "Rename room"; }
			std::string_view operator()(const SetDimensionCommand&) const { return "Edit dimension"; }
			std::string_view operator()(const MakeDimensionManualCommand&) const { return "Resize automatic dimension"; }
			std::string_view operator()(const DeleteDimensionCommand&) const { return "Delete dimension"; }
			std::string_view operator()(const ResetDimensionsCommand&) const { return "Restore automatic dimensions"; }
			std::string_view operator()(const SetRoomBaseboardCommand&) const { return "Change baseboard"; }
			std::string_view operator()(const AddFurnitureCommand&) const { return "Add furniture"; }
			std::string_view operator()(const ChangeFurnitureCommand&) const { return "Change furniture"; }
			std::string_view operator()(const DeleteFurnitureCommand&) const { return "Delete furniture"; }
			std::string_view operator()(const AddAssetCommand&) const { return "Import asset"; }
			std::string_view operator()(const ChangeAssetCommand&) const { return "Change asset"; }
			std::string_view operator()(const DeleteAssetCommand&) const { return "Delete asset"; }
			std::string_view operator()(const AddColumnCommand&) const { return "Add column"; }
			std::string_view operator()(const ChangeColumnCommand&) const { return "Change column"; }
			std::string_view operator()(const DeleteColumnCommand&) const { return "Delete column"; }
			std::string_view operator()(const AddServiceRunCommand&) const { return "Add service run"; }
			std::string_view operator()(const ChangeServiceRunCommand&) const { return "Change service run"; }
			std::string_view operator()(const DeleteServiceRunCommand&) const { return "Delete service run"; }
		};
	}

	std::string_view commandLabel(const Command& command)
	{
		return std::visit(CommandLabeler{}, command);
	}

	Status validateProject(const Project& project)
	{
		if (project.levels.empty())
		{
			return Error{ErrorCode::InvalidArgument, "A project needs at least one level"};
		}
		std::set<LevelId> levelIds;
		std::int64_t top = 0;
		for (const Level& level : project.levels)
		{
			if (Status valid = validateLevel(level); !valid)
			{
				return valid;
			}
			if (!levelIds.insert(level.id).second)
			{
				return Error{ErrorCode::InvalidArgument, "Duplicate level id " + toString(level.id)};
			}
			top += level.heightMm; // no overflow: each height is at most MaxAbsMillimeters and top stays below it
			if (top > MaxAbsMillimeters)
			{
				return Error{ErrorCode::OutOfRange, "The levels are taller than the model range"};
			}
		}
		std::set<AssetId> assetIds;
		for (const Asset& asset : project.assets)
		{
			if (Status valid = validateAsset(asset); !valid)
			{
				return valid;
			}
			if (!assetIds.insert(asset.id).second)
			{
				return Error{ErrorCode::InvalidArgument, "Duplicate asset id " + toString(asset.id)};
			}
		}

		std::set<WallId> ids;
		std::set<OpeningId> openingIds;
		for (const Wall& wall : project.walls)
		{
			if (Status valid = validateWall(wall); !valid)
			{
				return valid;
			}
			if (!project.findLevel(wall.level))
			{
				return Error{ErrorCode::InvalidArgument, "Wall " + toString(wall.id) + " is not on a level of the project"};
			}
			if (Status valid = validateWallFinishesIn(project, wall); !valid)
			{
				return valid;
			}
			if (!ids.insert(wall.id).second)
			{
				return Error{ErrorCode::InvalidArgument, "Duplicate wall id " + toString(wall.id)};
			}
			for (const Opening& opening : wall.openings)
			{
				if (!openingIds.insert(opening.id).second)
				{
					return Error{ErrorCode::InvalidArgument, "Duplicate opening id " + toString(opening.id)};
				}
			}
		}
		std::set<std::string> rooms;
		for (const RoomFinish& finish : project.roomFinishes)
		{
			if (finish.room.empty() || !rooms.insert(finish.room).second)
			{
				return Error{ErrorCode::InvalidArgument, "Room finishes need a unique room key"};
			}
			if (Status valid = validateFinishIn(project, finish.floor, Surface::Floor); !valid)
			{
				return valid;
			}
			if (Status valid = validateFinishIn(project, finish.ceiling, Surface::Ceiling); !valid)
			{
				return valid;
			}
		}
		std::set<std::string> dimensionIds;
		for (const Dimension& entry : project.dimensions)
		{
			if (!dimensionIds.insert(entry.id).second) return Error{ErrorCode::InvalidArgument, "Duplicate dimension id"};
			if (Status valid = validateDimension(project, entry); !valid) return valid;
		}
		std::set<std::string> trimmedRooms;
		for (const RoomBaseboard& entry : project.roomBaseboards)
		{
			if (!trimmedRooms.insert(entry.room).second) return Error{ErrorCode::InvalidArgument, "Duplicate room baseboard key"};
			if (Status valid = validateRoomBaseboard(project, entry); !valid) return valid;
		}
		std::set<std::string> namedRooms;
		for (const RoomName& entry : project.roomNames)
		{
			if (entry.room.empty() || entry.name.empty() || entry.name.find_first_not_of(' ') == std::string::npos || !namedRooms.insert(entry.room).second)
			{
				return Error{ErrorCode::InvalidArgument, "Room names need a unique room key and a nonblank name"};
			}
			if (Status valid = validateRoomName(entry.name); !valid) return valid;
		}
		std::set<FurnitureId> furnitureIds;
		for (const Furniture& piece : project.furniture)
		{
			if (Status valid = validateFurnitureIn(project, piece); !valid)
			{
				return valid;
			}
			if (piece.id.isNil() || !furnitureIds.insert(piece.id).second)
			{
				return Error{ErrorCode::InvalidArgument, "Duplicate or nil furniture id " + toString(piece.id)};
			}
			if (!project.findLevel(piece.level))
			{
				return Error{ErrorCode::InvalidArgument, "Furniture " + toString(piece.id) + " is not on a level of the project"};
			}
		}
		std::set<ColumnId> columnIds;
		for (const Column& column : project.columns)
		{
			if (Status valid = validateColumnIn(project, column); !valid)
			{
				return valid;
			}
			if (column.id.isNil() || !columnIds.insert(column.id).second)
			{
				return Error{ErrorCode::InvalidArgument, "Duplicate or nil column id " + toString(column.id)};
			}
			if (!project.findLevel(column.level))
			{
				return Error{ErrorCode::InvalidArgument, "Column " + toString(column.id) + " is not on a level of the project"};
			}
		}
		std::set<ServiceRunId> runIds;
		for (const ServiceRun& run : project.services)
		{
			if (Status valid = validateServiceRun(run); !valid)
			{
				return valid;
			}
			if (run.id.isNil() || !runIds.insert(run.id).second)
			{
				return Error{ErrorCode::InvalidArgument, "Duplicate or nil service run id " + toString(run.id)};
			}
			if (!project.findLevel(run.level))
			{
				return Error{ErrorCode::InvalidArgument, "Service run " + toString(run.id) + " is not on a level of the project"};
			}
		}
		return Status::success();
	}

	Document::Document(UuidSource newUuidSource)
		: newUuid(std::move(newUuidSource))
	{
		F101_ASSERT(static_cast<bool>(newUuid));
		model.levels.push_back(Level{LevelId{newUuid()}});
	}

	Result<Transaction> Document::buildTransaction(const Command& command)
	{
		const std::string label(commandLabel(command));

		if (const auto* create = std::get_if<CreateWallCommand>(&command))
		{
			const Result<LevelId> level = resolveLevel(model, create->level);
			if (!level)
			{
				return level.error();
			}
			Wall wall{WallId{newUuid()}, create->start, create->end, create->thicknessMm, create->heightMm};
			wall.level = level.value();
			if (Status valid = validateWall(wall); !valid)
			{
				return valid.error();
			}
			F101_ASSERT(!findWallIndex(model, wall.id).has_value());
			return Transaction{label, {WallChange{wall.id, model.walls.size(), std::nullopt, wall}}, {}};
		}
		if (const auto* remove = std::get_if<DeleteWallCommand>(&command))
		{
			const std::optional<std::size_t> index = findWallIndex(model, remove->wall);
			if (!index)
			{
				return wallNotFound(remove->wall);
			}
			return Transaction{label, {WallChange{remove->wall, *index, model.walls[*index], std::nullopt}}, {}};
		}
		if (const auto* move = std::get_if<MoveWallEndpointCommand>(&command))
		{
			return modifyWall(model, move->wall, label, [move](Wall& wall)
			{
				(move->endpoint == WallEndpoint::Start ? wall.start : wall.end) = move->position;
				return Status::success();
			});
		}
		if (const auto* joint = std::get_if<MoveJointCommand>(&command))
		{
			const Result<LevelId> level = resolveLevel(model, joint->level);
			if (!level)
			{
				return level.error();
			}
			Transaction transaction{label, {}, {}};
			bool found = false;
			for (std::size_t index = 0; index < model.walls.size(); ++index)
			{
				const Wall& before = model.walls[index];
				if (before.level != level.value() || (before.start != joint->from && before.end != joint->from))
				{
					continue;
				}
				found = true;
				Wall after = before;
				(after.start == joint->from ? after.start : after.end) = joint->to;
				if (Status valid = validateWall(after); !valid)
				{
					return valid.error();
				}
				if (!(after == before))
				{
					transaction.walls.push_back(WallChange{before.id, index, before, after});
				}
			}
			if (!found)
			{
				return Error{ErrorCode::NotFound, "No wall ends at (" + std::to_string(joint->from.x_mm) + ", " +
					std::to_string(joint->from.y_mm) + ") mm"};
			}
			return transaction;
		}
		if (const auto* move = std::get_if<TranslateWallCommand>(&command))
		{
			const Wall* selected = model.findWall(move->wall);
			if (!selected) return wallNotFound(move->wall);
			// Bound the delta before integer addition, including commands supplied outside the UI.
			if (move->delta.dx_mm < -2 * MaxAbsMillimeters || move->delta.dx_mm > 2 * MaxAbsMillimeters ||
				move->delta.dy_mm < -2 * MaxAbsMillimeters || move->delta.dy_mm > 2 * MaxAbsMillimeters)
			{
				return Error{ErrorCode::OutOfRange, "Wall movement would leave the model range"};
			}
			Transaction transaction{label, {}, {}};
			for (std::size_t index = 0; index < model.walls.size(); ++index)
			{
				const Wall& before = model.walls[index];
				if (before.level != selected->level) continue;
				Wall after = before;
				for (Point2D* point : {&after.start, &after.end})
				{
					// Compare with the original joints: neither translation depends on the other having happened.
					if (*point == selected->start || *point == selected->end)
					{
						point->x_mm += move->delta.dx_mm;
						point->y_mm += move->delta.dy_mm;
					}
				}
				if (after == before) continue;
				if (Status valid = validateWall(after); !valid) return valid.error();
				transaction.walls.push_back({before.id, index, before, after});
			}
			return transaction;
		}
		if (const auto* length = std::get_if<ChangeWallLengthCommand>(&command))
		{
			return modifyWall(model, length->wall, label, [length](Wall& wall) -> Status
			{
				if (length->lengthMm <= 0 || length->lengthMm > MaxAbsMillimeters)
				{
					return Error{ErrorCode::OutOfRange, "Wall length must be positive"};
				}
				const PlanPoint direction{static_cast<double>(wall.end.x_mm), static_cast<double>(wall.end.y_mm)};
				const Result<Point2D> end = pointAtDistance(wall.start, direction, length->lengthMm);
				if (!end)
				{
					return Error{ErrorCode::OutOfRange, "Wall end would leave the model range"};
				}
				wall.end = end.value();
				return Status::success();
			});
		}
		if (const auto* add = std::get_if<AddOpeningCommand>(&command))
		{
			if (Status valid = validateFinishIn(model, add->opening.finish, Surface::Carpentry); !valid)
			{
				return valid.error();
			}
			Opening opening = add->opening;
			opening.id = OpeningId{newUuid()};
			Result<Transaction> added = modifyWall(model, add->wall, label, [&opening](Wall& wall)
			{
				wall.openings.push_back(opening);
				sortOpenings(wall);
				return Status::success();
			});
			if (added)
			{
				if (Status clear = checkOpeningClear(added.value(), opening.id); !clear)
				{
					return clear.error();
				}
			}
			return added;
		}
		if (const auto* change = std::get_if<ChangeOpeningCommand>(&command))
		{
			if (Status valid = validateFinishIn(model, change->opening.finish, Surface::Carpentry); !valid)
			{
				return valid.error();
			}
			Result<Transaction> changed = modifyWall(model, change->wall, label, [change](Wall& wall) -> Status
			{
				for (Opening& opening : wall.openings)
				{
					if (opening.id == change->opening.id)
					{
						opening = change->opening;
						sortOpenings(wall);
						return Status::success();
					}
				}
				return openingNotFound(change->opening.id);
			});
			if (changed)
			{
				if (Status clear = checkOpeningClear(changed.value(), change->opening.id); !clear)
				{
					return clear.error();
				}
			}
			return changed;
		}
		if (const auto* removal = std::get_if<DeleteOpeningCommand>(&command))
		{
			return modifyWall(model, removal->wall, label, [removal](Wall& wall) -> Status
			{
				const auto found = std::find_if(wall.openings.begin(), wall.openings.end(),
					[removal](const Opening& opening) { return opening.id == removal->opening; });
				if (found == wall.openings.end())
				{
					return openingNotFound(removal->opening);
				}
				wall.openings.erase(found);
				return Status::success();
			});
		}
		if (const auto* add = std::get_if<AddLevelCommand>(&command))
		{
			const Level level{LevelId{newUuid()}, add->name, add->heightMm, add->slabThicknessMm};
			if (Status valid = validateLevel(level); !valid)
			{
				return valid.error();
			}
			Project after = model;
			after.levels.push_back(level);
			if (Status valid = validateProject(after); !valid)
			{
				return valid.error(); // e.g. the building would leave the model range
			}
			return Transaction{label, {}, {LevelChange{level.id, model.levels.size(), std::nullopt, level}}};
		}
		if (const auto* change = std::get_if<ChangeLevelCommand>(&command))
		{
			const std::optional<std::size_t> index = model.levelIndex(change->level.id);
			if (!index)
			{
				return levelNotFound(change->level.id);
			}
			if (Status valid = validateLevel(change->level); !valid)
			{
				return valid.error();
			}
			Project after = model;
			after.levels[*index] = change->level;
			if (Status valid = validateProject(after); !valid)
			{
				return valid.error();
			}
			Transaction transaction{label, {}, {}};
			if (change->level == model.levels[*index])
			{
				return transaction;
			}
			const Level& before = model.levels[*index];
			transaction.levels.push_back(LevelChange{change->level.id, *index, before, change->level});
			// Walls as tall as the level (floor to floor) stay so: they follow its new height. Other walls keep theirs.
			if (change->level.heightMm != before.heightMm)
			{
				for (std::size_t wall = 0; wall < model.walls.size(); ++wall)
				{
					const Wall& current = model.walls[wall];
					if (current.level != before.id || current.heightMm != before.heightMm)
					{
						continue;
					}
					Wall followed = current;
					followed.heightMm = change->level.heightMm;
					if (Status valid = validateWall(followed); !valid)
					{
						return valid.error(); // e.g. a window that no longer fits under the top
					}
					transaction.walls.push_back(WallChange{current.id, wall, current, followed});
				}
			}
			return transaction;
		}
		if (const auto* removal = std::get_if<DeleteLevelCommand>(&command))
		{
			const std::optional<std::size_t> index = model.levelIndex(removal->level);
			if (!index)
			{
				return levelNotFound(removal->level);
			}
			if (model.levels.size() == 1)
			{
				return Error{ErrorCode::InvalidArgument, "The last level cannot be deleted"};
			}
			Transaction transaction{label, {}, {LevelChange{removal->level, *index, model.levels[*index], std::nullopt}}};
			for (std::size_t wall = 0; wall < model.walls.size(); ++wall)
			{
				if (model.walls[wall].level == removal->level)
				{
					transaction.walls.push_back(WallChange{model.walls[wall].id, wall, model.walls[wall], std::nullopt});
				}
			}
			for (std::size_t piece = 0; piece < model.furniture.size(); ++piece)
			{
				if (model.furniture[piece].level == removal->level)
				{
					transaction.furniture.push_back(
						FurnitureChange{model.furniture[piece].id, piece, model.furniture[piece], std::nullopt});
				}
			}
			for (std::size_t i = 0; i < model.dimensions.size(); ++i)
			{
				const Dimension& entry = model.dimensions[i];
				if (entry.level == removal->level) transaction.dimensions.push_back({entry.id, i, entry, std::nullopt});
			}
			for (std::size_t column = 0; column < model.columns.size(); ++column)
			{
				if (model.columns[column].level == removal->level)
				{
					transaction.columns.push_back(ColumnChange{model.columns[column].id, column, model.columns[column], std::nullopt});
				}
			}
			for (std::size_t run = 0; run < model.services.size(); ++run)
			{
				if (model.services[run].level == removal->level)
				{
					transaction.services.push_back(ServiceRunChange{model.services[run].id, run, model.services[run], std::nullopt});
				}
			}
			return transaction;
		}
		if (const auto* add = std::get_if<AddColumnCommand>(&command))
		{
			Column column = add->column;
			column.id = ColumnId{newUuid()};
			const Result<LevelId> level = resolveLevel(model, column.level);
			if (!level)
			{
				return level.error();
			}
			column.level = level.value();
			if (Status valid = validateColumnIn(model, column); !valid)
			{
				return valid.error();
			}
			Transaction transaction{label, {}, {}, {}};
			transaction.columns.push_back(ColumnChange{column.id, model.columns.size(), std::nullopt, column});
			return transaction;
		}
		if (const auto* change = std::get_if<ChangeColumnCommand>(&command))
		{
			const std::optional<std::size_t> index = findIndex(model.columns, change->column.id);
			if (!index)
			{
				return columnNotFound(change->column.id);
			}
			Column column = change->column;
			const Result<LevelId> level = resolveLevel(model, column.level);
			if (!level)
			{
				return level.error();
			}
			column.level = level.value();
			if (Status valid = validateColumnIn(model, column); !valid)
			{
				return valid.error();
			}
			Transaction transaction{label, {}, {}, {}};
			if (!(column == model.columns[*index]))
			{
				transaction.columns.push_back(ColumnChange{column.id, *index, model.columns[*index], column});
			}
			return transaction;
		}
		if (const auto* removal = std::get_if<DeleteColumnCommand>(&command))
		{
			const std::optional<std::size_t> index = findIndex(model.columns, removal->column);
			if (!index)
			{
				return columnNotFound(removal->column);
			}
			Transaction transaction{label, {}, {}, {}};
			transaction.columns.push_back(ColumnChange{removal->column, *index, model.columns[*index], std::nullopt});
			return transaction;
		}
		if (const auto* add = std::get_if<AddServiceRunCommand>(&command))
		{
			ServiceRun run = add->run;
			run.id = ServiceRunId{newUuid()};
			const Result<LevelId> level = resolveLevel(model, run.level);
			if (!level)
			{
				return level.error();
			}
			run.level = level.value();
			if (Status valid = validateServiceRun(run); !valid)
			{
				return valid.error();
			}
			Transaction transaction{label, {}, {}, {}};
			transaction.services.push_back(ServiceRunChange{run.id, model.services.size(), std::nullopt, run});
			return transaction;
		}
		if (const auto* change = std::get_if<ChangeServiceRunCommand>(&command))
		{
			const std::optional<std::size_t> index = findIndex(model.services, change->run.id);
			if (!index)
			{
				return serviceRunNotFound(change->run.id);
			}
			ServiceRun run = change->run;
			const Result<LevelId> level = resolveLevel(model, run.level);
			if (!level)
			{
				return level.error();
			}
			run.level = level.value();
			if (Status valid = validateServiceRun(run); !valid)
			{
				return valid.error();
			}
			Transaction transaction{label, {}, {}, {}};
			if (!(run == model.services[*index]))
			{
				transaction.services.push_back(ServiceRunChange{run.id, *index, model.services[*index], run});
			}
			return transaction;
		}
		if (const auto* removal = std::get_if<DeleteServiceRunCommand>(&command))
		{
			const std::optional<std::size_t> index = findIndex(model.services, removal->run);
			if (!index)
			{
				return serviceRunNotFound(removal->run);
			}
			Transaction transaction{label, {}, {}, {}};
			transaction.services.push_back(ServiceRunChange{removal->run, *index, model.services[*index], std::nullopt});
			return transaction;
		}
		if (const auto* add = std::get_if<AddFurnitureCommand>(&command))
		{
			Furniture piece = add->furniture;
			piece.id = FurnitureId{newUuid()};
			const Result<LevelId> level = resolveLevel(model, piece.level);
			if (!level)
			{
				return level.error();
			}
			piece.level = level.value();
			if (Status valid = validateFurnitureIn(model, piece); !valid)
			{
				return valid.error();
			}
			F101_ASSERT(!model.findFurniture(piece.id));
			Transaction transaction{label, {}, {}, {}};
			transaction.furniture.push_back(FurnitureChange{piece.id, model.furniture.size(), std::nullopt, piece});
			return transaction;
		}
		if (const auto* change = std::get_if<ChangeFurnitureCommand>(&command))
		{
			const std::optional<std::size_t> index = findIndex(model.furniture, change->furniture.id);
			if (!index)
			{
				return furnitureNotFound(change->furniture.id);
			}
			Furniture piece = change->furniture;
			const Result<LevelId> level = resolveLevel(model, piece.level);
			if (!level)
			{
				return level.error();
			}
			piece.level = level.value();
			if (Status valid = validateFurnitureIn(model, piece); !valid)
			{
				return valid.error();
			}
			Transaction transaction{label, {}, {}, {}};
			if (!(piece == model.furniture[*index]))
			{
				transaction.furniture.push_back(FurnitureChange{piece.id, *index, model.furniture[*index], piece});
			}
			return transaction;
		}
		if (const auto* removal = std::get_if<DeleteFurnitureCommand>(&command))
		{
			const std::optional<std::size_t> index = findIndex(model.furniture, removal->furniture);
			if (!index)
			{
				return furnitureNotFound(removal->furniture);
			}
			Transaction transaction{label, {}, {}, {}};
			transaction.furniture.push_back(
				FurnitureChange{removal->furniture, *index, model.furniture[*index], std::nullopt});
			return transaction;
		}
		if (const auto* add = std::get_if<AddAssetCommand>(&command))
		{
			if (Status valid = validateAsset(add->asset); !valid)
			{
				return valid.error();
			}
			if (model.findAsset(add->asset.id))
			{
				return Error{ErrorCode::InvalidArgument, "Duplicate asset id " + toString(add->asset.id)};
			}
			Transaction transaction{label, {}, {}, {}};
			transaction.assets.push_back(AssetChange{add->asset.id, model.assets.size(), std::nullopt, add->asset});
			return transaction;
		}
		if (const auto* change = std::get_if<ChangeAssetCommand>(&command))
		{
			const std::optional<std::size_t> index = findIndex(model.assets, change->asset.id);
			if (!index)
			{
				return assetNotFound(change->asset.id);
			}
			const Asset& before = model.assets[*index];
			if (change->asset.kind != before.kind || change->asset.file != before.file)
			{
				return Error{ErrorCode::InvalidArgument, "An asset keeps its kind and its file"};
			}
			if (Status valid = validateAsset(change->asset); !valid)
			{
				return valid.error();
			}
			Project after = model;
			after.assets[*index] = change->asset;
			if (Status valid = validateProject(after); !valid)
			{
				return valid.error(); // e.g. a texture no longer allowed on a surface it is used on
			}
			Transaction transaction{label, {}, {}, {}};
			if (!(change->asset == before))
			{
				transaction.assets.push_back(AssetChange{before.id, *index, before, change->asset});
			}
			return transaction;
		}
		if (const auto* removal = std::get_if<DeleteAssetCommand>(&command))
		{
			const std::optional<std::size_t> index = findIndex(model.assets, removal->asset);
			if (!index)
			{
				return assetNotFound(removal->asset);
			}
			if (const std::size_t uses = assetUses(model, removal->asset); uses > 0)
			{
				return Error{ErrorCode::InvalidArgument, "'" + model.assets[*index].name + "' is in use (" + std::to_string(uses) +
					(uses == 1 ? " surface or piece of furniture)" : " surfaces or pieces of furniture)")};
			}
			Transaction transaction{label, {}, {}, {}};
			transaction.assets.push_back(AssetChange{removal->asset, *index, model.assets[*index], std::nullopt});
			return transaction;
		}
		if (const auto* finish = std::get_if<SetWallFinishCommand>(&command))
		{
			if (Status valid = validateFinishIn(model, finish->finish, Surface::Wall); !valid)
			{
				return valid.error();
			}
			return modifyWall(model, finish->wall, label, [finish](Wall& wall)
			{
				wall.finish(finish->side) = finish->finish;
				return Status::success();
			});
		}
		if (const auto* setting = std::get_if<SetDimensionCommand>(&command))
		{
			Dimension entry = setting->dimension;
			if (entry.id.empty()) entry.id = toString(newUuid());
			const auto level = resolveLevel(model, entry.level);
			if (!level) return level.error();
			entry.level = level.value();
			if (Status valid = validateDimension(model, entry); !valid) return valid.error();
			const auto index = findIndex(model.dimensions, entry.id);
			const std::optional<Dimension> before = index ? std::optional<Dimension>(model.dimensions[*index]) : std::nullopt;
			Transaction transaction{label, {}, {}};
			if (before != std::optional<Dimension>(entry))
				transaction.dimensions.push_back({entry.id, index.value_or(model.dimensions.size()), before, entry});
			return transaction;
		}
		if (const auto* conversion = std::get_if<MakeDimensionManualCommand>(&command))
		{
			Dimension hidden = conversion->dimension;
			if (!hidden.automatic || hidden.hidden) return Error{ErrorCode::InvalidArgument, "Choose a visible automatic dimension"};
			if (Status valid = validateDimension(model, hidden); !valid) return valid.error();
			Dimension manual = hidden;
			manual.id = toString(newUuid());
			manual.automatic = false;
			hidden.hidden = true;
			const auto index = findIndex(model.dimensions, hidden.id);
			const std::optional<Dimension> before = index ? std::optional<Dimension>(model.dimensions[*index]) : std::nullopt;
			Transaction transaction{label, {}, {}};
			transaction.dimensions.push_back({hidden.id, index.value_or(model.dimensions.size()), before, hidden});
			transaction.dimensions.push_back({manual.id, model.dimensions.size() + (index ? 0 : 1), std::nullopt, manual});
			return transaction;
		}
		if (const auto* removal = std::get_if<DeleteDimensionCommand>(&command))
		{
			const auto index = findIndex(model.dimensions, removal->id);
			if (!index) return Error{ErrorCode::NotFound, "No dimension with id " + removal->id};
			Transaction transaction{label, {}, {}};
			transaction.dimensions.push_back({removal->id, *index, model.dimensions[*index], std::nullopt});
			return transaction;
		}
		if (const auto* reset = std::get_if<ResetDimensionsCommand>(&command))
		{
			const auto level = resolveLevel(model, reset->level);
			if (!level) return level.error();
			Transaction transaction{label, {}, {}};
			for (std::size_t i = 0; i < model.dimensions.size(); ++i)
			{
				const auto& entry = model.dimensions[i];
				if (entry.automatic && entry.level == level.value()) transaction.dimensions.push_back({entry.id, i, entry, std::nullopt});
			}
			return transaction;
		}
		if (const auto* setting = std::get_if<SetRoomBaseboardCommand>(&command))
		{
			const RoomBaseboard& entry = setting->baseboard;
			if (Status valid = validateRoomBaseboard(model, entry); !valid) return valid.error();
			const auto index = findIndex(model.roomBaseboards, entry.room);
			const std::optional<RoomBaseboard> before = index ? std::optional<RoomBaseboard>(model.roomBaseboards[*index]) : std::nullopt;
			const std::optional<RoomBaseboard> after = entry;
			Transaction transaction{label, {}, {}, {}};
			if (before != after) transaction.roomBaseboards.push_back({entry.room, index.value_or(model.roomBaseboards.size()), before, after});
			return transaction;
		}
		if (const auto* rename = std::get_if<SetRoomNameCommand>(&command))
		{
			if (rename->room.empty()) return Error{ErrorCode::InvalidArgument, "No room"};
			if (Status valid = validateRoomName(rename->name); !valid) return valid.error();
			const auto first = rename->name.find_first_not_of(' ');
			const std::string name = first == std::string::npos ? std::string() : rename->name.substr(first, rename->name.find_last_not_of(' ') - first + 1);
			const auto index = findIndex(model.roomNames, rename->room);
			const std::optional<RoomName> before = index ? std::optional<RoomName>(model.roomNames[*index]) : std::nullopt;
			const std::optional<RoomName> after = name.empty() ? std::nullopt : std::optional<RoomName>(RoomName{rename->room, name});
			Transaction transaction{label, {}, {}, {}};
			if (before != after) transaction.roomNames.push_back({rename->room, index.value_or(model.roomNames.size()), before, after});
			return transaction;
		}
		if (const auto* roomFinish = std::get_if<SetRoomFinishCommand>(&command))
		{
			if (roomFinish->room.empty())
			{
				return Error{ErrorCode::InvalidArgument, "No room"};
			}
			if (Status valid = validateFinishIn(model, roomFinish->floor, Surface::Floor); !valid)
			{
				return valid.error();
			}
			if (Status valid = validateFinishIn(model, roomFinish->ceiling, Surface::Ceiling); !valid)
			{
				return valid.error();
			}
			const std::optional<std::size_t> index = findIndex(model.roomFinishes, roomFinish->room);
			std::optional<RoomFinish> before;
			if (index)
			{
				before = model.roomFinishes[*index];
			}
			std::optional<RoomFinish> after;
			if (!roomFinish->floor.empty() || !roomFinish->ceiling.empty())
			{
				after = RoomFinish{roomFinish->room, roomFinish->floor, roomFinish->ceiling};
			}
			Transaction transaction{label, {}, {}, {}};
			if (before != after)
			{
				transaction.roomFinishes.push_back(
					RoomFinishChange{roomFinish->room, index.value_or(model.roomFinishes.size()), before, after});
			}
			return transaction;
		}
		if (const auto* thickness = std::get_if<ChangeWallThicknessCommand>(&command))
		{
			return modifyWall(model, thickness->wall, label, [thickness](Wall& wall)
			{
				wall.thicknessMm = thickness->thicknessMm;
				return Status::success();
			});
		}
		const auto& height = std::get<ChangeWallHeightCommand>(command);
		return modifyWall(model, height.wall, label, [&height](Wall& wall)
		{
			wall.heightMm = height.heightMm;
			return Status::success();
		});
	}

	Result<Transaction> Document::execute(const Command& command)
	{
		Result<Transaction> built = buildTransaction(command);
		if (!built)
		{
			return built;
		}
		Transaction transaction = built.value();
		// Geometry edits split only the junctions involving an edited axis. Opening/finish edits and loaded projects
		// retain their topology. Build on a copy so a blocked cut never applies half of an operation (ADR-0025).
		std::vector<WallId> edited;
		for (const WallChange& change : transaction.walls)
		{
			if (change.after && (!change.before || change.before->axis() != change.after->axis())) edited.push_back(change.id);
		}
		if (!edited.empty())
		{
			Project prospective = model;
			applyChanges(prospective.walls, transaction.walls, true);
			const std::size_t unsplitCount = prospective.walls.size();
			if (Status valid = splitJoinedWalls(prospective, edited, newUuid); !valid) return valid.error();
			const bool roomSettings = !model.roomFinishes.empty() || !model.roomBaseboards.empty() || !model.roomNames.empty();
			if (roomTransfer && prospective.walls.size() > unsplitCount && roomSettings)
			{
				const RoomInheritance inherited = roomTransfer(model, prospective);
				inheritRoomEntries(prospective.roomFinishes, transaction.roomFinishes, inherited.finishes);
				inheritRoomEntries(prospective.roomBaseboards, transaction.roomBaseboards, inherited.baseboards);
				inheritRoomEntries(prospective.roomNames, transaction.roomNames, inherited.names);
			}
			if (Status valid = validateProject(prospective); !valid) return valid.error();
			// Keep the initiating change first: addWall and WallTool return that wall's retained identity.
			for (WallChange& change : transaction.walls)
			{
				if (change.after) change.after = *prospective.findWall(change.id);
			}
			for (std::size_t index = 0; index < prospective.walls.size(); ++index)
			{
				const Wall& after = prospective.walls[index];
				const auto recorded = std::find_if(transaction.walls.begin(), transaction.walls.end(),
					[&after](const WallChange& change) { return change.id == after.id; });
				if (recorded != transaction.walls.end()) continue;
				const Wall* before = model.findWall(after.id);
				if (!before || *before != after) transaction.walls.push_back({after.id, index,
					before ? std::optional<Wall>(*before) : std::nullopt, after});
			}
		}
		std::erase_if(transaction.walls, [](const WallChange& change) { return change.before == change.after; });
		if (transaction.empty())
		{
			return transaction; // nothing changes: no history entry, no notification
		}
		apply(transaction, true);
		undoStack.push_back(transaction);
		redoStack.clear();
		return transaction;
	}

	Status Document::undo()
	{
		if (undoStack.empty())
		{
			return Error{ErrorCode::NotFound, "Nothing to undo"};
		}
		Transaction transaction = std::move(undoStack.back());
		undoStack.pop_back();
		apply(transaction, false);
		redoStack.push_back(std::move(transaction));
		return Status::success();
	}

	Status Document::redo()
	{
		if (redoStack.empty())
		{
			return Error{ErrorCode::NotFound, "Nothing to redo"};
		}
		Transaction transaction = std::move(redoStack.back());
		redoStack.pop_back();
		apply(transaction, true);
		undoStack.push_back(std::move(transaction));
		return Status::success();
	}

	std::string_view Document::undoLabel() const
	{
		return undoStack.empty() ? std::string_view() : std::string_view(undoStack.back().label);
	}

	std::string_view Document::redoLabel() const
	{
		return redoStack.empty() ? std::string_view() : std::string_view(redoStack.back().label);
	}

	Status Document::replaceProject(Project replacement)
	{
		if (Status valid = validateProject(replacement); !valid)
		{
			return valid;
		}
		ChangeSet changes;
		for (const Wall& wall : model.walls)
		{
			changes.removedWalls.push_back(wall.id);
		}
		for (const Wall& wall : replacement.walls)
		{
			changes.addedWalls.push_back(wall.id);
		}
		for (const Level& level : model.levels)
		{
			changes.removedLevels.push_back(level.id);
		}
		for (const Level& level : replacement.levels)
		{
			changes.addedLevels.push_back(level.id);
		}
		for (const Project* side : {&model, &replacement})
		{
			for (const Dimension& entry : side->dimensions) changes.changedDimensions.push_back(entry.id);
			for (const RoomBaseboard& entry : side->roomBaseboards) changes.changedRoomBaseboards.push_back(entry.room);
			for (const RoomName& entry : side->roomNames) changes.changedRoomNames.push_back(entry.room);
			for (const RoomFinish& finish : side->roomFinishes)
			{
				changes.changedRoomFinishes.push_back(finish.room);
			}
		}
		for (const Furniture& piece : model.furniture)
		{
			changes.removedFurniture.push_back(piece.id);
		}
		for (const Furniture& piece : replacement.furniture)
		{
			changes.addedFurniture.push_back(piece.id);
		}
		for (const Asset& asset : model.assets)
		{
			changes.removedAssets.push_back(asset.id);
		}
		for (const Asset& asset : replacement.assets)
		{
			changes.addedAssets.push_back(asset.id);
		}
		for (const Column& column : model.columns)
		{
			changes.removedColumns.push_back(column.id);
		}
		for (const Column& column : replacement.columns)
		{
			changes.addedColumns.push_back(column.id);
		}
		for (const ServiceRun& run : model.services)
		{
			changes.removedServiceRuns.push_back(run.id);
		}
		for (const ServiceRun& run : replacement.services)
		{
			changes.addedServiceRuns.push_back(run.id);
		}
		model = std::move(replacement);
		undoStack.clear();
		redoStack.clear();
		notify(changes);
		return Status::success();
	}

	Result<WallId> Document::addWall(Point2D start, Point2D end, std::int64_t thicknessMm, std::int64_t heightMm,
		const LevelId& level)
	{
		const Result<Transaction> applied = execute(CreateWallCommand{start, end, thicknessMm, heightMm, level});
		if (!applied)
		{
			return applied.error();
		}
		return applied.value().walls.front().id;
	}

	Status Document::setWallEnd(const WallId& id, Point2D end)
	{
		const Result<Transaction> applied = execute(MoveWallEndpointCommand{id, WallEndpoint::End, end});
		return applied ? Status::success() : Status(applied.error());
	}

	Status Document::removeWall(const WallId& id)
	{
		const Result<Transaction> applied = execute(DeleteWallCommand{id});
		return applied ? Status::success() : Status(applied.error());
	}

	Result<OpeningId> Document::addOpening(const WallId& wall, const Opening& opening)
	{
		const Result<Transaction> applied = execute(AddOpeningCommand{wall, opening});
		if (!applied)
		{
			return applied.error();
		}
		// The new opening is the one the wall did not have before.
		const WallChange& change = applied.value().walls.front();
		for (const Opening& added : change.after->openings)
		{
			if (!change.before->findOpening(added.id))
			{
				return added.id;
			}
		}
		F101_ASSERT(false);
		return Error{ErrorCode::NotFound, "Opening not added"};
	}

	Result<LevelId> Document::addLevel(std::int64_t heightMm, std::int64_t slabThicknessMm)
	{
		const Result<Transaction> applied = execute(AddLevelCommand{heightMm, slabThicknessMm, {}});
		if (!applied)
		{
			return applied.error();
		}
		return applied.value().levels.front().id;
	}

	Result<FurnitureId> Document::addFurniture(const Furniture& furniture)
	{
		const Result<Transaction> applied = execute(AddFurnitureCommand{furniture});
		if (!applied)
		{
			return applied.error();
		}
		return applied.value().furniture.front().id;
	}

	Result<ColumnId> Document::addColumn(const Column& column)
	{
		const Result<Transaction> applied = execute(AddColumnCommand{column});
		if (!applied)
		{
			return applied.error();
		}
		return applied.value().columns.front().id;
	}

	Result<ServiceRunId> Document::addServiceRun(const ServiceRun& run)
	{
		const Result<Transaction> applied = execute(AddServiceRunCommand{run});
		if (!applied)
		{
			return applied.error();
		}
		return applied.value().services.front().id;
	}

	void Document::apply(const Transaction& transaction, bool forward)
	{
		// Levels and assets first: walls, finishes and furniture being restored refer to them.
		applyChanges(model.levels, transaction.levels, forward);
		applyChanges(model.assets, transaction.assets, forward); // before what uses them
		applyChanges(model.walls, transaction.walls, forward);
		applyChanges(model.roomFinishes, transaction.roomFinishes, forward);
		applyChanges(model.roomNames, transaction.roomNames, forward);
		applyChanges(model.roomBaseboards, transaction.roomBaseboards, forward);
		applyChanges(model.dimensions, transaction.dimensions, forward);
		applyChanges(model.furniture, transaction.furniture, forward);
		applyChanges(model.columns, transaction.columns, forward);
		applyChanges(model.services, transaction.services, forward);
		notify(changeSetOf(transaction, forward));
	}

	std::vector<AxisSpan> Document::openingObstructions(const Project& levelWalls, const Wall& wall) const
	{
		return obstructions ? obstructions(levelWalls, wall) : std::vector<AxisSpan>();
	}

	Status Document::checkOpeningClear(const Transaction& transaction, const OpeningId& opening) const
	{
		if (!obstructions || transaction.walls.empty() || !transaction.walls.front().after)
		{
			return Status::success();
		}
		const Wall& after = *transaction.walls.front().after;
		const Opening* checked = after.findOpening(opening);
		if (!checked)
		{
			return Status::success();
		}
		// Only where what it takes moves or grows (a pocket counts, ADR-0014): a finish change never fails because of a
		// joint made after the opening.
		const std::optional<Wall>& beforeWall = transaction.walls.front().before;
		const Opening* previous = beforeWall ? beforeWall->findOpening(opening) : nullptr;
		const AxisSpan taken = occupiedSpan(*checked);
		if (previous && occupiedSpan(*previous) == taken)
		{
			return Status::success();
		}
		// The level as it would be, with the changed wall.
		Project level = wallsOnLevel(model, model.levelOf(after));
		for (Wall& wall : level.walls)
		{
			if (wall.id == after.id)
			{
				wall = after;
			}
		}
		for (const AxisSpan& span : obstructions(level, after))
		{
			if (taken.fromMm < span.toMm && taken.toMm > span.fromMm)
			{
				const bool hole = checked->offsetMm < span.toMm && checked->offsetMm + checked->widthMm > span.fromMm;
				return Error{ErrorCode::OutOfRange, std::string(!hole ? "The sliding door's pocket" :
					checked->kind == OpeningKind::Door ? "The door" : "The window") + " runs into a wall joined to this one (" +
					std::to_string(span.fromMm) + " to " + std::to_string(span.toMm) + " mm along it)"};
			}
		}
		return Status::success();
	}

	std::size_t Document::subscribe(ChangeListener listener)
	{
		F101_ASSERT(static_cast<bool>(listener));
		const std::size_t handle = nextListenerHandle++;
		listeners.emplace_back(handle, std::move(listener));
		return handle;
	}

	void Document::unsubscribe(std::size_t handle)
	{
		std::erase_if(listeners, [handle](const auto& entry) { return entry.first == handle; });
	}

	void Document::notify(const ChangeSet& changes) const
	{
		// Copy: a listener may subscribe or unsubscribe while being notified.
		const auto snapshot = listeners;
		for (const auto& entry : snapshot)
		{
			entry.second(changes);
		}
	}
}
