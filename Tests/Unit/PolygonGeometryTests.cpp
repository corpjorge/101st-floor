#include "doctest.h"

#include "F101/Geometry/PolygonGeometry.h"

#include <cmath>

using namespace f101;
using namespace f101::geometry;

namespace
{
	double trianglesArea(const std::vector<cad::PlanPoint>& outline, const std::vector<std::uint32_t>& triangles)
	{
		double area = 0.0;
		for (std::size_t t = 0; t < triangles.size(); t += 3)
		{
			const cad::PlanPoint& a = outline[triangles[t]];
			const cad::PlanPoint& b = outline[triangles[t + 1]];
			const cad::PlanPoint& c = outline[triangles[t + 2]];
			const double twice = (b.x_mm - a.x_mm) * (c.y_mm - a.y_mm) - (b.y_mm - a.y_mm) * (c.x_mm - a.x_mm);
			CHECK(twice > 0.0); // every triangle counter-clockwise
			area += twice / 2.0;
		}
		return area;
	}

	// L-shaped room: 4 x 3 m minus a 2 x 1.5 m notch.
	const std::vector<cad::PlanPoint> lShape{{0, 0}, {4000, 0}, {4000, 1500}, {2000, 1500}, {2000, 3000}, {0, 3000}};
}

TEST_CASE("signed area follows the orientation")
{
	const std::vector<cad::PlanPoint> square{{0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}};
	CHECK(signedArea(square) == 1'000'000.0);
	CHECK(signedArea({square.rbegin(), square.rend()}) == -1'000'000.0);
	CHECK(signedArea(lShape) == 9'000'000.0);
}

TEST_CASE("a concave polygon is covered exactly by counter-clockwise triangles")
{
	const std::vector<std::uint32_t> triangles = triangulatePolygon(lShape);
	CHECK(triangles.size() == 3 * 4);
	CHECK(trianglesArea(lShape, triangles) == doctest::Approx(9'000'000.0));
}

TEST_CASE("collinear vertices are skipped; clockwise or degenerate outlines give nothing")
{
	const std::vector<cad::PlanPoint> withMidpoints{{0, 0}, {500, 0}, {1000, 0}, {1000, 1000}, {0, 1000}, {0, 500}};
	CHECK(trianglesArea(withMidpoints, triangulatePolygon(withMidpoints)) == doctest::Approx(1'000'000.0));
	CHECK(triangulatePolygon({lShape.rbegin(), lShape.rend()}).empty());
	CHECK(triangulatePolygon({{0, 0}, {1000, 0}, {2000, 0}}).empty());
}

TEST_CASE("point containment")
{
	CHECK(containsPoint(lShape, {1000, 2000}));
	CHECK_FALSE(containsPoint(lShape, {3000, 2000})); // in the notch
	CHECK_FALSE(containsPoint(lShape, {-10, 10}));
}

TEST_CASE("prisms are closed and flat-shaded, with top, side and bottom sections")
{
	const MeshData prism = buildPrismMesh(lShape, 0.0, 10.0, PrismSlots{"floor", "edge", "under"});
	REQUIRE(prism.sections.size() == 3);
	CHECK(prism.sections[0].slot == "floor");
	CHECK(prism.sections[1].slot == "under");
	CHECK(prism.sections[2].slot == "edge");
	CHECK(prism.triangleCount() == 4 + 4 + 2 * 6);
	const Bounds3 b = computeBounds(prism);
	CHECK(b.max.x == 4000.0);
	CHECK(b.max.y == 3000.0);
	CHECK(b.max.z == 10.0);
	CHECK(buildPrismMesh({{0, 0}, {1000, 0}}, 0.0, 10.0).positions.empty());
}
