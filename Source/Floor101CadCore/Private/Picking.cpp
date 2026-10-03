#include "F101/Cad/Picking.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace f101::cad
{
	namespace
	{
		// Slab test on one axis; narrows [tMin, tMax]. Returns false when the ray misses the slab.
		bool clipSlab(double origin, double direction, double low, double high, double& tMin, double& tMax)
		{
			if (std::fabs(direction) < 1e-12)
			{
				return origin >= low && origin <= high;
			}
			double t0 = (low - origin) / direction;
			double t1 = (high - origin) / direction;
			if (t0 > t1)
			{
				std::swap(t0, t1);
			}
			tMin = std::max(tMin, t0);
			tMax = std::min(tMax, t1);
			return tMin <= tMax;
		}

		// Entry distance of the ray into the wall's box (in the wall's local frame), if any.
		std::optional<double> intersectWall(const Wall& wall, const Ray3& ray)
		{
			const double length = wall.lengthMm();
			const double axisX = static_cast<double>(wall.end.x_mm - wall.start.x_mm) / length;
			const double axisY = static_cast<double>(wall.end.y_mm - wall.start.y_mm) / length;
			const double leftX = -axisY;
			const double leftY = axisX;
			const double relX = ray.originX - static_cast<double>(wall.start.x_mm);
			const double relY = ray.originY - static_cast<double>(wall.start.y_mm);
			const double half = static_cast<double>(wall.thicknessMm) / 2.0;

			double tMin = 0.0; // only in front of the origin
			double tMax = std::numeric_limits<double>::infinity();
			if (!clipSlab(relX * axisX + relY * axisY, ray.directionX * axisX + ray.directionY * axisY, 0.0, length, tMin, tMax) ||
				!clipSlab(relX * leftX + relY * leftY, ray.directionX * leftX + ray.directionY * leftY, -half, half, tMin, tMax) ||
				!clipSlab(ray.originZ, ray.directionZ, 0.0, static_cast<double>(wall.heightMm), tMin, tMax))
			{
				return std::nullopt;
			}
			return tMin;
		}
	}

	std::optional<WallHit> pickWall(const Project& project, const Ray3& ray, const std::vector<WallId>& ignoreWalls)
	{
		std::optional<WallHit> nearest;
		for (const Wall& wall : project.walls)
		{
			if (std::find(ignoreWalls.begin(), ignoreWalls.end(), wall.id) != ignoreWalls.end())
			{
				continue;
			}
			if (const std::optional<double> distance = intersectWall(wall, ray))
			{
				if (!nearest || *distance < nearest->distance)
				{
					nearest = WallHit{wall.id, *distance};
				}
			}
		}
		return nearest;
	}

	std::optional<FurnitureHit> pickFurniture(const Project& project, const Ray3& ray, const LevelId& level)
	{
		std::optional<FurnitureHit> nearest;
		for (const Furniture& piece : project.furniture)
		{
			if (piece.level != level)
			{
				continue;
			}
			// In the piece's own frame (a turn about z): an axis-aligned box.
			const PlanPoint origin = planToFurniture(piece, PlanPoint{ray.originX, ray.originY});
			const FurnitureAxes axes = furnitureAxes(piece);
			const double directionX = ray.directionX * axes.x.x_mm + ray.directionY * axes.x.y_mm;
			const double directionY = ray.directionX * axes.y.x_mm + ray.directionY * axes.y.y_mm;
			const double halfWidth = static_cast<double>(piece.widthMm) / 2.0;
			const double halfDepth = static_cast<double>(piece.depthMm) / 2.0;
			double tMin = 0.0;
			double tMax = std::numeric_limits<double>::infinity();
			if (clipSlab(origin.x_mm, directionX, -halfWidth, halfWidth, tMin, tMax) &&
				clipSlab(origin.y_mm, directionY, -halfDepth, halfDepth, tMin, tMax) &&
				clipSlab(ray.originZ, ray.directionZ, 0.0, static_cast<double>(piece.heightMm), tMin, tMax) &&
				(!nearest || tMin < nearest->distance))
			{
				nearest = FurnitureHit{piece.id, tMin};
			}
		}
		return nearest;
	}

	std::optional<ColumnHit> pickColumn(const Project& project, const Ray3& ray, const LevelId& level)
	{
		const Level* onLevel = project.findLevel(level);
		if (!onLevel)
		{
			return std::nullopt;
		}
		std::optional<ColumnHit> nearest;
		for (const Column& column : project.columns)
		{
			double tMin = 0.0;
			double tMax = std::numeric_limits<double>::infinity();
			if (column.level != level || !clipSlab(ray.originZ, ray.directionZ, 0.0, static_cast<double>(onLevel->heightMm), tMin, tMax))
			{
				continue;
			}
			const double originX = ray.originX - static_cast<double>(column.position.x_mm);
			const double originY = ray.originY - static_cast<double>(column.position.y_mm);
			bool entered = false;
			if (column.shape == ColumnShape::Round)
			{
				// Where the ray is inside the circle in plan: |origin + t direction| <= radius.
				const double radius = static_cast<double>(column.widthMm) / 2.0;
				const double a = ray.directionX * ray.directionX + ray.directionY * ray.directionY;
				const double b = 2.0 * (originX * ray.directionX + originY * ray.directionY);
				const double c = originX * originX + originY * originY - radius * radius;
				if (a < 1e-18)
				{
					entered = c <= 0.0; // straight down or up: inside the circle or not
				}
				else if (const double discriminant = b * b - 4.0 * a * c; discriminant >= 0.0)
				{
					const double root = std::sqrt(discriminant);
					tMin = std::max(tMin, (-b - root) / (2.0 * a));
					tMax = std::min(tMax, (-b + root) / (2.0 * a));
					entered = tMin <= tMax;
				}
			}
			else
			{
				// In the column's own frame, from its footprint: an axis-aligned box.
				const std::array<PlanPoint, 4> corners = columnFootprint(column);
				const double width = static_cast<double>(column.widthMm);
				const double depth = static_cast<double>(column.depthMm);
				const PlanPoint x{(corners[1].x_mm - corners[0].x_mm) / width, (corners[1].y_mm - corners[0].y_mm) / width};
				const PlanPoint y{(corners[3].x_mm - corners[0].x_mm) / depth, (corners[3].y_mm - corners[0].y_mm) / depth};
				entered = clipSlab(originX * x.x_mm + originY * x.y_mm, ray.directionX * x.x_mm + ray.directionY * x.y_mm,
						-width / 2.0, width / 2.0, tMin, tMax) &&
					clipSlab(originX * y.x_mm + originY * y.y_mm, ray.directionX * y.x_mm + ray.directionY * y.y_mm,
						-depth / 2.0, depth / 2.0, tMin, tMax);
			}
			if (entered && (!nearest || tMin < nearest->distance))
			{
				nearest = ColumnHit{column.id, tMin};
			}
		}
		return nearest;
	}

	std::optional<ServiceRunHit> pickServiceRun(const Project& project, const Ray3& ray, const LevelId& level, double toleranceMm,
		ServiceKindSet kinds)
	{
		const auto dot = [](double ax, double ay, double az, double bx, double by, double bz) { return ax * bx + ay * by + az * bz; };
		const double a = dot(ray.directionX, ray.directionY, ray.directionZ, ray.directionX, ray.directionY, ray.directionZ);
		if (a < 1e-18)
		{
			return std::nullopt;
		}
		std::optional<ServiceRunHit> nearest;
		for (const ServiceRun& run : project.services)
		{
			if (run.level != level || !kinds.contains(run.kind))
			{
				continue;
			}
			const double reach = static_cast<double>(serviceKindInfo(run.kind).diameterMm) / 2.0 + toleranceMm;
			for (std::size_t index = 1; index < run.points.size(); ++index)
			{
				// Closest approach of the ray (t >= 0) and the segment from `from` (s in [0, 1]).
				const ServicePoint& from = run.points[index - 1];
				const ServicePoint& to = run.points[index];
				const double sx = static_cast<double>(to.x_mm - from.x_mm);
				const double sy = static_cast<double>(to.y_mm - from.y_mm);
				const double sz = static_cast<double>(to.z_mm - from.z_mm);
				const double rx = ray.originX - static_cast<double>(from.x_mm);
				const double ry = ray.originY - static_cast<double>(from.y_mm);
				const double rz = ray.originZ - static_cast<double>(from.z_mm);
				const double b = dot(ray.directionX, ray.directionY, ray.directionZ, sx, sy, sz);
				const double c = dot(ray.directionX, ray.directionY, ray.directionZ, rx, ry, rz);
				const double e = dot(sx, sy, sz, sx, sy, sz);
				const double f = dot(sx, sy, sz, rx, ry, rz);
				const double denominator = a * e - b * b;
				double t = denominator > 1e-9 * a * e ? std::max(0.0, (b * f - c * e) / denominator) : 0.0;
				const double s = std::clamp((b * t + f) / e, 0.0, 1.0);
				t = std::max(0.0, (b * s - c) / a);
				const double gapX = rx + ray.directionX * t - sx * s;
				const double gapY = ry + ray.directionY * t - sy * s;
				const double gapZ = rz + ray.directionZ * t - sz * s;
				if (std::sqrt(dot(gapX, gapY, gapZ, gapX, gapY, gapZ)) <= reach && (!nearest || t < nearest->distance))
				{
					nearest = ServiceRunHit{run.id, t};
				}
			}
		}
		return nearest;
	}

	std::optional<double> intersectPost(const Ray3& ray, Point2D center, double sizeMm, double heightMm)
	{
		const double half = sizeMm / 2.0;
		const double centerX = static_cast<double>(center.x_mm);
		const double centerY = static_cast<double>(center.y_mm);
		double tMin = 0.0;
		double tMax = std::numeric_limits<double>::infinity();
		if (!clipSlab(ray.originX, ray.directionX, centerX - half, centerX + half, tMin, tMax) ||
			!clipSlab(ray.originY, ray.directionY, centerY - half, centerY + half, tMin, tMax) ||
			!clipSlab(ray.originZ, ray.directionZ, 0.0, heightMm, tMin, tMax))
		{
			return std::nullopt;
		}
		return tMin;
	}

	std::optional<PlanPoint> intersectFloor(const Ray3& ray)
	{
		if (ray.directionZ >= -1e-12)
		{
			return std::nullopt; // parallel to the floor or going up
		}
		const double t = -ray.originZ / ray.directionZ;
		if (t < 0.0)
		{
			return std::nullopt; // origin below the floor, looking further down
		}
		return PlanPoint{ray.originX + ray.directionX * t, ray.originY + ray.directionY * t};
	}
}
