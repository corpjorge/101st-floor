#include "F101/Cad/Snapping.h"

#include "F101/Cad/Units.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

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

		bool isIgnored(const Wall& wall, const std::vector<WallId>& ignoreWalls)
		{
			return std::find(ignoreWalls.begin(), ignoreWalls.end(), wall.id) != ignoreWalls.end();
		}

		std::optional<Point2D> nearestEndpoint(const Project& project, PlanPoint cursor, double toleranceMm,
			const std::vector<WallId>& ignoreWalls, const std::optional<Point2D>& exclude)
		{
			std::optional<Point2D> best;
			double bestDistance = toleranceMm;
			for (const Wall& wall : project.walls)
			{
				if (isIgnored(wall, ignoreWalls))
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

		// A wall's axis as start + unit direction * [0, length].
		struct Axis
		{
			PlanPoint start;
			PlanPoint direction;
			double length = 0.0;
		};

		Axis axisOf(const Wall& wall)
		{
			const PlanPoint start = toPlan(wall.start);
			const double length = wall.lengthMm();
			return Axis{start, PlanPoint{(static_cast<double>(wall.end.x_mm) - start.x_mm) / length,
				(static_cast<double>(wall.end.y_mm) - start.y_mm) / length}, length};
		}

		PlanPoint pointOnAxis(const Axis& axis, double distanceMm)
		{
			return PlanPoint{axis.start.x_mm + axis.direction.x_mm * distanceMm, axis.start.y_mm + axis.direction.y_mm * distanceMm};
		}

		struct AxisPosition
		{
			double along = 0.0;  // from the start, mm
			double across = 0.0; // distance from the axis line, mm
		};

		AxisPosition locate(const Axis& axis, PlanPoint p)
		{
			const double rx = p.x_mm - axis.start.x_mm;
			const double ry = p.y_mm - axis.start.y_mm;
			return AxisPosition{rx * axis.direction.x_mm + ry * axis.direction.y_mm, std::fabs(rx * axis.direction.y_mm - ry * axis.direction.x_mm)};
		}

		// True when p lies in the wall's body: drawing from there must not snap back onto the same wall.
		bool holds(const Wall& wall, const Axis& axis, PlanPoint p)
		{
			const AxisPosition position = locate(axis, p);
			return position.along >= 0.0 && position.along <= axis.length &&
				position.across <= static_cast<double>(wall.thicknessMm) / 2.0 + 1.0;
		}

		// Nearest point on a wall's axis within tolerance (a T junction on that wall).
		std::optional<SnapResult> nearestOnWall(const Project& project, PlanPoint cursor, double toleranceMm,
			const std::vector<WallId>& ignoreWalls, const std::optional<Point2D>& anchor)
		{
			std::optional<SnapResult> best;
			double bestDistance = toleranceMm;
			for (const Wall& wall : project.walls)
			{
				const Axis axis = axisOf(wall);
				if (isIgnored(wall, ignoreWalls) || (anchor && holds(wall, axis, toPlan(*anchor))))
				{
					continue;
				}
				const AxisPosition position = locate(axis, cursor);
				if (position.along > 0.0 && position.along < axis.length && position.across <= bestDistance)
				{
					const PlanPoint onAxis = pointOnAxis(axis, position.along);
					best = SnapResult{Point2D{roundClamped(onAxis.x_mm), roundClamped(onAxis.y_mm)}, SnapKind::Wall, onAxis};
					bestDistance = position.across;
				}
			}
			return best;
		}

		// Where the ray anchor + t direction (t > 0) crosses a wall's axis, if that is within tolerance of the cursor.
		std::optional<SnapResult> rayOnWall(const Project& project, Point2D anchor, double directionX, double directionY,
			PlanPoint cursor, double toleranceMm, const std::vector<WallId>& ignoreWalls)
		{
			std::optional<SnapResult> best;
			double bestDistance = toleranceMm;
			const PlanPoint origin = toPlan(anchor);
			for (const Wall& wall : project.walls)
			{
				const Axis axis = axisOf(wall);
				if (isIgnored(wall, ignoreWalls) || holds(wall, axis, origin))
				{
					continue;
				}
				const double denominator = directionX * axis.direction.y_mm - directionY * axis.direction.x_mm;
				if (std::fabs(denominator) < 1e-9)
				{
					continue;
				}
				const double rx = axis.start.x_mm - origin.x_mm;
				const double ry = axis.start.y_mm - origin.y_mm;
				const double t = (rx * axis.direction.y_mm - ry * axis.direction.x_mm) / denominator;
				const double s = (rx * directionY - ry * directionX) / denominator;
				if (t <= 0.0 || s <= 0.0 || s >= axis.length)
				{
					continue;
				}
				const PlanPoint crossing{origin.x_mm + directionX * t, origin.y_mm + directionY * t};
				const double d = std::hypot(crossing.x_mm - cursor.x_mm, crossing.y_mm - cursor.y_mm);
				if (d <= bestDistance)
				{
					// Keeps the snapped angle; the rounded point stays within 1 mm of the axis.
					best = SnapResult{pointAlong(anchor, directionX, directionY, t), SnapKind::Wall, crossing};
					bestDistance = d;
				}
			}
			return best;
		}
	}

	SnapResult snapPoint(const Project& project, PlanPoint cursor, const SnapSettings& settings, const std::vector<WallId>& ignoreWalls)
	{
		if (settings.endpointToleranceMm > 0.0)
		{
			if (const std::optional<Point2D> endpoint = nearestEndpoint(project, cursor, settings.endpointToleranceMm, ignoreWalls, std::nullopt))
			{
				return SnapResult{*endpoint, SnapKind::Endpoint, toPlan(*endpoint)};
			}
		}
		if (settings.wallToleranceMm > 0.0)
		{
			if (const std::optional<SnapResult> onWall = nearestOnWall(project, cursor, settings.wallToleranceMm, ignoreWalls, std::nullopt))
			{
				return *onWall;
			}
		}
		const Point2D point{roundToStep(cursor.x_mm, settings.gridMm), roundToStep(cursor.y_mm, settings.gridMm)};
		return SnapResult{point, settings.gridMm > 0 ? SnapKind::Grid : SnapKind::Free, cursor};
	}

	SnapResult snapFromAnchor(const Project& project, Point2D anchor, PlanPoint cursor, const SnapSettings& settings,
		const std::vector<WallId>& ignoreWalls)
	{
		if (settings.endpointToleranceMm > 0.0)
		{
			if (const std::optional<Point2D> endpoint = nearestEndpoint(project, cursor, settings.endpointToleranceMm, ignoreWalls, anchor))
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

		double directionX = dx / length;
		double directionY = dy / length;
		if (settings.angleStepDegrees > 0.0)
		{
			const double step = settings.angleStepDegrees * std::numbers::pi / 180.0;
			const double angle = std::round(std::atan2(dy, dx) / step) * step;
			directionX = std::cos(angle);
			directionY = std::sin(angle);
		}

		if (settings.wallToleranceMm > 0.0)
		{
			// Onto a wall: at the snapped angle if that crosses one near the cursor, otherwise at its nearest point.
			if (settings.angleStepDegrees > 0.0)
			{
				if (const std::optional<SnapResult> crossing = rayOnWall(project, anchor, directionX, directionY, cursor, settings.wallToleranceMm, ignoreWalls))
				{
					return *crossing;
				}
			}
			if (const std::optional<SnapResult> onWall = nearestOnWall(project, cursor, settings.wallToleranceMm, ignoreWalls, anchor))
			{
				return *onWall;
			}
		}

		if (settings.angleStepDegrees > 0.0)
		{
			// Length measured along the snapped direction, then rounded to the grid.
			const double projected = dx * directionX + dy * directionY;
			const std::int64_t snappedLength = roundToStep(projected, settings.gridMm);
			const PlanPoint constrained{static_cast<double>(anchor.x_mm) + directionX * projected,
				static_cast<double>(anchor.y_mm) + directionY * projected};
			return SnapResult{pointAlong(anchor, directionX, directionY, static_cast<double>(snappedLength)), SnapKind::Angle, constrained};
		}

		const std::int64_t snappedLength = roundToStep(length, settings.gridMm);
		return SnapResult{pointAlong(anchor, directionX, directionY, static_cast<double>(snappedLength)),
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
