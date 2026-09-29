#pragma once

#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Project.h"
#include "F101/Geometry/MeshData.h"

#include <cstdint>
#include <string>
#include <vector>

// Rooms: the closed regions enclosed by walls (ADR-0009). Derived from the walls on every compile; never stored.
namespace f101::geometry
{
	struct Room
	{
		// Deterministic identity: the lowest bounding wall id, the side of it the room is on and which segment of that
		// wall (split at T junctions and crossings), e.g. "<uuid>/room-left-0". Stable while that wall bounds the room.
		std::string key;
		std::vector<cad::PlanPoint> outline; // floor: inner wall faces, counter-clockwise, mm
		double areaMm2 = 0.0;                // net floor area (inside the walls)
		cad::PlanPoint labelPoint;           // a point inside the floor, for its label
		std::vector<cad::WallId> walls;      // bounding walls, sorted, unique
	};

	// Wall axes form a planar graph: ends that meet, ends inside another wall's body (T) and crossing axes are joined
	// there. Every bounded face of that graph is a room; its floor is inset from the axes by each wall's
	// half-thickness (walls that stick into a room are cut out of its floor). Faces too small for a floor are dropped,
	// and a group of walls standing free inside a room does not cut a hole in it. Sorted by key.
	// Precondition: every wall passes cad::validateWallGeometry.
	FLOOR101GEOMETRY_API std::vector<Room> detectRooms(const cad::Project& project);

	inline constexpr double FloorThicknessMm = 10.0;
	namespace floor_slots
	{
		inline constexpr const char* Top = "floor";
		inline constexpr const char* Side = "floor_edge";
		inline constexpr const char* Bottom = "floor_bottom";
	}

	// Floor slab of a room: from z = 0 to FloorThicknessMm, over its outline.
	FLOOR101GEOMETRY_API MeshData buildFloorMesh(const Room& room);
}
