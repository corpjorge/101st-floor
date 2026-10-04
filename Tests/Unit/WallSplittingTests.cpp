#include "doctest.h"

#include "F101/Cad/Document.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Scene/SceneCompiler.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Geometry/PolygonGeometry.h"

using namespace f101;
using namespace f101::cad;

TEST_CASE("a T creates independent wall segments with their finishes and one undo step")
{
	Document document{makeSequentialUuidSource()};
	const WallId host = document.addWall({0, 0}, {10000, 0}, 120, 2800).value();
	REQUIRE(document.execute(SetWallFinishCommand{host, WallSide::Left, "brick-red"}).ok());
	REQUIRE(document.execute(SetWallFinishCommand{host, WallSide::Right, "paint-sage"}).ok());
	const auto before = document.project().walls;
	std::vector<ChangeSet> notices;
	document.subscribe([&notices](const ChangeSet& changes) { notices.push_back(changes); });
	const WallId branch = document.addWall({4000, 0}, {4000, 3000}, 120, 2800).value();
	REQUIRE(document.project().walls.size() == 3);
	CHECK(document.project().findWall(branch)->end == Point2D{4000, 3000});
	CHECK(document.project().findWall(host)->end == Point2D{4000, 0});
	const WallId tail = document.project().walls.back().id;
	CHECK(tail != host);
	CHECK(document.project().findWall(tail)->start == Point2D{4000, 0});
	CHECK(document.project().findWall(tail)->end == Point2D{10000, 0});
	CHECK(document.project().findWall(tail)->finishLeft == "brick-red");
	CHECK(document.project().findWall(tail)->finishRight == "paint-sage");
	CHECK(notices.size() == 1);
	CHECK(notices.back().addedWalls.size() == 2);
	CHECK(notices.back().modifiedWalls == std::vector<WallId>{host});
	const auto divided = document.project().walls;
	REQUIRE(document.undo().ok());
	CHECK(document.project().walls == before);
	REQUIRE(document.redo().ok());
	CHECK(document.project().walls == divided);
	REQUIRE(document.execute(SetWallFinishCommand{tail, WallSide::Left, "paint-sage"}).ok());
	CHECK(document.project().findWall(host)->finishLeft == "brick-red");
	REQUIRE(document.removeWall(tail).ok());
	CHECK(document.project().findWall(host)->end == Point2D{4000, 0});
	CHECK(document.project().findWall(branch) != nullptr);
}

TEST_CASE("crossings split both walls and multiple intersections split every span")
{
	Document document{makeSequentialUuidSource()};
	const WallId a = document.addWall({2000, -2000}, {2000, 2000}, 120, 2800).value();
	const WallId b = document.addWall({6000, -2000}, {6000, 2000}, 120, 2800).value();
	const WallId crossing = document.addWall({0, 0}, {10000, 0}, 120, 2800).value();
	REQUIRE(document.project().walls.size() == 7);
	CHECK(document.project().findWall(a)->end == Point2D{2000, 0});
	CHECK(document.project().findWall(b)->end == Point2D{6000, 0});
	CHECK(document.project().findWall(crossing)->end == Point2D{2000, 0});
	CHECK(validateProject(document.project()).ok());
	REQUIRE(document.undo().ok());
	CHECK(document.project().walls.size() == 2);
	REQUIRE(document.redo().ok());
	CHECK(document.project().walls.size() == 7);
}

TEST_CASE("face joins and endpoint edits also divide the host")
{
	Document document{makeSequentialUuidSource()};
	const WallId host = document.addWall({0, 0}, {10000, 0}, 120, 2800).value();
	const WallId branch = document.addWall({4000, 3000}, {4000, 1000}, 120, 2800).value();
	REQUIRE(document.setWallEnd(branch, {4000, 60}).ok());
	REQUIRE(document.project().walls.size() == 3);
	CHECK(document.project().findWall(branch)->end == Point2D{4000, 0});
	CHECK(document.project().findWall(host)->end == Point2D{4000, 0});
	REQUIRE(document.undo().ok());
	CHECK(document.project().walls.size() == 2);
	CHECK(document.project().findWall(branch)->end == Point2D{4000, 1000});
}

