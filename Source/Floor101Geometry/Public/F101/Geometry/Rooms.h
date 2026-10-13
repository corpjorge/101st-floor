#pragma once

#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/Project.h"
#include "F101/Cad/Document.h"
#include "F101/Geometry/MeshData.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Rooms: the closed regions enclosed by walls (ADR-0009). Derived from the walls on every compile; never stored.
namespace f101::geometry
{
	// A stretch of a room's floor edge along one wall face: from the face (or free end) it meets at one end to the one it
	// meets at the other, counter-clockwise around the floor, so the room is on its left.
	struct RoomSide
	{
		cad::WallId wall;
		bool left = true;         // the face is the wall's left one (the room is on the wall's left)
		std::size_t segment = 0;  // which segment of the wall, as in Room::key
		cad::PlanPoint start;
		cad::PlanPoint end;
	};

	struct Room
	{
		// Deterministic identity: the lowest bounding wall id, the side of it the room is on and which segment of that
		// wall (split at T junctions and crossings), e.g. "<uuid>/room-left-0". Stable while that wall bounds the room.
		std::string key;
		std::vector<cad::PlanPoint> outline; // floor: inner wall faces, counter-clockwise, mm
		double areaMm2 = 0.0;                // net floor area (inside the walls)
		cad::PlanPoint labelPoint;           // a point inside the floor, for its label
		std::vector<cad::WallId> walls;      // bounding walls, sorted, unique
		// The floor edge wall by wall, in outline order. A wall met straight on continues it in a new side; the end cap of
		// a wall standing into the room is no side. Sides left without length by thick walls are dropped.
		std::vector<RoomSide> sides;
	};

	// Wall axes form a planar graph: ends that meet, ends inside another wall's body (T) and crossing axes are joined
	// there. Every bounded face of that graph is a room; its floor is inset from the axes by each wall's
	// half-thickness (walls that stick into a room are cut out of its floor). Faces too small for a floor are dropped,
	// and a group of walls standing free inside a room does not cut a hole in it. Sorted by key.
	// Precondition: every wall passes cad::validateWallGeometry.
	FLOOR101GEOMETRY_API std::vector<Room> detectRooms(const cad::Project& project);

	// For cad::EditController: the key of the room of `levelWalls` whose floor contains the point.
	FLOOR101GEOMETRY_API cad::RoomLocator makeRoomLocator();
	// For cad::Document: finishes, names and baseboards follow derived rooms through automatic wall splitting
	// (ADR-0025). Each new room takes every setting it lacks from the first old room (by key) of its level that has that
	// setting and whose floor contains the new room's label point. Rooms are detected once per level and project.
	FLOOR101GEOMETRY_API cad::Document::RoomTransfer makeRoomTransfer();

	inline constexpr double FloorThicknessMm = 10.0;
	namespace floor_slots
	{
		inline constexpr const char* Top = "floor";
		inline constexpr const char* Side = "floor_edge";
		inline constexpr const char* Bottom = "floor_bottom";
	}

	// Floor slab of a room: from z = 0 to FloorThicknessMm, over its outline.
	FLOOR101GEOMETRY_API MeshData buildFloorMesh(const Room& room);

	namespace slab_slots
	{
		inline constexpr const char* Ceiling = "ceiling"; // underside, seen from the room below
		inline constexpr const char* Side = "slab_edge";
		inline constexpr const char* Top = "slab_top";
	}

	// Structural slab over a room (ADR-0011), between two heights (mm), over the same outline as its floor: it fits
	// between the walls that run up to the next level.
	FLOOR101GEOMETRY_API MeshData buildSlabMesh(const Room& room, double bottomZ, double topZ);
}
