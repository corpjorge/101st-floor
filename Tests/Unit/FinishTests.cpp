#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Scene/FinishTextures.h"
#include "F101/Scene/SceneCompiler.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::cad;

namespace
{
	Ray3 down(double x, double y)
	{
		return Ray3{x, y, 20000.0, 0.0, 0.0, -1.0};
	}

	// A 4 x 3 m room; the first wall runs east along y = 0, so its left face (north) looks into the room.
	struct Room4x3
	{
		Document document{makeSequentialUuidSource()};
		std::vector<WallId> walls = build();

		std::vector<WallId> build()
		{
			std::vector<WallId> ids;
			for (const auto& [start, end] : std::vector<std::pair<Point2D, Point2D>>{
					 {{0, 0}, {4000, 0}}, {{4000, 0}, {4000, 3000}}, {{4000, 3000}, {0, 3000}}, {{0, 3000}, {0, 0}}})
			{
				ids.push_back(document.addWall(start, end, 120, 2800).value());
			}
			return ids;
		}

		std::string roomKey() const
		{
			const std::vector<geometry::Room> rooms = geometry::detectRooms(document.project());
			REQUIRE(rooms.size() == 1);
			return rooms[0].key;
		}
	};

	std::vector<std::string> finishesOf(const scene::Scene& scene, const std::string& nodeId)
	{
		const scene::SceneNode* node = scene.findNode(scene::SceneNodeId{nodeId});
		REQUIRE(node != nullptr);
		return node->finishes;
	}

	std::string finishOfSlot(const scene::Scene& scene, const std::string& nodeId, const std::string& slot)
	{
		const scene::SceneNode* node = scene.findNode(scene::SceneNodeId{nodeId});
		REQUIRE(node != nullptr);
		for (std::size_t i = 0; i < node->mesh.sections.size(); ++i)
		{
			if (node->mesh.sections[i].slot == slot)
			{
				return node->finishes[i];
			}
		}
		FAIL("no section " << slot);
		return {};
	}
}

TEST_CASE("the finish catalogue: unique ids, the defaults exist and allow their surfaces")
{
	std::set<std::string_view> ids;
	for (const FinishInfo& finish : finishCatalogue())
	{
		CHECK(ids.insert(finish.id).second);
		CHECK(finish.surfaces != 0);
		CHECK(finish.roughness > 0.0);
		CHECK(finish.roughness <= 1.0);
		if (finish.pattern != FinishPattern::Plain && finish.pattern != FinishPattern::Speckle)
		{
			CHECK(finish.unitWidthMm > 0);
			CHECK(finish.unitHeightMm > 0);
		}
	}
	CHECK(findFinish(DefaultWallFinish)->allows(Surface::Wall));
	CHECK(findFinish(DefaultFloorFinish)->allows(Surface::Floor));
	CHECK(findFinish(DefaultCeilingFinish)->allows(Surface::Ceiling));
	CHECK(resolveFinish("", Surface::Floor) == DefaultFloorFinish);
	CHECK(resolveFinish("brick-red", Surface::Wall) == "brick-red");
	CHECK(validateFinish("", Surface::Wall).ok());
	CHECK(validateFinish("nope", Surface::Wall).error().code == ErrorCode::NotFound);
	CHECK(validateFinish("wood-oak", Surface::Wall).error().code == ErrorCode::InvalidArgument); // a floor finish
	CHECK(validateFinish("wood-oak", Surface::Wall).error().message == "The finish 'wood-oak' is not for walls");
}

