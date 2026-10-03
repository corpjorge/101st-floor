#pragma once

#include "F101/Cad/Asset.h"
#include "F101/Cad/Column.h"
#include "F101/Cad/Export.h"
#include "F101/Cad/Finishes.h"
#include "F101/Cad/Furniture.h"
#include "F101/Cad/Level.h"
#include "F101/Cad/Services.h"
#include "F101/Cad/Wall.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace f101::cad
{
	// Finishes of a room (ADR-0012). Rooms are derived (ADR-0009), so their finishes are kept by room key: they stay
	// with the room while its key does (while its lowest-id wall bounds it). Empty: the surface's default.
	struct RoomFinish
	{
		std::string room; // geometry::Room::key
		FinishId floor;
		FinishId ceiling; // underside of the slab over the room

		friend bool operator==(const RoomFinish&, const RoomFinish&) = default;
	};

	// The editable architectural model: the source of truth of 101st Floor.
	struct Project
	{
		std::vector<Level> levels; // bottom to top; a valid project has at least one
		std::vector<Wall> walls;   // creation order
		std::vector<RoomFinish> roomFinishes; // only rooms with a finish of their own
		std::vector<Furniture> furniture;     // creation order (ADR-0016)
		std::vector<Asset> assets;            // imported textures and models, in import order (ADR-0017)
		std::vector<Column> columns;          // creation order (ADR-0021)
		std::vector<ServiceRun> services;     // runs of the building services, creation order (ADR-0021)

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

		const RoomFinish* findRoomFinish(const std::string& room) const
		{
			for (const RoomFinish& finish : roomFinishes)
			{
				if (finish.room == room)
				{
					return &finish;
				}
			}
			return nullptr;
		}

		const Furniture* findFurniture(const FurnitureId& id) const
		{
			for (const Furniture& piece : furniture)
			{
				if (piece.id == id)
				{
					return &piece;
				}
			}
			return nullptr;
		}

		const Column* findColumn(const ColumnId& id) const
		{
			for (const Column& column : columns)
			{
				if (column.id == id)
				{
					return &column;
				}
			}
			return nullptr;
		}

		const ServiceRun* findServiceRun(const ServiceRunId& id) const
		{
			for (const ServiceRun& run : services)
			{
				if (run.id == id)
				{
					return &run;
				}
			}
			return nullptr;
		}

		const Asset* findAsset(const AssetId& id) const
		{
			for (const Asset& asset : assets)
			{
				if (asset.id == id)
				{
					return &asset;
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

	// Where a wall of `levelWalls` must not have an opening because other walls join it there: the mitred ends, where
	// another wall meets it in a T, crossings (ADR-0013). Joins are derived in the Geometry Engine, which provides this
	// (geometry::makeWallObstructions); without it openings are only checked against the wall itself.
	using WallObstructions = std::function<std::vector<AxisSpan>(const Project& levelWalls, const Wall& wall)>;

	// Imported assets in use (ADR-0017).
	// A catalogue finish (validateFinish) or an imported texture of the project that allows the surface.
	FLOOR101CADCORE_API Status validateFinishIn(const Project& project, std::string_view finish, Surface surface);
	// Default size, ranges, placement and body finish of a furniture type: a catalogue type, or an imported model of the
	// project (its size read from the file, MinModelSizeMm to MaxModelSizeMm, against a wall as the asset says, no body
	// finish: it shows its own materials). The spec's id views `type`. nullopt for an unknown type.
	FLOOR101CADCORE_API std::optional<FurnitureType> furnitureSpec(const Project& project, std::string_view type);
	// validateFurniture, and its type (a catalogue type or a model of the project) and finish (validateFinishIn).
	FLOOR101CADCORE_API Status validateFurnitureIn(const Project& project, const Furniture& furniture);
	// validateColumn, and its finish (validateFinishIn for walls).
	FLOOR101CADCORE_API Status validateColumnIn(const Project& project, const Column& column);
	// How many wall faces, openings, room surfaces, pieces of furniture and columns use an asset.
	FLOOR101CADCORE_API std::size_t assetUses(const Project& project, const AssetId& asset);

	// Height of a level's floor above the first level's floor (the sum of the heights below it); 0 for an unknown level.
	FLOOR101CADCORE_API std::int64_t levelElevationMm(const Project& project, const LevelId& level);

	// The project with only the walls that stand on `level` (Project::levelOf), for per-level queries: picking,
	// joins and rooms never mix levels.
	FLOOR101CADCORE_API Project wallsOnLevel(const Project& project, const LevelId& level);

	// What snapping sees while drawing on `level`: its walls and, as guides to stack walls exactly, the walls of the level
	// just below it. Guides only attract the cursor; walls of different levels never join.
	FLOOR101CADCORE_API Project snapReference(const Project& project, const LevelId& level);
}
