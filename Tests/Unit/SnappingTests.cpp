#include "doctest.h"

#include "F101/Cad/Snapping.h"

#include <cmath>
#include <numbers>

using namespace f101;
using namespace f101::cad;

namespace
{
	Project oneWall()
	{
		Project project;
		project.walls.push_back(Wall{WallId{Uuid{0, 1}}, {0, 0}, {3550, 0}, 120, 2400});
		return project;
	}
}

TEST_CASE("free points snap to the grid")
{
	const SnapResult snapped = snapPoint(Project{}, {1024.0, -26.0}, SnapSettings{});
	CHECK(snapped.point == Point2D{1000, -50});
	CHECK(snapped.kind == SnapKind::Grid);

	SnapSettings noGrid;
	noGrid.gridMm = 0;
	CHECK(snapPoint(Project{}, {1024.4, -26.6}, noGrid).point == Point2D{1024, -27});
	CHECK(snapPoint(Project{}, {1024.4, -26.6}, noGrid).kind == SnapKind::Free);
}

TEST_CASE("existing endpoints win over the grid within the tolerance")
{
	const Project project = oneWall();
	const SnapResult near = snapPoint(project, {3500.0, 90.0}, SnapSettings{});
	CHECK(near.point == Point2D{3550, 0});
	CHECK(near.kind == SnapKind::Endpoint);

	const SnapResult far = snapPoint(project, {3300.0, 200.0}, SnapSettings{});
	CHECK(far.kind == SnapKind::Grid);

	// A wall never snaps to itself while one of its ends is dragged.
	CHECK(snapPoint(project, {3500.0, 90.0}, SnapSettings{}, WallId{Uuid{0, 1}}).kind == SnapKind::Grid);
}

TEST_CASE("the second point snaps to 0/15/30/45/90 degrees with the length on the grid")
{
	const Point2D anchor{0, 0};
	const SnapSettings settings;

	// Almost horizontal: exactly on the X axis, length rounded to 50 mm.
	const SnapResult horizontal = snapFromAnchor(Project{}, anchor, {3549.0, 40.0}, settings);
	CHECK(horizontal.point == Point2D{3550, 0});
	CHECK(horizontal.kind == SnapKind::Angle);

	CHECK(snapFromAnchor(Project{}, anchor, {-15.0, 2380.0}, settings).point == Point2D{0, 2400});
	CHECK(snapFromAnchor(Project{}, anchor, {-3010.0, 0.0}, settings).point == Point2D{-3000, 0});

	// 45 degrees: 1000 mm along the diagonal.
	const SnapResult diagonal = snapFromAnchor(Project{}, anchor, {700.0, 720.0}, settings);
	CHECK(diagonal.point == Point2D{707, 707});
}

TEST_CASE("an existing endpoint wins over the angle, but never the anchor itself")
{
	const Project project = oneWall();
	CHECK(snapFromAnchor(project, {0, 3000}, {3520.0, 60.0}, SnapSettings{}).point == Point2D{3550, 0});
	CHECK(snapFromAnchor(project, {0, 0}, {40.0, 30.0}, SnapSettings{}).kind != SnapKind::Endpoint);
}

TEST_CASE("typed lengths follow the cursor direction")
{
	CHECK(pointAtDistance({0, 0}, {10.0, 0.0}, 3550).value() == Point2D{3550, 0});
	CHECK(pointAtDistance({100, 100}, {100.0, -50.0}, 2400).value() == Point2D{100, -2300});
	CHECK(pointAtDistance({0, 0}, {0.0, 0.0}, 500).value() == Point2D{500, 0}); // no direction: +X
	CHECK(pointAtDistance({0, 0}, {3.0, 4.0}, 5000).value() == Point2D{3000, 4000});
}

TEST_CASE("typed lengths on snapped angles read back as typed (error under 0.5 mm)")
{
	for (int degrees = 0; degrees < 360; degrees += 15)
	{
		const double radians = degrees * std::numbers::pi / 180.0;
		const PlanPoint toward{1000.0 * std::cos(radians), 1000.0 * std::sin(radians)};
		for (const std::int64_t length : {1, 7, 120, 999, 3000, 3550, 12345})
		{
			CAPTURE(degrees);
			CAPTURE(length);
			const Point2D end = pointAtDistance({0, 0}, toward, length).value();
			const double actual = std::hypot(static_cast<double>(end.x_mm), static_cast<double>(end.y_mm));
			CHECK(std::fabs(actual - static_cast<double>(length)) < 0.5);
		}
	}
	// 75 degrees, 3000 mm: rounding each coordinate would give (777, 2897) = 2999.4 mm.
	CHECK(pointAtDistance({0, 0}, {258.819, 965.926}, 3000).value() == Point2D{776, 2898});
}

TEST_CASE("an angle snap keeps the exact direction for typed lengths")
{
	const SnapResult snapped = snapFromAnchor(Project{}, {0, 0}, {310.0, 1170.0}, SnapSettings{});
	REQUIRE(snapped.kind == SnapKind::Angle);
	const double angle = std::atan2(snapped.constrained.y_mm, snapped.constrained.x_mm) * 180.0 / std::numbers::pi;
	CHECK(angle == doctest::Approx(75.0).epsilon(1e-12));
	CHECK(std::fabs(std::hypot(static_cast<double>(snapped.point.x_mm), static_cast<double>(snapped.point.y_mm)) - 1200.0) < 0.5);
}
