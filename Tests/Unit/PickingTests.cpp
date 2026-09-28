#include "doctest.h"

#include "F101/Cad/Picking.h"

using namespace f101;
using namespace f101::cad;

namespace
{
	Project walls()
	{
		Project project;
		project.walls.push_back(Wall{WallId{Uuid{0, 1}}, {0, 0}, {3550, 0}, 120, 2400});      // along X
		project.walls.push_back(Wall{WallId{Uuid{0, 2}}, {0, 1000}, {3000, 5000}, 200, 2400}); // diagonal
		return project;
	}
}

TEST_CASE("a ray looking down onto a wall picks it")
{
	const auto hit = pickWall(walls(), Ray3{1000.0, 20.0, 5000.0, 0.0, 0.0, -1.0});
	REQUIRE(hit.has_value());
	CHECK(hit->wall == WallId{Uuid{0, 1}});
	CHECK(hit->distance == doctest::Approx(2600.0)); // top face at z = 2400
}

TEST_CASE("a horizontal ray hits the side face at the right distance")
{
	const auto hit = pickWall(walls(), Ray3{1000.0, -1000.0, 1200.0, 0.0, 1.0, 0.0});
	REQUIRE(hit.has_value());
	CHECK(hit->wall == WallId{Uuid{0, 1}});
	CHECK(hit->distance == doctest::Approx(940.0)); // right face at y = -60
}

TEST_CASE("misses: beside, above, behind the origin")
{
	CHECK_FALSE(pickWall(walls(), Ray3{1000.0, 200.0, 5000.0, 0.0, 0.0, -1.0}).has_value()); // 200 mm beside
	CHECK_FALSE(pickWall(walls(), Ray3{1000.0, -1000.0, 2500.0, 0.0, 1.0, 0.0}).has_value()); // over the top
	CHECK_FALSE(pickWall(walls(), Ray3{1000.0, -1000.0, 1200.0, 0.0, -1.0, 0.0}).has_value()); // looking away
	CHECK_FALSE(pickWall(walls(), Ray3{4000.0, 0.0, 5000.0, 0.0, 0.0, -1.0}).has_value());    // past the end
}

TEST_CASE("diagonal walls are picked in their own frame, the nearest one wins")
{
	// Middle of the diagonal wall, seen from above.
	const auto diagonal = pickWall(walls(), Ray3{1500.0, 3000.0, 3000.0, 0.0, 0.0, -1.0});
	REQUIRE(diagonal.has_value());
	CHECK(diagonal->wall == WallId{Uuid{0, 2}});

	// A ray along Y at x = 300 crosses both walls; the X wall (y ~ 0) comes first.
	const auto first = pickWall(walls(), Ray3{300.0, -5000.0, 1000.0, 0.0, 1.0, 0.0});
	REQUIRE(first.has_value());
	CHECK(first->wall == WallId{Uuid{0, 1}});
}

TEST_CASE("the cursor ray meets the floor plane")
{
	const auto point = intersectFloor(Ray3{0.0, 0.0, 1000.0, 1.0, 2.0, -1.0});
	REQUIRE(point.has_value());
	CHECK(point->x_mm == doctest::Approx(1000.0));
	CHECK(point->y_mm == doctest::Approx(2000.0));
	CHECK_FALSE(intersectFloor(Ray3{0.0, 0.0, 1000.0, 1.0, 0.0, 0.0}).has_value()); // parallel
	CHECK_FALSE(intersectFloor(Ray3{0.0, 0.0, 1000.0, 0.0, 0.0, 1.0}).has_value()); // looking up
}

TEST_CASE("posts (endpoint handles) are hit from above and from the side")
{
	const Point2D center{3550, 0};
	const auto fromAbove = intersectPost(Ray3{3600.0, -100.0, 5000.0, 0.0, 0.0, -1.0}, center, 240.0, 2500.0);
	REQUIRE(fromAbove.has_value());
	CHECK(*fromAbove == doctest::Approx(2500.0));
	const auto fromSide = intersectPost(Ray3{0.0, 0.0, 1000.0, 1.0, 0.0, 0.0}, center, 240.0, 2500.0);
	REQUIRE(fromSide.has_value());
	CHECK(*fromSide == doctest::Approx(3430.0));
	CHECK_FALSE(intersectPost(Ray3{3700.0, 0.0, 5000.0, 0.0, 0.0, -1.0}, center, 240.0, 2500.0).has_value()); // beside
	CHECK_FALSE(intersectPost(Ray3{0.0, 0.0, 2600.0, 1.0, 0.0, 0.0}, center, 240.0, 2500.0).has_value());    // above
}
