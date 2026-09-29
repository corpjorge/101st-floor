#include "F101/Geometry/WallGeometryBuilder.h"

#include "F101/Cad/Assert.h"
#include "MeshBuilding.h"

namespace f101::geometry
{
	using namespace detail;

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
		auto at = [](const cad::PlanPoint& p, double z) { return Vec3{p.x_mm, p.y_mm, z}; };
		auto uv = [](double u, double v) { return Vec2{u * MetersPerMillimeter, v * MetersPerMillimeter}; };

		MeshData mesh;
		mesh.positions.reserve(24);
		mesh.normals.reserve(24);
		mesh.uvs.reserve(24);
		mesh.sections.reserve(6);

		// Long faces: u along the axis, v up. They stay parallel to the axis whatever the ends look like.
		auto longFace = [&](const char* slot, const cad::PlanPoint& from, const cad::PlanPoint& to, const cad::PlanPoint& normal)
		{
			addQuad(mesh, slot,
				Corner{at(from, 0), uv(along(from), 0)}, Corner{at(to, 0), uv(along(to), 0)},
				Corner{at(to, height), uv(along(to), height)}, Corner{at(from, height), uv(along(from), height)},
				Vec3{normal.x_mm, normal.y_mm, 0.0});
		};
		longFace(wall_slots::Left, footprint.startLeft, footprint.endLeft, left);
		longFace(wall_slots::Right, footprint.startRight, footprint.endRight, scale(left, -1.0));

		// Top and bottom: u along the axis, v across the thickness (0 on the right edge).
		auto flatFace = [&](const char* slot, double z, double normalZ)
		{
			auto corner = [&](const cad::PlanPoint& p) { return Corner{at(p, z), uv(along(p), across(p) + half)}; };
			addQuad(mesh, slot, corner(footprint.startLeft), corner(footprint.endLeft), corner(footprint.endRight),
				corner(footprint.startRight), Vec3{0.0, 0.0, normalZ});
		};
		flatFace(wall_slots::Top, height, 1.0);
		flatFace(wall_slots::Bottom, 0.0, -1.0);

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

		return mesh;
	}
}
