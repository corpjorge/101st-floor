#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"

using namespace f101;
using namespace f101::cad;

namespace
{
	// Rays in scene space: looking straight down at a plan point, like a top view.
	Ray3 down(double x, double y)
	{
		return Ray3{x, y, 10000.0, 0.0, 0.0, -1.0};
	}

	struct Editing
	{
		Document document{makeSequentialUuidSource()};
		EditController editor{document, EditSettings{}};
		WallId wall = document.addWall({0, 0}, {3550, 0}, 120, 2400).value();
	};
}

TEST_CASE_FIXTURE(Editing, "a click selects the wall under the cursor, a click on empty floor clears it")
{
	CHECK(editor.mode() == EditMode::Select);
	REQUIRE(editor.pointerPressed(down(1000.0, 30.0)).ok());
	REQUIRE(editor.pointerReleased(down(1000.0, 30.0)).ok());
	CHECK(editor.selection() == wall);
	CHECK(editor.view().selectedWall == wall);
	CHECK(editor.view().handles == std::vector<Point2D>{{0, 0}, {3550, 0}});

	REQUIRE(editor.pointerPressed(down(1000.0, 2000.0)).ok());
	CHECK(editor.selection().isNil());
	CHECK(editor.view().handles.empty());
}

TEST_CASE_FIXTURE(Editing, "dragging an end handle moves that endpoint in one undoable command")
{
	REQUIRE(editor.select(wall).ok());
	REQUIRE(editor.pointerPressed(down(3560.0, 10.0)).ok()); // on the end handle
	CHECK(editor.isDragging());
	CHECK(document.project().walls[0].end == Point2D{3550, 0}); // pressing alone moves nothing

	editor.pointerMoved(down(4213.0, 35.0));
	const EditView dragging = editor.view();
	REQUIRE(dragging.previewWalls.size() == 1);
	CHECK(dragging.previewWalls[0].id == wall);
	CHECK(dragging.previewWalls[0].end == Point2D{4200, 0}); // angle + grid snapping from the fixed start
	CHECK(document.project().walls[0].end == Point2D{3550, 0});

	REQUIRE(editor.pointerReleased(down(4213.0, 35.0)).ok());
	CHECK_FALSE(editor.isDragging());
	CHECK(document.project().walls[0].end == Point2D{4200, 0});
	CHECK(document.undoLabel() == "Move wall endpoint");
	CHECK(editor.view().previewWalls.empty());

	REQUIRE(editor.undo().ok());
	CHECK(document.project().walls[0].end == Point2D{3550, 0});
	CHECK(editor.selection() == wall); // undoing a move keeps the selection
}

TEST_CASE_FIXTURE(Editing, "a start handle drag snaps to another wall's endpoint")
{
	const WallId other = document.addWall({5000, 3000}, {8000, 3000}, 120, 2400).value();
	REQUIRE(editor.select(other).ok());
	REQUIRE(editor.pointerPressed(down(5000.0, 3000.0)).ok());
	editor.pointerMoved(down(3600.0, 60.0)); // near the first wall's end
	REQUIRE(editor.pointerReleased(down(3600.0, 60.0)).ok());
	CHECK(document.project().findWall(other)->start == Point2D{3550, 0});
}

TEST_CASE_FIXTURE(Editing, "a click on a handle without moving leaves the wall as it was")
{
	REQUIRE(document.setWallEnd(wall, {3553, 7}).ok()); // off-grid end
	REQUIRE(editor.select(wall).ok());
	REQUIRE(editor.pointerPressed(down(3553.0, 7.0)).ok());
	REQUIRE(editor.pointerReleased(down(3553.0, 7.0)).ok());
	CHECK(document.project().walls[0].end == Point2D{3553, 7});
	REQUIRE(editor.undo().ok()); // undoes the setWallEnd above: the click recorded nothing
	CHECK(document.project().walls[0].end == Point2D{3550, 0});
}

TEST_CASE_FIXTURE(Editing, "Esc drops a drag, then the selection")
{
	REQUIRE(editor.select(wall).ok());
	REQUIRE(editor.pointerPressed(down(0.0, 0.0)).ok());
	editor.pointerMoved(down(-1000.0, 0.0));
	editor.cancel();
	CHECK_FALSE(editor.isDragging());
	CHECK(editor.selection() == wall);
	REQUIRE(editor.pointerReleased(down(-1000.0, 0.0)).ok());
	CHECK(document.project().walls[0].start == Point2D{0, 0});
	editor.cancel();
	CHECK(editor.selection().isNil());
}

