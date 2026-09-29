#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace f101::cad
{
	enum class OpeningKind
	{
		Door,
		Window,
	};

	// "door" / "window" (project.json, tools).
	FLOOR101CADCORE_API std::string_view kindName(OpeningKind kind);

	// A door or window: a rectangular hole through its host wall (ADR-0010). Measured along the wall axis from the wall's
	// start to the opening's near edge; doors stand on the floor (sill 0).
	struct Opening
	{
		OpeningId id;
		OpeningKind kind = OpeningKind::Door;
		std::int64_t offsetMm = 0; // wall start -> opening edge, along the axis
		std::int64_t widthMm = 0;
		std::int64_t heightMm = 0;
		std::int64_t sillMm = 0; // floor -> bottom of the opening

		friend bool operator==(const Opening&, const Opening&) = default;
	};

	// Parametric straight wall. The axis runs from start to end; the thickness is centred on the axis. Its geometry is
	// local to its level (z = 0 at the level's floor).
	// Geometry is always regenerated from these parameters, never stored or scaled.
	struct Wall
	{
		WallId id;
		Point2D start;
		Point2D end;
		std::int64_t thicknessMm = 0;
		std::int64_t heightMm = 0;
		std::vector<Opening> openings; // sorted by offset, never overlapping
		LevelId level; // the floor it stands on (ADR-0011); its base is that level's elevation

		friend bool operator==(const Wall&, const Wall&) = default;

		Segment2D axis() const { return Segment2D{start, end}; }
		double lengthMm() const { return axis().lengthMm(); }

		const Opening* findOpening(const OpeningId& opening) const
		{
			for (const Opening& candidate : openings)
			{
				if (candidate.id == opening)
				{
					return &candidate;
				}
			}
			return nullptr;
		}
	};

	// Invariants every wall in a project satisfies: identities (wall and openings) plus validateWallGeometry.
	FLOOR101CADCORE_API Status validateWall(const Wall& wall);
	// Non-degenerate axis, positive dimensions, model range, and openings that fit: inside the length, below the top
	// (at least a lintel of 1 mm), doors on the floor, sorted by offset and not overlapping. Enough to build geometry
	// (e.g. a preview without ids).
	FLOOR101CADCORE_API Status validateWallGeometry(const Wall& wall);
}
