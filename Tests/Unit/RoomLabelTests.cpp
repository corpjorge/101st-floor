#include <doctest/doctest.h>

#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Geometry/Rooms.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::cad;

// Sizes and moved texts of room names and areas on the plan (ADR-0035).
namespace
{
	Ray3 above(double x, double y) { return {x, y, 10000, 0, 0, -1}; }

	// The narrow bathroom of ADR-0034: 3.05 x 1.07 m on the axes, 2.93 x 0.95 m inside; its label point is (1525, 535) and
	// its name and area move west, to x 725, off the interior figure "2.93" at (1525, 345).
	Project bath(const LevelId& level)
	{
		Project project;
		project.walls = {Wall{WallId{Uuid{0, 1}}, {0, 0}, {3050, 0}, 120, 2800}, Wall{WallId{Uuid{0, 2}}, {3050, 0}, {3050, 1070}, 120, 2800},
			Wall{WallId{Uuid{0, 3}}, {3050, 1070}, {0, 1070}, 120, 2800}, Wall{WallId{Uuid{0, 4}}, {0, 1070}, {0, 0}, 120, 2800}};
		for (Wall& wall : project.walls) wall.level = level;
		return project;
	}

	// A document holding the bathroom on its ground floor, edited with the 2D plan shown.
	struct Plan
	{
		Document document{makeSequentialUuidSource()};
		EditController editor{document, EditSettings{}};
		std::string room;

		Plan()
		{
			Project project = document.project();
			const Project walls = bath(project.levels.front().id);
			project.walls = walls.walls;
			REQUIRE(document.replaceProject(project).ok());
			room = geometry::detectRooms(document.project()).front().key;
			editor.setDimensionsVisible(true);
			editor.setDimensionProvider([](const Project& p, const LevelId& level) { return drawing::buildPlanDrawing(p, level).dimensions; });
			editor.setRoomTextProvider([](const Project& p, const LevelId& level) { return drawing::buildPlanDrawing(p, level).roomTexts; });
			editor.setRoomLocator(geometry::makeRoomLocator());
		}

		drawing::PlanDrawing draw(const drawing::PlanOverlay& overlay = {}) const
		{
			return drawing::buildPlanDrawing(document.project(), editor.activeLevel(), overlay);
		}
	};

	const drawing::Label* labelOf(const drawing::PlanDrawing& plan, std::string_view text)
	{
		const auto found = std::find_if(plan.labels.begin(), plan.labels.end(), [text](const drawing::Label& l) { return l.text == text; });
		return found == plan.labels.end() ? nullptr : &*found;
	}

	const RoomText* textOf(const drawing::PlanDrawing& plan, bool area)
	{
		const auto found = std::find_if(plan.roomTexts.begin(), plan.roomTexts.end(), [area](const RoomText& t) { return t.area == area; });
		return found == plan.roomTexts.end() ? nullptr : &*found;
	}

	bool at(PlanPoint point, double x, double y)
	{
		return std::fabs(point.x_mm - x) < 0.01 && std::fabs(point.y_mm - y) < 0.01;
	}
}

TEST_CASE("room label layouts are reversible edits; back to the defaults removes the room's entry")
{
	Document document{makeSequentialUuidSource()};
	std::vector<std::string> notified;
	document.subscribe([&](const ChangeSet& changes) { notified = changes.changedRoomLabels; });
	RoomLabel label{"room-a", 120, 100, Vector2D{-300, 160}, std::nullopt};
	REQUIRE(document.execute(SetRoomLabelCommand{label}).ok());
	REQUIRE(document.project().findRoomLabel("room-a") != nullptr);
	CHECK(*document.project().findRoomLabel("room-a") == label);
	CHECK(notified == std::vector<std::string>{"room-a"});
	CHECK(document.execute(SetRoomLabelCommand{label}).value().empty());
	CHECK(document.undoLabel() == "Change room label");
	REQUIRE(document.execute(SetRoomLabelCommand{RoomLabel{"room-a"}}).ok());
	CHECK(document.project().roomLabels.empty());
	REQUIRE(document.undo().ok());
	CHECK(*document.project().findRoomLabel("room-a") == label);
	REQUIRE(document.undo().ok());
	CHECK(document.project().roomLabels.empty());
	REQUIRE(document.redo().ok());
	CHECK(*document.project().findRoomLabel("room-a") == label);
	CHECK(document.execute(SetRoomLabelCommand{RoomLabel{"room-b"}}).value().empty()); // nothing to store
}

