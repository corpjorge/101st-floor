#pragma once

#include "F101/Cad/Column.h"
#include "F101/Cad/Services.h"
#include "F101/Geometry/MeshData.h"

#include <vector>

// Columns and runs of the building services (ADR-0021): their solids, derived on every compile and never stored.
namespace f101::geometry
{
	namespace services_slots
	{
		inline constexpr const char* Column = "column"; // in the column's finish
		inline constexpr const char* Pipe = "pipe";     // in its kind's colour
	}

	// Facets of a round column and of a pipe.
	inline constexpr int RoundColumnSides = 32;
	inline constexpr int PipeSides = 12;

	// A column's outline in plan, counter-clockwise: its rectangle, or a RoundColumnSides-gon around its circle.
	FLOOR101GEOMETRY_API std::vector<cad::PlanPoint> columnOutline(const cad::Column& column);

	// A column in scene space local to its level, from z = 0 up to `heightMm` (its level's height, like the walls): a
	// flat-shaded box, or a smooth cylinder with flat ends. One section; UVs in metres.
	FLOOR101GEOMETRY_API MeshData buildColumnMesh(const cad::Column& column, double heightMm);

	// A run in scene space local to its level: a smooth tube of its kind's diameter along each segment, a ball at every
	// bend so the tubes join, and flat ends. One section; UVs in metres (around the tube, along the run).
	FLOOR101GEOMETRY_API MeshData buildServiceRunMesh(const cad::ServiceRun& run);
}
