#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"

#include <cstdint>

namespace f101::cad
{
	// Parametric straight wall. The axis runs from start to end; the thickness is centred on the axis.
	// Geometry is always regenerated from these parameters, never stored or scaled.
	struct Wall
	{
		WallId id;
		Point2D start;
		Point2D end;
		std::int64_t thicknessMm = 0;
		std::int64_t heightMm = 0;

		friend bool operator==(const Wall&, const Wall&) = default;

		Segment2D axis() const { return Segment2D{start, end}; }
		double lengthMm() const { return axis().lengthMm(); }
	};

	// Invariants every wall in a project satisfies: identity plus validateWallGeometry.
	FLOOR101CADCORE_API Status validateWall(const Wall& wall);
	// Non-degenerate axis, positive dimensions, model range. Enough to build geometry (e.g. a preview without an id).
	FLOOR101CADCORE_API Status validateWallGeometry(const Wall& wall);
}
