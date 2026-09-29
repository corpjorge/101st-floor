#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Project.h"
#include "F101/Cad/Result.h"

#include <cstdint>
#include <vector>

// Snapping belongs to the CAD core, not to the renderer (master prompt §20).
namespace f101::cad
{
	struct SnapSettings
	{
		std::int64_t gridMm = 50;           // 0 disables the grid
		double angleStepDegrees = 15.0;     // 0 disables angle snapping; 15 covers 0/15/30/45/90 degrees
		double endpointToleranceMm = 150.0; // radius around existing wall endpoints; 0 disables
		double wallToleranceMm = 150.0;     // distance to a wall axis that snaps onto it (T junctions); 0 disables
	};

	enum class SnapKind
	{
		Free,
		Grid,
		Endpoint,
		Wall, // onto another wall's axis
		Angle,
	};

	struct SnapResult
	{
		Point2D point;
		SnapKind kind = SnapKind::Free;
		// The cursor after the snap's constraint but before any rounding: the endpoint itself, the cursor projected onto
		// the snapped angle, or the cursor. Its direction from the anchor is exact, so a typed length follows it.
		PlanPoint constrained;
	};

	// Snaps a free point (e.g. a wall start): the nearest existing wall endpoint within tolerance, else the nearest point
	// on a wall's axis within tolerance, else the grid. Walls in `ignoreWalls` are skipped.
	FLOOR101CADCORE_API SnapResult snapPoint(const Project& project, PlanPoint cursor, const SnapSettings& settings,
		const std::vector<WallId>& ignoreWalls = {});

	// Snaps the second point of a segment that starts at `anchor`: an existing endpoint within tolerance (other than the
	// anchor); else a wall's axis within tolerance, where the angle-snapped direction crosses it if that is near the
	// cursor, otherwise at its nearest point; else the direction is rounded to the angle step and the length to the
	// grid. Walls in `ignoreWalls` are skipped, and so is any wall whose body holds the anchor.
	FLOOR101CADCORE_API SnapResult snapFromAnchor(const Project& project, Point2D anchor, PlanPoint cursor,
		const SnapSettings& settings, const std::vector<WallId>& ignoreWalls = {});

	// Point at `lengthMm` from `anchor` in the direction of `toward` (+X if toward == anchor), in whole millimetres.
	// Exact along the axes. Otherwise, of the four millimetre points around the exact one, the one whose distance to the
	// anchor is closest to lengthMm: the length error stays under 0.5 mm (it reads as the typed length at 1 mm
	// resolution), where rounding each coordinate on its own could reach 0.7 mm.
	FLOOR101CADCORE_API Result<Point2D> pointAtDistance(Point2D anchor, PlanPoint toward, std::int64_t lengthMm);
}
