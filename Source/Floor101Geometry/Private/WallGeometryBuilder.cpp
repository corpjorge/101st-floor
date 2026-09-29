#include "F101/Geometry/WallGeometryBuilder.h"

#include "F101/Cad/Assert.h"
#include "MeshBuilding.h"

#include <algorithm>
#include <vector>

namespace f101::geometry
{
	using namespace detail;

	namespace
	{
		// A hole through the wall in axis coordinates: along [u0, u1] from the wall start, heights [v0, v1] (mm).
		struct Hole
		{
			double u0 = 0.0;
			double u1 = 0.0;
			double v0 = 0.0;
			double v1 = 0.0;
		};

		// Keeps the part of a convex polygon where along(p) lies in [low, high] (plan clip, Sutherland-Hodgman).
		template <typename Along>
		std::vector<cad::PlanPoint> clipAlong(std::vector<cad::PlanPoint> polygon, double low, double high, Along along)
		{
			for (const bool keepAbove : {true, false})
			{
				const double limit = keepAbove ? low : high;
				auto inside = [&](const cad::PlanPoint& p) { return keepAbove ? along(p) >= limit : along(p) <= limit; };
				std::vector<cad::PlanPoint> clipped;
				for (std::size_t i = 0; i < polygon.size(); ++i)
				{
					const cad::PlanPoint& a = polygon[i];
					const cad::PlanPoint& b = polygon[(i + 1) % polygon.size()];
					if (inside(a))
					{
						clipped.push_back(a);
					}
					if (inside(a) != inside(b))
					{
						const double t = (limit - along(a)) / (along(b) - along(a));
						clipped.push_back(add(a, scale(sub(b, a), t)));
					}
				}
				polygon = std::move(clipped);
			}
			return polygon;
		}
	}

	MeshData buildWallMesh(const cad::Wall& wall)
	{
		return buildWallMesh(wall, plainFootprint(wall));
	}

