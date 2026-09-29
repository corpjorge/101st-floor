#include "F101/Geometry/WallJoins.h"

#include "F101/Cad/Assert.h"
#include "F101/Cad/Wall.h"
#include "F101/Geometry/PolygonGeometry.h"
#include "MeshBuilding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>

namespace f101::geometry
{
	using namespace detail;

	namespace
	{
		constexpr double ParallelEpsilon = 1e-9;
		constexpr double DuplicateEpsilonMm = 1e-6;
		constexpr double MinFillAreaMm2 = 1.0;
		constexpr double BodyToleranceMm = 1e-6;

		// One wall end at a joint. Directions and corners are relative to `away` (from the joint along the wall):
		// "left" is its counter-clockwise side.
		struct WallEnd
		{
			std::size_t wall = 0;
			bool atStart = true;
			cad::PlanPoint away;
			double half = 0.0;
			double angle = 0.0;
			cad::PlanPoint left;
			cad::PlanPoint right;
		};

		struct PointLess
		{
			bool operator()(const cad::Point2D& a, const cad::Point2D& b) const
			{
				return a.x_mm != b.x_mm ? a.x_mm < b.x_mm : a.y_mm < b.y_mm;
			}
		};

		cad::PlanPoint axisDirection(const cad::Wall& wall)
		{
			const cad::PlanPoint d = sub(toPlan(wall.end), toPlan(wall.start));
			return scale(d, 1.0 / length2(d));
		}

		// Intersection of the lines p + t d and q + s e; none when they are (nearly) parallel.
		std::optional<cad::PlanPoint> intersectLines(cad::PlanPoint p, cad::PlanPoint d, cad::PlanPoint q, cad::PlanPoint e)
		{
			const double denominator = cross2(d, e);
			if (std::fabs(denominator) < ParallelEpsilon)
			{
				return std::nullopt;
			}
			return add(p, scale(d, cross2(sub(q, p), e) / denominator));
		}

		cad::PlanPoint leftEdge(const cad::PlanPoint& joint, const WallEnd& end)
		{
			return add(joint, scale(leftOf(end.away), end.half));
		}

		cad::PlanPoint rightEdge(const cad::PlanPoint& joint, const WallEnd& end)
		{
			return sub(joint, scale(leftOf(end.away), end.half));
		}

		// At the start the away direction is the axis, so the end's left is the wall's left; at the end it is reversed.
		void storeEnd(WallFootprint& footprint, const WallEnd& end)
		{
			if (end.atStart)
			{
				footprint.startLeft = end.left;
				footprint.startRight = end.right;
			}
			else
			{
				footprint.endRight = end.left;
				footprint.endLeft = end.right;
			}
		}

		// A wall (other than those ending at the joint) whose body contains the joint strictly between its ends.
		const cad::Wall* hostWall(const cad::Project& project, const cad::PlanPoint& joint, const std::vector<WallEnd>& ends)
		{
			const cad::Wall* host = nullptr;
			double hostDistance = std::numeric_limits<double>::infinity();
			for (std::size_t index = 0; index < project.walls.size(); ++index)
			{
				if (std::any_of(ends.begin(), ends.end(), [index](const WallEnd& end) { return end.wall == index; }))
				{
					continue;
				}
				const cad::Wall& wall = project.walls[index];
				const cad::PlanPoint axis = axisDirection(wall);
				const cad::PlanPoint relative = sub(joint, toPlan(wall.start));
				const double along = dot2(relative, axis);
				const double across = std::fabs(cross2(axis, relative));
				if (along <= 0.0 || along >= wall.lengthMm() || across > static_cast<double>(wall.thicknessMm) / 2.0 + BodyToleranceMm)
				{
					continue;
				}
				if (across < hostDistance)
				{
					host = &wall;
					hostDistance = across;
				}
			}
			return host;
		}

		// T: every end at the joint is cut at the host's face on its own side.
		void cutAtHost(const cad::PlanPoint& joint, const cad::Wall& host, std::vector<WallEnd>& ends, const cad::Project& project)
		{
			const cad::PlanPoint hostAxis = axisDirection(host);
			const cad::PlanPoint hostStart = toPlan(host.start);
			const double hostHalf = static_cast<double>(host.thicknessMm) / 2.0;
			for (WallEnd& end : ends)
			{
				const cad::Wall& wall = project.walls[end.wall];
				const cad::PlanPoint farEnd = toPlan(end.atStart ? wall.end : wall.start);
				const double side = cross2(hostAxis, sub(farEnd, hostStart));
				if (std::fabs(side) < ParallelEpsilon)
				{
					continue; // runs along the host: nothing to cut
				}
				const cad::PlanPoint face = add(hostStart, scale(leftOf(hostAxis), side > 0.0 ? hostHalf : -hostHalf));
				const std::optional<cad::PlanPoint> left = intersectLines(leftEdge(joint, end), end.away, face, hostAxis);
				const std::optional<cad::PlanPoint> right = intersectLines(rightEdge(joint, end), end.away, face, hostAxis);
				if (left && right)
				{
					end.left = *left;
					end.right = *right;
				}
			}
		}