TEST_CASE("the catalogue offers many paints, floors and furniture finishes, grouped by category (MVP 11)")
{
	std::map<FinishCategory, int> byCategory;
	int floors = 0;
	for (const FinishInfo& finish : finishCatalogue())
	{
		++byCategory[finish.category];
		floors += finish.allows(Surface::Floor) ? 1 : 0;
		if (finish.category == FinishCategory::Paint)
		{
			CHECK(finish.allows(Surface::Wall));
			CHECK(finish.allows(Surface::Ceiling));
			CHECK_FALSE(finish.allows(Surface::Floor));
		}
		if (finish.pattern == FinishPattern::Checker)
		{
			CHECK_FALSE(finish.accentColor == finish.color);
		}
		if (finish.pattern == FinishPattern::Herringbone)
		{
			CHECK(finish.unitWidthMm >= 2 * finish.unitHeightMm);
		}
		CHECK((finish.opacity < 1.0) <= (finish.category == FinishCategory::Glass));
	}
	CHECK(byCategory[FinishCategory::Paint] >= 20);
	CHECK(byCategory[FinishCategory::Tiles] >= 8);
	CHECK(byCategory[FinishCategory::Wood] >= 10);
	CHECK(byCategory[FinishCategory::Stone] >= 6);
	CHECK(byCategory[FinishCategory::Textile] >= 3);
	CHECK(byCategory[FinishCategory::Fabric] >= 6);
	CHECK(floors >= 30);
	// Ids from earlier projects keep their surfaces.
	CHECK(findFinish("tile-grey")->allows(Surface::Floor));
	CHECK_FALSE(findFinish("tile-grey")->allows(Surface::Wall));
	CHECK(findFinish("wood-oak")->allows(Surface::Carpentry));
	CHECK(findFinish("glass-clear")->opacity < 1.0);
}

TEST_CASE("herringbone and checker textures: planks both ways, stepping; colours alternating")
{
	// Thick black joints make the planks measurable in pixels.
	FinishInfo parquet = *findFinish("parquet-herringbone-oak");
	parquet.jointMm = 20;
	parquet.jointColor = Rgb8{0, 0, 0};
	const scene::FinishTexture herringbone = scene::buildFinishTexture(parquet, 256);
	CHECK(herringbone.repeatWidthMm == doctest::Approx(1200.0)); // 2 x 6 widths of 100 mm
	CHECK(herringbone.repeatHeightMm == doctest::Approx(1200.0));
	const scene::TextureImage& image = herringbone.mips.front();
	auto joint = [&image](int x, int y) { return image.rgba[(static_cast<std::size_t>(y) * image.width + x) * 4] < 60; };
	int longestAcross = 0;
	int longestDown = 0;
	int joints = 0;
	for (int line = 0; line < image.width; ++line)
	{
		int across = 0;
		int down = 0;
		for (int step = 0; step < image.width; ++step)
		{
			across = joint(step, line) ? 0 : across + 1;
			down = joint(line, step) ? 0 : down + 1;
			longestAcross = std::max(longestAcross, across);
			longestDown = std::max(longestDown, down);
			joints += joint(step, line) ? 1 : 0;
		}
	}
	// A 600 mm plank is 128 px: runs of about that length both ways, never a whole repeat.
	CHECK(longestAcross >= 110);
	CHECK(longestAcross < 140);
	CHECK(longestDown >= 110);
	CHECK(longestDown < 140);
	CHECK(joints > image.width * image.height / 10);
	CHECK(joints < image.width * image.height / 3);

	const FinishInfo& checker = *findFinish("tile-checker-bw");
	const scene::FinishTexture board = scene::buildFinishTexture(checker, 256);
	const scene::TextureImage& squares = board.mips.front();
	auto red = [&squares](int x, int y) { return squares.rgba[(static_cast<std::size_t>(y) * squares.width + x) * 4]; };
	// Four units per repeat: 64 px each; unit centres at 32 and 96.
	CHECK(red(32, 32) > 200);
	CHECK(red(96, 32) < 70);
	CHECK(red(96, 96) > 200);
	CHECK(red(32, 96) < 70);
}

