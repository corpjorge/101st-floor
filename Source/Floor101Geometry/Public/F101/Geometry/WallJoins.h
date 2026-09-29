#pragma once

#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Project.h"
#include "F101/Geometry/MeshData.h"

#include <cstdint>
#include <vector>

// How walls meet (ADR-0009). Derived from the walls' geometry on every compile; never stored in the project.
namespace f101::geometry
{
	// Plan outline of a wall once its connections are resolved (mm). Corners are named relative to the axis
	// (start -> end, left = counter-clockwise side); the left and right edges stay parallel to the axis.
	struct WallFootprint
	{
		cad::PlanPoint startRight;
		cad::PlanPoint startLeft;
		cad::PlanPoint endRight;
		cad::PlanPoint endLeft;
	};

	// Where three or more wall ends meet, their mitred ends leave a polygon at the centre; a prism fills it so the
	// junction is closed from above. Identified by the lowest wall id at the joint and which of its ends is there.
	struct JointFill
	{
		cad::WallId wall;
		bool atStart = true;
		std::vector<cad::PlanPoint> outline; // counter-clockwise
		std::int64_t heightMm = 0;           // the tallest wall at the joint
	};

	struct WallJoins
	{
		std::vector<WallFootprint> footprints; // one per wall, in project order
		std::vector<JointFill> fills;
	};

	// Sharper corners than this (miter point further than MiterLimit x the thicker half-thickness from the joint) get
	// square ends instead of a long spike.
	inline constexpr double MiterLimit = 4.0;

	// Rectangle of a wall on its own: centred on the axis, square ends.
	FLOOR101GEOMETRY_API WallFootprint plainFootprint(const cad::Wall& wall);

	// Resolves every wall's footprint:
	// - ends that share a point are mitred with their angular neighbours (L, Y, X...), whatever their thicknesses;
	//   collinear neighbours meet square; three or more ends also get a JointFill;
	// - an end inside another wall's body, away from that wall's ends (T), is cut at that wall's face;
	// - a free end is square.
	// A wall whose resolved ends would cross falls back to its plain footprint.
	// Precondition: every wall passes cad::validateWallGeometry.
	FLOOR101GEOMETRY_API WallJoins solveWallJoins(const cad::Project& project);

	// Along `wall`'s axis, the spans an opening must keep clear of (ADR-0013), in whole mm rounded outwards and merged: at
	// each end, up to where both long faces start (mitres, cuts at another wall), and wherever another wall of
	// `levelWalls` reaches into its band (T joints, crossings, walls alongside). `wall` may differ from its copy in
	// `levelWalls` (a change being checked): its own footprint is solved from `levelWalls`.
	FLOOR101GEOMETRY_API std::vector<cad::AxisSpan> wallObstructions(const cad::Project& levelWalls, const cad::Wall& wall);

	// For cad::Document::setWallObstructions and the editor.
	FLOOR101GEOMETRY_API cad::WallObstructions makeWallObstructions();
}