TEST_CASE("junctions on other levels, coincident ends and parallel walls do not divide walls")
{
	Document document{makeSequentialUuidSource()};
	const LevelId ground = document.project().levels.front().id;
	const LevelId upper = document.addLevel().value();
	REQUIRE(document.addWall({0, 0}, {10000, 0}, 120, 2800, ground).ok());
	REQUIRE(document.addWall({4000, -1000}, {4000, 1000}, 120, 2800, upper).ok());
	REQUIRE(document.addWall({10000, 0}, {10000, 3000}, 120, 2800, ground).ok());
	REQUIRE(document.addWall({1000, 0}, {2000, 0}, 120, 2800, ground).ok());
	CHECK(document.project().walls.size() == 4);
}

TEST_CASE("openings migrate to the correct segment with their identity and local offset")
{
	Document document{makeSequentialUuidSource()};
	const WallId host = document.addWall({0, 0}, {10000, 0}, 120, 2800).value();
	const OpeningId door = document.addOpening(host, Opening{{}, OpeningKind::Door, 500, 900, 2100}).value();
	const OpeningId window = document.addOpening(host, Opening{{}, OpeningKind::Window, 6000, 1200, 1000, 900}).value();
	const auto before = document.project().walls;
	REQUIRE(document.addWall({4000, 0}, {4000, 3000}, 120, 2800).ok());
	const Wall& tail = document.project().walls.back();
	CHECK(document.project().findWall(host)->findOpening(door) != nullptr);
	CHECK(document.project().findWall(host)->findOpening(window) == nullptr);
	REQUIRE(tail.findOpening(window) != nullptr);
	CHECK(tail.findOpening(window)->offsetMm == 2000);
	CHECK(tail.findOpening(window)->heightMm == 1000);
	REQUIRE(document.undo().ok());
	CHECK(document.project().walls == before);
}

TEST_CASE("a cut through an opening or sliding pocket rejects the entire edit without notification")
{
	for (const bool sliding : {false, true})
	{
		Document document{makeSequentialUuidSource()};
		const WallId host = document.addWall({0, 0}, {10000, 0}, 120, 2800).value();
		Opening door{{}, OpeningKind::Door, 3500, 900, 2100};
		if (sliding) { door.operation = OpeningOperation::Sliding; door.hinge = OpeningEnd::End; }
		REQUIRE(document.addOpening(host, door).ok());
		const auto before = document.project().walls;
		const std::string label(document.undoLabel());
		int notices = 0;
		document.subscribe([&notices](const ChangeSet&) { ++notices; });
		CHECK_FALSE(document.addWall({sliding ? 4800 : 4000, 0}, {sliding ? 4800 : 4000, 3000}, 120, 2800).ok());
		CHECK(document.project().walls == before);
		CHECK(document.undoLabel() == label);
		CHECK(notices == 0);
	}
}

TEST_CASE("diagonal crossings quantize one shared joint and do not split again on finish edits")
{
	Document document{makeSequentialUuidSource()};
	REQUIRE(document.addWall({0, 0}, {8001, 3007}, 120, 2800).ok());
	REQUIRE(document.addWall({3333, -2000}, {3333, 5000}, 120, 2800).ok());
	REQUIRE(document.project().walls.size() == 4);
	const Point2D joint{3333, 1253};
	int endsAtJoint = 0;
	for (const Wall& wall : document.project().walls)
	{
		endsAtJoint += wall.start == joint ? 1 : 0;
		endsAtJoint += wall.end == joint ? 1 : 0;
	}
	CHECK(endsAtJoint == 4);
	const WallId segment = document.project().walls.back().id;
	REQUIRE(document.execute(SetWallFinishCommand{segment, WallSide::Left, "brick-red"}).ok());
	CHECK(document.project().walls.size() == 4);
	const auto loaded = readProjectJson(writeProjectJson(document.project()));
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls == document.project().walls);
	const auto scene = scene::compileScene(loaded.value());
	for (const Wall& wall : loaded.value().walls) CHECK(scene.findNode(scene::wallBodyNodeId(wall.id)) != nullptr);
}