		// L, Y, X...: each end's left side meets its counter-clockwise neighbour's right side.
		void mitre(const cad::PlanPoint& joint, std::vector<WallEnd>& ends)
		{
			std::sort(ends.begin(), ends.end(), [](const WallEnd& a, const WallEnd& b) { return a.angle < b.angle; });
			for (std::size_t i = 0; i < ends.size(); ++i)
			{
				WallEnd& current = ends[i];
				WallEnd& next = ends[(i + 1) % ends.size()];
				const cad::PlanPoint currentEdge = leftEdge(joint, current);
				const cad::PlanPoint nextEdge = rightEdge(joint, next);
				const std::optional<cad::PlanPoint> corner = intersectLines(currentEdge, current.away, nextEdge, next.away);
				const double limit = MiterLimit * std::max(current.half, next.half);
				if (corner && length2(sub(*corner, joint)) <= limit)
				{
					current.left = *corner;
					next.right = *corner;
				}
				else
				{
					current.left = currentEdge; // collinear or too sharp: square ends through the joint
					next.right = nextEdge;
				}
			}
		}

		std::optional<JointFill> fillFor(const std::vector<WallEnd>& ends, const cad::Project& project)
		{
			std::vector<cad::PlanPoint> outline;
			for (const WallEnd& end : ends) // already sorted counter-clockwise
			{
				for (const cad::PlanPoint& point : {end.right, end.left})
				{
					if (outline.empty() || length2(sub(point, outline.back())) > DuplicateEpsilonMm)
					{
						outline.push_back(point);
					}
				}
			}
			while (outline.size() > 1 && length2(sub(outline.front(), outline.back())) <= DuplicateEpsilonMm)
			{
				outline.pop_back();
			}
			if (outline.size() < 3 || signedArea(outline) < MinFillAreaMm2)
			{
				return std::nullopt;
			}

			JointFill fill;
			fill.outline = std::move(outline);
			const WallEnd* identity = &ends.front();
			for (const WallEnd& end : ends)
			{
				const cad::Wall& wall = project.walls[end.wall];
				fill.heightMm = std::max(fill.heightMm, wall.heightMm);
				if (wall.id < project.walls[identity->wall].id)
				{
					identity = &end;
				}
			}
			fill.wall = project.walls[identity->wall].id;
			fill.atStart = identity->atStart;
			return fill;
		}

		// Both edges must still run forward from start to end, or the resolved ends crossed.
		bool isValid(const cad::Wall& wall, const WallFootprint& footprint)
		{
			const cad::PlanPoint axis = axisDirection(wall);
			const cad::PlanPoint start = toPlan(wall.start);
			auto along = [&](const cad::PlanPoint& p) { return dot2(sub(p, start), axis); };
			return along(footprint.startLeft) < along(footprint.endLeft) && along(footprint.startRight) < along(footprint.endRight);
		}
	}

	WallFootprint plainFootprint(const cad::Wall& wall)
	{
		const cad::PlanPoint axis = axisDirection(wall);
		const cad::PlanPoint side = scale(leftOf(axis), static_cast<double>(wall.thicknessMm) / 2.0);
		const cad::PlanPoint start = toPlan(wall.start);
		const cad::PlanPoint end = toPlan(wall.end);
		return WallFootprint{sub(start, side), add(start, side), sub(end, side), add(end, side)};
	}

	WallJoins solveWallJoins(const cad::Project& project)
	{
		WallJoins joins;
		joins.footprints.reserve(project.walls.size());
		std::map<cad::Point2D, std::vector<WallEnd>, PointLess> joints;
		for (std::size_t index = 0; index < project.walls.size(); ++index)
		{
			const cad::Wall& wall = project.walls[index];
			F101_ASSERT(cad::validateWallGeometry(wall).ok());
			joins.footprints.push_back(plainFootprint(wall));
			const cad::PlanPoint axis = axisDirection(wall);
			const double half = static_cast<double>(wall.thicknessMm) / 2.0;
			joints[wall.start].push_back(WallEnd{index, true, axis, half, std::atan2(axis.y_mm, axis.x_mm), {}, {}});
			const cad::PlanPoint back = scale(axis, -1.0);
			joints[wall.end].push_back(WallEnd{index, false, back, half, std::atan2(back.y_mm, back.x_mm), {}, {}});
		}

		for (auto& [point, ends] : joints)
		{
			const cad::PlanPoint joint = toPlan(point);
			for (WallEnd& end : ends)
			{
				end.left = leftEdge(joint, end); // square end until resolved
				end.right = rightEdge(joint, end);
			}
			if (const cad::Wall* host = hostWall(project, joint, ends))
			{
				cutAtHost(joint, *host, ends, project);
			}
			else if (ends.size() >= 2)
			{
				mitre(joint, ends);
				if (ends.size() >= 3)
				{
					if (std::optional<JointFill> fill = fillFor(ends, project))
					{
						joins.fills.push_back(std::move(*fill));
					}
				}
			}
			for (const WallEnd& end : ends)
			{
				storeEnd(joins.footprints[end.wall], end);
			}
		}

		for (std::size_t index = 0; index < project.walls.size(); ++index)
		{
			if (!isValid(project.walls[index], joins.footprints[index]))
			{
				joins.footprints[index] = plainFootprint(project.walls[index]);
			}
		}
		return joins;
	}

