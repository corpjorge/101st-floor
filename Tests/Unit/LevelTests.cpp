#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Geometry/MeshData.h"
#include "F101/Scene/SceneCompiler.h"

#include <string>
#include <vector>

using namespace f101;
using namespace f101::cad;

namespace
{
	// Rays in scene space: looking straight down at a plan point, like a top view.
	Ray3 down(double x, double y)
	{
		return Ray3{x, y, 20000.0, 0.0, 0.0, -1.0};
	}

	// A 4 x 3 m room on the ground floor and the same room stacked on a second level.
	struct TwoStoreys
	{
		Document document{makeSequentialUuidSource()};
		LevelId ground = document.project().levels.front().id;
		LevelId upper = document.addLevel(3000, 300).value();
		std::vector<WallId> groundWalls = room(ground, 2800);
		std::vector<WallId> upperWalls = room(upper, 3000);

		std::vector<WallId> room(const LevelId& level, std::int64_t height)
		{
			std::vector<WallId> ids;
			for (const auto& [start, end] : std::vector<std::pair<Point2D, Point2D>>{
					 {{0, 0}, {4000, 0}}, {{4000, 0}, {4000, 3000}}, {{4000, 3000}, {0, 3000}}, {{0, 3000}, {0, 0}}})
			{
				ids.push_back(document.addWall(start, end, 120, height, level).value());
			}
			return ids;
		}
	};

	double minZ(const scene::SceneNode& node)
	{
		return geometry::computeBounds(node.mesh).min.z;
	}

	double maxZ(const scene::SceneNode& node)
	{
		return geometry::computeBounds(node.mesh).max.z;
	}
}

