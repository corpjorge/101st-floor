#include "F101/Cad/Column.h"

#include "F101/Cad/Furniture.h"
#include "F101/Cad/Units.h"

#include <cmath>
#include <string>

namespace f101::cad
{
	namespace
	{
		// Its own frame: x across its width, y across its depth (quarter turns exact, like furniture).
		std::pair<PlanPoint, PlanPoint> axesOf(const Column& column)
		{
			switch (column.rotationCentidegrees)
			{
			case 0: return {{1.0, 0.0}, {0.0, 1.0}};
			case 9000: return {{0.0, 1.0}, {-1.0, 0.0}};
			case 18000: return {{-1.0, 0.0}, {0.0, -1.0}};
			case 27000: return {{0.0, -1.0}, {1.0, 0.0}};
			default: break;
			}
			constexpr double RadiansPerCentidegree = 3.14159265358979323846 / 18000.0;
			const double angle = static_cast<double>(column.rotationCentidegrees) * RadiansPerCentidegree;
			return {{std::cos(angle), std::sin(angle)}, {-std::sin(angle), std::cos(angle)}};
		}

		std::string sizeRange()
		{
			return formatLength(static_cast<double>(MinColumnSizeMm), LengthUnit::Centimeter) + " to " +
				formatLength(static_cast<double>(MaxColumnSizeMm), LengthUnit::Centimeter);
		}
	}

	std::string_view columnShapeId(ColumnShape shape)
	{
		return shape == ColumnShape::Round ? "round" : "rectangle";
	}

	std::optional<ColumnShape> parseColumnShape(std::string_view id)
	{
		if (id == "rectangle")
		{
			return ColumnShape::Rectangle;
		}
		if (id == "round")
		{
			return ColumnShape::Round;
		}
		return std::nullopt;
	}

	Column makeColumn(ColumnShape shape, Point2D position, std::int64_t widthMm, std::int64_t depthMm, std::int64_t rotationCentidegrees)
	{
		Column column;
		column.position = position;
		column.shape = shape;
		column.widthMm = widthMm;
		column.depthMm = shape == ColumnShape::Round ? widthMm : depthMm;
		column.rotationCentidegrees = shape == ColumnShape::Round ? 0 : rotationCentidegrees;
		return column;
	}

	std::string_view resolveColumnFinish(const Column& column)
	{
		return column.finish.empty() ? DefaultColumnFinish : std::string_view(column.finish);
	}

	Status validateColumn(const Column& column)
	{
		for (const std::int64_t size : {column.widthMm, column.depthMm})
		{
			if (size < MinColumnSizeMm || size > MaxColumnSizeMm)
			{
				return Error{ErrorCode::OutOfRange, "A column's sides go from " + sizeRange()};
			}
		}
		if (column.shape == ColumnShape::Round && (column.depthMm != column.widthMm || column.rotationCentidegrees != 0))
		{
			return Error{ErrorCode::InvalidArgument, "A round column has one diameter and no rotation"};
		}
		if (column.rotationCentidegrees < 0 || column.rotationCentidegrees >= FullTurnCentidegrees)
		{
			return Error{ErrorCode::OutOfRange, "A rotation goes from 0 to 359.99 degrees"};
		}
		for (const std::int64_t coordinate : {column.position.x_mm, column.position.y_mm})
		{
			if (coordinate < -MaxAbsMillimeters || coordinate > MaxAbsMillimeters)
			{
				return Error{ErrorCode::OutOfRange, "Column position outside the model range"};
			}
		}
		return validateFinish(column.finish, Surface::Wall);
	}

	std::array<PlanPoint, 4> columnFootprint(const Column& column)
	{
		const auto [x, y] = axesOf(column);
		const double w = static_cast<double>(column.widthMm) / 2.0;
		const double d = static_cast<double>(column.depthMm) / 2.0;
		const double cx = static_cast<double>(column.position.x_mm);
		const double cy = static_cast<double>(column.position.y_mm);
		const auto at = [&](double u, double v) { return PlanPoint{cx + x.x_mm * u + y.x_mm * v, cy + x.y_mm * u + y.y_mm * v}; };
		return {at(-w, -d), at(w, -d), at(w, d), at(-w, d)};
	}

	bool columnContains(const Column& column, PlanPoint point, double marginMm)
	{
		const double dx = point.x_mm - static_cast<double>(column.position.x_mm);
		const double dy = point.y_mm - static_cast<double>(column.position.y_mm);
		if (column.shape == ColumnShape::Round)
		{
			return std::hypot(dx, dy) <= static_cast<double>(column.widthMm) / 2.0 + marginMm;
		}
		const auto [x, y] = axesOf(column);
		return std::abs(dx * x.x_mm + dy * x.y_mm) <= static_cast<double>(column.widthMm) / 2.0 + marginMm &&
			std::abs(dx * y.x_mm + dy * y.y_mm) <= static_cast<double>(column.depthMm) / 2.0 + marginMm;
	}
}
