#pragma once

#include "F101/Cad/Furniture.h"
#include "F101/Geometry/MeshData.h"

#include <string_view>
#include <vector>

// Furniture (ADR-0016): the shape of each piece, derived from its type and dimensions on every compile, never stored.
namespace f101::geometry
{
	namespace furniture_slots
	{
		inline constexpr const char* Body = "body";       // the main parts, in the piece's finish
		inline constexpr const char* Linen = "linen";     // mattresses and pillows
		inline constexpr const char* Fabric = "fabric";   // duvets
		inline constexpr const char* Ceramic = "ceramic"; // basins set into another finish
		inline constexpr const char* Metal = "metal";     // taps, sinks, handles, feet
		inline constexpr const char* Stone = "stone";     // worktops
		inline constexpr const char* Dark = "dark";       // hobs, plinths and handles of appliances
	}

	// A box of a piece in its own frame (mm): x across its width from the centre, y from its front (-depth/2) to its back
	// (+depth/2), z up from the floor it stands on.
	struct FurnitureBox
	{
		const char* slot = furniture_slots::Body;
		double x0 = 0.0;
		double x1 = 0.0;
		double y0 = 0.0;
		double y1 = 0.0;
		double z0 = 0.0;
		double z1 = 0.0;
	};

	// A closed outline drawn inside a piece's footprint on plans (a mattress, a basin, a hob), in its own frame.
	using FurnitureOutline = std::vector<cad::PlanPoint>;

	struct FurnitureShape
	{
		std::vector<FurnitureBox> boxes;
		std::vector<FurnitureOutline> planDetails;
	};

	// The boxes of a piece by type, sized from its width, depth and height and kept inside them (taps and hobs may rise
	// a little above), and the details a plan shows inside its outline. Empty for an unknown type.
	FLOOR101GEOMETRY_API FurnitureShape furnitureShape(const cad::Furniture& furniture);

	// The finish of a section of a piece: its own (cad::resolveFurnitureFinish) for the body, a fixed one for the rest.
	FLOOR101GEOMETRY_API std::string_view furnitureSlotFinish(const cad::Furniture& furniture, std::string_view slot);

	// The piece in scene space local to its level: its boxes turned and placed, standing on a room floor
	// (z = FloorThicknessMm). Flat-shaded closed boxes; UVs in metres.
	FLOOR101GEOMETRY_API MeshData buildFurnitureMesh(const cad::Furniture& furniture);
}
