#pragma once

#include "F101/Cad/Wall.h"
#include "F101/Geometry/MeshData.h"
#include "F101/Geometry/WallJoins.h"

// Door and window carpentry (ADR-0013): derived from each opening on every compile, never stored.
namespace f101::geometry
{
	namespace carpentry_slots
	{
		inline constexpr const char* Frame = "frame";       // jambs, head, window bottom rail and mullion
		inline constexpr const char* Leaf = "leaf";         // the door leaf
		inline constexpr const char* Hardware = "hardware"; // door handles
		inline constexpr const char* Glass = "glass";       // the window pane
	}

	inline constexpr double FrameWidthMm = 50.0;      // visible width of a frame profile
	inline constexpr double FrameDepthMm = 70.0;      // at most the wall thickness
	inline constexpr double LeafThicknessMm = 40.0;
	inline constexpr double LeafClearanceMm = 3.0;    // gap between leaf and frame
	inline constexpr double DoorFloorGapMm = 10.0;    // under the leaf
	inline constexpr double GlassThicknessMm = 6.0;
	inline constexpr double MullionFromWidthMm = 1300.0; // wider windows get a central mullion

	// The carpentry in an opening's hole, in scene space like the wall mesh (z = 0 at the wall's floor): a frame centred
	// in the wall thickness (jambs and head; windows also a bottom rail, and a mullion when wider than MullionFromWidthMm);
	// doors a closed leaf with a handle on each face; windows a pane. The hole is trimmed like the wall's (to where both
	// faces exist); empty if nothing is left of it. Flat-shaded closed boxes; UVs in metres.
	FLOOR101GEOMETRY_API MeshData buildOpeningMesh(const cad::Wall& wall, const WallFootprint& footprint, const cad::Opening& opening);
}