TEST_CASE_FIXTURE(Editing, "Delete removes the selected wall; undo brings it back")
{
	CHECK_FALSE(editor.deleteSelection().ok());
	REQUIRE(editor.select(wall).ok());
	REQUIRE(editor.deleteSelection().ok());
	CHECK(document.project().walls.empty());
	CHECK(editor.selection().isNil());
	REQUIRE(editor.undo().ok());
	CHECK(document.project().walls.size() == 1);
	REQUIRE(editor.redo().ok());
	CHECK(document.project().walls.empty());
}

TEST_CASE_FIXTURE(Editing, "the properties of the selection take typed lengths")
{
	CHECK(editor.setSelectionLength("420").error().code == ErrorCode::NotFound);
	REQUIRE(editor.select(wall).ok());
	REQUIRE(editor.setSelectionLength("4.2 m").ok());
	CHECK(document.project().walls[0].end == Point2D{4200, 0});
	REQUIRE(editor.setSelectionThickness("15").ok()); // cm by default
	CHECK(document.project().walls[0].thicknessMm == 150);
	REQUIRE(editor.setSelectionHeight("2700 mm").ok());
	CHECK(document.project().walls[0].heightMm == 2700);

	CHECK(editor.setSelectionLength("abc").error().code == ErrorCode::ParseError);
	CHECK_FALSE(editor.setSelectionThickness("0").ok());
	CHECK_FALSE(editor.setSelectionHeight("-10").ok());
	Wall expected{wall, {0, 0}, {4200, 0}, 150, 2700};
	expected.level = editor.activeLevel();
	CHECK(document.project().walls[0] == expected);

	REQUIRE(editor.undo().ok());
	CHECK(document.project().walls[0].heightMm == 2400);
}

TEST_CASE_FIXTURE(Editing, "Wall mode: the snapped cursor, the typed length and the preview are in the view")
{
	editor.setMode(EditMode::Wall);
	editor.pointerMoved(down(5012.0, 3021.0));
	CHECK(editor.view().handles == std::vector<Point2D>{{5000, 3000}});
	CHECK(editor.view().previewWalls.empty());

	REQUIRE(editor.pointerPressed(down(5012.0, 3021.0)).ok());
	REQUIRE(editor.pointerReleased(down(5012.0, 3021.0)).ok());
	editor.pointerMoved(down(5020.0, 5800.0));
	REQUIRE(editor.view().previewWalls.size() == 1);
	editor.appendText("3.55 m");
	CHECK(editor.view().previewWalls[0].end == Point2D{5000, 6550});
	editor.backspace();
	editor.backspace(); // "3.55" -> centimetres
	CHECK(editor.view().previewWalls[0].end == Point2D{5000, 3036}); // 35.5 mm rounds half away from zero
	editor.appendText("x");
	CHECK(editor.wallTool().typedText() == "3.55");
}

TEST_CASE_FIXTURE(Editing, "Wall mode creates exact walls and Esc returns to Select mode")
{
	REQUIRE(editor.select(wall).ok());
	editor.setMode(EditMode::Wall);
	CHECK(editor.selection().isNil());

	REQUIRE(editor.pointerPressed(down(3540.0, 12.0)).ok()); // snaps to the wall's end
	editor.pointerMoved(down(3560.0, 2000.0));
	editor.appendText("300");
	REQUIRE(editor.confirm().ok());
	REQUIRE(document.project().walls.size() == 2);
	CHECK(document.project().walls[1].start == Point2D{3550, 0});
	CHECK(document.project().walls[1].end == Point2D{3550, 3000});

	editor.cancel(); // stops drafting
	CHECK(editor.mode() == EditMode::Wall);
	CHECK(editor.wallTool().state() == WallTool::State::Idle);
	editor.cancel(); // leaves Wall mode
	CHECK(editor.mode() == EditMode::Select);
}

TEST_CASE_FIXTURE(Editing, "undo while drawing cancels the wall in progress first")
{
	editor.setMode(EditMode::Wall);
	REQUIRE(editor.pointerPressed(down(0.0, 3000.0)).ok());
	CHECK(editor.wallTool().state() == WallTool::State::Drafting);
	REQUIRE(editor.undo().ok()); // undoes the fixture's wall
	CHECK(editor.wallTool().state() == WallTool::State::Idle);
	CHECK(document.project().walls.empty());
}

TEST_CASE_FIXTURE(Editing, "pointing at the sky draws nothing and reports why")
{
	editor.setMode(EditMode::Wall);
	const Status pressed = editor.pointerPressed(Ray3{0.0, -5000.0, 1000.0, 1.0, 0.0, 0.5}); // away from the wall
	CHECK_FALSE(pressed.ok());
	CHECK_FALSE(editor.pointerOnPlan().has_value());
	CHECK(editor.view().handles.empty());
}

