#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Project.h"
#include "F101/Cad/Result.h"

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace f101::cad
{
	// Entities affected by one successful mutation.
	struct ChangeSet
	{
		std::vector<WallId> addedWalls;
		std::vector<WallId> modifiedWalls;
		std::vector<WallId> removedWalls;
	};

	using ChangeListener = std::function<void(const ChangeSet&)>;

	// A project being edited. Every mutation is validated, and a rejected mutation leaves the project untouched.
	// Listeners run synchronously after each successful mutation (geometry/scene regeneration hooks in here).
	// Commands with undo/redo (MVP 1) are built on top of this API.
	class Document
	{
	public:
		FLOOR101CADCORE_API explicit Document(UuidSource newUuid);

		const Project& project() const { return model; }

		FLOOR101CADCORE_API Result<WallId> addWall(Point2D start, Point2D end, std::int64_t thicknessMm, std::int64_t heightMm);
		FLOOR101CADCORE_API Status setWallEnd(const WallId& id, Point2D end);
		FLOOR101CADCORE_API Status removeWall(const WallId& id);

		// Returns a handle for unsubscribe().
		FLOOR101CADCORE_API std::size_t subscribe(ChangeListener listener);
		FLOOR101CADCORE_API void unsubscribe(std::size_t handle);

	private:
		Wall* findWall(const WallId& id);
		void notify(const ChangeSet& changes) const;

		Project model;
		UuidSource newUuid;
		std::vector<std::pair<std::size_t, ChangeListener>> listeners;
		std::size_t nextListenerHandle = 1;
	};
}
