#include "doctest.h"

#include "F101/Geometry/PolygonGeometry.h"
#include "F101/Geometry/Rooms.h"

#include <cstdint>
#include <algorithm>
#include <initializer_list>

using namespace f101;
using namespace f101::geometry;

namespace
{
	struct Plan
	{
		cad::Project project;
		std::uint64_t nextId = 1;

		cad::WallId add(cad::Point2D start, cad::Point2D end, std::int64_t thickness = 120)
		{
			const cad::WallId id{cad::Uuid{0, nextId++}};
			project.walls.push_back(cad::Wall{id, start, end, thickness, 2400});
			return id;
		}

		// Walls from point to point, closed back to the first one.
		void loop(std::initializer_list<cad::Point2D> points, std::int64_t thickness = 120)
		{
			const std::vector<cad::Point2D> p(points);
			for (std::size_t i = 0; i < p.size(); ++i)
			{
				add(p[i], p[(i + 1) % p.size()], thickness);
			}
		}
	};

	constexpr double SquareArea = 3880.0 * 2880.0; // 4 x 3 m on the axes, 120 mm walls
}

TEST_CASE("four walls closing a 4 x 3 m rectangle make one room with the net area")
{
	Plan plan;
	plan.loop({{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}});
	const std::vector<Room> rooms = detectRooms(plan.project);
	REQUIRE(rooms.size() == 1);
	const Room& room = rooms[0];
	CHECK(room.areaMm2 == doctest::Approx(SquareArea));
	CHECK(room.outline.size() == 4);
	CHECK(room.walls.size() == 4);
	CHECK(containsPoint(room.outline, room.labelPoint));
	CHECK(room.labelPoint.x_mm == doctest::Approx(2000.0));
	CHECK(room.labelPoint.y_mm == doctest::Approx(1500.0));
	CHECK(room.key == "00000000-0000-0000-0000-000000000001/room-left-0");
}

TEST_CASE("the drawing direction does not matter")
{
	Plan plan;
	plan.loop({{0, 0}, {0, 3000}, {4000, 3000}, {4000, 0}});
	const std::vector<Room> rooms = detectRooms(plan.project);
	REQUIRE(rooms.size() == 1);
	CHECK(rooms[0].areaMm2 == doctest::Approx(SquareArea));
	CHECK(rooms[0].key == "00000000-0000-0000-0000-000000000001/room-right-0"); // on the right of the first wall
}

TEST_CASE("an open outline or a gap encloses nothing")
{
	Plan open;
	open.add({0, 0}, {4000, 0});
	open.add({4000, 0}, {4000, 3000});
	open.add({4000, 3000}, {0, 3000});
	CHECK(detectRooms(open.project).empty());

	Plan gap;
	gap.loop({{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}});
	gap.project.walls[3].end = {0, 200}; // stops 200 mm short
	CHECK(detectRooms(gap.project).empty());
	CHECK(detectRooms(cad::Project{}).empty());
}

TEST_CASE("a wall between two walls (T at both ends) splits the room in two")
{
	for (const std::int64_t endY : {std::int64_t{0}, std::int64_t{60}, std::int64_t{35}})
	{
		CAPTURE(endY); // on the axis, on the inner face, or anywhere inside the wall
		Plan plan;
		plan.loop({{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}});
		plan.add({2000, 3000 - endY}, {2000, endY});
		const std::vector<Room> rooms = detectRooms(plan.project);
		REQUIRE(rooms.size() == 2);
		for (const Room& room : rooms)
		{
			CHECK(room.areaMm2 == doctest::Approx(1880.0 * 2880.0));
			CHECK(room.walls.size() == 4);
		}
		CHECK(rooms[0].key != rooms[1].key);
	}
}

TEST_CASE("crossing walls split the room where they cross")
{
	Plan plan;
	plan.loop({{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}});
	plan.add({2000, 0}, {2000, 3000});
	plan.add({0, 1500}, {4000, 1500});
	const std::vector<Room> rooms = detectRooms(plan.project);
	REQUIRE(rooms.size() == 4);
	for (const Room& room : rooms)
	{
		CHECK(room.areaMm2 == doctest::Approx(1880.0 * 1380.0));
	}
}

TEST_CASE("a wall sticking into a room is cut out of its floor")
{
	Plan plan;
	plan.loop({{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}});
	plan.add({2000, 3000}, {2000, 1500});
	const std::vector<Room> rooms = detectRooms(plan.project);
	REQUIRE(rooms.size() == 1);
	CHECK(rooms[0].areaMm2 == doctest::Approx(SquareArea - 120.0 * (2940.0 - 1500.0)));
	CHECK(rooms[0].walls.size() == 5);
}

