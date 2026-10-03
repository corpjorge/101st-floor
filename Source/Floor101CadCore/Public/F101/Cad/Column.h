#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Finishes.h"
#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

// Columns (ADR-0021): the vertical members that hold the building up, one level each, standing from its floor as tall as
// the level (through its slab, like its walls). The model stores what and where; the solid is derived in the Geometry
// Engine.
namespace f101::cad
{
	enum class ColumnShape
	{
		Rectangle,
		Round,
	};

	// "rectangle", "round" (project.json).
	FLOOR101CADCORE_API std::string_view columnShapeId(ColumnShape shape);
	FLOOR101CADCORE_API std::optional<ColumnShape> parseColumnShape(std::string_view id);

	inline constexpr std::int64_t DefaultColumnSizeMm = 300;
	inline constexpr std::int64_t MinColumnSizeMm = 100;
	inline constexpr std::int64_t MaxColumnSizeMm = 3000;
	inline constexpr std::string_view DefaultColumnFinish = "concrete";

	struct Column
	{
		ColumnId id;
		LevelId level;
		Point2D position; // centre, plan mm local to its level
		ColumnShape shape = ColumnShape::Rectangle;
		std::int64_t widthMm = DefaultColumnSizeMm; // a round column: its diameter
		std::int64_t depthMm = DefaultColumnSizeMm; // a round column: its diameter too
		std::int64_t rotationCentidegrees = 0;      // a rectangle turned counter-clockwise, [0, 36000); a round one: 0
		FinishId finish;                            // a finish for walls; empty: concrete

		friend bool operator==(const Column&, const Column&) = default;
	};

	// A column of that shape and size at a place (id and level unset). A round column takes `widthMm` as its diameter.
	FLOOR101CADCORE_API Column makeColumn(ColumnShape shape, Point2D position, std::int64_t widthMm = DefaultColumnSizeMm,
		std::int64_t depthMm = DefaultColumnSizeMm, std::int64_t rotationCentidegrees = 0);

	// The finish it shows: its own, else concrete.
	FLOOR101CADCORE_API std::string_view resolveColumnFinish(const Column& column);

	// Sizes from MinColumnSizeMm to MaxColumnSizeMm, a round one as wide as deep and not turned, a rotation in [0, 36000), a
	// position in the model range and a finish for walls (imported textures are checked with the project).
	FLOOR101CADCORE_API Status validateColumn(const Column& column);

	// The corners of its footprint, counter-clockwise (a round column: the square around it).
	FLOOR101CADCORE_API std::array<PlanPoint, 4> columnFootprint(const Column& column);
	// Whether a plan point is on its footprint (a round one: its circle), grown by `marginMm`.
	FLOOR101CADCORE_API bool columnContains(const Column& column, PlanPoint point, double marginMm = 0.0);
}
