#include <doctest/doctest.h>
#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Drawing/SvgWriter.h"

#include <algorithm>

using namespace f101;
using namespace f101::cad;
namespace
{
	Ray3 above(double x, double y, double z = 10000) { return {x, y, z, 0, 0, -1}; }
	void dimensions(EditController& editor)
	{
		editor.setDimensionsVisible(true);
		editor.setDimensionProvider([](const Project& p, const LevelId& level) { return drawing::buildPlanDrawing(p, level).dimensions; });
	}
	bool hasFigure(const drawing::PlanDrawing& plan, std::string_view text)
	{
		return std::any_of(plan.labels.begin(), plan.labels.end(), [text](const auto& label) { return label.text == text; });
	}
}

TEST_CASE("manual measurements use three clicks, snap endpoints and persist reversible precise edits")
{
	Document doc{makeSequentialUuidSource()};
	EditController editor{doc, {}};
	dimensions(editor);
	editor.setMode(EditMode::Dimension);
	REQUIRE(editor.pointerPressed(above(1, 2)).ok());
	CHECK(doc.project().dimensions.empty());
	REQUIRE(editor.pointerPressed(above(3002, 4003)).ok());
	CHECK(doc.project().dimensions.empty());
	editor.pointerMoved(above(-500, 2000));
	REQUIRE(editor.dimensionPreview());
	CHECK(editor.dimensionPreview()->start == Point2D{0, 0});
	CHECK(editor.dimensionPreview()->end == Point2D{3000, 4000});
	REQUIRE(editor.pointerPressed(above(-500, 2000)).ok());
	REQUIRE(doc.project().dimensions.size() == 1);
	const Dimension original = doc.project().dimensions.front();
	CHECK(editor.mode() == EditMode::Select);
	CHECK(original.offsetMm == 1600);
	CHECK((original.end - original.start).lengthMm() == 5000);
	CHECK(editor.selectedDimension()->id == original.id);
	std::vector<std::string> changes;
	doc.subscribe([&changes](const ChangeSet& c) { changes = c.changedDimensions; });
	REQUIRE(editor.setDimensionOffset("-15.5 cm").ok());
	CHECK(changes == std::vector<std::string>{original.id});
	CHECK(editor.selectedDimension()->offsetMm == -155);
	REQUIRE(editor.setDimensionCoordinate("3001 mm", 2).ok());
	REQUIRE(editor.setDimensionText("Paso libre <A&B>").ok());
	const auto plan = drawing::buildPlanDrawing(doc.project(), original.level);
	CHECK(hasFigure(plan, "Paso libre <A&B>"));
	drawing::PlanOverlay selection;
	selection.selectedDimension = original.id;
	const auto selected = drawing::buildPlanDrawing(doc.project(), original.level, selection);
	CHECK(std::any_of(selected.labels.begin(), selected.labels.end(), [](const auto& label) { return label.ink == drawing::Ink::DimensionSelected; }));
	CHECK(drawing::writeSvg(plan).find("Paso libre &lt;A&amp;B&gt;") != std::string::npos);
	const auto saved = readProjectJson(writeProjectJson(doc.project()));
	REQUIRE(saved.ok());
	CHECK(saved.value().dimensions == doc.project().dimensions);
	REQUIRE(editor.deleteSelection().ok());
	CHECK(doc.project().dimensions.empty());
	REQUIRE(editor.undo().ok());
	CHECK(doc.project().dimensions.size() == 1);
	REQUIRE(editor.redo().ok());
	CHECK(doc.project().dimensions.empty());
	CHECK(doc.project().walls.empty());
}