TEST_CASE_FIXTURE(Editing, "reset drops the selection and any gesture; a vanished selection reads as nil")
{
	REQUIRE(editor.select(wall).ok());
	REQUIRE(document.removeWall(wall).ok()); // changed outside the controller (e.g. MCP)
	CHECK(editor.selection().isNil());
	CHECK(editor.view().handles.empty());
	REQUIRE(document.undo().ok());
	CHECK(editor.selection() == wall); // still remembered while it exists
	editor.reset();
	CHECK(editor.selection().isNil());
	CHECK(editor.select(WallId{Uuid{9, 9}}).error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(Editing, "dragging a shared corner moves every wall at it; the preview shows them all")
{
	const WallId east = document.addWall({3550, 0}, {3550, 3000}, 120, 2400).value();
	REQUIRE(editor.select(wall).ok());
	REQUIRE(editor.pointerPressed(down(3550.0, 0.0)).ok());
	editor.pointerMoved(down(3560.0, 20.0)); // close to the old corner: the joined walls do not pull it back
	editor.pointerMoved(down(4012.0, 12.0));
	const EditView dragging = editor.view();
	REQUIRE(dragging.previewWalls.size() == 2);
	CHECK(dragging.previewWalls[0].end == Point2D{4000, 0});
	CHECK(dragging.previewWalls[1].start == Point2D{4000, 0});

	REQUIRE(editor.pointerReleased(down(4012.0, 12.0)).ok());
	CHECK(document.project().findWall(wall)->end == Point2D{4000, 0});
	CHECK(document.project().findWall(east)->start == Point2D{4000, 0});
	CHECK(document.undoLabel() == "Move corner");
	REQUIRE(editor.undo().ok());
	CHECK(document.project().findWall(east)->start == Point2D{3550, 0});
}

TEST_CASE_FIXTURE(Editing, "pointing at the top of a wall from an oblique view targets that wall, not the floor behind it")
{
	editor.setMode(EditMode::Wall);
	// Aimed at (1000, 0) on top of the 2400 mm wall; this ray meets the floor 4.6 m further north.
	editor.pointerMoved(Ray3{1000.0, -5000.0, 5000.0, 0.0, 5000.0, -2600.0});
	REQUIRE(editor.pointerOnPlan().has_value());
	CHECK(editor.pointerOnPlan()->y_mm == doctest::Approx(0.0));
	CHECK(editor.wallTool().hoverPoint() == Point2D{1000, 0});
	CHECK(editor.wallTool().snapKind() == SnapKind::Wall);
}

TEST_CASE_FIXTURE(Editing, "a corner grabbed high up its handle keeps that offset while dragging")
{
	REQUIRE(editor.select(wall).ok());
	// Grab the end handle at its top (2500 mm) from an oblique view: the floor behind is 5 m north of the corner.
	REQUIRE(editor.pointerPressed(Ray3{3550.0, -5000.0, 5000.0, 0.0, 5000.0, -2500.0}).ok());
	REQUIRE(editor.isDragging());
	// 200 mm further east at the same height.
	editor.pointerMoved(Ray3{3550.0, -5000.0, 5000.0, 200.0, 5000.0, -2500.0});
	REQUIRE(editor.view().previewWalls.size() == 1);
	CHECK(editor.view().previewWalls[0].end == Point2D{3950, 0});
}

TEST_CASE_FIXTURE(Editing, "the door tool previews a door centred on the pointer and a click adds it")
{
	editor.setMode(EditMode::Door);
	editor.pointerMoved(down(1510.0, 40.0)); // over the wall
	REQUIRE(editor.openingPlacement().has_value());
	CHECK(editor.openingPlacement()->wall == wall);
	CHECK(editor.openingPlacement()->opening.offsetMm == 1050); // centre 1510 - 450, on the 50 mm grid
	const EditView preview = editor.view();
	REQUIRE(preview.previewWalls.size() == 1);
	CHECK(preview.previewWalls[0].openings.size() == 1);
	CHECK(preview.handles == std::vector<Point2D>{{1050, 0}, {1950, 0}});

	REQUIRE(editor.pointerPressed(down(1510.0, 40.0)).ok());
	const Wall& host = *document.project().findWall(wall);
	REQUIRE(host.openings.size() == 1);
	CHECK(host.openings[0].kind == OpeningKind::Door);
	CHECK(host.openings[0].widthMm == 900);
	CHECK(host.openings[0].heightMm == 2100);
	CHECK(document.undoLabel() == "Add door");
	// That place is taken now: the tool offers the nearest free stretch, right next to it.
	REQUIRE(editor.openingPlacement().has_value());
	CHECK(editor.openingPlacement()->opening.offsetMm == 1950);

	// Near the end the door is kept inside the wall; away from walls there is nothing to place.
	editor.pointerMoved(down(3540.0, 0.0));
	CHECK(editor.openingPlacement()->opening.offsetMm == 3550 - 900);
	editor.pointerMoved(down(1500.0, 2000.0));
	CHECK_FALSE(editor.openingPlacement().has_value());
	CHECK_FALSE(editor.pointerPressed(down(1500.0, 2000.0)).ok());
	editor.cancel();
	CHECK(editor.mode() == EditMode::Select);
}

TEST_CASE_FIXTURE(Editing, "the window tool adds a window with a sill")
{
	editor.setMode(EditMode::Window);
	REQUIRE(editor.pointerPressed(down(2000.0, 0.0)).ok());
	const Opening& window = document.project().findWall(wall)->openings.at(0);
	CHECK(window.kind == OpeningKind::Window);
	CHECK(window.sillMm == 900);
	CHECK(window.offsetMm == 1400);
}

TEST_CASE_FIXTURE(Editing, "clicking a wall where it has an opening selects the opening; its properties take typed lengths")
{
	const OpeningId door = document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Door, 1000, 900, 2100, 0}).value();
	REQUIRE(editor.pointerPressed(down(1400.0, 0.0)).ok());
	REQUIRE(editor.selectedOpening().has_value());
	CHECK(editor.selectedOpening()->opening.id == door);
	CHECK(editor.selection().isNil()); // the wall itself is not the selection
	CHECK(editor.view().selectedWall == wall);
	CHECK(editor.view().handles == std::vector<Point2D>{{1000, 0}, {1900, 0}});

	REQUIRE(editor.setOpeningWidth("80").ok());
	REQUIRE(editor.setOpeningOffset("1.2 m").ok());
	CHECK(editor.selectedOpening()->opening.widthMm == 800);
	CHECK(editor.selectedOpening()->opening.offsetMm == 1200);
	CHECK_FALSE(editor.setOpeningSill("10").ok()); // doors stay on the floor
	CHECK_FALSE(editor.setOpeningHeight("240").ok()); // no lintel left

	REQUIRE(editor.pointerPressed(down(2600.0, 0.0)).ok()); // elsewhere on the wall: the wall
	CHECK(editor.selection() == wall);
	CHECK_FALSE(editor.selectedOpening().has_value());

	REQUIRE(editor.selectOpening(wall, door).ok());
	REQUIRE(editor.deleteSelection().ok());
	CHECK(document.project().findWall(wall)->openings.empty());
	CHECK(document.undoLabel() == "Delete opening");
	CHECK_FALSE(editor.selectedOpening().has_value());
	REQUIRE(editor.undo().ok());
	CHECK(document.project().findWall(wall)->openings.size() == 1);
}

