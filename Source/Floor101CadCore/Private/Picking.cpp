#include "F101/Cad/Picking.h"

#include <algorithm>
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

	std::optional<WallHit> pickWall(const Project& project, const Ray3& ray)
	{
		std::optional<WallHit> nearest;
		for (const Wall& wall : project.walls)
		{
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