TEST_CASE("invalid room label layouts leave the project and its history unchanged")
{
	Document document{makeSequentialUuidSource()};
	const std::string before = writeProjectJson(document.project());
	CHECK_FALSE(document.execute(SetRoomLabelCommand{RoomLabel{""}}).ok());
	CHECK_FALSE(document.execute(SetRoomLabelCommand{RoomLabel{"a", MinRoomTextHeightMm - 1}}).ok());
	CHECK_FALSE(document.execute(SetRoomLabelCommand{RoomLabel{"a", 200, MaxRoomTextHeightMm + 1}}).ok());
	CHECK_FALSE(document.execute(SetRoomLabelCommand{RoomLabel{"a", 200, 150, Vector2D{MaxRoomTextOffsetMm + 1, 0}}}).ok());
	CHECK_FALSE(document.execute(SetRoomLabelCommand{RoomLabel{"a", 200, 150, std::nullopt, Vector2D{0, -MaxRoomTextOffsetMm - 1}}}).ok());
	CHECK(writeProjectJson(document.project()) == before);
	CHECK_FALSE(document.canUndo());
	CHECK(document.execute(SetRoomLabelCommand{RoomLabel{"a", MinRoomTextHeightMm, MaxRoomTextHeightMm,
		Vector2D{MaxRoomTextOffsetMm, -MaxRoomTextOffsetMm}}}).ok());
}

TEST_CASE("room label layouts round trip in schema 14; older projects have none")
{
	Document document{makeSequentialUuidSource()};
	REQUIRE(document.execute(SetRoomLabelCommand{RoomLabel{"room-a", 120, 100, Vector2D{-300, 160}, std::nullopt}}).ok());
	REQUIRE(document.execute(SetRoomLabelCommand{RoomLabel{"room-b", 200, 150, std::nullopt, Vector2D{0, -900}}}).ok());
	const std::string encoded = writeProjectJson(document.project());
	CHECK(encoded.find("\"schemaVersion\": 15") != std::string::npos);
	CHECK(encoded.find("\"roomLabels\"") != std::string::npos);
	const auto decoded = readProjectJson(encoded);
	REQUIRE(decoded.ok());
	CHECK(decoded.value().roomLabels == document.project().roomLabels);
	CHECK(writeProjectJson(decoded.value()) == encoded);
	const auto legacy = readProjectJson(R"({"format":"101st-floor-project","schemaVersion":13,"units":"mm","levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[]})");
	REQUIRE(legacy.ok());
	CHECK(legacy.value().roomLabels.empty());
	CHECK(writeProjectJson(Document{makeSequentialUuidSource()}.project()).find("roomLabels") == std::string::npos);
}

TEST_CASE("malformed, out of range and duplicate room label layouts cannot be loaded")
{
	const std::string head = R"({"format":"101st-floor-project","schemaVersion":14,"units":"mm","levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[],"roomLabels":)";
	REQUIRE(readProjectJson(head + R"([{"room":"a","nameHeightMm":120,"areaHeightMm":100,"areaOffset":[0,-900]}])" + "}").ok());
	for (const std::string labels : {R"({})", R"([5])", R"([{"room":"a"}])", R"([{"room":"a","nameHeightMm":200.5,"areaHeightMm":150}])",
		R"([{"room":"a","nameHeightMm":"200","areaHeightMm":150}])", R"([{"room":"a","nameHeightMm":40,"areaHeightMm":150}])",
		R"([{"room":"a","nameHeightMm":200,"areaHeightMm":1001}])", R"([{"room":"a","nameHeightMm":200,"areaHeightMm":150,"nameOffset":[1]}])",
		R"([{"room":"a","nameHeightMm":200,"areaHeightMm":150,"nameOffset":[0.5,1]}])",
		R"([{"room":"a","nameHeightMm":200,"areaHeightMm":150,"areaOffset":[0,100001]}])",
		R"([{"room":"","nameHeightMm":200,"areaHeightMm":150}])",
		R"([{"room":"a","nameHeightMm":200,"areaHeightMm":150},{"room":"a","nameHeightMm":120,"areaHeightMm":150}])"})
	{
		CAPTURE(labels);
		CHECK_FALSE(readProjectJson(head + labels + "}").ok());
	}
}