TEST_CASE("a new document has one ground level; walls go on it by default")
{
	Document document{makeSequentialUuidSource()};
	REQUIRE(document.project().levels.size() == 1);
	const Level& ground = document.project().levels.front();
	CHECK(ground.heightMm == DefaultLevelHeightMm);
	CHECK(ground.slabThicknessMm == DefaultSlabThicknessMm);
	CHECK(ground.wallHeightMm() == 2800);
	CHECK(ground.clearHeightMm() == 2550);
	CHECK(ground.name.empty());
	const WallId wall = document.addWall({0, 0}, {1000, 0}, 120, 2400).value();
	CHECK(document.project().findWall(wall)->level == ground.id);
	CHECK(document.addWall({0, 0}, {1000, 0}, 120, 2400, LevelId{Uuid{9, 9}}).error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(TwoStoreys, "levels stack: elevations are derived from the heights below")
{
	CHECK(levelElevationMm(document.project(), ground) == 0);
	CHECK(levelElevationMm(document.project(), upper) == 2800);
	CHECK(document.project().levelIndex(upper) == 1u);

	// A taller ground floor lifts the level above it. Its full-height walls follow it; a lower wall keeps its height.
	const WallId low = document.addWall({0, 5000}, {2000, 5000}, 120, 1000).value();
	Level taller = document.project().levels[0];
	taller.heightMm = 3100;
	const Transaction changed = document.execute(ChangeLevelCommand{taller}).value();
	CHECK(changed.levels.size() == 1);
	CHECK(changed.walls.size() == 4); // one undoable step with the level and its four full-height walls
	CHECK(document.undoLabel() == "Change level");
	CHECK(levelElevationMm(document.project(), upper) == 3100);
	CHECK(document.project().findWall(groundWalls[0])->heightMm == 3100);
	CHECK(document.project().findWall(low)->heightMm == 1000);
	CHECK(document.project().findWall(upperWalls[0])->heightMm == 3000); // other levels untouched
	REQUIRE(document.undo().ok());
	CHECK(levelElevationMm(document.project(), upper) == 2800);
	CHECK(document.project().findWall(groundWalls[0])->heightMm == 2800);

	// A lower level that would leave a window above the top of a following wall is refused, and changes nothing.
	REQUIRE(document.addOpening(groundWalls[0], Opening{OpeningId{}, OpeningKind::Window, 500, 1200, 1200, 1400}).ok());
	Level lower = document.project().levels[0];
	lower.heightMm = 2600;
	CHECK(document.execute(ChangeLevelCommand{lower}).error().code == ErrorCode::OutOfRange);
	CHECK(document.project().levels[0].heightMm == 2800);

	// Unchanged: not recorded. Invalid: refused.
	CHECK(document.execute(ChangeLevelCommand{document.project().levels[0]}).value().empty());
	Level thickSlab = document.project().levels[0];
	thickSlab.slabThicknessMm = 2800;
	CHECK(document.execute(ChangeLevelCommand{thickSlab}).error().code == ErrorCode::OutOfRange);
	Level missing = thickSlab;
	missing.id = LevelId{Uuid{9, 9}};
	CHECK(document.execute(ChangeLevelCommand{missing}).error().code == ErrorCode::NotFound);
	CHECK(document.execute(AddLevelCommand{MaxAbsMillimeters, 250, {}}).error().code == ErrorCode::OutOfRange);
}

TEST_CASE_FIXTURE(TwoStoreys, "deleting a level removes its walls in one step; undo brings back both")
{
	REQUIRE(document.execute(DeleteLevelCommand{ground}).ok());
	CHECK(document.undoLabel() == "Delete level");
	REQUIRE(document.project().levels.size() == 1);
	CHECK(document.project().levels[0].id == upper);
	CHECK(levelElevationMm(document.project(), upper) == 0); // the level above moves down
	CHECK(document.project().walls.size() == 4);
	for (const WallId& id : groundWalls)
	{
		CHECK(document.project().findWall(id) == nullptr);
	}
	CHECK(document.execute(DeleteLevelCommand{upper}).error().code == ErrorCode::InvalidArgument); // the last one

	REQUIRE(document.undo().ok());
	CHECK(document.project().levels[0].id == ground);
	CHECK(document.project().levels[1].id == upper);
	REQUIRE(document.project().walls.size() == 8);
	CHECK(document.project().walls[0].id == groundWalls[0]); // original order
	CHECK(validateProject(document.project()).ok());
	REQUIRE(document.redo().ok());
	CHECK(document.project().walls.size() == 4);
}

TEST_CASE_FIXTURE(TwoStoreys, "a corner move only moves the walls of its level, not the ones stacked on it")
{
	REQUIRE(document.execute(MoveJointCommand{{4000, 0}, {4500, 0}, upper}).ok());
	CHECK(document.project().findWall(upperWalls[0])->end == Point2D{4500, 0});
	CHECK(document.project().findWall(upperWalls[1])->start == Point2D{4500, 0});
	CHECK(document.project().findWall(groundWalls[0])->end == Point2D{4000, 0});
	// A nil level is the first one.
	REQUIRE(document.execute(MoveJointCommand{{0, 0}, {0, -500}, {}}).ok());
	CHECK(document.project().findWall(groundWalls[0])->start == Point2D{0, -500});
	CHECK(document.project().findWall(upperWalls[0])->start == Point2D{0, 0});
	CHECK(document.execute(MoveJointCommand{{0, 0}, {1, 1}, LevelId{Uuid{9, 9}}}).error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(TwoStoreys, "the scene lifts each level, solves rooms per level and puts a slab over each room")
{
	const scene::Scene all = scene::compileScene(document.project());
	REQUIRE(all.rooms.size() == 2); // the same plan on both levels: two rooms, not one
	CHECK(all.rooms[0].level == ground);
	CHECK(all.rooms[1].level == upper);
	CHECK(all.rooms[1].elevationMm == 2800);
	CHECK(all.rooms[0].room.key != all.rooms[1].room.key);

	const scene::SceneNode& groundWall = *all.findNode(scene::wallBodyNodeId(groundWalls[0]));
	const scene::SceneNode& upperWall = *all.findNode(scene::wallBodyNodeId(upperWalls[0]));
	CHECK(minZ(groundWall) == 0.0);
	CHECK(maxZ(groundWall) == 2800.0);
	CHECK(minZ(upperWall) == 2800.0);
	CHECK(maxZ(upperWall) == 5800.0);
	CHECK(upperWall.level == upper);

	const scene::SceneNode& groundSlab = *all.findNode(scene::SceneNodeId{all.rooms[0].room.key + "/slab"});
	const scene::SceneNode& upperFloor = *all.findNode(scene::SceneNodeId{all.rooms[1].room.key + "/floor"});
	const scene::SceneNode& upperSlab = *all.findNode(scene::SceneNodeId{all.rooms[1].room.key + "/slab"});
	CHECK(minZ(groundSlab) == 2550.0); // the top band of the ground floor, between its walls
	CHECK(maxZ(groundSlab) == 2800.0);
	CHECK(minZ(upperFloor) == 2800.0); // the upper floor finish lies on it
	CHECK(minZ(upperSlab) == 5500.0);  // 300 mm roof slab of the top level
	CHECK(maxZ(upperSlab) == 5800.0);
	CHECK(groundSlab.sourceKind == scene::SourceKind::Slab);
	CHECK(groundSlab.style == scene::NodeStyle::Normal);
}

TEST_CASE_FIXTURE(TwoStoreys, "editing a level hides the levels above and its own slabs, and dims the levels below")
{
	scene::ViewOverlay overlay;
	overlay.activeLevel = ground;
	const scene::Scene onGround = scene::compileScene(document.project(), overlay);
	CHECK(onGround.findNode(scene::wallBodyNodeId(upperWalls[0])) == nullptr);
	for (const scene::SceneNode& node : onGround.nodes)
	{
		CHECK(node.sourceKind != scene::SourceKind::Slab); // the rooms being edited are open from above
		CHECK(node.style == scene::NodeStyle::Normal);
	}
	REQUIRE(onGround.rooms.size() == 1);

	overlay.activeLevel = upper;
	overlay.markers = {{0, 0}};
	const scene::Scene onUpper = scene::compileScene(document.project(), overlay);
	CHECK(onUpper.findNode(scene::wallBodyNodeId(groundWalls[0]))->style == scene::NodeStyle::Context);
	CHECK(onUpper.findNode(scene::wallBodyNodeId(upperWalls[0]))->style == scene::NodeStyle::Normal);
	CHECK(onUpper.findNode(scene::SceneNodeId{onUpper.rooms[0].room.key + "/slab"})->style == scene::NodeStyle::Context);
	CHECK(onUpper.findNode(scene::SceneNodeId{onUpper.rooms[1].room.key + "/slab"}) == nullptr);
	CHECK(minZ(*onUpper.findNode(scene::SceneNodeId{"marker/0"})) == 2800.0); // handles on the edited level

	overlay.showAllLevels = true;
	const scene::Scene everything = scene::compileScene(document.project(), overlay);
	CHECK(everything.nodes.size() == scene::compileScene(document.project()).nodes.size() + 1); // + the marker
	CHECK(everything.findNode(scene::wallBodyNodeId(groundWalls[0]))->style == scene::NodeStyle::Normal);
}

TEST_CASE_FIXTURE(TwoStoreys, "the editor works on the active level: pointer, picking, new walls and levels")
{
	EditController editor{document, EditSettings{}};
	CHECK(editor.activeLevel() == ground);
	CHECK(editor.view().activeLevel == ground);

	// The same ray picks the ground wall, then, on the upper level, the wall stacked on it.
	REQUIRE(editor.pointerPressed(down(2000.0, 20.0)).ok());
	CHECK(editor.selection() == groundWalls[0]);
	REQUIRE(editor.levelUp().ok());
	CHECK(editor.activeLevel() == upper);
	CHECK(editor.selection().isNil()); // changing level drops the selection
	CHECK(editor.levelUp().error().code == ErrorCode::OutOfRange);
	REQUIRE(editor.pointerPressed(down(2000.0, 20.0)).ok());
	CHECK(editor.selection() == upperWalls[0]);

	// A wall drawn on the upper level stands on it, as tall as the level.
	editor.setMode(EditMode::Wall);
	REQUIRE(editor.pointerPressed(down(1000.0, 1000.0)).ok());
	editor.pointerMoved(down(3000.0, 1000.0));
	REQUIRE(editor.view().previewWalls.size() == 1);
	CHECK(editor.view().previewWalls[0].level == upper);
	REQUIRE(editor.confirm().ok());
	const Wall& drawn = document.project().walls.back();
	CHECK(drawn.level == upper);
	CHECK(drawn.heightMm == 3000);
	CHECK(drawn.start == Point2D{1000, 1000});
	editor.setMode(EditMode::Select);

	// Selecting a wall of another level switches to it.
	REQUIRE(editor.select(groundWalls[1]).ok());
	CHECK(editor.activeLevel() == ground);
	CHECK(editor.levelDown().error().code == ErrorCode::OutOfRange);

	// A new level goes on top, like the one below it, and becomes active; deleting it returns to the one below.
	REQUIRE(editor.addLevel().ok());
	REQUIRE(document.project().levels.size() == 3);
	CHECK(editor.activeLevel() == document.project().levels[2].id);
	CHECK(document.project().levels[2].heightMm == 3000);
	CHECK(levelElevationMm(document.project(), editor.activeLevel()) == 5800);
	REQUIRE(editor.setLevelHeight("320").ok()); // cm
	CHECK(document.project().levels[2].heightMm == 3200);
	REQUIRE(editor.setLevelSlabThickness("35 cm").ok());
	CHECK(document.project().levels[2].slabThicknessMm == 350);
	REQUIRE(editor.setLevelName("Cubierta").ok());
	CHECK(document.project().levels[2].name == "Cubierta");
	CHECK(editor.setLevelHeight("abc").error().code == ErrorCode::ParseError);
	CHECK(editor.setLevelName(std::string(201, 'x')).error().code == ErrorCode::OutOfRange);
	REQUIRE(editor.deleteActiveLevel().ok());
	CHECK(editor.activeLevel() == upper);

	// Undoing the deletion restores the level; undoing its creation leaves the editor on an existing level.
	REQUIRE(editor.undo().ok());
	CHECK(document.project().levels.size() == 3);
	while (document.project().levels.size() == 3)
	{
		REQUIRE(editor.undo().ok());
	}
	CHECK(document.project().findLevel(editor.activeLevel()) != nullptr);
}

TEST_CASE("walls of different levels never join or form rooms together")
{
	Document document{makeSequentialUuidSource()};
	const LevelId upper = document.addLevel().value();
	// An L split across two levels: no mitre, no joint fill.
	REQUIRE(document.addWall({0, 0}, {4000, 0}, 120, 2800).ok());
	REQUIRE(document.addWall({4000, 0}, {4000, 3000}, 120, 2800, upper).ok());
	const scene::Scene scene = scene::compileScene(document.project());
	REQUIRE(scene.nodes.size() == 2);
	CHECK(geometry::computeBounds(scene.nodes[0].mesh).max.x == 4000.0); // square end: not mitred into the other
}

TEST_CASE("walls of the level below guide the cursor, to stack walls exactly")
{
	Document document{makeSequentialUuidSource()};
	REQUIRE(document.addWall({0, 0}, {3550, 0}, 120, 2800).ok());
	const LevelId upper = document.addLevel().value();
	const LevelId top = document.addLevel().value();

	WallTool tool{document, WallToolSettings{}};
	tool.setLevel(upper);
	tool.pointerMoved({3530.0, 40.0});
	CHECK(tool.hoverPoint() == Point2D{3550, 0}); // the end of the wall below
	CHECK(tool.snapKind() == SnapKind::Endpoint);
	REQUIRE(tool.click({3530.0, 40.0}).ok());
	tool.pointerMoved({3540.0, 2000.0});
	tool.appendText("300");
	const WallId stacked = tool.confirm().value();
	CHECK(document.project().findWall(stacked)->start == Point2D{3550, 0});
	CHECK(document.project().findWall(stacked)->level == upper);
	// Two levels up, the ground wall is no guide.
	tool.cancel();
	tool.setLevel(top);
	tool.pointerMoved({20.0, 40.0});
	CHECK(tool.hoverPoint() == Point2D{0, 50}); // the grid, not the ground wall's start
	CHECK(snapReference(document.project(), top).walls.size() == 1);
}