TEST_CASE("cancel, invalid endpoints and off-plane input do not create measurements or damage undo history")
{
	Document doc{makeSequentialUuidSource()};
	EditController editor{doc, {}};
	dimensions(editor);
	editor.setMode(EditMode::Dimension);
	REQUIRE(editor.pointerPressed(above(0, 0)).ok());
	CHECK_FALSE(editor.pointerPressed(above(0, 0)).ok());
	editor.cancel();
	CHECK(editor.dimensionPointCount() == 0);
	CHECK_FALSE(doc.canUndo());
	REQUIRE(editor.pointerPressed(above(0, 0)).ok());
	REQUIRE(editor.pointerPressed(above(1000, 0)).ok());
	editor.cancel();
	CHECK(doc.project().dimensions.empty());
	editor.setDimensionsVisible(false);
	CHECK_FALSE(editor.pointerPressed(above(1000, 0)).ok());
	Dimension invalid{"manual", doc.project().levels.front().id, {0, 0}, {0, 0}};
	CHECK_FALSE(doc.execute(SetDimensionCommand{invalid}).ok());
	invalid.end = {1000, 0};
	REQUIRE(doc.execute(SetDimensionCommand{invalid}).ok());
	dimensions(editor);
	REQUIRE(editor.selectDimension("manual").ok());
	CHECK_FALSE(editor.setDimensionCoordinate("0", 2).ok());
	CHECK_FALSE(editor.setDimensionCoordinate("12", 7).ok());
	CHECK_FALSE(editor.setDimensionOffset("1000000001 mm").ok());
	CHECK_FALSE(editor.setDimensionText("line\nbreak").ok());
	CHECK(doc.project().dimensions.front() == invalid);
}

TEST_CASE("automatic dimensions can be selected, dragged, hidden and restored while tracking wall edits")
{
	Document doc{makeSequentialUuidSource()};
	const auto wall = doc.addWall({0, 0}, {4000, 0}, 120, 2800).value();
	EditController editor{doc, {}};
	dimensions(editor);
	const LevelId level = editor.activeLevel();
	const auto initial = drawing::buildPlanDrawing(doc.project(), level).dimensions.front();
	const auto [a, b] = dimensionLine(initial);
	const Ray3 click = above((a.x_mm + b.x_mm) / 2, a.y_mm);
	REQUIRE(editor.pointerPressed(click).ok());
	REQUIRE(editor.selectedDimension());
	CHECK(editor.selection().isNil());
	editor.pointerMoved(above(2000, a.y_mm + 300));
	REQUIRE(editor.pointerReleased(click).ok());
	CHECK(editor.selectedDimension()->offsetMm == initial.offsetMm + 300);
	REQUIRE(editor.setDimensionText("Exterior").ok());
	CHECK_FALSE(editor.setDimensionCoordinate("2000", 2).ok());
	REQUIRE(doc.execute(ChangeWallLengthCommand{wall, 5000}).ok());
	CHECK(editor.selectedDimension()->end == Point2D{5000, 0});
	CHECK(editor.selectedDimension()->text == "Exterior");
	REQUIRE(editor.setDimensionText("").ok());
	CHECK(hasFigure(drawing::buildPlanDrawing(doc.project(), level), "5.00"));
	REQUIRE(editor.deleteSelection().ok());
	CHECK(drawing::buildPlanDrawing(doc.project(), level).dimensions.empty());
	CHECK(doc.project().walls.front().lengthMm() == 5000);
	const auto persisted = readProjectJson(writeProjectJson(doc.project()));
	REQUIRE(persisted.ok());
	CHECK(drawing::buildPlanDrawing(persisted.value(), level).dimensions.empty());
	REQUIRE(editor.restoreAutomaticDimensions().ok());
	CHECK(drawing::buildPlanDrawing(doc.project(), level).dimensions.size() == 1);
	REQUIRE(editor.undo().ok());
	CHECK(drawing::buildPlanDrawing(doc.project(), level).dimensions.empty());
	REQUIRE(editor.redo().ok());
	CHECK(doc.project().dimensions.empty());
}

