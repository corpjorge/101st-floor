#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Finishes.h"
#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Furniture (ADR-0016): parametric pieces from a fixed catalogue, placed on a level. Their shape is derived from the
// type and the dimensions in the Geometry Engine; the model only stores what to build and where.
namespace f101::cad
{
	// A furniture type of the catalogue: its default size, the sizes it allows and how it is placed.
	struct FurnitureType
	{
		std::string_view id; // e.g. "sofa"
		std::int64_t widthMm = 0;  // along its front
		std::int64_t depthMm = 0;  // front to back
		std::int64_t heightMm = 0;
		std::int64_t minWidthMm = 0;
		std::int64_t maxWidthMm = 0;
		std::int64_t minDepthMm = 0;
		std::int64_t maxDepthMm = 0;
		std::int64_t minHeightMm = 0;
		std::int64_t maxHeightMm = 0;
		bool againstWall = false;   // placed near a wall, its back goes against it
		std::string_view bodyFinish; // finish of its main parts unless the piece has its own
	};

	// Every furniture type, in display order.
	FLOOR101CADCORE_API const std::vector<FurnitureType>& furnitureCatalogue();
	// Null if the id is not in the catalogue.
	FLOOR101CADCORE_API const FurnitureType* findFurnitureType(std::string_view id);

	inline constexpr std::int64_t FullTurnCentidegrees = 36000;

	// A piece of furniture on a level. Its footprint is a width x depth rectangle centred on `position`; in its own frame
	// the width runs along x, the depth along y with the back at +y (so it faces -y), turned counter-clockwise by
	// `rotationCentidegrees` (hundredths of a degree: whole numbers keep any wall's angle to well under a millimetre).
	struct Furniture
	{
		FurnitureId id;
		std::string type;
		LevelId level;
		Point2D position; // plan, mm, local to its level
		std::int64_t rotationCentidegrees = 0; // [0, 36000)
		std::int64_t widthMm = 0;
		std::int64_t depthMm = 0;
		std::int64_t heightMm = 0;
		FinishId finish; // its main parts; empty: the type's body finish

		friend bool operator==(const Furniture&, const Furniture&) = default;
	};

	// A piece of `type` with its default size at a place (id and level unset).
	FLOOR101CADCORE_API Furniture makeFurniture(std::string_view type, Point2D position, std::int64_t rotationCentidegrees = 0);

	// The finish its main parts show: its own, else its type's.
	FLOOR101CADCORE_API std::string_view resolveFurnitureFinish(const Furniture& furniture);

	// A known type, dimensions inside the type's ranges, a rotation in [0, 36000), a position in the model range and a
	// finish for furniture. Its identity and level are checked with the project.
	FLOOR101CADCORE_API Status validateFurniture(const Furniture& furniture);

	// Whole hundredths of a degree in [0, 36000) for any angle in degrees.
	FLOOR101CADCORE_API std::int64_t toCentidegrees(double degrees);

	// Its own frame in plan: `x` (along its width) and `y` (towards its back) as unit vectors.
	struct FurnitureAxes
	{
		PlanPoint x;
		PlanPoint y;
	};

	FLOOR101CADCORE_API FurnitureAxes furnitureAxes(const Furniture& furniture);
	// The plan point of (x, y) in its own frame (mm, from the centre of its footprint).
	FLOOR101CADCORE_API PlanPoint furnitureToPlan(const Furniture& furniture, double x, double y);
	// A plan point in its own frame.
	FLOOR101CADCORE_API PlanPoint planToFurniture(const Furniture& furniture, PlanPoint point);
	// The corners of its footprint, counter-clockwise from front left: (-w/2, -d/2), (w/2, -d/2), (w/2, d/2), (-w/2, d/2).
	FLOOR101CADCORE_API std::array<PlanPoint, 4> furnitureFootprint(const Furniture& furniture);
	// Whether a plan point is on its footprint, grown by `marginMm` on every side.
	FLOOR101CADCORE_API bool furnitureContains(const Furniture& furniture, PlanPoint point, double marginMm = 0.0);
}
