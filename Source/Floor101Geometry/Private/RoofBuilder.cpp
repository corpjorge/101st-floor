#include "F101/Geometry/RoofBuilder.h"
#include "F101/Geometry/PolygonGeometry.h"
#include "MeshBuilding.h"

#include <array>
#include <limits>
#include <numbers>

namespace f101::geometry
{
	namespace
	{
		using namespace detail;
		using Polygon = std::vector<cad::PlanPoint>;

		// Work in cross-slope u, upslope v coordinates; 0 drains south, then east, north, west.
		cad::PlanPoint toRoof(cad::PlanPoint p, int quarter)
		{
			switch (quarter)
			{
			case 1: return {p.y_mm, -p.x_mm};
			case 2: return {-p.x_mm, -p.y_mm};
			case 3: return {-p.y_mm, p.x_mm};
			default: return p;
			}
		}
		cad::PlanPoint fromRoof(cad::PlanPoint p, int quarter) { return toRoof(p, (4 - quarter) % 4); }

		Polygon clip(const Polygon& input, double edge, bool isU, bool above)
		{
			Polygon result;
			if (input.empty()) return result;
			auto coordinate = [isU](cad::PlanPoint p) { return isU ? p.x_mm : p.y_mm; };
			cad::PlanPoint a = input.back();
			double da = (coordinate(a) - edge) * (above ? 1.0 : -1.0);
			for (const auto b : input)
			{
				const double db = (coordinate(b) - edge) * (above ? 1.0 : -1.0);
				if ((da >= 0.0) != (db >= 0.0)) result.push_back(add(a, scale(sub(b, a), da / (da - db))));
				if (db >= 0.0) result.push_back(b);
				a = b; da = db;
			}
			// Clipping exactly on a triangle corner can repeat a point: discard degenerate edges.
			Polygon clean;
			for (auto p : result) if (clean.empty() || length2(sub(p, clean.back())) > 0.001) clean.push_back(p);
			if (clean.size() > 1 && length2(sub(clean.front(), clean.back())) < 0.001) clean.pop_back();
			return clean;
		}

		void append(MeshData& target, const MeshData& source)
		{
			const auto offset = static_cast<std::uint32_t>(target.positions.size());
			target.positions.insert(target.positions.end(), source.positions.begin(), source.positions.end());
			target.normals.insert(target.normals.end(), source.normals.begin(), source.normals.end());
			target.uvs.insert(target.uvs.end(), source.uvs.begin(), source.uvs.end());
			for (const auto& section : source.sections)
			{
				auto& destination = sectionFor(target, section.slot.c_str());
				for (auto index : section.indices) destination.indices.push_back(offset + index);
			}
		}
	}

	MeshData buildRoofMesh(const Room& room, double levelTopMm, const cad::RoomRoof& roof)
	{
		MeshData mesh;
		if (!cad::validateRoomRoof(roof)) return mesh;
		const auto indices = triangulatePolygon(room.outline);
		if (indices.empty()) return mesh;
		Polygon outline;
		double minU = std::numeric_limits<double>::max(), maxU = -minU, minV = minU, maxV = -minU;
		for (auto p : room.outline)
		{
			p = toRoof(p, roof.fallQuarterTurns); outline.push_back(p);
			minU = std::min(minU, p.x_mm); maxU = std::max(maxU, p.x_mm);
			minV = std::min(minV, p.y_mm); maxV = std::max(maxV, p.y_mm);
		}
		const double slope = static_cast<double>(roof.riseMm) / std::max(1.0, maxV - minV);
		constexpr double Pitch = 177.0, Step = Pitch / 12.0;
		auto wave = [minU](double u) { return 12.0 * (1.0 - std::cos((u - minU) * 2.0 * std::numbers::pi / Pitch)); };
		// Each original triangle is clipped separately: concave rooms and narrow notches stay exact.
		for (std::size_t t = 0; t < indices.size(); t += 3)
		{
			const Polygon triangle{outline[indices[t]], outline[indices[t + 1]], outline[indices[t + 2]]};
			auto prism = [&](const Polygon& polygon, double bottom, double top, const char* slot, double gradientU, double offset)
			{
				if (polygon.size() < 3) return;
				MeshData part = buildPrismMesh(polygon, bottom, top, {slot, slot, slot});
				for (auto& p : part.positions)
				{
					p.z += levelTopMm + slope * (p.y - minV) + gradientU * p.x + offset;
					const auto xy = fromRoof({p.x, p.y}, roof.fallQuarterTurns); p.x = xy.x_mm; p.y = xy.y_mm;
				}
				for (auto& n : part.normals)
				{
					n = normalized({n.x - gradientU * n.z, n.y - slope * n.z, n.z});
					const auto xy = fromRoof({n.x, n.y}, roof.fallQuarterTurns); n.x = xy.x_mm; n.y = xy.y_mm;
				}
				append(mesh, part);
			};
			for (double u = minU; u < maxU - 0.001; u += Step)
			{
				const double end = std::min(maxU, u + Step), a = wave(u), gradient = (wave(end) - a) / (end - u);
				prism(clip(clip(triangle, u, true, true), end, true, false), 0.0, 3.0, roof_slots::Sheet, gradient, a - gradient * u);
			}
			// Purlins perpendicular to the fall, plus two edge rails. These are visible white metal profiles.
			const int bays = std::max(1, static_cast<int>(std::ceil((maxV - minV) / 1200.0)));
			for (int i = 0; i <= bays; ++i)
			{
				const double v = minV + (maxV - minV) * i / bays;
				prism(clip(clip(triangle, v - 20.0, false, true), v + 20.0, false, false), -90.0, -2.0, roof_slots::Frame, 0.0, 0.0);
			}
			for (double u : {minU + 20.0, maxU - 20.0})
				prism(clip(clip(triangle, u - 20.0, true, true), u + 20.0, true, false), -90.0, -2.0, roof_slots::Frame, 0.0, 0.0);
		}
		// Translucent perimeter infill closes the gap between horizontal walls and the pitched sheets.
		for (std::size_t i = 0; i < outline.size(); ++i)
		{
			const auto a = outline[i], b = outline[(i + 1) % outline.size()];
			const auto pa = fromRoof(a, roof.fallQuarterTurns), pb = fromRoof(b, roof.fallQuarterTurns);
			const Vec3 normal = normalized({pb.y_mm - pa.y_mm, pa.x_mm - pb.x_mm, 0});
			const double za = slope * (a.y_mm - minV), zb = slope * (b.y_mm - minV);
			if (std::max(za, zb) < 0.001) continue;
			auto corner = [levelTopMm](cad::PlanPoint p, double z) { return Corner{{p.x_mm, p.y_mm, levelTopMm + z}, {p.x_mm * .001, z * .001}}; };
			std::vector<Corner> infill{corner(pa, 0), corner(pb, 0)};
			if (zb > 0.001) infill.push_back(corner(pb, zb));
			if (za > 0.001) infill.push_back(corner(pa, za));
			addConvexPolygon(mesh, roof_slots::Sheet, infill, normal);
			addConvexPolygon(mesh, roof_slots::Sheet, infill, {-normal.x, -normal.y, 0});
		}
		return mesh;
	}
}
