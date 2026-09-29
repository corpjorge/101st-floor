#pragma once

#include "F101/Cad/Wall.h"
#include "F101/Geometry/MeshData.h"
#include "F101/Geometry/WallJoins.h"

// Door and window carpentry (ADR-0013, ADR-0014): derived from each opening on every compile, never stored.
namespace f101::geometry
{
	namespace carpentry_slots
	{
		inline constexpr const char* Frame = "frame";       // jambs, head, window bottom rail and fixed mullion
		inline constexpr const char* Leaf = "leaf";         // door leaves and window sashes (their profiles)
		inline constexpr const char* Hardware = "hardware"; // handles and pulls
		inline constexpr const char* Glass = "glass";       // window panes
		inline constexpr const char* Swing = "swing";       // plan symbol of a door swing, on the floor
	}

	inline constexpr double FrameWidthMm = 50.0;      // visible width of a frame profile
	inline constexpr double FrameDepthMm = 70.0;      // at most the wall thickness
	inline constexpr double LeafThicknessMm = 40.0;
	inline constexpr double LeafClearanceMm = 3.0;    // gap between leaf (or sash) and frame
	inline constexpr double DoorFloorGapMm = 10.0;    // under the leaf
	inline constexpr double GlassThicknessMm = 6.0;
	inline constexpr double MullionFromWidthMm = 1300.0; // wider fixed windows get a central mullion
	inline constexpr double SashProfileMm = 50.0;     // visible width of a window sash profile
	inline constexpr double SashDepthMm = 50.0;       // thickness of a hinged sash
	inline constexpr double SlidingOverlapMm = 50.0;  // the two sashes of a sliding window overlap in the middle
	inline constexpr double OpenDegrees = 90.0;       // a hinged leaf 100 % open
	inline constexpr double SwingLineMm = 20.0;       // width of the lines of a swing symbol
	inline constexpr double SwingLiftMm = 3.0;        // above a room's floor

	// The carpentry in an opening's hole, in scene space like the wall mesh (z = 0 at the wall's floor): a frame centred
	// in the wall thickness (jambs and head; windows also a bottom rail) and what fills it by operation (ADR-0014):
	// - door, hinged: a leaf with a handle on each face, turned about its hinges towards `opensTo` by openPercent of 90
	//   degrees; double: two half leaves hinged at both jambs; sliding: a leaf moved into the wall towards `hinge` by
	//   openPercent of its width (its pocket), with a pull on each face;
	// - window, hinged and double: sashes (profiles and pane) turned like door leaves, with a handle on the `opensTo`
	//   face; sliding: two overlapping sashes on two tracks, the one on the `opensTo` side moving towards `hinge`; fixed: a
	//   pane, and a mullion when wider than MullionFromWidthMm.
	// The hole is trimmed like the wall's (to where both faces exist); empty if nothing is left of it. Flat-shaded closed
	// boxes; UVs in metres.
	FLOOR101GEOMETRY_API MeshData buildOpeningMesh(const cad::Wall& wall, const WallFootprint& footprint, const cad::Opening& opening);

	// The plan symbol of a hinged or double door (ADR-0014): for each leaf, the arc its free edge sweeps from closed to 90
	// degrees and the leaf fully open, as flat lines SwingLineMm wide lying SwingLiftMm over a room's floor, on the
	// `opensTo` side. Empty for windows and sliding doors. Section `swing`.
	FLOOR101GEOMETRY_API MeshData buildOpeningSymbolMesh(const cad::Wall& wall, const WallFootprint& footprint, const cad::Opening& opening);
}
