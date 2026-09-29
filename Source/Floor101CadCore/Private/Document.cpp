#include "F101/Cad/Document.h"

#include "F101/Cad/Snapping.h"
#include "F101/Cad/Units.h"

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

		std::optional<std::size_t> findWallIndex(const Project& project, const WallId& id)
		{
			for (std::size_t index = 0; index < project.walls.size(); ++index)
			{
				if (project.walls[index].id == id)
				{
					return index;
				}
			}
			return std::nullopt;
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
			Transaction transaction{std::move(label), {}};
			if (!(after == before))
			{
				transaction.walls.push_back(WallChange{id, *index, before, after});
			}
			return transaction;
		}

		ChangeSet changeSetOf(const Transaction& transaction, bool forward)
		{
			ChangeSet changes;
			for (const WallChange& change : transaction.walls)
			{
				const bool existedBefore = forward ? change.before.has_value() : change.after.has_value();
				const bool existsAfter = forward ? change.after.has_value() : change.before.has_value();
				if (!existedBefore && existsAfter)
				{
					changes.addedWalls.push_back(change.id);
				}
				else if (existedBefore && !existsAfter)
				{
					changes.removedWalls.push_back(change.id);
				}
				else
				{
					changes.modifiedWalls.push_back(change.id);
				}
			}
			return changes;
		}

		struct CommandLabeler
		{
			std::string_view operator()(const CreateWallCommand&) const { return "Create wall"; }
			std::string_view operator()(const DeleteWallCommand&) const { return "Delete wall"; }
			std::string_view operator()(const MoveWallEndpointCommand&) const { return "Move wall endpoint"; }
			std::string_view operator()(const MoveJointCommand&) const { return "Move corner"; }
			std::string_view operator()(const ChangeWallLengthCommand&) const { return "Change wall length"; }
			std::string_view operator()(const ChangeWallThicknessCommand&) const { return "Change wall thickness"; }
			std::string_view operator()(const ChangeWallHeightCommand&) const { return "Change wall height"; }
		};
	}

	std::string_view commandLabel(const Command& command)
	{
		return std::visit(CommandLabeler{}, command);
	}

	Status validateProject(const Project& project)
	{
		std::set<WallId> ids;
		for (const Wall& wall : project.walls)
		{
			if (Status valid = validateWall(wall); !valid)
			{
				return valid;
			}
			if (!ids.insert(wall.id).second)
			{
				return Error{ErrorCode::InvalidArgument, "Duplicate wall id " + toString(wall.id)};
			}
		}
		return Status::success();
	}

	Document::Document(UuidSource newUuidSource)
		: newUuid(std::move(newUuidSource))
	{
		F101_ASSERT(static_cast<bool>(newUuid));
	}

	Result<Transaction> Document::buildTransaction(const Command& command)
	{
		const std::string label(commandLabel(command));

		if (const auto* create = std::get_if<CreateWallCommand>(&command))
		{
			const Wall wall{WallId{newUuid()}, create->start, create->end, create->thicknessMm, create->heightMm};
			if (Status valid = validateWall(wall); !valid)
			{
				return valid.error();
			}
			F101_ASSERT(!findWallIndex(model, wall.id).has_value());
			return Transaction{label, {WallChange{wall.id, model.walls.size(), std::nullopt, wall}}};
		}
		if (const auto* remove = std::get_if<DeleteWallCommand>(&command))
		{
			const std::optional<std::size_t> index = findWallIndex(model, remove->wall);
			if (!index)
			{
				return wallNotFound(remove->wall);
			}
			return Transaction{label, {WallChange{remove->wall, *index, model.walls[*index], std::nullopt}}};
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
			Transaction transaction{label, {}};
			bool found = false;
			for (std::size_t index = 0; index < model.walls.size(); ++index)
			{
				const Wall& before = model.walls[index];
				if (before.start != joint->from && before.end != joint->from)
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
		const Transaction& transaction = built.value();
		if (transaction.walls.empty())
		{
			return built; // nothing changes: no history entry, no notification
		}
		apply(transaction, true);
		undoStack.push_back(transaction);
		redoStack.clear();
		return built;
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
		model = std::move(replacement);
		undoStack.clear();
		redoStack.clear();
		notify(changes);
		return Status::success();
	}

	Result<WallId> Document::addWall(Point2D start, Point2D end, std::int64_t thicknessMm, std::int64_t heightMm)
	{
		const Result<Transaction> applied = execute(CreateWallCommand{start, end, thicknessMm, heightMm});
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

	void Document::apply(const Transaction& transaction, bool forward)
	{
		// Removals first (from the back), then insertions/replacements in index order, so indices stay valid.
		std::vector<const WallChange*> ordered;
		for (const WallChange& change : transaction.walls)
		{
			ordered.push_back(&change);
		}
		std::sort(ordered.begin(), ordered.end(), [](const WallChange* a, const WallChange* b) { return a->index < b->index; });

		for (auto it = ordered.rbegin(); it != ordered.rend(); ++it)
		{
			const std::optional<Wall>& target = forward ? (*it)->after : (*it)->before;
			if (!target)
			{
				const std::optional<std::size_t> index = findWallIndex(model, (*it)->id);
				F101_ASSERT(index.has_value());
				model.walls.erase(model.walls.begin() + static_cast<std::ptrdiff_t>(*index));
			}
		}
		for (const WallChange* change : ordered)
		{
			const std::optional<Wall>& target = forward ? change->after : change->before;
			if (!target)
			{
				continue;
			}
			if (const std::optional<std::size_t> index = findWallIndex(model, change->id))
			{
				model.walls[*index] = *target;
			}
			else
			{
				const std::size_t position = std::min(change->index, model.walls.size());
				model.walls.insert(model.walls.begin() + static_cast<std::ptrdiff_t>(position), *target);
			}
		}
		notify(changeSetOf(transaction, forward));
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
