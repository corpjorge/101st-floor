#pragma once

#include "F101/Geometry/Rooms.h"

namespace f101::geometry
{
	// Closed prisms along the inner wall faces. Openings are subtracted in both plan and height;
	// high window sills leave the strip intact. Starts on top of the room's floor (ADR-0030).
	FLOOR101GEOMETRY_API MeshData buildBaseboardMesh(const Room& room, const cad::Project& levelWalls,
		const cad::RoomBaseboard& setting);
}
