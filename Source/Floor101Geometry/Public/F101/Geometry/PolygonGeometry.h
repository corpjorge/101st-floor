#pragma once

#include "F101/Cad/Geometry2D.h"
#include "F101/Geometry/MeshData.h"

#include <cstdint>
#include <vector>

// Plan polygons (mm) and the prisms built on them: joint fills, room floors.
namespace f101::geometry
{
	// mm²; positive when the outline runs counter-clockwise.
	FLOOR101GEOMETRY_API double signedArea(const std::vector<cad::PlanPoint>& outline);

	// Even-odd test; points exactly on the boundary may go either way.
	FLOOR101GEOMETRY_API bool containsPoint(const std::vector<cad::PlanPoint>& outline, cad::PlanPoint point);

	// Triangles covering a simple counter-clockwise polygon (ear clipping), as index triples into the outline, each
	// counter-clockwise. Collinear vertices are allowed. Empty if the outline is degenerate, clockwise or self-crossing.
	FLOOR101GEOMETRY_API std::vector<std::uint32_t> triangulatePolygon(const std::vector<cad::PlanPoint>& outline);

	struct PrismSlots
	{
		const char* top = "top";
		const char* side = "side";
		const char* bottom = "bottom";
	};

	// Closed flat-shaded prism over a simple counter-clockwise outline, between two heights (mm). UVs in metres: plan
	// x/y on the top and bottom, perimeter distance and height on the sides. Empty if the outline cannot be triangulated.
	FLOOR101GEOMETRY_API MeshData buildPrismMesh(const std::vector<cad::PlanPoint>& outline, double bottomZ, double topZ,
		const PrismSlots& slots = {});
}