	MeshData buildWallMesh(const cad::Wall& wall, const WallFootprint& footprint)
	{
		F101_ASSERT(cad::validateWallGeometry(wall).ok());

		const double height = static_cast<double>(wall.heightMm);
		const double half = static_cast<double>(wall.thicknessMm) / 2.0;
		const cad::PlanPoint start = toPlan(wall.start);
		const cad::PlanPoint axisVector = sub(toPlan(wall.end), start);
		const cad::PlanPoint axis = scale(axisVector, 1.0 / length2(axisVector));
		const cad::PlanPoint left = leftOf(axis);

		// Position along the axis from the start and sideways offset (+ = left), in mm.
		auto along = [&](const cad::PlanPoint& p) { return dot2(sub(p, start), axis); };
		auto across = [&](const cad::PlanPoint& p) { return dot2(sub(p, start), left); };
		// Point of a face line (side = +half left, -half right) at a distance along the axis.
		auto onSide = [&](double u, double side) { return add(add(start, scale(axis, u)), scale(left, side)); };
		auto at = [](const cad::PlanPoint& p, double z) { return Vec3{p.x_mm, p.y_mm, z}; };
		auto uv = [](double u, double v) { return Vec2{u * MetersPerMillimeter, v * MetersPerMillimeter}; };

		// Holes stay where both long faces exist: an opening that reaches a mitre or a cut (after a wall was moved, ADR-0013)
		// is trimmed there, so the mesh stays closed.
		const double faceStart = std::max(along(footprint.startLeft), along(footprint.startRight));
		const double faceEnd = std::min(along(footprint.endLeft), along(footprint.endRight));
		std::vector<Hole> holes;
		for (const cad::Opening& opening : wall.openings)
		{
			const double u0 = std::max(static_cast<double>(opening.offsetMm), faceStart);
			const double u1 = std::min(static_cast<double>(opening.offsetMm + opening.widthMm), faceEnd);
			if (u0 < u1)
			{
				holes.push_back(Hole{u0, u1, static_cast<double>(opening.sillMm), static_cast<double>(opening.sillMm + opening.heightMm)});
			}
		}

		MeshData mesh;
		mesh.positions.reserve(24);
		mesh.normals.reserve(24);
		mesh.uvs.reserve(24);
		mesh.sections.reserve(7);

		// Long faces: u along the axis, v up. They stay parallel to the axis whatever the ends look like; around holes
		// they are split into a grid of rectangles and the cells inside a hole are left out.
		auto longFace = [&](const char* slot, const cad::PlanPoint& from, const cad::PlanPoint& to, double side)
		{
			const double uStart = along(from);
			const double uEnd = along(to);
			std::vector<double> us{uStart, uEnd};
			std::vector<double> vs{0.0, height};
			for (const Hole& hole : holes)
			{
				for (const double u : {hole.u0, hole.u1})
				{
					if (u > uStart && u < uEnd)
					{
						us.push_back(u);
					}
				}
				for (const double v : {hole.v0, hole.v1})
				{
					if (v > 0.0 && v < height)
					{
						vs.push_back(v);
					}
				}
			}
			std::sort(us.begin(), us.end());
			us.erase(std::unique(us.begin(), us.end()), us.end());
			std::sort(vs.begin(), vs.end());
			vs.erase(std::unique(vs.begin(), vs.end()), vs.end());

			const cad::PlanPoint outward = scale(left, side > 0.0 ? 1.0 : -1.0);
			for (std::size_t i = 0; i + 1 < us.size(); ++i)
			{
				for (std::size_t j = 0; j + 1 < vs.size(); ++j)
				{
					const double uMid = (us[i] + us[i + 1]) / 2.0;
					const double vMid = (vs[j] + vs[j + 1]) / 2.0;
					const bool inHole = std::any_of(holes.begin(), holes.end(), [&](const Hole& hole)
					{
						return uMid > hole.u0 && uMid < hole.u1 && vMid > hole.v0 && vMid < hole.v1;
					});
					if (inHole)
					{
						continue;
					}
					const cad::PlanPoint a = i == 0 ? from : onSide(us[i], side);
					const cad::PlanPoint b = i + 2 == us.size() ? to : onSide(us[i + 1], side);
					addQuad(mesh, slot,
						Corner{at(a, vs[j]), uv(us[i], vs[j])}, Corner{at(b, vs[j]), uv(us[i + 1], vs[j])},
						Corner{at(b, vs[j + 1]), uv(us[i + 1], vs[j + 1])}, Corner{at(a, vs[j + 1]), uv(us[i], vs[j + 1])},
						Vec3{outward.x_mm, outward.y_mm, 0.0});
				}
			}
		};
		longFace(wall_slots::Left, footprint.startLeft, footprint.endLeft, half);
		longFace(wall_slots::Right, footprint.startRight, footprint.endRight, -half);

		// Top and bottom: u along the axis, v across the thickness (0 on the right edge). Doors cut the bottom.
		auto flatCorner = [&](const cad::PlanPoint& p, double z) { return Corner{at(p, z), uv(along(p), across(p) + half)}; };
		addQuad(mesh, wall_slots::Top, flatCorner(footprint.startLeft, height), flatCorner(footprint.endLeft, height),
			flatCorner(footprint.endRight, height), flatCorner(footprint.startRight, height), Vec3{0.0, 0.0, 1.0});
		std::vector<std::pair<double, double>> floorGaps;
		for (const Hole& hole : holes)
		{
			if (hole.v0 <= 0.0)
			{
				floorGaps.emplace_back(hole.u0, hole.u1);
			}
		}
		if (floorGaps.empty())
		{
			addQuad(mesh, wall_slots::Bottom, flatCorner(footprint.startLeft, 0.0), flatCorner(footprint.endLeft, 0.0),
				flatCorner(footprint.endRight, 0.0), flatCorner(footprint.startRight, 0.0), Vec3{0.0, 0.0, -1.0});
		}
		else
		{
			const std::vector<cad::PlanPoint> outline{footprint.startRight, footprint.endRight, footprint.endLeft, footprint.startLeft};
			double low = -1e300; // gaps are sorted: openings are sorted by offset
			for (std::size_t k = 0; k <= floorGaps.size(); ++k)
			{
				const double high = k < floorGaps.size() ? floorGaps[k].first : 1e300;
				std::vector<Corner> corners;
				for (const cad::PlanPoint& p : clipAlong(outline, low, high, along))
				{
					corners.push_back(flatCorner(p, 0.0));
				}
				addConvexPolygon(mesh, wall_slots::Bottom, corners, Vec3{0.0, 0.0, -1.0});
				low = k < floorGaps.size() ? floorGaps[k].second : low;
			}
		}

		// End caps (square, mitred or cut at another wall): u from the right corner, v up; outward along the axis.
		auto cap = [&](const char* slot, const cad::PlanPoint& right, const cad::PlanPoint& capLeft, double outward)
		{
			const cad::PlanPoint edge = sub(capLeft, right);
			const double width = length2(edge);
			cad::PlanPoint normal = scale(cad::PlanPoint{edge.y_mm, -edge.x_mm}, 1.0 / width);
			if (dot2(normal, axis) * outward < 0.0)
			{
				normal = scale(normal, -1.0);
			}
			addQuad(mesh, slot,
				Corner{at(capLeft, 0), uv(width, 0)}, Corner{at(right, 0), uv(0, 0)},
				Corner{at(right, height), uv(0, height)}, Corner{at(capLeft, height), uv(width, height)},
				Vec3{normal.x_mm, normal.y_mm, 0.0});
		};
		cap(wall_slots::Start, footprint.startRight, footprint.startLeft, -1.0);
		cap(wall_slots::End, footprint.endRight, footprint.endLeft, 1.0);

		// Reveals: the inside of each hole, facing into it. Jambs: u across (0 on the right face), v up.
		for (const Hole& hole : holes)
		{
			for (const auto& [u, facing] : {std::pair<double, double>{hole.u0, 1.0}, {hole.u1, -1.0}})
			{
				const cad::PlanPoint r = onSide(u, -half);
				const cad::PlanPoint l = onSide(u, half);
				addQuad(mesh, wall_slots::Reveal,
					Corner{at(r, hole.v0), uv(0, hole.v0)}, Corner{at(l, hole.v0), uv(2 * half, hole.v0)},
					Corner{at(l, hole.v1), uv(2 * half, hole.v1)}, Corner{at(r, hole.v1), uv(0, hole.v1)},
					Vec3{axis.x_mm * facing, axis.y_mm * facing, 0.0});
			}
			auto horizontal = [&](double z, double normalZ)
			{
				auto corner = [&](double u, double side) { return Corner{at(onSide(u, side), z), uv(u, side + half)}; };
				addQuad(mesh, wall_slots::Reveal, corner(hole.u0, half), corner(hole.u1, half), corner(hole.u1, -half),
					corner(hole.u0, -half), Vec3{0.0, 0.0, normalZ});
			};
			horizontal(hole.v1, -1.0); // head
			if (hole.v0 > 0.0)
			{
				horizontal(hole.v0, 1.0); // window sill
			}
		}
		return mesh;
	}
}