TEST_CASE("automatic wall splitting gives each new room the layout, offsets from its own label point")
{
	Document document{makeSequentialUuidSource()};
	document.setRoomTransfer(geometry::makeRoomTransfer());
	Project project = document.project();
	project.walls = {Wall{WallId{Uuid{0, 100}}, {0, 0}, {10000, 0}, 120, 2800}, Wall{WallId{Uuid{0, 101}}, {10000, 0}, {10000, 4000}, 120, 2800},
		Wall{WallId{Uuid{0, 102}}, {10000, 4000}, {0, 4000}, 120, 2800}, Wall{WallId{Uuid{0, 103}}, {0, 4000}, {0, 0}, 120, 2800}};
	for (Wall& wall : project.walls) wall.level = project.levels.front().id;
	const RoomLabel layout{geometry::detectRooms(project).front().key, 300, 120, Vector2D{100, 200}, std::nullopt};
	project.roomLabels.push_back(layout);
	REQUIRE(document.replaceProject(project).ok());
	REQUIRE(document.addWall({4000, 0}, {4000, 4000}, 120, 2800).ok());
	const auto rooms = geometry::detectRooms(document.project());
	REQUIRE(rooms.size() == 2);
	for (const auto& room : rooms)
	{
		const RoomLabel* inherited = document.project().findRoomLabel(room.key);
		REQUIRE(inherited != nullptr);
		CHECK(inherited->nameHeightMm == 300);
		CHECK(inherited->areaHeightMm == 120);
		CHECK(inherited->nameOffset == Vector2D{100, 200});
		CHECK_FALSE(inherited->areaOffset);
		const drawing::PlanDrawing plan = drawing::buildPlanDrawing(document.project(), project.levels.front().id);
		const auto name = std::find_if(plan.roomTexts.begin(), plan.roomTexts.end(),
			[&room](const RoomText& t) { return t.room == room.key && !t.area; });
		REQUIRE(name != plan.roomTexts.end());
		CHECK(at(name->centre, room.labelPoint.x_mm + 100.0, room.labelPoint.y_mm + 200.0));
	}
	REQUIRE(document.undo().ok());
	CHECK(document.project().roomLabels == project.roomLabels);
}

TEST_CASE("the plan writes each room's texts at its sizes; moved ones stay put, the others still avoid the figures")
{
	const Project project = bath(LevelId());
	const std::string room = geometry::detectRooms(project).front().key;
	const drawing::PlanDrawing automatic = drawing::buildPlanDrawing(project, LevelId());
	REQUIRE(automatic.roomTexts.size() == 2);
	// Published boxes are what the plan paints: same centre and height, the width of the text at that height.
	for (const RoomText& text : automatic.roomTexts)
	{
		const drawing::Label* label = labelOf(automatic, text.area ? "2.78 m2" : "Room 1");
		REQUIRE(label != nullptr);
		CHECK(text.room == room);
		CHECK(at(text.centre, label->at.x_mm, label->at.y_mm));
		CHECK(text.heightMm == label->heightMm);
		CHECK(text.widthMm == doctest::Approx(planTextWidthMm(textCharacters(label->text), label->heightMm)));
		CHECK(at(text.anchor, 1525.0, 535.0));
	}
	CHECK(at(textOf(automatic, false)->centre, 725.0, 695.0));
	CHECK(at(textOf(automatic, true)->centre, 725.0, 400.0));

	// Smaller texts, closer to each other, need to move less far off the figure.
	Project small = project;
	small.roomLabels.push_back(RoomLabel{room, 100, 80});
	const drawing::PlanDrawing fitted = drawing::buildPlanDrawing(small, LevelId());
	CHECK(labelOf(fitted, "Room 1")->heightMm == 100.0);
	CHECK(labelOf(fitted, "2.78 m2")->heightMm == 80.0);
	CHECK(textOf(fitted, false)->widthMm == doctest::Approx(planTextWidthMm(6, 100.0)));
	CHECK(at(textOf(fitted, false)->centre, 950.0, 535.0 + 80.0));
	CHECK(at(textOf(fitted, true)->centre, 950.0, 535.0 - 72.0));

	// The name moved onto the figure stays there; the area, still automatic, keeps out of the figure's way.
	Project moved = project;
	moved.roomLabels.push_back(RoomLabel{room, 200, 150, Vector2D{0, -190}});
	const drawing::PlanDrawing over = drawing::buildPlanDrawing(moved, LevelId());
	CHECK(at(textOf(over, false)->centre, 1525.0, 345.0));
	CHECK(at(textOf(over, true)->centre, 725.0, 400.0));
	moved.roomLabels.front().areaOffset = Vector2D{1200, -100};
	CHECK(at(textOf(drawing::buildPlanDrawing(moved, LevelId()), true)->centre, 2725.0, 435.0));

	// The editing view: the selected room's texts in the selection ink, a drag's layout instead of the stored one.
	drawing::PlanOverlay overlay;
	overlay.selectedRoom = room;
	overlay.previewRoomLabel = RoomLabel{room, 300, 150, Vector2D{-500, 0}};
	const drawing::PlanDrawing editing = drawing::buildPlanDrawing(moved, LevelId(), overlay);
	CHECK(labelOf(editing, "Room 1")->ink == drawing::Ink::DimensionSelected);
	CHECK(labelOf(editing, "Room 1")->heightMm == 300.0);
	CHECK(at(textOf(editing, false)->centre, 1025.0, 535.0));
	CHECK(labelOf(drawing::buildPlanDrawing(moved, LevelId()), "Room 1")->ink == drawing::Ink::Label);
}