TEST_CASE_FIXTURE(Room4x3, "wall faces and room surfaces take finishes as undoable commands")
{
	REQUIRE(document.execute(SetWallFinishCommand{walls[0], WallSide::Left, "brick-red"}).ok());
	CHECK(document.undoLabel() == "Change finish");
	CHECK(document.project().findWall(walls[0])->finishLeft == "brick-red");
	CHECK(document.project().findWall(walls[0])->finishRight.empty());
	CHECK(document.execute(SetWallFinishCommand{walls[0], WallSide::Right, "tile-grey"}).error().code == ErrorCode::InvalidArgument);
	CHECK(document.execute(SetWallFinishCommand{walls[0], WallSide::Right, "nope"}).error().code == ErrorCode::NotFound);

	const std::string room = roomKey();
	REQUIRE(document.execute(SetRoomFinishCommand{room, "wood-oak", "paint-sand"}).ok());
	CHECK(document.undoLabel() == "Change room finish");
	REQUIRE(document.project().findRoomFinish(room) != nullptr);
	CHECK(document.project().findRoomFinish(room)->floor == "wood-oak");
	CHECK(document.execute(SetRoomFinishCommand{room, "paint-sage", ""}).error().code == ErrorCode::InvalidArgument); // wall paint
	CHECK(document.execute(SetRoomFinishCommand{room, "wood-oak", "paint-sand"}).value().empty()); // unchanged: no step
	// Both empty: the room goes back to the defaults and has no entry.
	REQUIRE(document.execute(SetRoomFinishCommand{room, "", ""}).ok());
	CHECK(document.project().roomFinishes.empty());
	REQUIRE(document.undo().ok());
	CHECK(document.project().findRoomFinish(room)->ceiling == "paint-sand");
	REQUIRE(document.undo().ok());
	CHECK(document.project().roomFinishes.empty());
	REQUIRE(document.undo().ok());
	CHECK(document.project().findWall(walls[0])->finishLeft.empty());
}

TEST_CASE_FIXTURE(Room4x3, "finishes go to the right sections of the scene; the selected room's floor is Selected")
{
	REQUIRE(document.execute(SetWallFinishCommand{walls[0], WallSide::Left, "brick-red"}).ok());
	const std::string room = roomKey();
	REQUIRE(document.execute(SetRoomFinishCommand{room, "wood-oak", "concrete"}).ok());

	const scene::Scene scene = scene::compileScene(document.project());
	const std::string wall = toString(walls[0]) + "/body";
	CHECK(finishOfSlot(scene, wall, "left") == "brick-red");
	CHECK(finishOfSlot(scene, wall, "right") == "plaster-white");
	CHECK(finishOfSlot(scene, wall, "top") == "plaster-white");
	for (const std::string& finish : finishesOf(scene, room + "/floor"))
	{
		CHECK(finish == "wood-oak");
	}
	CHECK(finishOfSlot(scene, room + "/slab", "ceiling") == "concrete");
	CHECK(finishOfSlot(scene, room + "/slab", "slab_top") == "plaster-white");

	// A finish change changes the node's hash, so consumers update it, while the mesh stays the same.
	const scene::SceneNode before = *scene.findNode(scene::SceneNodeId{wall});
	REQUIRE(document.execute(SetWallFinishCommand{walls[0], WallSide::Left, "paint-sage"}).ok());
	const scene::SceneNode after = *scene::compileScene(document.project()).findNode(scene::SceneNodeId{wall});
	CHECK(scene::hashMesh(after.mesh) == scene::hashMesh(before.mesh));
	CHECK(after.contentHash != before.contentHash);

	scene::ViewOverlay overlay;
	overlay.selectedRoom = room;
	CHECK(scene::compileScene(document.project(), overlay).findNode(scene::SceneNodeId{room + "/floor"})->style ==
		scene::NodeStyle::Selected);
}

