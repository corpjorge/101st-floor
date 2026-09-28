#include "F101/Cad/Snapping.h"

#include "F101/Cad/Units.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

namespace f101::cad
{
	namespace
	{
		double distance(PlanPoint a, Point2D b)
		{
			return std::hypot(a.x_mm - static_cast<double>(b.x_mm), a.y_mm - static_cast<double>(b.y_mm));
		}

		// Nearest mm; clamped to the model range so an off-plan cursor still yields a valid point.
		std::int64_t roundClamped(double valueMm)
		{
			const double limit = static_cast<double>(MaxAbsMillimeters);
			return std::llround(std::clamp(valueMm, -limit, limit));
		}

		std::int64_t roundToStep(double valueMm, std::int64_t stepMm)
		{
			if (stepMm <= 0)
			{
				return roundClamped(valueMm);
			}
			const std::int64_t steps = std::llround(valueMm / static_cast<double>(stepMm));
			return std::clamp(steps * stepMm, -MaxAbsMillimeters, MaxAbsMillimeters);
		}

		std::optional<Point2D> nearestEndpoint(const Project& project, PlanPoint cursor, double toleranceMm,
			const WallId& ignoreWall, const std::optional<Point2D>& exclude)
		{
			std::optional<Point2D> best;
			double bestDistance = toleranceMm;
			for (const Wall& wall : project.walls)
			{
				if (!ignoreWall.isNil() && wall.id == ignoreWall)
				{
					continue;
				}
				for (const Point2D& endpoint : {wall.start, wall.end})
				{
					if (exclude && endpoint == *exclude)
					{
						continue;
					}
					const double d = distance(cursor, endpoint);
					if (d <= bestDistance)
					{
						bestDistance = d;
						best = endpoint;
					}
				}
			}
			return best;
		}

		PlanPoint toPlan(Point2D point)
		{
			return PlanPoint{static_cast<double>(point.x_mm), static_cast<double>(point.y_mm)};
		}

		// Millimetre point at `lengthMm` from `anchor` along the unit direction: of the four around the exact point
		// (clamped to the model range), the one whose distance to the anchor is closest to lengthMm, then the nearest.
		Point2D pointAlong(Point2D anchor, double directionX, double directionY, double lengthMm)
		{
			const double limit = static_cast<double>(MaxAbsMillimeters);
			const double anchorX = static_cast<double>(anchor.x_mm);
			const double anchorY = static_cast<double>(anchor.y_mm);
			const double exactX = std::clamp(anchorX + directionX * lengthMm, -limit, limit);
			const double exactY = std::clamp(anchorY + directionY * lengthMm, -limit, limit);

			Point2D best;
			double bestLengthError = std::numeric_limits<double>::infinity();
			double bestDistance = std::numeric_limits<double>::infinity();
			for (const double x : {std::floor(exactX), std::ceil(exactX)})
			{
				for (const double y : {std::floor(exactY), std::ceil(exactY)})
				{
					const double lengthError = std::fabs(std::hypot(x - anchorX, y - anchorY) - lengthMm);
					const double distanceToExact = std::hypot(x - exactX, y - exactY);
					const bool closerLength = lengthError < bestLengthError - 1e-9;
					const bool sameLength = std::fabs(lengthError - bestLengthError) <= 1e-9;
					if (closerLength || (sameLength && distanceToExact < bestDistance))
					{
						best = Point2D{std::llround(x), std::llround(y)};
						bestLengthError = lengthError;
						bestDistance = distanceToExact;
					}
				}
			}
			return best;
		}
	}

	SnapResult snapPoint(const Project& project, PlanPoint cursor, const SnapSettings& settings, const WallId& ignoreWall)
	{
		if (settings.endpointToleranceMm > 0.0)
		{
			if (const std::optional<Point2D> endpoint = nearestEndpoint(project, cursor, settings.endpointToleranceMm, ignoreWall, std::nullopt))
			{
				return SnapResult{*endpoint, SnapKind::Endpoint, toPlan(*endpoint)};
			}
		}
		const Point2D point{roundToStep(cursor.x_mm, settings.gridMm), roundToStep(cursor.y_mm, settings.gridMm)};
		return SnapResult{point, settings.gridMm > 0 ? SnapKind::Grid : SnapKind::Free, cursor};
	}

	SnapResult snapFromAnchor(const Project& project, Point2D anchor, PlanPoint cursor, const SnapSettings& settings,
		const WallId& ignoreWall)
	{
		if (settings.endpointToleranceMm > 0.0)
		{
			if (const std::optional<Point2D> endpoint = nearestEndpoint(project, cursor, settings.endpointToleranceMm, ignoreWall, anchor))
			{
				return SnapResult{*endpoint, SnapKind::Endpoint, toPlan(*endpoint)};
			}
		}

		const double dx = cursor.x_mm - static_cast<double>(anchor.x_mm);
		const double dy = cursor.y_mm - static_cast<double>(anchor.y_mm);
		const double length = std::hypot(dx, dy);
		if (length < 1e-9)
		{
			return SnapResult{anchor, SnapKind::Free, cursor};
		}

		if (settings.angleStepDegrees > 0.0)
		{
			const double step = settings.angleStepDegrees * std::numbers::pi / 180.0;
			const double angle = std::round(std::atan2(dy, dx) / step) * step;
			const double directionX = std::cos(angle);
			const double directionY = std::sin(angle);
			// Length measured along the snapped direction, then rounded to the grid.
			const double projected = dx * directionX + dy * directionY;
			const std::int64_t snappedLength = roundToStep(projected, settings.gridMm);
			const PlanPoint constrained{static_cast<double>(anchor.x_mm) + directionX * projected,
				static_cast<double>(anchor.y_mm) + directionY * projected};
			return SnapResult{pointAlong(anchor, directionX, directionY, static_cast<double>(snappedLength)), SnapKind::Angle, constrained};
		}

		const std::int64_t snappedLength = roundToStep(length, settings.gridMm);
		return SnapResult{pointAlong(anchor, dx / length, dy / length, static_cast<double>(snappedLength)),
			settings.gridMm > 0 ? SnapKind::Grid : SnapKind::Free, cursor};
	}

	Result<Point2D> pointAtDistance(Point2D anchor, PlanPoint toward, std::int64_t lengthMm)
	{
		double dx = toward.x_mm - static_cast<double>(anchor.x_mm);
		double dy = toward.y_mm - static_cast<double>(anchor.y_mm);
		double length = std::hypot(dx, dy);
		if (length < 1e-9)
		{
			dx = 1.0;
			dy = 0.0;
			length = 1.0;
		}
		const double limit = static_cast<double>(MaxAbsMillimeters);
		const double endX = static_cast<double>(anchor.x_mm) + dx / length * static_cast<double>(lengthMm);
		const double endY = static_cast<double>(anchor.y_mm) + dy / length * static_cast<double>(lengthMm);
		if (!(std::fabs(endX) <= limit - 1.0) || !(std::fabs(endY) <= limit - 1.0))
		{
			return Error{ErrorCode::OutOfRange, "The point would leave the model range"};
		}
		return pointAlong(anchor, dx / length, dy / length, static_cast<double>(lengthMm));
	}
}