TEST_CASE("opening-chain overrides belong to stable boundaries and never hide newly inserted spans")
{
	Document doc{makeSequentialUuidSource()};
	const auto wall = doc.addWall({0, 0}, {10000, 0}, 120, 2800).value();
	Opening door;
	door.kind = OpeningKind::Door; door.offsetMm = 5000; door.widthMm = 900; door.heightMm = 2100;
	const auto opening = doc.addOpening(wall, door).value();
	EditController editor{doc, {}}; dimensions(editor);
	auto plan = drawing::buildPlanDrawing(doc.project(), editor.activeLevel());
	const auto span = *std::find_if(plan.dimensions.begin(), plan.dimensions.end(), [](const auto& d) { return d.start.x_mm == 5000 && d.end.x_mm == 5900; });
	REQUIRE(editor.selectDimension(span.id).ok());
	REQUIRE(editor.deleteSelection().ok());
	door = *doc.project().findWall(wall)->findOpening(opening); door.offsetMm = 6000;
	REQUIRE(doc.execute(ChangeOpeningCommand{wall, door}).ok());
	Opening second = door; second.id = {}; second.offsetMm = 1000;
	REQUIRE(doc.addOpening(wall, second).ok());
	plan = drawing::buildPlanDrawing(doc.project(), editor.activeLevel());
	CHECK(plan.dimensions.size() == 5); // five chain spans + total, minus hidden door span
	CHECK(std::none_of(plan.dimensions.begin(), plan.dimensions.end(), [&span](const auto& d) { return d.id == span.id; }));
	CHECK(std::any_of(plan.dimensions.begin(), plan.dimensions.end(), [](const auto& d) { return d.start.x_mm == 1000 && d.end.x_mm == 1900; }));
}

TEST_CASE("measurements respect levels and level deletion is reversible including hidden automatic overrides")
{
	Document doc{makeSequentialUuidSource()};
	const auto ground = doc.project().levels.front().id;
	const auto upper = doc.addLevel().value();
	Dimension d{"ground", ground, {0, 0}, {1, 0}, -100};
	REQUIRE(doc.execute(SetDimensionCommand{d}).ok());
	CHECK(hasFigure(drawing::buildPlanDrawing(doc.project(), ground), "0.001"));
	d.id = "upper"; d.level = upper;
	REQUIRE(doc.execute(SetDimensionCommand{d}).ok());
	CHECK(drawing::buildPlanDrawing(doc.project(), upper).dimensions.size() == 1);
	d.id = "auto/removed-wall/overall"; d.automatic = true; d.hidden = true;
	REQUIRE(doc.execute(SetDimensionCommand{d}).ok());
	REQUIRE(doc.execute(DeleteLevelCommand{upper}).ok());
	CHECK(doc.project().dimensions.size() == 1);
	REQUIRE(doc.undo().ok());
	CHECK(doc.project().dimensions.size() == 3);
	EditController editor{doc, {}}; dimensions(editor);
	REQUIRE(editor.setActiveLevel(upper).ok());
	REQUIRE(editor.selectDimension("upper").ok());
	REQUIRE(editor.setActiveLevel(ground).ok());
	CHECK_FALSE(editor.selectedDimension());
	REQUIRE(editor.restoreAutomaticDimensions().ok());
	CHECK(doc.project().dimensions.size() == 3); // only ground defaults restored
	REQUIRE(editor.setActiveLevel(upper).ok());
	REQUIRE(editor.restoreAutomaticDimensions().ok());
	CHECK(doc.project().dimensions.size() == 2);
}

TEST_CASE("dimension JSON rejects malformed fields duplicates and accepts legacy projects without annotations")
{
	Document doc{makeSequentialUuidSource()};
	Dimension d{"test", doc.project().levels.front().id, {0, 0}, {1000, 0}, 300};
	REQUIRE(doc.execute(SetDimensionCommand{d}).ok());
	const std::string json = writeProjectJson(doc.project());
	for (const auto& pair : {std::pair{std::string("\"offsetMm\": 300"), std::string("\"offsetMm\": 300.5")},
		std::pair{std::string("\"hidden\": false"), std::string("\"hidden\": 1")},
		std::pair{std::string("\"automatic\": false"), std::string("\"automatic\": true")},
		std::pair{std::string("\"text\": \"\""), std::string("\"text\": 42")}})
	{
		std::string malformed = json;
		malformed.replace(malformed.find(pair.first), pair.first.size(), pair.second);
		CHECK_FALSE(readProjectJson(malformed).ok());
	}
	Project duplicate = doc.project(); duplicate.dimensions.push_back(d);
	CHECK_FALSE(doc.replaceProject(duplicate).ok());
	duplicate.dimensions.clear();
	std::string legacy = writeProjectJson(duplicate);
	const std::string version = "\"schemaVersion\": 13";
	legacy.replace(legacy.find(version), version.size(), "\"schemaVersion\": 12");
	REQUIRE(readProjectJson(legacy).ok());
	CHECK(readProjectJson(legacy).value().dimensions.empty());
}