TEST_CASE_FIXTURE(Room4x3, "the Paint tool paints the face it hits, or the floor of the room under the pointer")
{
	EditController editor{document, EditSettings{}};
	editor.setRoomLocator(geometry::makeRoomLocator());
	editor.setMode(EditMode::Paint);
	CHECK(editor.setPaintFinish("nope").error().code == ErrorCode::NotFound);
	REQUIRE(editor.setPaintFinish("brick-red").ok());

	// From above, the north half of the south wall (y = 0..60) is its left face; the south half its right face.
	REQUIRE(editor.pointerPressed(down(2000.0, 30.0)).ok());
	CHECK(document.project().findWall(walls[0])->finishLeft == "brick-red");
	REQUIRE(editor.pointerPressed(down(2000.0, -30.0)).ok());
	CHECK(document.project().findWall(walls[0])->finishRight == "brick-red");

	// Inside the room: its floor. A wall finish is refused there, a floor finish applies (keeping the ceiling).
	CHECK(editor.pointerPressed(down(2000.0, 1500.0)).error().code == ErrorCode::InvalidArgument);
	REQUIRE(editor.setPaintFinish("tile-grey").ok());
	REQUIRE(document.execute(SetRoomFinishCommand{roomKey(), "", "concrete"}).ok());
	REQUIRE(editor.pointerPressed(down(2000.0, 1500.0)).ok());
	CHECK(document.project().findRoomFinish(roomKey())->floor == "tile-grey");
	CHECK(document.project().findRoomFinish(roomKey())->ceiling == "concrete");
	// Outside every room and wall: nothing to paint.
	CHECK(editor.pointerPressed(down(9000.0, 9000.0)).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE_FIXTURE(Room4x3, "Paint tells what a click would paint; a floor finish looks past the walls, down to the floor")
{
	EditController editor{document, EditSettings{}};
	editor.setRoomLocator(geometry::makeRoomLocator());
	REQUIRE(editor.setPaintFinish("paint-sage").ok());
	editor.pointerMoved(down(2000.0, 30.0));
	CHECK_FALSE(editor.paintTarget().has_value()); // not in Paint mode
	editor.setMode(EditMode::Paint);
	REQUIRE(editor.paintTarget().has_value());
	CHECK(editor.paintTarget()->wall == walls[0]);
	CHECK(editor.paintTarget()->side == WallSide::Left);
	editor.pointerMoved(down(2000.0, 1500.0));
	CHECK_FALSE(editor.paintTarget().has_value()); // a wall paint over the floor: nothing
	CHECK(editor.pointerPressed(down(2000.0, 1500.0)).error().message == "Point at a wall: this finish is for walls");

	// Laminate: only floors. Over the floor, the room; seen from outside, past the wall in front, the floor behind it.
	REQUIRE(editor.setPaintFinish("laminate-oak-light").ok());
	REQUIRE(editor.paintTarget().has_value()); // the target follows the finish at once
	CHECK(editor.paintTarget()->room == roomKey());
	const Ray3 slanted{2000.0, -1500.0, 2000.0, 0.0, 1.0, -0.6}; // meets the south wall's outer face, then the room's floor
	REQUIRE(editor.setPaintFinish("paint-sage").ok());
	editor.pointerMoved(slanted);
	REQUIRE(editor.paintTarget().has_value());
	CHECK(editor.paintTarget()->wall == walls[0]); // a wall paint: that face
	CHECK(editor.paintTarget()->side == WallSide::Right);
	REQUIRE(editor.setPaintFinish("laminate-oak-light").ok());
	REQUIRE(editor.paintTarget().has_value());
	CHECK(editor.paintTarget()->wall.isNil());
	CHECK(editor.paintTarget()->room == roomKey());
	REQUIRE(editor.pointerPressed(slanted).ok());
	CHECK(document.project().findRoomFinish(roomKey())->floor == "laminate-oak-light");
	CHECK(document.project().findWall(walls[0])->finishLeft.empty());

	// No room there (the walls do not close it): nothing, and why.
	editor.pointerMoved(down(9000.0, 9000.0));
	CHECK_FALSE(editor.paintTarget().has_value());
	CHECK(editor.pointerPressed(down(9000.0, 9000.0)).error().message == "Point at the floor of a room: walls must close it all round");
	editor.setMode(EditMode::Select);
	CHECK_FALSE(editor.paintTarget().has_value());
}

TEST_CASE_FIXTURE(Room4x3, "a click on a room floor selects the room; its finishes are set from the selection")
{
	EditController editor{document, EditSettings{}};
	editor.setRoomLocator(geometry::makeRoomLocator());
	REQUIRE(editor.pointerPressed(down(2000.0, 1500.0)).ok());
	CHECK(editor.selectedRoom() == roomKey());
	CHECK(editor.selection().isNil());
	CHECK(editor.view().selectedRoom == roomKey());
	REQUIRE(editor.setSelectedRoomFloor("wood-walnut").ok());
	REQUIRE(editor.setSelectedRoomCeiling("wood-oak").ok());
	CHECK(document.project().findRoomFinish(roomKey())->floor == "wood-walnut");
	CHECK(document.project().findRoomFinish(roomKey())->ceiling == "wood-oak");
	CHECK(editor.setSelectedRoomCeiling("tile-grey").error().code == ErrorCode::InvalidArgument); // not for ceilings

	// Selecting a wall drops the room; the wall's faces are set from its selection.
	REQUIRE(editor.pointerPressed(down(2000.0, 30.0)).ok());
	CHECK(editor.selectedRoom().empty());
	REQUIRE(editor.setWallFinish(WallSide::Right, "concrete").ok());
	CHECK(document.project().findWall(walls[0])->finishRight == "concrete");
	REQUIRE(editor.setWallFinish(WallSide::Right, "").ok()); // back to the default
	CHECK(document.project().findWall(walls[0])->finishRight.empty());

	// Without a room locator, a click on the floor only clears the selection.
	EditController blind{document, EditSettings{}};
	REQUIRE(blind.pointerPressed(down(2000.0, 1500.0)).ok());
	CHECK(blind.selectedRoom().empty());
}

TEST_CASE_FIXTURE(Room4x3, "schema 4 writes finishes only where set and reads them back; invalid finishes are refused")
{
	REQUIRE(document.execute(SetWallFinishCommand{walls[1], WallSide::Right, "brick-red"}).ok());
	REQUIRE(document.execute(SetRoomFinishCommand{roomKey(), "wood-oak", ""}).ok());
	const std::string json = writeProjectJson(document.project());
	CHECK(json.find("\"finishes\": {\n        \"right\": \"brick-red\"\n      }") != std::string::npos);
	CHECK(json.find("\"left\"") == std::string::npos);
	CHECK(json.find("\"ceiling\"") == std::string::npos);
	const Result<Project> loaded = readProjectJson(json);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls == document.project().walls);
	CHECK(loaded.value().roomFinishes == document.project().roomFinishes);

	std::string wrongSurface = json;
	wrongSurface.replace(wrongSurface.find("\"wood-oak\""), 10, "\"brick-red\"");
	CHECK(readProjectJson(wrongSurface).error().code == ErrorCode::InvalidArgument);
	std::string unknown = json;
	unknown.replace(unknown.find("\"brick-red\""), 11, "\"gold-leaf\"");
	CHECK(readProjectJson(unknown).error().code == ErrorCode::NotFound);
}

TEST_CASE("finish textures: tileable repeats sized from the units, full mip chains, deterministic")
{
	for (const FinishInfo& finish : finishCatalogue())
	{
		CAPTURE(finish.id);
		const scene::FinishTexture texture = scene::buildFinishTexture(finish, 64);
		REQUIRE_FALSE(texture.mips.empty());
		const scene::TextureImage& top = texture.mips.front();
		CHECK(top.rgba.size() == static_cast<std::size_t>(top.width) * top.height * 4);
		CHECK(texture.mips.back().width == 1);
		CHECK(texture.mips.back().height == 1);
		for (std::size_t level = 1; level < texture.mips.size(); ++level)
		{
			CHECK(texture.mips[level].width == std::max(1, texture.mips[level - 1].width / 2));
		}
		CHECK(scene::buildFinishTexture(finish, 64).mips.front().rgba == top.rgba);
	}

	const scene::FinishTexture bricks = scene::buildFinishTexture(*findFinish("brick-red"), 64);
	CHECK(bricks.repeatWidthMm == 4 * 250.0); // four 240 mm bricks with 10 mm joints
	CHECK(bricks.repeatHeightMm == 8 * 65.0);  // eight courses
	CHECK(bricks.mips.size() == 7);           // 64, 32, ..., 1
	// Mortar and brick are both there.
	std::set<std::uint8_t> reds;
	for (std::size_t i = 0; i < bricks.mips[0].rgba.size(); i += 4)
	{
		reds.insert(bricks.mips[0].rgba[i]);
	}
	CHECK(*reds.rbegin() - *reds.begin() > 40);

	const scene::FinishTexture plain = scene::buildFinishTexture(*findFinish("plaster-white"));
	CHECK(plain.mips.front().width == 4); // one colour: no need for 512 pixels
	CHECK(plain.mips.front().rgba[0] == 238);

	// What renderers use: the roughness and texture of an id; an unknown id shows flat magenta, never passes unnoticed.
	CHECK(scene::buildFinishLook("brick-red", 64).roughness == 0.9);
	CHECK(scene::buildFinishLook("brick-red", 64).texture.repeatWidthMm == 1000.0);
	const scene::FinishLook missing = scene::buildFinishLook("gold-leaf", 64);
	CHECK(missing.texture.mips.front().rgba[0] == 255);
	CHECK(missing.texture.mips.front().rgba[1] == 0);
	CHECK(missing.texture.mips.front().rgba[2] == 255);
}
