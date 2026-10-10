#include <doctest/doctest.h>
#include "F101/Cad/DimensionEditor.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Drawing/SvgWriter.h"

#include <algorithm>
#include <cmath>

using namespace f101;
using namespace f101::cad;
namespace
{
	Ray3 above(double x, double y, double z = 10000) { return {x, y, z, 0, 0, -1}; }
	PlanPoint gripOf(const Dimension& d, int index)
	{
		std::array<PlanPoint, 4> handles;
		dimensionHandles(d, handles);
		return handles[index];
	}
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

TEST_CASE("manual measurements use exact clicked points and persist reversible precise edits")
{
	Document doc{makeSequentialUuidSource()};
	EditController editor{doc, {}};
	dimensions(editor);
	editor.setMode(EditMode::Dimension);
	REQUIRE(editor.pointerPressed(above(1, 2)).ok());
	CHECK(doc.project().dimensions.empty());
	REQUIRE(editor.pointerPressed(above(3001, 4002)).ok());
	CHECK(doc.project().dimensions.empty());
	editor.pointerMoved(above(-499, 2002));
	REQUIRE(editor.dimensionPreview());
	CHECK(editor.dimensionPreview()->start == Point2D{1, 2});
	CHECK(editor.dimensionPreview()->end == Point2D{3001, 4002});
	REQUIRE(editor.pointerPressed(above(-499, 2002)).ok());
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
	CHECK(hasFigure(plan, "Paso libre <A&B> (5)")); // a custom text keeps the real measurement beside it
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

TEST_CASE("manual dimensions near wall faces and corners bypass construction snapping at millimetre resolution")
{
	Document doc{makeSequentialUuidSource()};
	REQUIRE(doc.addWall({0, 0}, {4000, 0}, 120, 2800).ok());
	EditController editor{doc, {}};
	dimensions(editor);
	editor.setMode(EditMode::Dimension);
	REQUIRE(editor.pointerPressed(above(61.4, 60.5)).ok());
	editor.pointerMoved(above(3938.4, 60.5));
	REQUIRE(editor.dimensionPreview());
	CHECK(editor.dimensionPreview()->start == Point2D{61, 61});
	CHECK(editor.dimensionPreview()->end == Point2D{3938, 61});
	REQUIRE(editor.pointerPressed(above(3938.4, 60.5)).ok());
	REQUIRE(editor.pointerPressed(above(2000, 500)).ok());
	const Dimension measurement = doc.project().dimensions.front();
	CHECK(measurement.start == Point2D{61, 61});
	CHECK(measurement.end == Point2D{3938, 61});
	CHECK(measurement.offsetMm == 439);
	CHECK(hasFigure(drawing::buildPlanDrawing(doc.project(), editor.activeLevel()), "3.877"));
	const auto reopened = readProjectJson(writeProjectJson(doc.project()));
	REQUIRE(reopened.ok());
	CHECK(reopened.value().dimensions.front() == measurement);
	REQUIRE(editor.undo().ok());
	CHECK(doc.project().dimensions.empty());
	REQUIRE(editor.redo().ok());
	CHECK(doc.project().dimensions.front() == measurement);
	CHECK(doc.project().walls.front().start == Point2D{0, 0});
	CHECK(doc.project().walls.front().end == Point2D{4000, 0});
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

TEST_CASE("all four manual dimension grips resize reversibly and preview without changing the document")
{
	for (int handle = 0; handle < 4; ++handle)
	{
		Document doc{makeSequentialUuidSource()};
		Dimension original{"manual", doc.project().levels.front().id, {1000, 1000}, {4000, 1000}, 600};
		REQUIRE(doc.execute(SetDimensionCommand{original}).ok());
		EditController editor{doc, {}};
		dimensions(editor);
		REQUIRE(editor.selectDimension(original.id).ok());
		const auto grip = gripOf(original, handle);
		const bool start = handle % 2 == 0;
		const double dx = start ? -300.0 : 300.0;
		REQUIRE(editor.pointerPressed(above(grip.x_mm + 3, grip.y_mm + 2)).ok());
		editor.pointerMoved(above(grip.x_mm + 3 + dx, grip.y_mm + 52));
		REQUIRE(editor.dimensionPreview());
		const auto preview = *editor.dimensionPreview();
		CHECK(doc.project().dimensions.front() == original);
		CHECK((start ? preview.end : preview.start) == (start ? original.end : original.start));
		CHECK((start ? preview.start : preview.end) == Point2D{start ? 700 : 4300, handle < 2 ? 1050 : 1000});
		CHECK(preview.offsetMm == 600);
		drawing::PlanOverlay overlay;
		overlay.selectedDimension = original.id;
		overlay.previewDimension = preview;
		const auto shown = drawing::buildPlanDrawing(doc.project(), editor.activeLevel(), overlay);
		CHECK(std::count_if(shown.fills.begin(), shown.fills.end(), [](const auto& fill) { return fill.ink == drawing::Ink::DimensionSelected; }) == 4);
		const auto exported = drawing::buildPlanDrawing(doc.project(), editor.activeLevel());
		CHECK(std::none_of(exported.fills.begin(), exported.fills.end(), [](const auto& fill) { return fill.ink == drawing::Ink::DimensionSelected; }));
		REQUIRE(editor.pointerReleased(above(grip.x_mm + dx, grip.y_mm + 50)).ok());
		CHECK(doc.project().dimensions.front() == preview);
		REQUIRE(editor.undo().ok());
		CHECK(doc.project().dimensions.front() == original);
		REQUIRE(editor.redo().ok());
		CHECK(doc.project().dimensions.front() == preview);
		REQUIRE(editor.selectDimension(original.id).ok());
		// Shrink the resized measurement from its offset corner, also for oblique axes.
		const auto shorteningGrip = gripOf(preview, 3);
		const Vector2D shorteningAxis = preview.end - preview.start;
		const double targetX = shorteningGrip.x_mm - 200.0 * static_cast<double>(shorteningAxis.dx_mm) / shorteningAxis.lengthMm();
		const double targetY = shorteningGrip.y_mm - 200.0 * static_cast<double>(shorteningAxis.dy_mm) / shorteningAxis.lengthMm();
		REQUIRE(editor.pointerPressed(above(shorteningGrip.x_mm, shorteningGrip.y_mm)).ok());
		editor.pointerMoved(above(targetX, targetY));
		REQUIRE(editor.pointerReleased(above(targetX, targetY)).ok());
		CHECK((editor.selectedDimension()->end - editor.selectedDimension()->start).lengthMm() < shorteningAxis.lengthMm());
		CHECK(editor.selectedDimension()->start == preview.start);
		CHECK(editor.selectedDimension()->offsetMm == preview.offsetMm);
		REQUIRE(editor.undo().ok());
		CHECK(doc.project().dimensions.front() == preview);
		REQUIRE(editor.selectDimension(original.id).ok());
		const auto now = gripOf(preview, handle);
		REQUIRE(editor.pointerPressed(above(now.x_mm, now.y_mm)).ok());
		editor.pointerMoved(above(now.x_mm + 150, now.y_mm + 250));
		editor.cancel();
		CHECK_FALSE(editor.isDragging());
		CHECK(doc.project().dimensions.front() == preview);
	}
}

TEST_CASE("resizing an automatic dimension converts it atomically while preserving overrides and wall geometry")
{
	for (const bool customized : {false, true})
	{
		Document doc{makeSequentialUuidSource()};
		REQUIRE(doc.addWall({0, 0}, {4000, 3000}, 120, 2800).ok());
		EditController editor{doc, {}};
		dimensions(editor);
		const auto automatic = drawing::buildPlanDrawing(doc.project(), editor.activeLevel()).dimensions.front();
		REQUIRE(editor.selectDimension(automatic.id).ok());
		if (customized)
		{
			REQUIRE(editor.setDimensionText("Paso libre").ok());
			REQUIRE(editor.setDimensionOffset("-400 mm").ok());
		}
		const auto original = *editor.selectedDimension();
		const auto snapshot = writeProjectJson(doc.project());
		const auto grip = gripOf(original, 3);
		REQUIRE(editor.pointerPressed(above(grip.x_mm, grip.y_mm)).ok());
		REQUIRE(editor.pointerReleased(above(grip.x_mm, grip.y_mm)).ok());
		CHECK(writeProjectJson(doc.project()) == snapshot);
		REQUIRE(editor.pointerPressed(above(grip.x_mm, grip.y_mm)).ok());
		editor.pointerMoved(above(grip.x_mm + 240, grip.y_mm + 180));
		REQUIRE(editor.dimensionPreview());
		CHECK(editor.dimensionPreview()->end == Point2D{4240, 3180});
		CHECK(writeProjectJson(doc.project()) == snapshot);
		REQUIRE(editor.pointerReleased(above(grip.x_mm + 240, grip.y_mm + 180)).ok());
		REQUIRE(editor.selectedDimension());
		const auto manual = *editor.selectedDimension();
		CHECK_FALSE(manual.automatic);
		CHECK(manual.start == original.start);
		CHECK(manual.end == Point2D{4240, 3180});
		CHECK(manual.text == original.text);
		CHECK(manual.offsetMm == original.offsetMm);
		CHECK(doc.project().findDimension(original.id)->hidden);
		CHECK(drawing::buildPlanDrawing(doc.project(), editor.activeLevel()).dimensions.size() == 1);
		CHECK(doc.project().walls.front().end == Point2D{4000, 3000});
		const auto resized = writeProjectJson(doc.project());
		REQUIRE(editor.undo().ok());
		CHECK(writeProjectJson(doc.project()) == snapshot);
		REQUIRE(editor.redo().ok());
		CHECK(writeProjectJson(doc.project()) == resized);
		const auto saved = readProjectJson(resized);
		REQUIRE(saved.ok());
		CHECK(saved.value().dimensions == doc.project().dimensions);
		CHECK(drawing::buildPlanDrawing(saved.value(), editor.activeLevel()).dimensions.front().id == manual.id);
		REQUIRE(editor.restoreAutomaticDimensions().ok());
		CHECK(drawing::buildPlanDrawing(doc.project(), editor.activeLevel()).dimensions.size() == 2);
	}
}

TEST_CASE("collapsed grips and invalid automatic conversions leave a valid measurement and history")
{
	Document doc{makeSequentialUuidSource()};
	Dimension manual{"manual", doc.project().levels.front().id, {1000, 1000}, {2000, 1000}, 600};
	REQUIRE(doc.execute(SetDimensionCommand{manual}).ok());
	CHECK_FALSE(doc.execute(MakeDimensionManualCommand{manual}).ok());
	EditController editor{doc, {}};
	dimensions(editor);
	REQUIRE(editor.selectDimension(manual.id).ok());
	REQUIRE(editor.pointerPressed(above(2000, 1600)).ok());
	editor.pointerMoved(above(1000, 1600));
	CHECK(editor.dimensionPreview()->end == manual.end);
	editor.pointerMoved(above(500, 1600));
	CHECK(editor.dimensionPreview()->end == manual.end);
	editor.setDimensionsVisible(false);
	CHECK_FALSE(editor.isDragging());
	CHECK(doc.project().dimensions.front() == manual);
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
	CHECK(hasFigure(drawing::buildPlanDrawing(doc.project(), level), "Exterior (5.00)")); // follows the model
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

TEST_CASE("short dimensions place their figure beyond the ticks without changing measurement and remain pickable")
{
	for (const Point2D end : {Point2D{80, 0}, Point2D{0, 80}, Point2D{48, 64}, Point2D{-80, 0}})
	{
		Document doc{makeSequentialUuidSource()};
		EditController editor{doc, {}};
		dimensions(editor);
		Dimension d{"short", editor.activeLevel(), {0, 0}, end, 600};
		REQUIRE(doc.execute(SetDimensionCommand{d}).ok());
		const std::string saved = writeProjectJson(doc.project());
		const auto plan = drawing::buildPlanDrawing(doc.project(), editor.activeLevel());
		REQUIRE(plan.dimensions.size() == 1);
		CHECK(plan.dimensions.front() == d);
		CHECK(dimensionLabelOutside(d));
		const PlanPoint at = dimensionLabelPoint(d);
		const Vector2D axis = end - d.start;
		const double along = (at.x_mm * static_cast<double>(axis.dx_mm) + at.y_mm * static_cast<double>(axis.dy_mm)) / axis.lengthMm();
		CHECK(along - dimensionLabelWidthMm(d) / 2.0 >= axis.lengthMm() + 99.0);
		REQUIRE(plan.labels.size() == 1);
		CHECK(plan.labels.front().at.x_mm == doctest::Approx(at.x_mm));
		CHECK(plan.labels.front().at.y_mm == doctest::Approx(at.y_mm));
		CHECK(plan.min.x_mm <= at.x_mm);
		CHECK(plan.max.x_mm >= at.x_mm);
		CHECK(drawing::writeSvg(plan).find("<rect x=") != std::string::npos);
		REQUIRE(editor.pointerPressed(above(at.x_mm, at.y_mm)).ok());
		REQUIRE(editor.pointerReleased(above(at.x_mm, at.y_mm)).ok());
		REQUIRE(editor.selectedDimension());
		CHECK(editor.selectedDimension()->id == d.id);
		CHECK(writeProjectJson(doc.project()) == saved);
	}
}

TEST_CASE("short automatic and custom figures retain identity and readable tick spacing")
{
	Document doc{makeSequentialUuidSource()};
	const auto wall = doc.addWall({0,0}, {0,80}, 120, 2800).value();
	const LevelId level = doc.project().levels.front().id;
	const auto plan = drawing::buildPlanDrawing(doc.project(), level);
	REQUIRE(plan.dimensions.size() == 1);
	const Dimension d = plan.dimensions.front();
	CHECK(d.automatic);
	CHECK(d.id == "auto/" + toString(wall) + "/overall");
	CHECK((d.end - d.start).lengthMm() == 80);
	CHECK(dimensionLabelOutside(d));
	const auto ticks = std::count_if(plan.strokes.begin(), plan.strokes.end(), [](const auto& stroke)
	{
		if (stroke.ink != drawing::Ink::Dimension || stroke.widthMm != 15.0 || stroke.points.size() != 2) return false;
		return std::hypot(stroke.points[1].x_mm - stroke.points[0].x_mm, stroke.points[1].y_mm - stroke.points[0].y_mm) < 80.0;
	});
	CHECK(ticks == 2);
	Dimension custom = d;
	custom.text = "Límite baño";
	REQUIRE(doc.execute(SetDimensionCommand{custom}).ok());
	const auto changed = drawing::buildPlanDrawing(doc.project(), level);
	CHECK(hasFigure(changed, custom.text + " (0.08)"));
	CHECK(changed.dimensions.front().start == d.start);
	CHECK(changed.dimensions.front().end == d.end);
	// The text, the brackets and the real figure, counted in UTF-8 glyphs rather than bytes.
	CHECK(dimensionLabelWidthMm(custom) == doctest::Approx(18 * 150.0 * 0.85));
	Dimension longDimension = d;
	longDimension.end = {0, 2000};
	CHECK_FALSE(dimensionLabelOutside(longDimension));
	CHECK(dimensionLabelPoint(longDimension).y_mm == 1000);
}

TEST_CASE("short opening chain callouts use a separate row from neighbouring figures")
{
	Document doc{makeSequentialUuidSource()};
	const auto wall = doc.addWall({0,0}, {4000,0}, 120, 2800).value();
	REQUIRE(doc.addOpening(wall, Opening{OpeningId{}, OpeningKind::Door, 80, 900, 2100, 0}).ok());
	const auto plan = drawing::buildPlanDrawing(doc.project(), doc.project().levels.front().id);
	const auto shortDim = std::find_if(plan.dimensions.begin(), plan.dimensions.end(), [](const Dimension& d) { return (d.end - d.start).lengthMm() == 80; });
	const auto openingDim = std::find_if(plan.dimensions.begin(), plan.dimensions.end(), [](const Dimension& d) { return (d.end - d.start).lengthMm() == 900; });
	REQUIRE(shortDim != plan.dimensions.end());
	REQUIRE(openingDim != plan.dimensions.end());
	CHECK(std::fabs(dimensionLabelPoint(*shortDim).y_mm - dimensionLabelPoint(*openingDim).y_mm) >= 300.0);
}

TEST_CASE("the dimension editor places, drags and hides dimensions on its own")
{
	Document doc{makeSequentialUuidSource()};
	REQUIRE(doc.addWall({0, 0}, {4000, 0}, 120, 2800).ok());
	const LevelId level = doc.project().levels.front().id;
	DimensionEditor editor{doc};
	const auto resolved = [](const Project& p, const LevelId& l) { return drawing::buildPlanDrawing(p, l).dimensions; };
	editor.setProvider(resolved);
	editor.setVisible(true);

	// Two measured points, then the line: the new dimension is created and selected.
	REQUIRE(editor.place({0, -3000}, level).ok());
	REQUIRE(editor.place({2000, -3000}, level).ok());
	CHECK(editor.draftPointCount() == 2);
	CHECK(editor.preview(PlanPoint{1000, -3500}, level, true)->offsetMm == -500);
	const auto placed = editor.place({1000, -3500}, level);
	REQUIRE(placed.ok());
	REQUIRE(placed.value().has_value());
	CHECK_FALSE(editor.hasDraft());
	const std::string manual = *placed.value();
	CHECK(editor.selected(level)->id == manual);
	CHECK(doc.project().findDimension(manual)->offsetMm == -500);

	// Its line moves across its axis by as much as the pointer, in one undoable change.
	const auto grab = editor.hit({1000, -3500}, level);
	REQUIRE(grab.has_value());
	CHECK(grab->grip == DimensionEditor::Grab::Line);
	editor.startDrag(*grab, {1000, -3500});
	editor.moveDrag({1000, -3800});
	CHECK(editor.preview(std::nullopt, level, false)->offsetMm == -800);
	REQUIRE(editor.release().ok());
	CHECK_FALSE(editor.dragging());
	CHECK(doc.project().findDimension(manual)->offsetMm == -800);

	// Deleting an automatic dimension hides it; it stays stored so the plan can leave it out.
	const std::vector<Dimension> shown = resolved(doc.project(), level);
	const auto first = std::find_if(shown.begin(), shown.end(), [](const Dimension& d) { return d.automatic; });
	REQUIRE(first != shown.end());
	REQUIRE(editor.select(first->id, level).ok());
	const auto hidden = editor.deleteSelected(level);
	REQUIRE(hidden.has_value());
	REQUIRE(hidden->ok());
	CHECK(doc.project().findDimension(first->id)->hidden);
	CHECK_FALSE(editor.selected(level).has_value());
	CHECK_FALSE(editor.select(first->id, level).ok());

	// Hiding the plan forgets the selection.
	REQUIRE(editor.select(manual, level).ok());
	editor.setVisible(false);
	CHECK_FALSE(editor.selected(level).has_value());
}

TEST_CASE("a custom dimension text shows the real measurement unless it already is that measurement")
{
	const LevelId level{Uuid{0, 1}};
	Dimension automatic{"auto/x/overall", level, {0, 0}, {3550, 0}};
	automatic.automatic = true;
	CHECK(dimensionFigure(automatic, ',') == "3,55");
	CHECK(dimensionLabel(automatic, ',') == "3,55");
	automatic.text = "eje";
	CHECK(dimensionLabel(automatic, ',') == "eje (3,55)");
	// The figure as typed is written once, whichever decimal separator the plan uses.
	automatic.text = "3,55";
	CHECK(dimensionLabel(automatic, ',') == "3,55");
	CHECK(dimensionLabel(automatic, '.') == "3,55");
	automatic.text = "3.50";
	CHECK(dimensionLabel(automatic, ',') == "3.50 (3,55)");

	Dimension manual{"manual", level, {0, 0}, {3555, 0}};
	CHECK(dimensionFigure(manual, '.') == "3.555"); // manual figures keep the millimetre
	manual.text = "Paso";
	CHECK(dimensionLabel(manual, ',') == "Paso (3,555)");
	CHECK(dimensionLabelWidthMm(manual) == doctest::Approx(12 * 150.0 * 0.85));
}