TEST_CASE_FIXTURE(Editing, "dragging a wall body translates both joints with exact grab offset and one undo step")
{
	const WallId left = document.addWall({0, 0}, {0, 3000}, 120, 2400).value();
	const WallId right = document.addWall({3550, 0}, {3550, 3000}, 120, 2400).value();
	const OpeningId door = document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Door, 2000, 900, 2100, 0}).value();
	const std::string before = writeProjectJson(document.project());
	REQUIRE(editor.pointerPressed(down(1003, 31)).ok());
	CHECK(editor.isDragging());
	editor.pointerMoved(down(1003, 31));
	CHECK(editor.view().handles == std::vector<Point2D>{{0, 0}, {3550, 0}});
	editor.pointerMoved(down(1240, 644));
	CHECK(editor.view().handles == std::vector<Point2D>{{237, 613}, {3787, 613}});
	CHECK(editor.view().previewWalls.size() == 3);
	CHECK(writeProjectJson(document.project()) == before);
	REQUIRE(editor.pointerReleased(down(9000, 9000)).ok()); // release itself does not move the wall
	CHECK_FALSE(editor.isDragging());
	const Wall& moved = *document.project().findWall(wall);
	CHECK(moved.start == Point2D{237, 613});
	CHECK(moved.end == Point2D{3787, 613});
	CHECK(moved.lengthMm() == 3550);
	CHECK(moved.findOpening(door)->offsetMm == 2000);
	CHECK(document.project().findWall(left)->start == moved.start);
	CHECK(document.project().findWall(left)->end == Point2D{0, 3000});
	CHECK(document.project().findWall(right)->start == moved.end);
	CHECK(document.project().findWall(right)->end == Point2D{3550, 3000});
	CHECK(document.undoLabel() == "Move wall");
	const std::string after = writeProjectJson(document.project());
	REQUIRE(editor.undo().ok());
	CHECK(writeProjectJson(document.project()) == before);
	REQUIRE(editor.redo().ok());
	CHECK(writeProjectJson(document.project()) == after);
	const auto reopened = readProjectJson(after);
	REQUIRE(reopened.ok());
	CHECK(writeProjectJson(reopened.value()) == after);
}

