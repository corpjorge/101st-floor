#include "F101/Geometry/WallGeometryBuilder.h"

#include "F101/Cad/Assert.h"

#include <utility>

namespace f101::geometry
{
	namespace
	{
		constexpr double MetersPerMillimeter = 0.001;

		Vec3 subtract(const Vec3& a, const Vec3& b)
		{
			return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
		}

		Vec3 cross(const Vec3& a, const Vec3& b)
		{
			return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
		}

		double dot(const Vec3& a, const Vec3& b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		struct Corner
		{
			Vec3 position;
			Vec2 uv;
		};

		// Appends a planar quad from its corners in perimeter order (either direction) and its outward normal.
		// The winding is derived from the normal, so every face is counter-clockwise seen from outside by construction.
		void addQuad(MeshData& mesh, const char* slot, Corner a, Corner b, Corner c, Corner d, const Vec3& normal)
		{
			if (dot(cross(subtract(b.position, a.position), subtract(c.position, a.position)), normal) < 0.0)
			{
				std::swap(b, d);
			}
			const auto base = static_cast<std::uint32_t>(mesh.positions.size());
			for (const Corner& corner : {a, b, c, d})
			{
				mesh.positions.push_back(corner.position);
				mesh.normals.push_back(normal);
				mesh.uvs.push_back(corner.uv);
			}
			mesh.sections.push_back(MeshSection{slot, {base, base + 1, base + 2, base, base + 2, base + 3}});
		}
	}

	MeshData buildWallMesh(const cad::Wall& wall)
	{
		F101_ASSERT(cad::validateWall(wall).ok());

		const double length = wall.lengthMm();
		const double height = static_cast<double>(wall.heightMm);
		const double half = static_cast<double>(wall.thicknessMm) / 2.0;
		const double startX = static_cast<double>(wall.start.x_mm);
		const double startY = static_cast<double>(wall.start.y_mm);
		// Unit axis direction and its left-hand normal in plan view.
		const double axisX = (static_cast<double>(wall.end.x_mm) - startX) / length;
		const double axisY = (static_cast<double>(wall.end.y_mm) - startY) / length;
		const double leftX = -axisY;
		const double leftY = axisX;

		// Point at a distance along the axis, offset sideways (+ = left) and at height z. All in mm.
		auto corner = [&](double along, double side, double z, double u, double v)
		{
			return Corner{Vec3{startX + axisX * along + leftX * side, startY + axisY * along + leftY * side, z},
				Vec2{u * MetersPerMillimeter, v * MetersPerMillimeter}};
		};

		MeshData mesh;
		mesh.positions.reserve(24);
		mesh.normals.reserve(24);
		mesh.uvs.reserve(24);
		mesh.sections.reserve(6);

		// Long faces: u along the axis, v up.
		addQuad(mesh, wall_slots::Left,
			corner(0, half, 0, 0, 0), corner(length, half, 0, length, 0),
			corner(length, half, height, length, height), corner(0, half, height, 0, height),
			Vec3{leftX, leftY, 0.0});
		addQuad(mesh, wall_slots::Right,
			corner(0, -half, 0, 0, 0), corner(length, -half, 0, length, 0),
			corner(length, -half, height, length, height), corner(0, -half, height, 0, height),
			Vec3{-leftX, -leftY, 0.0});

		// Top and bottom: u along the axis, v across the thickness.
		addQuad(mesh, wall_slots::Top,
			corner(0, half, height, 0, 2 * half), corner(length, half, height, length, 2 * half),
			corner(length, -half, height, length, 0), corner(0, -half, height, 0, 0),
			Vec3{0.0, 0.0, 1.0});
		addQuad(mesh, wall_slots::Bottom,
			corner(0, half, 0, 0, 2 * half), corner(length, half, 0, length, 2 * half),
			corner(length, -half, 0, length, 0), corner(0, -half, 0, 0, 0),
			Vec3{0.0, 0.0, -1.0});

		// End caps: u across the thickness, v up.
		addQuad(mesh, wall_slots::Start,
			corner(0, half, 0, 2 * half, 0), corner(0, -half, 0, 0, 0),
			corner(0, -half, height, 0, height), corner(0, half, height, 2 * half, height),
			Vec3{-axisX, -axisY, 0.0});
		addQuad(mesh, wall_slots::End,
			corner(length, half, 0, 2 * half, 0), corner(length, -half, 0, 0, 0),
			corner(length, -half, height, 0, height), corner(length, half, height, 2 * half, height),
			Vec3{axisX, axisY, 0.0});

		return mesh;
	}
}
