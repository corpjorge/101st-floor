#include "F101/Cad/Document.h"

#include <algorithm>
#include <string>

namespace f101::cad
{
	namespace
	{
		Error wallNotFound(const WallId& id)
		{
			return Error{ErrorCode::NotFound, "No wall with id " + toString(id)};
		}
	}

	Document::Document(UuidSource newUuidSource)
		: newUuid(std::move(newUuidSource))
	{
		F101_ASSERT(static_cast<bool>(newUuid));
	}

	Result<WallId> Document::addWall(Point2D start, Point2D end, std::int64_t thicknessMm, std::int64_t heightMm)
	{
		const Wall wall{WallId{newUuid()}, start, end, thicknessMm, heightMm};
		if (Status valid = validateWall(wall); !valid)
		{
			return valid.error();
		}
		F101_ASSERT(findWall(wall.id) == nullptr);
		model.walls.push_back(wall);
		notify(ChangeSet{{wall.id}, {}, {}});
		return wall.id;
	}

	Status Document::setWallEnd(const WallId& id, Point2D end)
	{
		Wall* wall = findWall(id);
		if (wall == nullptr)
		{
			return wallNotFound(id);
		}
		Wall edited = *wall;
		edited.end = end;
		if (Status valid = validateWall(edited); !valid)
		{
			return valid;
		}
		*wall = edited;
		notify(ChangeSet{{}, {id}, {}});
		return Status::success();
	}

	Status Document::removeWall(const WallId& id)
	{
		const auto it = std::find_if(model.walls.begin(), model.walls.end(), [&id](const Wall& wall) { return wall.id == id; });
		if (it == model.walls.end())
		{
			return wallNotFound(id);
		}
		model.walls.erase(it);
		notify(ChangeSet{{}, {}, {id}});
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

	Wall* Document::findWall(const WallId& id)
	{
		for (Wall& wall : model.walls)
		{
			if (wall.id == id)
			{
				return &wall;
			}
		}
		return nullptr;
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
