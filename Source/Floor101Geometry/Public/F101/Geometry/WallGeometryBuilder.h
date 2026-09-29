#pragma once

#include "F101/Cad/Wall.h"
#include "F101/Geometry/MeshData.h"
#include "F101/Geometry/WallJoins.h"

namespace f101::geometry
{
	// Material slots of a wall, one per face, so every face can carry its own material.
	// "left"/"right" are relative to the wall axis (start -> end); mapping them to interior/exterior is a room concern.
	namespace wall_slots
	{
		inline constexpr const char* Left = "left";
		inline constexpr const char* Right = "right";
		inline constexpr const char* Top = "top";
		inline constexpr const char* Bottom = "bottom";
		inline constexpr const char* Start = "start";
		inline constexpr const char* End = "end";
		inline constexpr const char* Reveal = "reveal"; // inside faces of door and window holes
	}

	// Closed prism for a straight wall over its footprint, from the floor (z = 0) to its height, flat-shaded, one section
	// per face (24 vertices and 12 triangles without openings). With plainFootprint(wall) it is the box centred on the
	// axis; with a resolved footprint (solveWallJoins) the end faces are mitred or cut at another wall. Each opening is a
	// hole through both long faces with its reveals (jambs, head, and the sill of a window); doors also cut the bottom.
	// Precondition: cad::validateWallGeometry(wall) succeeds (the id is not used).
	FLOOR101GEOMETRY_API MeshData buildWallMesh(const cad::Wall& wall, const WallFootprint& footprint);

	// buildWallMesh(wall, plainFootprint(wall)): the wall on its own.
	FLOOR101GEOMETRY_API MeshData buildWallMesh(const cad::Wall& wall);
}