	std::vector<cad::AxisSpan> wallObstructions(const cad::Project& levelWalls, const cad::Wall& wall)
	{
		const WallJoins joins = solveWallJoins(levelWalls);
		const cad::PlanPoint start = toPlan(wall.start);
		const cad::PlanPoint axis = axisDirection(wall);
		const cad::PlanPoint left = leftOf(axis);
		const double length = wall.lengthMm();
		const double half = static_cast<double>(wall.thicknessMm) / 2.0;
		auto along = [&](cad::PlanPoint p) { return dot2(sub(p, start), axis); };
		auto across = [&](cad::PlanPoint p) { return dot2(sub(p, start), left); };

		std::vector<std::pair<double, double>> spans;
		// The ends, up to where both long faces start: mitres and cuts at other walls.
		WallFootprint own = plainFootprint(wall);
		for (std::size_t index = 0; index < levelWalls.walls.size(); ++index)
		{
			if (levelWalls.walls[index].id == wall.id)
			{
				own = joins.footprints[index];
			}
		}
		const double faceStart = std::max(along(own.startLeft), along(own.startRight));
		const double faceEnd = std::min(along(own.endLeft), along(own.endRight));
		if (faceStart > 0.0)
		{
			spans.emplace_back(0.0, faceStart);
		}
		if (faceEnd < length)
		{
			spans.emplace_back(faceEnd, length);
		}

		// Other walls reaching into this wall's band (T joints, crossings, walls alongside): their footprint clipped to
		// the band, as a span along the axis. Walls meeting at an end fall within the end spans.
		const double band = half + 1.0; // a wall cut at this wall's face touches the band
		for (std::size_t index = 0; index < levelWalls.walls.size(); ++index)
		{
			if (levelWalls.walls[index].id == wall.id)
			{
				continue;
			}
			const WallFootprint& other = joins.footprints[index];
			std::vector<cad::PlanPoint> polygon; // (along, across)
			for (const cad::PlanPoint& corner : {other.startRight, other.endRight, other.endLeft, other.startLeft})
			{
				polygon.push_back(cad::PlanPoint{along(corner), across(corner)});
			}
			for (const double sign : {1.0, -1.0})
			{
				// Keep sign * across <= band (Sutherland-Hodgman).
				std::vector<cad::PlanPoint> clipped;
				for (std::size_t i = 0; i < polygon.size(); ++i)
				{
					const cad::PlanPoint& a = polygon[i];
					const cad::PlanPoint& b = polygon[(i + 1) % polygon.size()];
					const double da = sign * a.y_mm - band;
					const double db = sign * b.y_mm - band;
					if (da <= 0.0)
					{
						clipped.push_back(a);
					}
					if ((da < 0.0 && db > 0.0) || (da > 0.0 && db < 0.0))
					{
						clipped.push_back(add(a, scale(sub(b, a), da / (da - db))));
					}
				}
				polygon = std::move(clipped);
			}
			if (polygon.empty())
			{
				continue;
			}
			double low = std::numeric_limits<double>::infinity();
			double high = -std::numeric_limits<double>::infinity();
			for (const cad::PlanPoint& p : polygon)
			{
				low = std::min(low, p.x_mm);
				high = std::max(high, p.x_mm);
			}
			if (high > 0.0 && low < length)
			{
				spans.emplace_back(std::max(0.0, low), std::min(length, high));
			}
		}

		// Whole millimetres, rounded outwards, merged.
		std::sort(spans.begin(), spans.end());
		std::vector<cad::AxisSpan> merged;
		for (const auto& [low, high] : spans)
		{
			const cad::AxisSpan span{static_cast<std::int64_t>(std::floor(low)), static_cast<std::int64_t>(std::ceil(high))};
			if (!merged.empty() && span.fromMm <= merged.back().toMm)
			{
				merged.back().toMm = std::max(merged.back().toMm, span.toMm);
			}
			else
			{
				merged.push_back(span);
			}
		}
		return merged;
	}

	cad::WallObstructions makeWallObstructions()
	{
		return [](const cad::Project& levelWalls, const cad::Wall& wall) { return wallObstructions(levelWalls, wall); };
	}
}