TEST_CASE("each wall insets the floor by its own half-thickness")
{
	Plan plan;
	plan.add({0, 0}, {4000, 0});
	plan.add({4000, 0}, {4000, 3000});
	plan.add({4000, 3000}, {0, 3000});
	plan.add({0, 3000}, {0, 0}, 200);
	const std::vector<Room> rooms = detectRooms(plan.project);
	REQUIRE(rooms.size() == 1);
	CHECK(rooms[0].areaMm2 == doctest::Approx((3940.0 - 100.0) * 2880.0));
}

TEST_CASE("a U-shaped room gets its label inside, not in the notch")
{
	Plan plan;
	plan.loop({{0, 0}, {6000, 0}, {6000, 4000}, {4000, 4000}, {4000, 1500}, {2000, 1500}, {2000, 4000}, {0, 4000}});
	const std::vector<Room> rooms = detectRooms(plan.project);
	REQUIRE(rooms.size() == 1);
	CHECK(containsPoint(rooms[0].outline, rooms[0].labelPoint));
	CHECK(rooms[0].outline.size() == 8);
}

TEST_CASE("room keys are deterministic and survive moving a wall")
{
	Plan plan;
	plan.loop({{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}});
	const std::string key = detectRooms(plan.project)[0].key;
	CHECK(detectRooms(plan.project)[0].key == key);
	plan.project.walls[1].start = {4500, 0};
	plan.project.walls[0].end = {4500, 0};
	plan.project.walls[1].end = {4500, 3000};
	plan.project.walls[2].start = {4500, 3000};
	const std::vector<Room> moved = detectRooms(plan.project);
	REQUIRE(moved.size() == 1);
	CHECK(moved[0].key == key);
	CHECK(moved[0].areaMm2 == doctest::Approx(4380.0 * 2880.0));
}

TEST_CASE("floor slabs are 10 mm prisms over the room outline")
{
	Plan plan;
	plan.loop({{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}});
	const MeshData floor = buildFloorMesh(detectRooms(plan.project)[0]);
	const Bounds3 b = computeBounds(floor);
	CHECK(b.min.x == doctest::Approx(60.0));
	CHECK(b.max.x == doctest::Approx(3940.0));
	CHECK(b.min.z == 0.0);
	CHECK(b.max.z == FloorThicknessMm);
	CHECK(floor.sections[0].slot == std::string(floor_slots::Top));
}

TEST_CASE("overlapping collinear wall ends close adjacent rooms without changing the model")
{
	for (const bool reversed : {false, true})
	{
		Plan plan;
		plan.add({0, 0}, {4300, 0});
		plan.add({4300, 0}, {4300, 2550});
		plan.add(reversed ? cad::Point2D{4300, 2590} : cad::Point2D{4300, 2490},
			reversed ? cad::Point2D{4300, 2490} : cad::Point2D{4300, 2590});
		plan.add({4300, 2590}, {0, 2590});
		plan.add({0, 2590}, {0, 0});
		plan.add({2384, 0}, {2384, 2590});
		const auto rooms = detectRooms(plan.project);
		REQUIRE(rooms.size() == 2);
		CHECK(rooms[0].areaMm2 == doctest::Approx(2264.0 * 2470.0));
		CHECK(rooms[1].areaMm2 == doctest::Approx(1796.0 * 2470.0));
		CHECK(makeRoomLocator()(plan.project, {3300, 1300}) == rooms[1].key);
		CHECK(plan.project.walls[1].end == cad::Point2D{4300, 2550});
		CHECK(plan.project.walls.size() == 6);
		std::reverse(plan.project.walls.begin(), plan.project.walls.end());
		const auto reordered = detectRooms(plan.project);
		REQUIRE(reordered.size() == 2);
		CHECK(reordered[0].key == rooms[0].key);
		CHECK(reordered[1].key == rooms[1].key);
		CHECK(reordered[1].areaMm2 == doctest::Approx(rooms[1].areaMm2));
	}
}

TEST_CASE("merging coincident room graph segments does not close real gaps")
{
	Plan plan;
	plan.loop({{0, 0}, {4000, 0}, {4000, 3000}, {0, 3000}});
	plan.project.walls[1].end = {4000, 2000};
	plan.add({4000, 2200}, {4000, 3000});
	CHECK(detectRooms(plan.project).empty());
}
