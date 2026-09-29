#include "F101/Geometry/PolygonGeometry.h"

#include "MeshBuilding.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace f101::geometry
{
	using namespace detail;

	namespace
	{
		constexpr double RelativeEpsilon = 1e-9;

		// Twice the signed area of abc; positive when counter-clockwise.
		double turn(cad::PlanPoint a, cad::PlanPoint b, cad::PlanPoint c)
		{
			return cross2(sub(b, a), sub(c, b));
		}

		bool insideTriangle(cad::PlanPoint p, cad::PlanPoint a, cad::PlanPoint b, cad::PlanPoint c)
		{
			const double scaleSq = std::max({dot2(sub(b, a), sub(b, a)), dot2(sub(c, b), sub(c, b)), dot2(sub(a, c), sub(a, c))});
			const double epsilon = -RelativeEpsilon * scaleSq;
			return cross2(sub(b, a), sub(p, a)) >= epsilon && cross2(sub(c, b), sub(p, b)) >= epsilon &&
				cross2(sub(a, c), sub(p, c)) >= epsilon;
		}
	}

	double signedArea(const std::vector<cad::PlanPoint>& outline)
	{
		double twice = 0.0;
		for (std::size_t i = 0; i < outline.size(); ++i)
		{
			twice += cross2(outline[i], outline[(i + 1) % outline.size()]);
		}
		return twice / 2.0;
	}

	bool containsPoint(const std::vector<cad::PlanPoint>& outline, cad::PlanPoint point)
	{
		bool inside = false;
		for (std::size_t i = 0, j = outline.size() - 1; i < outline.size(); j = i++)
		{
			const cad::PlanPoint& a = outline[i];
			const cad::PlanPoint& b = outline[j];
			if ((a.y_mm > point.y_mm) != (b.y_mm > point.y_mm) &&
				point.x_mm < (b.x_mm - a.x_mm) * (point.y_mm - a.y_mm) / (b.y_mm - a.y_mm) + a.x_mm)
			{
				inside = !inside;
			}
		}
		return inside;
	}

	std::vector<std::uint32_t> triangulatePolygon(const std::vector<cad::PlanPoint>& outline)
	{
		std::vector<std::uint32_t> triangles;
		if (outline.size() < 3 || signedArea(outline) <= 0.0)
		{
			return triangles;
		}
		std::vector<std::uint32_t> remaining(outline.size());
		std::iota(remaining.begin(), remaining.end(), 0u);

		while (remaining.size() > 3)
		{
			bool clipped = false;
			const std::size_t count = remaining.size();
			for (std::size_t i = 0; i < count && !clipped; ++i)
			{
				const std::uint32_t previous = remaining[(i + count - 1) % count];
				const std::uint32_t current = remaining[i];
				const std::uint32_t next = remaining[(i + 1) % count];
				const cad::PlanPoint a = outline[previous];
				const cad::PlanPoint b = outline[current];
				const cad::PlanPoint c = outline[next];
				const double bend = turn(a, b, c);
				const double tolerance = RelativeEpsilon * length2(sub(b, a)) * length2(sub(c, b));
				if (std::fabs(bend) <= tolerance)
				{
					remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i)); // collinear: no triangle needed
					clipped = true;
					break;
				}
				if (bend < 0.0)
				{
					continue; // reflex vertex
				}
				bool ear = true;
				for (const std::uint32_t other : remaining)
				{
					if (other != previous && other != current && other != next && insideTriangle(outline[other], a, b, c))
					{
						ear = false;
						break;
					}
				}
				if (ear)
				{
					triangles.insert(triangles.end(), {previous, current, next});
					remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
					clipped = true;
				}
			}
			if (!clipped)
			{
				return {}; // no ear: the outline crosses itself
			}
		}
		if (turn(outline[remaining[0]], outline[remaining[1]], outline[remaining[2]]) > 0.0)
		{
			triangles.insert(triangles.end(), {remaining[0], remaining[1], remaining[2]});
		}
		return triangles;
	}

	MeshData buildPrismMesh(const std::vector<cad::PlanPoint>& outline, double bottomZ, double topZ, const PrismSlots& slots)
	{
		MeshData mesh;
		const std::vector<std::uint32_t> triangles = triangulatePolygon(outline);
		if (triangles.empty())
		{
			return mesh;
		}

		auto cap = [&](const char* slot, double z, double normalZ)
		{
			const auto base = static_cast<std::uint32_t>(mesh.positions.size());
			for (const cad::PlanPoint& p : outline)
			{
				mesh.positions.push_back(Vec3{p.x_mm, p.y_mm, z});
				mesh.normals.push_back(Vec3{0.0, 0.0, normalZ});
				mesh.uvs.push_back(Vec2{p.x_mm * MetersPerMillimeter, p.y_mm * MetersPerMillimeter});
			}
			MeshSection& section = sectionFor(mesh, slot);
			for (std::size_t t = 0; t < triangles.size(); t += 3)
			{
				// Counter-clockwise from above; the bottom is seen from below, so it is reversed.
				const std::uint32_t second = normalZ > 0.0 ? triangles[t + 1] : triangles[t + 2];
				const std::uint32_t third = normalZ > 0.0 ? triangles[t + 2] : triangles[t + 1];
				section.indices.insert(section.indices.end(), {base + triangles[t], base + second, base + third});
			}
		};
		cap(slots.top, topZ, 1.0);
		cap(slots.bottom, bottomZ, -1.0);

		double perimeter = 0.0;
		for (std::size_t i = 0; i < outline.size(); ++i)
		{
			const cad::PlanPoint a = outline[i];
			const cad::PlanPoint b = outline[(i + 1) % outline.size()];
			const double length = length2(sub(b, a));
			if (length <= 0.0)
			{
				continue;
			}
			// Counter-clockwise outline: the outside is on the right of each edge.
			const Vec3 normal{(b.y_mm - a.y_mm) / length, -(b.x_mm - a.x_mm) / length, 0.0};
			auto corner = [&](cad::PlanPoint p, double z, double u)
			{
				return Corner{Vec3{p.x_mm, p.y_mm, z}, Vec2{u * MetersPerMillimeter, z * MetersPerMillimeter}};
			};
			addQuad(mesh, slots.side, corner(a, bottomZ, perimeter), corner(b, bottomZ, perimeter + length),
				corner(b, topZ, perimeter + length), corner(a, topZ, perimeter), normal);
			perimeter += length;
		}
		return mesh;
	}
}
