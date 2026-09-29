#include "doctest.h"

#include "F101/Geometry/PolygonGeometry.h"
#include "F101/Geometry/WallGeometryBuilder.h"
#include "F101/Geometry/WallJoins.h"

#include <cmath>
#include <cstdint>

using namespace f101;
using namespace f101::geometry;

namespace
{
	std::uint64_t nextId = 1;

	cad::Wall wall(cad::Point2D start, cad::Point2D end, std::int64_t thickness = 120, std::int64_t height = 2400)
	{
		return cad::Wall{cad::WallId{cad::Uuid{0, nextId++}}, start, end, thickness, height};
	}

	void checkPoint(const cad::PlanPoint& actual, double x, double y)
	{
		CHECK(actual.x_mm == doctest::Approx(x).epsilon(1e-12));
		CHECK(actual.y_mm == doctest::Approx(y).epsilon(1e-12));
	}

	bool samePoint(const cad::PlanPoint& a, const cad::PlanPoint& b)
	{
		return std::fabs(a.x_mm - b.x_mm) < 1e-9 && std::fabs(a.y_mm - b.y_mm) < 1e-9;
	}

	bool sameFootprint(const WallFootprint& a, const WallFootprint& b)
	{
		return samePoint(a.startLeft, b.startLeft) && samePoint(a.startRight, b.startRight) &&
			samePoint(a.endLeft, b.endLeft) && samePoint(a.endRight, b.endRight);
	}

	std::vector<cad::PlanPoint> outline(const WallFootprint& f)
	{
		return {f.startRight, f.endRight, f.endLeft, f.startLeft};
	}
}

TEST_CASE("a wall on its own keeps its plain rectangle")
{
	cad::Project project;
	project.walls = {wall({0, 0}, {3550, 0})};
	const WallJoins joins = solveWallJoins(project);
	REQUIRE(joins.footprints.size() == 1);
	CHECK(joins.fills.empty());
	const WallFootprint& f = joins.footprints[0];
	checkPoint(f.startLeft, 0, 60);
	checkPoint(f.startRight, 0, -60);
	checkPoint(f.endLeft, 3550, 60);
	checkPoint(f.endRight, 3550, -60);
	CHECK(sameFootprint(f, plainFootprint(project.walls[0])));
}

TEST_CASE("L corner: both ends are mitred to the same inner and outer corner")
{
	cad::Project project;
	project.walls = {wall({0, 0}, {4000, 0}), wall({4000, 0}, {4000, 3000})};
	const WallJoins joins = solveWallJoins(project);
	CHECK(joins.fills.empty());
	const WallFootprint& east = joins.footprints[0];
	const WallFootprint& north = joins.footprints[1];
	checkPoint(east.endLeft, 3940, 60);   // inner corner
	checkPoint(east.endRight, 4060, -60); // outer corner
	checkPoint(north.startLeft, 3940, 60);
	checkPoint(north.startRight, 4060, -60);
	checkPoint(east.startLeft, 0, 60); // free ends stay square
	checkPoint(north.endLeft, 3940, 3000);

	// Together they cover the L with no overlap: the footprint areas add up to the L's area.
	const double area = signedArea(outline(east)) + signedArea(outline(north));
	const double expected = 4060.0 * 3060.0 - 3940.0 * 2940.0; // bounding rectangle minus the empty inner one
	CHECK(area == doctest::Approx(expected));
}

TEST_CASE("L corner with different thicknesses meets on both faces")
{
	cad::Project project;
	project.walls = {wall({0, 0}, {4000, 0}, 200), wall({4000, 0}, {4000, 3000}, 100)};
	const WallJoins joins = solveWallJoins(project);
	checkPoint(joins.footprints[0].endLeft, 3950, 100);
	checkPoint(joins.footprints[0].endRight, 4050, -100);
	checkPoint(joins.footprints[1].startLeft, 3950, 100);
	checkPoint(joins.footprints[1].startRight, 4050, -100);
}

TEST_CASE("T: an end inside another wall is cut at that wall's face; the host is untouched")
{
	for (const cad::Point2D end : {cad::Point2D{3000, 0}, cad::Point2D{3000, 100}, cad::Point2D{3000, 37}})
	{
		CAPTURE(end.y_mm);
		cad::Project project;
		project.walls = {wall({0, 0}, {6000, 0}, 200), wall({3000, 3000}, end, 100)};
		const WallJoins joins = solveWallJoins(project);
		CHECK(sameFootprint(joins.footprints[0], plainFootprint(project.walls[0])));
		const WallFootprint& stem = joins.footprints[1];
		checkPoint(stem.endLeft, 3050, 100); // heading south, the left side is east
		checkPoint(stem.endRight, 2950, 100);
		CHECK(joins.fills.empty());
	}
}