TEST_CASE("a face join preserves the world position of a branch opening")
{
	Document document{makeSequentialUuidSource()};
	const WallId branch = document.addWall({4000, 60}, {4000, 4000}, 120, 2800).value();
	const OpeningId window = document.addOpening(branch, Opening{{}, OpeningKind::Window, 1000, 900, 1000, 900}).value();
	REQUIRE(document.addWall({0, 0}, {10000, 0}, 120, 2800).ok());
	const Wall* joined = document.project().findWall(branch);
	REQUIRE(joined != nullptr);
	CHECK(joined->start == Point2D{4000, 0});
	CHECK(joined->findOpening(window)->offsetMm == 1060);
}

TEST_CASE("selecting painting and deleting one divided span leaves the other span intact")
{
	Document document{makeSequentialUuidSource()};
	const WallId first = document.addWall({0, 0}, {10000, 0}, 120, 2800).value();
	REQUIRE(document.addWall({4000, 0}, {4000, 3000}, 120, 2800).ok());
	const WallId tail = document.project().walls.back().id;
	EditController editor{document, EditSettings{}};
	const Ray3 tailRay{8000, 20, 10000, 0, 0, -1};
	REQUIRE(editor.pointerPressed(tailRay).ok());
	REQUIRE(editor.pointerReleased(tailRay).ok());
	CHECK(editor.selection() == tail);
	REQUIRE(editor.setWallFinish(WallSide::Left, "brick-red").ok());
	CHECK(document.project().findWall(first)->finishLeft.empty());
	CHECK(document.project().findWall(tail)->finishLeft == "brick-red");
	REQUIRE(editor.deleteSelection().ok());
	CHECK(document.project().findWall(tail) == nullptr);
	CHECK(document.project().findWall(first) != nullptr);
	REQUIRE(editor.undo().ok());
	CHECK(document.project().findWall(tail)->finishLeft == "brick-red");
}

TEST_CASE("room floors and ceilings inherit finishes even when split walls change the room keys")
{
	Document document{makeSequentialUuidSource()};
	document.setRoomFinishTransfer(geometry::makeRoomFinishTransfer());
	Project legacy = document.project();
	legacy.walls = {
		Wall{WallId{Uuid{0, 100}}, {0, 0}, {10000, 0}, 120, 2800},
		Wall{WallId{Uuid{0, 101}}, {10000, 0}, {10000, 4000}, 120, 2800},
		Wall{WallId{Uuid{0, 102}}, {10000, 4000}, {0, 4000}, 120, 2800},
		Wall{WallId{Uuid{0, 103}}, {0, 4000}, {0, 0}, 120, 2800}};
	for (Wall& wall : legacy.walls) wall.level = legacy.levels.front().id;
	const auto oldRooms = geometry::detectRooms(legacy);
	REQUIRE(oldRooms.size() == 1);
	legacy.roomFinishes.push_back({oldRooms.front().key, "tile-grey", "plaster-white"});
	REQUIRE(document.replaceProject(legacy).ok());
	REQUIRE(document.addWall({4000, 0}, {4000, 4000}, 120, 2800).ok());
	const auto rooms = geometry::detectRooms(document.project());
	REQUIRE(rooms.size() == 2);
	for (const auto& room : rooms)
	{
		CHECK(room.key != oldRooms.front().key);
		const RoomFinish* finish = document.project().findRoomFinish(room.key);
		REQUIRE(finish != nullptr);
		CHECK(finish->floor == "tile-grey");
		CHECK(finish->ceiling == "plaster-white");
	}
	const auto divided = document.project().roomFinishes;
	REQUIRE(document.undo().ok());
	CHECK(document.project().walls == legacy.walls);
	CHECK(document.project().roomFinishes == legacy.roomFinishes);
	REQUIRE(document.redo().ok());
	CHECK(document.project().roomFinishes == divided);
}
