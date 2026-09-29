#include "doctest.h"

#include "F101/Cad/EditController.h"

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
	CHECK(document.project().walls[0] == Wall{wall, {0, 0}, {4200, 0}, 150, 2700});

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
