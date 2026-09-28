#pragma once

#include <cmath>
#include <cstdint>

// Plan-view primitives of the persistent model: integer millimetres, right-handed, X east, Y north (ADR-0001/0003).
namespace f101::cad
{
	struct Vector2D
	{
		std::int64_t dx_mm = 0;
		std::int64_t dy_mm = 0;

		friend constexpr bool operator==(const Vector2D&, const Vector2D&) = default;

		double lengthMm() const
		{
			return std::hypot(static_cast<double>(dx_mm), static_cast<double>(dy_mm));
		}
	};

	struct Point2D
	{
		std::int64_t x_mm = 0;
		std::int64_t y_mm = 0;

		friend constexpr bool operator==(const Point2D&, const Point2D&) = default;
	};

	constexpr Vector2D operator-(const Point2D& to, const Point2D& from)
	{
		return Vector2D{to.x_mm - from.x_mm, to.y_mm - from.y_mm};
	}

	constexpr Point2D operator+(const Point2D& point, const Vector2D& offset)
	{
		return Point2D{point.x_mm + offset.dx_mm, point.y_mm + offset.dy_mm};
	}

	struct Segment2D
	{
		Point2D start;
		Point2D end;

		friend constexpr bool operator==(const Segment2D&, const Segment2D&) = default;

		constexpr Vector2D vector() const { return end - start; }
		double lengthMm() const { return vector().lengthMm(); }
	};
}