TEST_CASE("in the plan, a room's name and area are dragged one at a time, each drag one undoable change")
{
	Plan plan;
	// The name: pressing it selects the room; it follows the pointer and the release stores its offset.
	REQUIRE(plan.editor.pointerPressed(above(725, 695)).ok());
	CHECK(plan.editor.selectedRoom() == plan.room);
	CHECK(plan.editor.isDraggingRoomText());
	plan.editor.pointerMoved(above(1225, 695));
	REQUIRE(plan.editor.view().previewRoomLabel);
	CHECK(plan.editor.view().previewRoomLabel->nameOffset == Vector2D{-300, 160});
	CHECK(plan.document.project().roomLabels.empty()); // nothing changes until the release
	REQUIRE(plan.editor.pointerReleased(above(1225, 695)).ok());
	CHECK_FALSE(plan.editor.isDraggingRoomText());
	REQUIRE(plan.document.project().findRoomLabel(plan.room));
	CHECK(*plan.document.project().findRoomLabel(plan.room) == RoomLabel{plan.room, 200, 150, Vector2D{-300, 160}});
	CHECK(plan.editor.selectedRoom() == plan.room);
	CHECK_FALSE(plan.editor.view().previewRoomLabel);

	// The area, on its own: grabbed off its centre, it keeps that grip.
	const RoomText area = *textOf(plan.draw(), true);
	REQUIRE(plan.editor.pointerPressed(above(area.centre.x_mm + 200, area.centre.y_mm - 40)).ok());
	plan.editor.pointerMoved(above(area.centre.x_mm + 1700, area.centre.y_mm - 40));
	REQUIRE(plan.editor.pointerReleased(above(area.centre.x_mm + 1700, area.centre.y_mm - 40)).ok());
	const RoomLabel both = *plan.document.project().findRoomLabel(plan.room);
	CHECK(both.nameOffset == Vector2D{-300, 160});
	REQUIRE(both.areaOffset);
	CHECK(both.areaOffset == Vector2D{static_cast<std::int64_t>(std::llround(area.centre.x_mm + 1500 - 1525)),
		static_cast<std::int64_t>(std::llround(area.centre.y_mm - 535))});
	CHECK(at(textOf(plan.draw(), true)->centre, 1525.0 + both.areaOffset->dx_mm, 535.0 + both.areaOffset->dy_mm));

	REQUIRE(plan.editor.undo().ok());
	CHECK_FALSE(plan.document.project().findRoomLabel(plan.room)->areaOffset);
	REQUIRE(plan.editor.undo().ok());
	CHECK(plan.document.project().roomLabels.empty());
	CHECK_FALSE(plan.document.canUndo());
}

