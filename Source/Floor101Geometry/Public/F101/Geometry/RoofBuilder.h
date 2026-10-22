#pragma once

#include "F101/Cad/Project.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Geometry/MeshData.h"

namespace f101::geometry
{
	namespace roof_slots
	{
		inline constexpr const char* Sheet = "roof-sheet";
		inline constexpr const char* Frame = "roof-frame";
	}
	// Corrugated translucent sheets and their frame, clipped to the room, mm in scene space.
	// Low sheet eave at levelTopMm, rising as the saved setting specifies. Not a structural calculation.
	FLOOR101GEOMETRY_API MeshData buildRoofMesh(const Room& room, double levelTopMm, const cad::RoomRoof& roof);
}
