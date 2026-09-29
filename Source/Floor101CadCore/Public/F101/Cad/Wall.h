#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Finishes.h"
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

	// How a door or window opens (ADR-0014).
	enum class OpeningOperation
	{
		Hinged,       // one leaf (door) or sash (window) hinged at one jamb
		DoubleHinged, // two leaves or sashes hinged at both jambs, meeting in the middle
		Sliding,      // door: one leaf sliding into a pocket in the wall beside it; window: two sashes on two tracks
		Fixed,        // windows only: glazing that does not open
	};

	// "hinged" / "double" / "sliding" / "fixed" (project.json, tools).
	FLOOR101CADCORE_API std::string_view operationName(OpeningOperation operation);

	// An end of an opening along its wall: the side of its start edge (towards the wall start) or of its end edge.
	enum class OpeningEnd
	{
		Start,
		End,
	};

	// The long faces of a wall, seen along its axis from start to end.
	enum class WallSide
	{
		Left,
		Right,
	};

	// A door or window: a rectangular hole through its host wall (ADR-0010). Measured along the wall axis from the wall's
	// start to the opening's near edge; doors stand on the floor (sill 0). How it opens is part of it (ADR-0014); how open
	// it is only changes how it is shown.
	struct Opening
	{
		OpeningId id;
		OpeningKind kind = OpeningKind::Door;
		std::int64_t offsetMm = 0; // wall start -> opening edge, along the axis
		std::int64_t widthMm = 0;
		std::int64_t heightMm = 0;
		std::int64_t sillMm = 0; // floor -> bottom of the opening
		FinishId finish; // its frame and leaf (ADR-0013); empty: the default carpentry finish
		OpeningOperation operation = OpeningOperation::Hinged;
		// Hinged: the jamb with the hinges. Sliding: where the moving leaf goes (a door's pocket is on that side).
		OpeningEnd hinge = OpeningEnd::Start;
		// Hinged and double: the face of the wall the leaves swing towards. Sliding window: the face of the moving sash.
		WallSide opensTo = WallSide::Left;
		// 0 closed .. 100 fully open (hinged: 90 degrees; sliding: the leaf moved its whole travel). Always 0 when fixed.
		int openPercent = 0;

		friend bool operator==(const Opening&, const Opening&) = default;
	};

	// A stretch along a wall axis, in mm from its start.
	struct AxisSpan
	{
		std::int64_t fromMm = 0;
		std::int64_t toMm = 0;

		friend bool operator==(const AxisSpan&, const AxisSpan&) = default;
	};

	// The stretch of its wall an opening takes: its hole and, for a sliding door, the pocket its leaf slides into, as long
	// as the door is wide, on its `hinge` side (ADR-0014).
	FLOOR101CADCORE_API AxisSpan occupiedSpan(const Opening& opening);

	inline WallSide oppositeSide(WallSide side) { return side == WallSide::Left ? WallSide::Right : WallSide::Left; }
	inline OpeningEnd oppositeEnd(OpeningEnd end) { return end == OpeningEnd::Start ? OpeningEnd::End : OpeningEnd::Start; }

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
		FinishId finishLeft;  // finish of the left face (ADR-0012); empty: the default wall finish
		FinishId finishRight;

		friend bool operator==(const Wall&, const Wall&) = default;

		Segment2D axis() const { return Segment2D{start, end}; }
		double lengthMm() const { return axis().lengthMm(); }

		const FinishId& finish(WallSide side) const { return side == WallSide::Left ? finishLeft : finishRight; }
		FinishId& finish(WallSide side) { return side == WallSide::Left ? finishLeft : finishRight; }

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

	// Invariants every wall in a project satisfies: identities (wall and openings), finishes for walls, plus
	// validateWallGeometry.
	FLOOR101CADCORE_API Status validateWall(const Wall& wall);
	// Non-degenerate axis, positive dimensions, model range, and openings that fit: inside the length, below the top
	// (at least a lintel of 1 mm), doors on the floor, sorted by offset and not overlapping (pockets of sliding doors
	// included), an operation that suits the kind and a valid openness. Enough to build geometry (e.g. a preview without
	// ids).
	FLOOR101CADCORE_API Status validateWallGeometry(const Wall& wall);
}