TEST_CASE("a click or a shake does not move a room's text; Esc drops the drag; texts are only picked on the plan")
{
	Plan plan;
	REQUIRE(plan.editor.pointerPressed(above(725, 400)).ok()); // the area
	REQUIRE(plan.editor.pointerReleased(above(725, 400)).ok());
	REQUIRE(plan.editor.pointerPressed(above(725, 400)).ok());
	plan.editor.pointerMoved(above(735, 405)); // less than a fifth of its height
	REQUIRE(plan.editor.pointerReleased(above(735, 405)).ok());
	CHECK(plan.editor.selectedRoom() == plan.room);
	CHECK(plan.document.project().roomLabels.empty());
	CHECK_FALSE(plan.document.canUndo());

	REQUIRE(plan.editor.pointerPressed(above(725, 695)).ok());
	plan.editor.pointerMoved(above(2000, 695));
	plan.editor.cancel();
	CHECK_FALSE(plan.editor.isDragging());
	CHECK_FALSE(plan.editor.view().previewRoomLabel);
	REQUIRE(plan.editor.pointerReleased(above(2000, 695)).ok());
	CHECK(plan.document.project().roomLabels.empty());

	// Without the plan the texts are not there to grab: a press on the floor just selects the room.
	plan.editor.clearSelection();
	plan.editor.setDimensionsVisible(false);
	REQUIRE(plan.editor.pointerPressed(above(725, 695)).ok());
	CHECK_FALSE(plan.editor.isDraggingRoomText());
	CHECK(plan.editor.selectedRoom() == plan.room);
	plan.editor.pointerMoved(above(2000, 695));
	REQUIRE(plan.editor.pointerReleased(above(2000, 695)).ok());
	CHECK(plan.document.project().roomLabels.empty());

	// Hiding the plan mid-drag lets go of it.
	plan.editor.setDimensionsVisible(true);
	REQUIRE(plan.editor.pointerPressed(above(725, 695)).ok());
	plan.editor.pointerMoved(above(2000, 695));
	plan.editor.setDimensionsVisible(false);
	CHECK_FALSE(plan.editor.isDraggingRoomText());
	REQUIRE(plan.editor.pointerReleased(above(2000, 695)).ok());
	CHECK(plan.document.project().roomLabels.empty());
}

TEST_CASE("a room's text over a dimension figure is grabbed before the dimension")
{
	Plan plan;
	REQUIRE(plan.document.execute(SetRoomLabelCommand{RoomLabel{plan.room, 200, 150, Vector2D{0, -190}}}).ok()); // the name on "2.93"
	REQUIRE(plan.editor.pointerPressed(above(1525, 345)).ok());
	CHECK(plan.editor.isDraggingRoomText());
	CHECK_FALSE(plan.editor.selectedDimension());
	plan.editor.pointerMoved(above(1525, 745));
	REQUIRE(plan.editor.pointerReleased(above(1525, 745)).ok());
	CHECK(plan.document.project().findRoomLabel(plan.room)->nameOffset == Vector2D{0, 210});
	// Elsewhere on its line, away from the texts, the dimension is picked as before.
	REQUIRE(plan.editor.pointerPressed(above(2400, 230)).ok());
	CHECK_FALSE(plan.editor.isDraggingRoomText());
	REQUIRE(plan.editor.selectedDimension());
	REQUIRE(plan.editor.pointerReleased(above(2400, 230)).ok());
}

TEST_CASE("the selected room's text sizes are typed like lengths; positions go back to automatic keeping the sizes")
{
	Plan plan;
	CHECK_FALSE(plan.editor.setSelectedRoomNameHeight("12").ok()); // no room selected
	CHECK(plan.editor.selectedRoomLabel() == RoomLabel{});
	REQUIRE(plan.editor.selectRoom(plan.room).ok());
	CHECK(plan.editor.selectedRoomLabel() == RoomLabel{plan.room});
	REQUIRE(plan.editor.setSelectedRoomNameHeight("12").ok()); // cm
	REQUIRE(plan.editor.setSelectedRoomAreaHeight("90 mm").ok());
	CHECK(plan.editor.selectedRoomLabel() == RoomLabel{plan.room, 120, 90});
	CHECK(labelOf(plan.draw(), "Room 1")->heightMm == 120.0);
	CHECK_FALSE(plan.editor.setSelectedRoomAreaHeight("4").ok());
	CHECK_FALSE(plan.editor.setSelectedRoomAreaHeight("1.5 m").ok());
	CHECK_FALSE(plan.editor.setSelectedRoomAreaHeight("x").ok());
	CHECK(plan.editor.selectedRoomLabel().areaHeightMm == 90);

	REQUIRE(plan.editor.pointerPressed(above(textOf(plan.draw(), false)->centre.x_mm, textOf(plan.draw(), false)->centre.y_mm)).ok());
	plan.editor.pointerMoved(above(2400, 800));
	REQUIRE(plan.editor.pointerReleased(above(2400, 800)).ok());
	REQUIRE(plan.editor.selectedRoomLabel().nameOffset);
	REQUIRE(plan.editor.resetSelectedRoomTextPositions().ok());
	CHECK(plan.editor.selectedRoomLabel() == RoomLabel{plan.room, 120, 90});
	REQUIRE(plan.editor.setSelectedRoomNameHeight("20").ok());
	REQUIRE(plan.editor.setSelectedRoomAreaHeight("15").ok());
	CHECK(plan.document.project().roomLabels.empty()); // all automatic again
	REQUIRE(plan.editor.undo().ok());
	CHECK(plan.editor.selectedRoomLabel() == RoomLabel{plan.room, 200, 90});
}
