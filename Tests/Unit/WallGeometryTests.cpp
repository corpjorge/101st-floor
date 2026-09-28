#include "doctest.h"

#include "F101/Geometry/WallGeometryBuilder.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

using namespace f101;
using namespace f101::geometry;

namespace
{
	Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
	double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	Vec3 cross(const Vec3& a, const Vec3& b)
	{
		return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	}

	cad::Wall makeWall(cad::Point2D start, cad::Point2D end, std::int64_t thickness = 120, std::int64_t height = 2400)
	{
		return cad::Wall{cad::WallId{cad::Uuid{0, 1}}, start, end, thickness, height};
	}

	const MeshSection& section(const MeshData& mesh, const std::string& slot)
	{
		for (const MeshSection& s : mesh.sections)
		{
			if (s.slot == slot)
			{
				return s;
			}
		}
		FAIL("missing section " << slot);
		return mesh.sections.front();
	}
}

TEST_CASE("reference wall mesh: closed box, 24 vertices, 12 triangles, one section per face")
{
	const MeshData mesh = buildWallMesh(makeWall({0, 0}, {3550, 0}));
	CHECK(mesh.vertexCount() == 24);
	CHECK(mesh.normals.size() == 24);
	CHECK(mesh.uvs.size() == 24);
	CHECK(mesh.triangleCount() == 12);

	std::set<std::string> slots;
	for (const MeshSection& s : mesh.sections)
	{
		slots.insert(s.slot);
		CHECK(s.indices.size() == 6);
	}
	CHECK(slots == std::set<std::string>{"left", "right", "top", "bottom", "start", "end"});
}

TEST_CASE("reference wall bounds are exactly 3550 x 120 x 2400 mm, centred on the axis")
{
	const Bounds3 b = computeBounds(buildWallMesh(makeWall({0, 0}, {3550, 0})));
	CHECK(b.min.x == 0.0);
	CHECK(b.max.x == 3550.0);
	CHECK(b.min.y == -60.0);
	CHECK(b.max.y == 60.0);
	CHECK(b.min.z == 0.0);
	CHECK(b.max.z == 2400.0);
}

TEST_CASE("normals are unit length and point out of the solid")
{
	const MeshData mesh = buildWallMesh(makeWall({0, 0}, {3550, 0}));
	const Bounds3 b = computeBounds(mesh);
	const Vec3 centre{(b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2, (b.min.z + b.max.z) / 2};
	for (std::size_t i = 0; i < mesh.vertexCount(); ++i)
	{
		CHECK(std::sqrt(dot(mesh.normals[i], mesh.normals[i])) == doctest::Approx(1.0));
		CHECK(dot(mesh.normals[i], mesh.positions[i] - centre) > 0.0);
	}
}

TEST_CASE("every triangle is counter-clockwise seen from outside")
{
	for (const cad::Wall& wall : {makeWall({0, 0}, {3550, 0}), makeWall({500, -200}, {-2500, 3800})})
	{
		const MeshData mesh = buildWallMesh(wall);
		for (const MeshSection& s : mesh.sections)
		{
			for (std::size_t t = 0; t < s.indices.size(); t += 3)
			{
				const Vec3& a = mesh.positions[s.indices[t]];
				const Vec3& b = mesh.positions[s.indices[t + 1]];
				const Vec3& c = mesh.positions[s.indices[t + 2]];
				CHECK(dot(cross(b - a, c - a), mesh.normals[s.indices[t]]) > 0.0);
			}
		}
	}
}

TEST_CASE("the left face lies on the left of the axis")
{
	const MeshData mesh = buildWallMesh(makeWall({0, 0}, {3550, 0}));
	for (std::uint32_t index : section(mesh, "left").indices)
	{
		CHECK(mesh.positions[index].y == 60.0);
	}
	for (std::uint32_t index : section(mesh, "right").indices)
	{
		CHECK(mesh.positions[index].y == -60.0);
	}
}

TEST_CASE("changing the length regenerates the geometry instead of scaling it")
{
	const MeshData original = buildWallMesh(makeWall({0, 0}, {3550, 0}));
	const MeshData longer = buildWallMesh(makeWall({0, 0}, {4200, 0}));
	const Bounds3 b = computeBounds(longer);
	CHECK(b.max.x == 4200.0);
	CHECK(b.max.y - b.min.y == 120.0);
	CHECK(b.max.z == 2400.0);

	// Texture density stays at 1 UV unit per metre: a scaled mesh would stretch its UVs instead.
	double maxU = 0.0;
	for (const Vec2& uv : longer.uvs)
	{
		maxU = std::max(maxU, uv.u);
	}
	CHECK(maxU == doctest::Approx(4.2));
	CHECK(original.vertexCount() == longer.vertexCount());
}

TEST_CASE("a diagonal wall keeps its true thickness and length")
{
	const MeshData mesh = buildWallMesh(makeWall({0, 0}, {3000, 4000}));
	const Vec3& leftPoint = mesh.positions[section(mesh, "left").indices[0]];
	const Vec3& rightPoint = mesh.positions[section(mesh, "right").indices[0]];
	const Vec3& leftNormal = mesh.normals[section(mesh, "left").indices[0]];
	CHECK(dot(leftPoint - rightPoint, leftNormal) == doctest::Approx(120.0));

	double maxU = 0.0;
	for (std::uint32_t index : section(mesh, "left").indices)
	{
		maxU = std::max(maxU, mesh.uvs[index].u);
	}
	CHECK(maxU == doctest::Approx(5.0)); // 5000 mm along the axis
}

TEST_CASE("UVs are in metres")
{
	const MeshData mesh = buildWallMesh(makeWall({0, 0}, {3550, 0}));
	double maxU = 0.0;
	double maxV = 0.0;
	for (std::uint32_t index : section(mesh, "left").indices)
	{
		maxU = std::max(maxU, mesh.uvs[index].u);
		maxV = std::max(maxV, mesh.uvs[index].v);
	}
	CHECK(maxU == doctest::Approx(3.55));
	CHECK(maxV == doctest::Approx(2.4));
}