TEST_CASE("collinear walls meet square, with no fill")
{
	cad::Project project;
	project.walls = {wall({0, 0}, {2000, 0}), wall({2000, 0}, {4000, 0}, 200)};
	const WallJoins joins = solveWallJoins(project);
	checkPoint(joins.footprints[0].endLeft, 2000, 60);
	checkPoint(joins.footprints[0].endRight, 2000, -60);
	checkPoint(joins.footprints[1].startLeft, 2000, 100);
	checkPoint(joins.footprints[1].startRight, 2000, -100);
	CHECK(joins.fills.empty());
}

TEST_CASE("four ends in a cross are mitred and the centre is filled")
{
	cad::Project project;
	project.walls = {wall({0, 0}, {2000, 0}), wall({0, 0}, {0, 2000}), wall({0, 0}, {-2000, 0}), wall({0, 0}, {0, -2000}, 120, 2700)};
	const WallJoins joins = solveWallJoins(project);
	checkPoint(joins.footprints[0].startLeft, 60, 60);
	checkPoint(joins.footprints[0].startRight, 60, -60);
	REQUIRE(joins.fills.size() == 1);
	const JointFill& fill = joins.fills[0];
	CHECK(signedArea(fill.outline) == doctest::Approx(120.0 * 120.0));
	CHECK(fill.heightMm == 2700); // the tallest wall
	CHECK(fill.wall == project.walls[0].id);
	CHECK(fill.atStart);
}

TEST_CASE("three ends in a T shape leave a triangle to fill")
{
	cad::Project project;
	project.walls = {wall({0, 0}, {2000, 0}), wall({0, 0}, {-2000, 0}), wall({0, 0}, {0, 2000})};
	const WallJoins joins = solveWallJoins(project);
	REQUIRE(joins.fills.size() == 1);
	CHECK(signedArea(joins.fills[0].outline) == doctest::Approx(2.0 * 60.0 * 60.0));
}

TEST_CASE("very sharp corners fall back to square ends (miter limit)")
{
	cad::Project project;
	project.walls = {wall({0, 0}, {4000, 0}), wall({0, 0}, {4000, 200})};
	const WallJoins joins = solveWallJoins(project);
	CHECK(sameFootprint(joins.footprints[0], plainFootprint(project.walls[0])));
	CHECK(sameFootprint(joins.footprints[1], plainFootprint(project.walls[1])));
}

TEST_CASE("a wall whose cut ends would cross keeps its plain footprint")
{
	cad::Project project;
	project.walls = {wall({0, -1000}, {0, 1000}, 400), wall({-50, 0}, {150, 0})};
	const WallJoins joins = solveWallJoins(project);
	CHECK(sameFootprint(joins.footprints[1], plainFootprint(project.walls[1])));
}

TEST_CASE("a mitred wall mesh is a closed prism with outward faces over its footprint")
{
	cad::Project project;
	project.walls = {wall({0, 0}, {4000, 0}), wall({4000, 0}, {4000, 3000})};
	const WallJoins joins = solveWallJoins(project);
	const MeshData mesh = buildWallMesh(project.walls[0], joins.footprints[0]);
	CHECK(mesh.vertexCount() == 24);
	CHECK(mesh.triangleCount() == 12);
	const Bounds3 bounds = computeBounds(mesh);
	CHECK(bounds.max.x == doctest::Approx(4060.0));
	CHECK(bounds.min.x == doctest::Approx(0.0));

	// Every face points away from the wall's centre.
	const Vec3 centre{(bounds.min.x + bounds.max.x) / 2, (bounds.min.y + bounds.max.y) / 2, (bounds.min.z + bounds.max.z) / 2};
	for (const MeshSection& section : mesh.sections)
	{
		for (std::size_t t = 0; t < section.indices.size(); t += 3)
		{
			const Vec3& a = mesh.positions[section.indices[t]];
			const Vec3& b = mesh.positions[section.indices[t + 1]];
			const Vec3& c = mesh.positions[section.indices[t + 2]];
			const Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
			const Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
			const Vec3 n{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
			const Vec3 out{(a.x + b.x + c.x) / 3 - centre.x, (a.y + b.y + c.y) / 3 - centre.y, (a.z + b.z + c.z) / 3 - centre.z};
			CAPTURE(section.slot);
			CHECK(n.x * out.x + n.y * out.y + n.z * out.z > 0.0);
		}
	}
}

TEST_CASE("the plain footprint gives the same mesh as before joins existed")
{
	const cad::Wall reference = wall({0, 0}, {3550, 0});
	const MeshData viaDefault = buildWallMesh(reference);
	const MeshData viaFootprint = buildWallMesh(reference, plainFootprint(reference));
	CHECK(viaDefault.positions.size() == viaFootprint.positions.size());
	const Bounds3 b = computeBounds(viaDefault);
	CHECK(b.min.x == 0.0);
	CHECK(b.max.x == 3550.0);
	CHECK(b.min.y == -60.0);
	CHECK(b.max.y == 60.0);
}
