#pragma once

#include "F101/Cad/Wall.h"
#include "F101/Geometry/MeshData.h"

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
	}

	// Closed box for a straight wall: centred on its axis, from floor (z = 0) to its height. Flat-shaded, 24 vertices,
	// 12 triangles, one section per face. Precondition: cad::validateWallGeometry(wall) succeeds (the id is not used).
	FLOOR101GEOMETRY_API MeshData buildWallMesh(const cad::Wall& wall);
}