TEST_CASE("wall body dragging keeps vertical and diagonal lengths and angles")
{
	for (const Point2D end : {Point2D{0, 3000}, Point2D{3000, 3000}})
	{
		Document document{makeSequentialUuidSource()};
		EditController editor{document, EditSettings{}};
		const WallId id = document.addWall({101, 103}, end, 120, 2400).value();
		const Wall original = *document.project().findWall(id);
		const double x = (101.0 + static_cast<double>(end.x_mm)) / 2.0;
		const double y = (103.0 + static_cast<double>(end.y_mm)) / 2.0;
		REQUIRE(editor.pointerPressed(down(x, y)).ok());
		editor.pointerMoved(down(x - 417, y + 213));
		REQUIRE(editor.pointerReleased(down(x - 417, y + 213)).ok());
		const Wall& moved = *document.project().findWall(id);
		CHECK(moved.start == Point2D{-316, 316});
		CHECK(moved.end == Point2D{end.x_mm - 417, end.y_mm + 213});
		CHECK(moved.end - moved.start == original.end - original.start);
	}
}

TEST_CASE_FIXTURE(Editing, "wall body clicks and canceled drags preserve history and endpoint grips still resize")
{
	const std::string before = writeProjectJson(document.project());
	REQUIRE(editor.pointerPressed(down(1700, 25)).ok());
	REQUIRE(editor.pointerReleased(down(1700, 25)).ok());
	CHECK(document.undoLabel() == "Create wall");
	REQUIRE(editor.pointerPressed(down(1700, 25)).ok());
	editor.pointerMoved(down(1900, 700));
	editor.cancel();
	CHECK(editor.selection() == wall);
	CHECK_FALSE(editor.isDragging());
	REQUIRE(editor.pointerReleased(down(1900, 700)).ok());
	CHECK(writeProjectJson(document.project()) == before);
	REQUIRE(editor.pointerPressed(down(3550, 0)).ok());
	editor.pointerMoved(down(4100, 0));
	REQUIRE(editor.pointerReleased(down(4100, 0)).ok());
	CHECK(document.project().findWall(wall)->start == Point2D{0, 0});
	CHECK(document.project().findWall(wall)->end == Point2D{4100, 0});
	CHECK(document.undoLabel() == "Move wall endpoint");
}

TEST_CASE_FIXTURE(Editing, "invalid attached wall collapse keeps the last valid wall body preview")
{
	REQUIRE(document.addWall({0, 0}, {0, 1000}, 120, 2400).ok());
	REQUIRE(editor.pointerPressed(down(1700, 0)).ok());
	editor.pointerMoved(down(1700, 700));
	editor.pointerMoved(down(1700, 1000));
	CHECK(editor.view().handles == std::vector<Point2D>{{0, 700}, {3550, 700}});
	REQUIRE(editor.pointerReleased(down(1700, 1000)).ok());
	CHECK(document.project().findWall(wall)->start == Point2D{0, 700});
}

TEST_CASE_FIXTURE(Editing, "wall body dragging in perspective follows one floor plane through unrelated walls")
{
	REQUIRE(document.addWall({0, 1000}, {3550, 1000}, 120, 2400).ok());
	const Ray3 press{1000, -3800, 10000, 0, 0.5, -1}; // hits selected wall at its top; meets floor at y=1200
	REQUIRE(editor.pointerPressed(press).ok());
	CHECK(editor.selection() == wall);
	CHECK(editor.isDragging());
	editor.pointerMoved(press);
	CHECK(editor.view().handles == std::vector<Point2D>{{0, 0}, {3550, 0}});
	const Ray3 moved{1000, -3300, 10000, 0, 0.5, -1}; // crosses the unrelated wall, same floor-plane delta
	editor.pointerMoved(moved);
	REQUIRE(editor.pointerReleased(moved).ok());
	CHECK(document.project().findWall(wall)->start == Point2D{0, 500});
	CHECK(document.project().findWall(wall)->end == Point2D{3550, 500});
}
