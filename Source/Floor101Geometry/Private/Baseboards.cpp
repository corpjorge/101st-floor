#include "F101/Geometry/Baseboards.h"

#include "F101/Geometry/PolygonGeometry.h"
#include "MeshBuilding.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace f101::geometry
{
	using namespace detail;

	namespace
	{
		std::optional<cad::PlanPoint> intersectLines(cad::PlanPoint p, cad::PlanPoint d, cad::PlanPoint q, cad::PlanPoint e)
		{
			const double denominator = cross2(d, e);
			if (std::abs(denominator) < 1e-9) return std::nullopt;
			return add(p, scale(d, cross2(sub(q, p), e) / denominator));
		}

		struct Gap
		{
			double from, to, bottom, top;
		};

		void append(MeshData& target, const MeshData& piece)
		{
			const auto offset = static_cast<std::uint32_t>(target.positions.size());
			target.positions.insert(target.positions.end(), piece.positions.begin(), piece.positions.end());
			target.normals.insert(target.normals.end(), piece.normals.begin(), piece.normals.end());
			target.uvs.insert(target.uvs.end(), piece.uvs.begin(), piece.uvs.end());
			for (const MeshSection& source : piece.sections)
			{
				MeshSection& section = sectionFor(target, source.slot.c_str());
				for (const auto index : source.indices) section.indices.push_back(index + offset);
			}
		}

		std::vector<cad::PlanPoint> clip(const std::vector<cad::PlanPoint>& polygon, cad::PlanPoint origin,
			cad::PlanPoint direction, double at, bool keepAbove)
		{
			std::vector<cad::PlanPoint> result;
			if (polygon.empty()) return result;
			auto distance = [&](cad::PlanPoint point) { return (dot2(sub(point, origin), direction) - at) * (keepAbove ? 1.0 : -1.0); };
			cad::PlanPoint previous = polygon.back();
			double before = distance(previous);
			for (const auto point : polygon)
			{
				const double now = distance(point);
				if ((now >= 0.0) != (before >= 0.0)) result.push_back(add(previous, scale(sub(point, previous), before / (before - now))));
				if (now >= 0.0) result.push_back(point);
				previous = point;
				before = now;
			}
			result.erase(std::unique(result.begin(), result.end(), [](auto a, auto b) { return length2(sub(a, b)) < 1e-7; }), result.end());
			if (result.size() > 1 && length2(sub(result.front(), result.back())) < 1e-7) result.pop_back();
			return result;
		}

		void sortCuts(std::vector<double>& values)
		{
			std::sort(values.begin(), values.end());
			values.erase(std::unique(values.begin(), values.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }), values.end());
		}
	}

	MeshData buildBaseboardMesh(const Room& room, const cad::Project& levelWalls, const cad::RoomBaseboard& setting)
	{
		MeshData mesh;
		if (!setting.enabled || setting.heightMm < 1 || setting.thicknessMm < 1 || room.outline.size() < 3) return mesh;
		const auto& points = room.outline;
		const double thickness = static_cast<double>(setting.thicknessMm);
		const double bottom = FloorThicknessMm;
		const double top = bottom + static_cast<double>(setting.heightMm);
		std::vector<cad::PlanPoint> directions, inner;
		for (std::size_t i = 0; i < points.size(); ++i)
		{
			const auto delta = sub(points[(i + 1) % points.size()], points[i]);
			if (length2(delta) < 1e-6) return mesh;
			directions.push_back(scale(delta, 1.0 / length2(delta)));
		}
		for (std::size_t i = 0; i < points.size(); ++i)
		{
			const auto before = directions[(i + points.size() - 1) % points.size()];
			const auto after = directions[i];
			const auto a = add(points[i], scale(leftOf(before), thickness));
			const auto b = add(points[i], scale(leftOf(after), thickness));
			auto meet = intersectLines(a, before, b, after);
			if (!meet || length2(sub(*meet, points[i])) > thickness * 4.0)
			{
				meet = add(points[i], scale(add(leftOf(before), leftOf(after)), thickness / 2.0));
			}
			inner.push_back(*meet);
		}
		for (std::size_t edge = 0; edge < points.size(); ++edge)
		{
			const std::size_t next = (edge + 1) % points.size();
			const auto origin = points[edge];
			const auto direction = directions[edge];
			const double length = length2(sub(points[next], origin));
			std::vector<Gap> gaps;
			std::vector<double> xCuts{0.0, length}, zCuts{bottom, top};
			for (const auto& id : room.walls)
			{
				const cad::Wall* wall = levelWalls.findWall(id);
				if (!wall) continue;
				const auto axis = scale(sub(toPlan(wall->end), toPlan(wall->start)), 1.0 / wall->lengthMm());
				if (std::abs(cross2(axis, direction)) > 1e-6 ||
					std::abs(std::abs(cross2(axis, sub(origin, toPlan(wall->start)))) - static_cast<double>(wall->thicknessMm) / 2.0) > 1e-4) continue;
				for (const cad::Opening& opening : wall->openings)
				{
					const double low = static_cast<double>(opening.sillMm);
					const double high = low + static_cast<double>(opening.heightMm);
					if (low >= top || high <= bottom) continue;
					const auto start = add(toPlan(wall->start), scale(axis, static_cast<double>(opening.offsetMm)));
					const auto end = add(start, scale(axis, static_cast<double>(opening.widthMm)));
					const double a = dot2(sub(start, origin), direction), b = dot2(sub(end, origin), direction);
					const double from = std::clamp(std::min(a, b), 0.0, length), to = std::clamp(std::max(a, b), 0.0, length);
					if (to - from < 1e-6) continue;
					gaps.push_back({from, to, std::max(low, bottom), std::min(high, top)});
					xCuts.insert(xCuts.end(), {from, to});
					zCuts.insert(zCuts.end(), {std::max(low, bottom), std::min(high, top)});
				}
			}
			sortCuts(xCuts);
			sortCuts(zCuts);
			for (std::size_t x = 1; x < xCuts.size(); ++x)
			{
				const double middleX = (xCuts[x - 1] + xCuts[x]) / 2.0;
				std::vector<cad::PlanPoint> polygon{origin, points[next], inner[next], inner[edge]};
				if (xCuts[x - 1] > 0.0) polygon = clip(polygon, origin, direction, xCuts[x - 1], true);
				if (xCuts[x] < length) polygon = clip(polygon, origin, direction, xCuts[x], false);
				if (polygon.size() < 3 || signedArea(polygon) <= 1e-6) continue;
				for (std::size_t z = 1; z < zCuts.size(); ++z)
				{
					const double middleZ = (zCuts[z - 1] + zCuts[z]) / 2.0;
					if (std::any_of(gaps.begin(), gaps.end(), [&](const Gap& gap)
						{ return middleX > gap.from && middleX < gap.to && middleZ > gap.bottom && middleZ < gap.top; })) continue;
					append(mesh, buildPrismMesh(polygon, zCuts[z - 1], zCuts[z], {"baseboard", "baseboard", "baseboard"}));
				}
			}
		}
		return mesh;
	}
}
