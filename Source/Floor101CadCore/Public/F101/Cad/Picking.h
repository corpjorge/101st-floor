#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Project.h"
#include "F101/Cad/Snapping.h"

#include <optional>
#include <vector>

// Geometric queries used by interaction (selection, cursor on the floor). Engine-independent: callers convert their
// screen ray to scene space first (millimetres, right-handed, Z up).
namespace f101::cad
{
	struct Ray3
	{
		double originX = 0.0;
		double originY = 0.0;
		double originZ = 0.0;
		double directionX = 0.0;
		double directionY = 0.0;
		double directionZ = -1.0;
	};

	struct WallHit
	{
		WallId wall;
		double distance = 0.0; // along the ray, in units of the ray direction's length
	};

	// Nearest wall whose solid (axis-centred box from floor to height) the ray enters in front of its origin. Walls in
	// `ignoreWalls` are skipped.
	FLOOR101CADCORE_API std::optional<WallHit> pickWall(const Project& project, const Ray3& ray,
		const std::vector<WallId>& ignoreWalls = {});

	struct FurnitureHit
	{
		FurnitureId furniture;
		double distance = 0.0; // along the ray, in units of the ray direction's length
	};

	// Nearest piece of furniture on `level` whose box (its footprint, from the floor to its height) the ray enters in front
	// of its origin. The ray is in the level's space (its floor at z = 0).
	FLOOR101CADCORE_API std::optional<FurnitureHit> pickFurniture(const Project& project, const Ray3& ray,
		const LevelId& level);

	struct ColumnHit
	{
		ColumnId column;
		double distance = 0.0; // along the ray, in units of the ray direction's length
	};

	// Nearest column on `level` whose solid (its outline from the floor up to the level's height) the ray enters in front
	// of its origin. The ray is in the level's space (its floor at z = 0).
	FLOOR101CADCORE_API std::optional<ColumnHit> pickColumn(const Project& project, const Ray3& ray, const LevelId& level);

	struct ServiceRunHit
	{
		ServiceRunId run;
		double distance = 0.0; // along the ray to where it passes closest to the run, in units of the direction's length
	};

	// Nearest run on `level` that the ray passes within its pipe's radius plus `toleranceMm` of, in front of its origin.
	// The ray is in the level's space.
	FLOOR101CADCORE_API std::optional<ServiceRunHit> pickServiceRun(const Project& project, const Ray3& ray,
		const LevelId& level, double toleranceMm);

	// Entry distance of the ray into an axis-aligned square post centred on `center` (e.g. an endpoint handle), from the
	// floor to `heightMm`, `sizeMm` wide; nullopt if it misses.
	FLOOR101CADCORE_API std::optional<double> intersectPost(const Ray3& ray, Point2D center, double sizeMm, double heightMm);

	// Where the ray meets the floor plane z = 0, if it goes down to it.
	FLOOR101CADCORE_API std::optional<PlanPoint> intersectFloor(const Ray3& ray);
}
