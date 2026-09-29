#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Level.h"
#include "F101/Cad/Wall.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace f101::cad
{
	// The editable architectural model: the source of truth of 101st Floor.
	struct Project
	{
		std::vector<Level> levels; // bottom to top; a valid project has at least one
		std::vector<Wall> walls;   // creation order

		const Wall* findWall(const WallId& id) const
		{
			for (const Wall& wall : walls)
			{
				if (wall.id == id)
				{
					return &wall;
				}
			}
			return nullptr;
		}

		const Level* findLevel(const LevelId& id) const
		{
			for (const Level& level : levels)
			{
				if (level.id == id)
				{
					return &level;
				}
			}
			return nullptr;
		}

		std::optional<std::size_t> levelIndex(const LevelId& id) const
		{
			for (std::size_t index = 0; index < levels.size(); ++index)
			{
				if (levels[index].id == id)
				{
					return index;
				}
			}
			return std::nullopt;
		}

		// The level a wall stands on: its own, or the first level if it names none that exists (e.g. a wall built
		// without levels in a test or a preview); nil if the project has no levels.
		LevelId levelOf(const Wall& wall) const
		{
			if (findLevel(wall.level))
			{
				return wall.level;
			}
			return levels.empty() ? LevelId() : levels.front().id;
		}
	};

	// Height of a level's floor above the first level's floor (the sum of the heights below it); 0 for an unknown level.
	FLOOR101CADCORE_API std::int64_t levelElevationMm(const Project& project, const LevelId& level);

	// The project with only the walls that stand on `level` (Project::levelOf), for per-level queries: picking,
	// joins and rooms never mix levels.
	FLOOR101CADCORE_API Project wallsOnLevel(const Project& project, const LevelId& level);

	// What snapping sees while drawing on `level`: its walls and, as guides to stack walls exactly, the walls of the level
	// just below it. Guides only attract the cursor; walls of different levels never join.
	FLOOR101CADCORE_API Project snapReference(const Project& project, const LevelId& level);
}
