#include "doctest.h"

#include "F101/Drawing/PlanDrawing.h"
#include "F101/Drawing/SvgWriter.h"
#include "F101/Geometry/PolygonGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::cad;
using namespace f101::drawing;

// Plan drawings and their SVG (ADR-0015).
namespace
{
	// A 5 x 4 m room drawn counter-clockwise with 200 mm walls (the room is on each wall's left):
	// south: a window 1600 at 700 (sill 900: cut) and a door 900 at 3300 hinged at its end, opening into the room;
	// east: a high window (sill 1500: above the cut); north: a sliding door 800 at 1200 into a pocket at its end; west: plain.
	Project room()
	{
		Project project;
		Wall south{WallId{Uuid{0, 1}}, {0, 0}, {5000, 0}, 200, 2800};
		south.openings.push_back(Opening{OpeningId{Uuid{0, 11}}, OpeningKind::Window, 700, 1600, 1300, 900, {}, OpeningOperation::DoubleHinged});
		south.openings.push_back(Opening{OpeningId{Uuid{0, 12}}, OpeningKind::Door, 3300, 900, 2100, 0, {}, OpeningOperation::Hinged,
			OpeningEnd::End, WallSide::Left, 0});
		Wall east{WallId{Uuid{0, 2}}, {5000, 0}, {5000, 4000}, 200, 2800};
		east.openings.push_back(Opening{OpeningId{Uuid{0, 13}}, OpeningKind::Window, 1400, 1200, 800, 1500});
		Wall north{WallId{Uuid{0, 3}}, {5000, 4000}, {0, 4000}, 200, 2800};
		north.openings.push_back(Opening{OpeningId{Uuid{0, 14}}, OpeningKind::Door, 1200, 800, 2100, 0, {}, OpeningOperation::Sliding,
			OpeningEnd::End, WallSide::Left, 0});
		Wall west{WallId{Uuid{0, 4}}, {0, 4000}, {0, 0}, 200, 2800};
		project.walls = {south, east, north, west};
		return project;
	}

	bool covered(const PlanDrawing& drawing, PlanPoint point, Ink ink = Ink::Cut)
	{
		return std::any_of(drawing.fills.begin(), drawing.fills.end(),
			[&](const Fill& fill) { return fill.ink == ink && geometry::containsPoint(fill.outline, point); });
	}

	std::size_t countFills(const PlanDrawing& drawing, Ink ink)
	{
		return static_cast<std::size_t>(std::count_if(drawing.fills.begin(), drawing.fills.end(), [ink](const Fill& f) { return f.ink == ink; }));
	}

	bool near(PlanPoint a, PlanPoint b)
	{
		return std::fabs(a.x_mm - b.x_mm) < 0.01 && std::fabs(a.y_mm - b.y_mm) < 0.01;
	}

	const Stroke* findStroke(const PlanDrawing& drawing, Ink ink, PlanPoint first, PlanPoint last)
	{
		for (const Stroke& stroke : drawing.strokes)
		{
			if (stroke.ink == ink && near(stroke.points.front(), first) && near(stroke.points.back(), last))
			{
				return &stroke;
			}
		}
		return nullptr;
	}

	const Label* findLabel(const PlanDrawing& drawing, const std::string& text, PlanPoint at)
	{
		for (const Label& label : drawing.labels)
		{
			if (label.text == text && near(label.at, at))
			{
				return &label;
			}
		}
		return nullptr;
	}

	// Walls by id, so a test decides which of two opposite faces is named first (the lower id).
	Wall wallOf(std::uint64_t id, Point2D start, Point2D end, std::int64_t thickness = 120)
	{
		return Wall{WallId{Uuid{0, id}}, start, end, thickness, 2800};
	}

	std::vector<Dimension> interior(const PlanDrawing& drawing)
	{
		std::vector<Dimension> found;
		std::copy_if(drawing.dimensions.begin(), drawing.dimensions.end(), std::back_inserter(found),
			[](const Dimension& d) { return d.id.find("/interior-") != std::string::npos; });
		return found;
	}

	const Dimension* findDimension(const PlanDrawing& drawing, const std::string& id)
	{
		const auto found = std::find_if(drawing.dimensions.begin(), drawing.dimensions.end(), [&id](const Dimension& d) { return d.id == id; });
		return found == drawing.dimensions.end() ? nullptr : &*found;
	}

	std::size_t occurrences(const std::string& text, const std::string& what)
	{
		std::size_t count = 0;
		for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size()))
		{
			++count;
		}
		return count;
	}
}

TEST_CASE("figures are metres with two decimals, rounded half away from zero, in the drawing's separator")
{
	CHECK(formatMeters(3550.0, '.') == "3.55");
	CHECK(formatMeters(3550.0, ',') == "3,55");
	CHECK(formatMeters(5.0, '.') == "0.01");
	CHECK(formatMeters(-70.0, '.') == "-0.07");
	CHECK(formatMeters(1234567.0, ',') == "1234,57");
	CHECK(formatMeters(0.0, '.') == "0.00");
}

TEST_CASE("walls are cut solid, with gaps where the plan cuts doors and windows")
{
	const PlanDrawing drawing = buildPlanDrawing(room(), LevelId());
	CHECK(countFills(drawing, Ink::Cut) == 3 + 1 + 2 + 1); // south in three pieces, the east window is above the cut
	CHECK(covered(drawing, {2800.0, 0.0}));
	CHECK_FALSE(covered(drawing, {1500.0, 0.0}));  // the window
	CHECK_FALSE(covered(drawing, {3750.0, 0.0}));  // the door
	CHECK(covered(drawing, {5000.0, 2000.0}));     // the high window is not cut
	CHECK_FALSE(covered(drawing, {3400.0, 4000.0})); // the sliding door's gap
	CHECK(covered(drawing, {2600.0, 4000.0}));     // its pocket is inside the wall
	CHECK(covered(drawing, {-50.0, -90.0}));       // the mitred corners are part of the cut
	for (const Fill& fill : drawing.fills)
	{
		CHECK(geometry::signedArea(fill.outline) > 0.0);
		CHECK(fill.triangles.size() % 3 == 0);
		CHECK_FALSE(fill.triangles.empty());
	}
}

TEST_CASE("windows show both faces, their frame and glass; doors open at 90 degrees with the arc of their free edge")
{
	const PlanDrawing drawing = buildPlanDrawing(room(), LevelId());
	CHECK(findStroke(drawing, Ink::Carpentry, {700.0, 0.0}, {2300.0, 0.0}) != nullptr);   // the glass
	CHECK(findStroke(drawing, Ink::Carpentry, {700.0, -100.0}, {2300.0, -100.0}) != nullptr); // outer face
	CHECK(findStroke(drawing, Ink::Carpentry, {700.0, 100.0}, {2300.0, 100.0}) != nullptr);   // inner face

	// The door: hinged at its end (x = 4150 inside the 50 mm frame), its 800 mm leaf standing into the room.
	const Stroke* leaf = findStroke(drawing, Ink::Carpentry, {4150.0, 100.0}, {4110.0, 100.0});
	REQUIRE(leaf != nullptr);
	CHECK(leaf->closed);
	CHECK(near(leaf->points[1], {4150.0, 900.0}));
	const Stroke* swing = findStroke(drawing, Ink::Swing, {3350.0, 100.0}, {4150.0, 900.0});
	REQUIRE(swing != nullptr);
	CHECK(swing->points.size() == 17);

	// The sliding door: its leaf closed in the gap, its pocket dashed along the axis, at its end.
	const Stroke* pocket = findStroke(drawing, Ink::Hidden, {3000.0, 4000.0}, {2200.0, 4000.0});
	REQUIRE(pocket != nullptr);
	CHECK(pocket->dashed);
	CHECK(std::count_if(drawing.strokes.begin(), drawing.strokes.end(), [](const Stroke& s) { return s.ink == Ink::Swing; }) == 1);
}

TEST_CASE("dimensions stand outside: a chain through the openings and the overall length, measured on the axes")
{
	const PlanDrawing drawing = buildPlanDrawing(room(), LevelId());
	// South: the room is on its left (north), so its dimensions go south, 600 and 1100 mm off its outer face.
	CHECK(findStroke(drawing, Ink::Dimension, {0.0, -700.0}, {5000.0, -700.0}) != nullptr);
	CHECK(findStroke(drawing, Ink::Dimension, {0.0, -1200.0}, {5000.0, -1200.0}) != nullptr);
	CHECK(findLabel(drawing, "0.70", {350.0, -835.0}) != nullptr);
	CHECK(findLabel(drawing, "1.60", {1500.0, -835.0}) != nullptr);
	CHECK(findLabel(drawing, "1.00", {2800.0, -835.0}) != nullptr);
	CHECK(findLabel(drawing, "0.90", {3750.0, -835.0}) != nullptr);
	CHECK(findLabel(drawing, "0.80", {4600.0, -835.0}) != nullptr);
	CHECK(findLabel(drawing, "5.00", {2500.0, -1335.0}) != nullptr);
	// West, without openings: just its length, on the chain line, read upwards.
	const Label* west = findLabel(drawing, "4.00", {-835.0, 2000.0});
	REQUIRE(west != nullptr);
	CHECK(west->angleDegrees == doctest::Approx(90.0));
	// Extension lines from 100 mm off the face to 100 mm past the line; a tick at each point.
	CHECK(findStroke(drawing, Ink::Dimension, {700.0, -200.0}, {700.0, -800.0}) != nullptr);

	DrawingTexts spanish;
	spanish.decimalSeparator = ',';
	CHECK(findLabel(buildPlanDrawing(room(), LevelId(), {}, spanish), "0,70", {350.0, -835.0}) != nullptr);
}

TEST_CASE("interior dimensions measure each room face to face, inside it, once per pair of opposite faces")
{
	const PlanDrawing drawing = buildPlanDrawing(room(), LevelId());
	const std::vector<Dimension> inside = interior(drawing);
	REQUIRE(inside.size() == 2); // the north and west faces repeat the south and east ones
	// South: from the west face to the east face (200 mm walls), 150 mm into the room; openings do not split it.
	const Dimension* south = findDimension(drawing, "auto/" + toString(WallId{Uuid{0, 1}}) + "/interior-left-0");
	REQUIRE(south != nullptr);
	CHECK(south->automatic);
	CHECK(south->start == Point2D{100, 100});
	CHECK(south->end == Point2D{4900, 100});
	CHECK(south->offsetMm == 150);
	CHECK(findStroke(drawing, Ink::Dimension, {100.0, 250.0}, {4900.0, 250.0}) != nullptr);
	CHECK(findStroke(drawing, Ink::Dimension, {100.0, 200.0}, {100.0, 350.0}) != nullptr); // extension line
	CHECK(findLabel(drawing, "4.80", {2500.0, 385.0}) != nullptr);
	// East: from the south face to the north one, read upwards.
	const Dimension* east = findDimension(drawing, "auto/" + toString(WallId{Uuid{0, 2}}) + "/interior-left-0");
	REQUIRE(east != nullptr);
	CHECK(east->start == Point2D{4900, 100});
	CHECK(east->end == Point2D{4900, 3900});
	const Label* figure = findLabel(drawing, "3.80", {4615.0, 2000.0});
	REQUIRE(figure != nullptr);
	CHECK(figure->angleDegrees == doctest::Approx(90.0));
	// The room keeps its name and area at its label point: no figure covers them.
	CHECK(findLabel(drawing, "Room 1", {2500.0, 2160.0}) != nullptr);
	CHECK(findLabel(drawing, "18.24 m2", {2500.0, 1865.0}) != nullptr);
}

TEST_CASE("interior dimensions follow the room's shape: an L gets every stretch, collinear walls make one")
{
	Project l;
	l.walls = {wallOf(1, {0, 0}, {6000, 0}), wallOf(2, {6000, 0}, {6000, 3000}), wallOf(3, {6000, 3000}, {3000, 3000}),
		wallOf(4, {3000, 3000}, {3000, 5000}), wallOf(5, {3000, 5000}, {0, 5000}), wallOf(6, {0, 5000}, {0, 0})};
	std::vector<std::string> figures;
	for (const Dimension& d : interior(buildPlanDrawing(l, LevelId())))
	{
		figures.push_back(dimensionFigure(d, '.'));
	}
	std::sort(figures.begin(), figures.end());
	CHECK(figures == std::vector<std::string>{"2.00", "2.88", "2.88", "3.00", "4.88", "5.88"});

	// The south side in two walls, split where a wall leaves the room southwards: one face, named after the lower wall.
	Project split;
	split.walls = {wallOf(1, {0, 0}, {2500, 0}), wallOf(2, {2500, 0}, {5000, 0}), wallOf(3, {5000, 0}, {5000, 3000}),
		wallOf(4, {5000, 3000}, {0, 3000}), wallOf(5, {0, 3000}, {0, 0}), wallOf(6, {2500, 0}, {2500, -2000})};
	const PlanDrawing drawing = buildPlanDrawing(split, LevelId());
	const Dimension* south = findDimension(drawing, "auto/" + toString(WallId{Uuid{0, 1}}) + "/interior-left-0");
	REQUIRE(south != nullptr);
	CHECK(south->start == Point2D{60, 60});
	CHECK(south->end == Point2D{4940, 60});
	CHECK(findDimension(drawing, "auto/" + toString(WallId{Uuid{0, 2}}) + "/interior-left-0") == nullptr);
	CHECK(interior(drawing).size() == 2);

	// A thicker wall on the same line steps the face: two stretches, and the short side of each part is measured.
	Project step;
	step.walls = {wallOf(1, {0, 0}, {3000, 0}), wallOf(2, {3000, 0}, {6000, 0}, 200), wallOf(3, {6000, 0}, {6000, 3000}),
		wallOf(4, {6000, 3000}, {0, 3000}), wallOf(5, {0, 3000}, {0, 0})};
	figures.clear();
	for (const Dimension& d : interior(buildPlanDrawing(step, LevelId())))
	{
		figures.push_back(dimensionFigure(d, '.'));
	}
	std::sort(figures.begin(), figures.end());
	CHECK(figures == std::vector<std::string>{"2.84", "2.88", "2.94", "2.94", "5.88"});
}

TEST_CASE("between two rooms, the face that carries the partition's length gets no interior dimension")
{
	// The partition has the lowest id, so its faces would come first among opposite faces.
	Project project;
	project.walls = {wallOf(1, {2500, 0}, {2500, 4000}, 100), wallOf(2, {0, 0}, {2500, 0}), wallOf(3, {2500, 0}, {5000, 0}),
		wallOf(4, {5000, 0}, {5000, 4000}), wallOf(5, {5000, 4000}, {2500, 4000}), wallOf(6, {2500, 4000}, {0, 4000}),
		wallOf(7, {0, 4000}, {0, 0})};
	const PlanDrawing drawing = buildPlanDrawing(project, LevelId());
	const std::string partition = "auto/" + toString(WallId{Uuid{0, 1}});
	REQUIRE(findDimension(drawing, partition + "/partition") != nullptr); // along its left (west) face
	CHECK(findDimension(drawing, partition + "/interior-left-0") == nullptr);
	// West room: its depth on its own west face instead.
	const Dimension* west = findDimension(drawing, "auto/" + toString(WallId{Uuid{0, 7}}) + "/interior-left-0");
	REQUIRE(west != nullptr);
	CHECK(west->start == Point2D{60, 3940});
	CHECK(west->end == Point2D{60, 60});
	// East room: on the partition's east face, which comes before the outer wall's.
	const Dimension* east = findDimension(drawing, partition + "/interior-right-0");
	REQUIRE(east != nullptr);
	CHECK(east->start == Point2D{2550, 3940});
	CHECK(east->end == Point2D{2550, 60});
	CHECK(findDimension(drawing, "auto/" + toString(WallId{Uuid{0, 4}}) + "/interior-left-0") == nullptr);
	CHECK(interior(drawing).size() == 4);
}

TEST_CASE("in a narrow room, the name and area move sideways off the interior figures and stay on the floor")
{
	// 3.05 x 1.07 m on the axes: 2.93 x 0.95 m inside, too shallow for the name, the area and a figure above each other.
	Project bath;
	bath.walls = {wallOf(1, {0, 0}, {3050, 0}), wallOf(2, {3050, 0}, {3050, 1070}), wallOf(3, {3050, 1070}, {0, 1070}),
		wallOf(4, {0, 1070}, {0, 0})};
	const PlanDrawing drawing = buildPlanDrawing(bath, LevelId());
	const Label* figure = findLabel(drawing, "2.93", {1525.0, 345.0});
	REQUIRE(figure != nullptr);
	REQUIRE(findLabel(drawing, "0.95", {2705.0, 535.0}) != nullptr);
	const auto name = std::find_if(drawing.labels.begin(), drawing.labels.end(), [](const Label& l) { return l.text == "Room 1"; });
	const auto area = std::find_if(drawing.labels.begin(), drawing.labels.end(), [](const Label& l) { return l.text == "2.78 m2"; });
	REQUIRE(name != drawing.labels.end());
	REQUIRE(area != drawing.labels.end());
	// Moved sideways (west first), at their usual heights.
	CHECK(name->at.x_mm == doctest::Approx(725.0));
	CHECK(name->at.y_mm == doctest::Approx(535.0 + 160.0));
	CHECK(area->at.x_mm == doctest::Approx(725.0));
	CHECK(area->at.y_mm == doctest::Approx(535.0 - 135.0));
	// The area ends before the figure starts, with room to spare; both texts stay inside the walls.
	const double areaHalf = planTextWidthMm(textCharacters(area->text), area->heightMm) / 2.0;
	const double nameHalf = planTextWidthMm(textCharacters(name->text), name->heightMm) / 2.0;
	CHECK(area->at.x_mm + areaHalf <= figure->at.x_mm - planTextWidthMm(4, FigureHeightMm) / 2.0 - 75.0);
	CHECK(name->at.x_mm - nameHalf >= 60.0);
}

TEST_CASE("the view can hide the interior dimensions; the room's name and area then stay at its label point")
{
	Project bath;
	bath.walls = {wallOf(1, {0, 0}, {3050, 0}), wallOf(2, {3050, 0}, {3050, 1070}), wallOf(3, {3050, 1070}, {0, 1070}),
		wallOf(4, {0, 1070}, {0, 0})};
	PlanOverlay overlay;
	overlay.interiorDimensions = false;
	const PlanDrawing drawing = buildPlanDrawing(bath, LevelId(), overlay);
	CHECK(interior(drawing).empty());
	CHECK(drawing.dimensions.size() == 4); // the walls' own, outside
	CHECK(findLabel(drawing, "2.93", {1525.0, 345.0}) == nullptr);
	CHECK(findLabel(drawing, "Room 1", {1525.0, 535.0 + 160.0}) != nullptr);
	CHECK(findLabel(drawing, "2.78 m2", {1525.0, 535.0 - 135.0}) != nullptr);
	CHECK(PlanOverlay{}.interiorDimensions); // shown unless the view says otherwise
}

TEST_CASE("a wall between two rooms only gets its length; rooms get their name and area")
{
	Project project = room();
	project.walls.push_back(Wall{WallId{Uuid{0, 5}}, {2500, 0}, {2500, 4000}, 100, 2800});
	DrawingTexts texts;
	texts.roomName = "Habitación {n}";
	texts.areaUnit = " m²";
	texts.decimalSeparator = ',';
	const PlanDrawing drawing = buildPlanDrawing(project, LevelId(), {}, texts);
	// The partition runs north: its left is west, where its figure stands 300 mm off the face.
	CHECK(findLabel(drawing, "4,00", {2015.0, 2000.0}) != nullptr);
	std::vector<std::string> names;
	for (const Label& label : drawing.labels)
	{
		if (label.ink == Ink::Label)
		{
			names.push_back(label.text);
		}
	}
	std::sort(names.begin(), names.end());
	CHECK(names == std::vector<std::string>{"8,93 m²", "8,93 m²", "Habitación 1", "Habitación 2"}); // 2350 x 3800 each

	const PlanDrawing single = buildPlanDrawing(room(), LevelId(), {}, texts);
	CHECK(std::count_if(single.labels.begin(), single.labels.end(), [](const Label& l) { return l.text == "18,24 m²"; }) == 1);
}

TEST_CASE("the editing overlay tints the selected and previewed walls and adds the handles")
{
	PlanOverlay overlay;
	overlay.selectedWall = WallId{Uuid{0, 1}};
	overlay.previewWalls.push_back(Wall{WallId{}, {1000, 1000}, {2000, 1000}, 100, 2800});
	overlay.handles = {{0, 0}, {5000, 0}};
	const PlanDrawing drawing = buildPlanDrawing(room(), LevelId(), overlay);
	CHECK(countFills(drawing, Ink::Selected) == 3);
	CHECK(countFills(drawing, Ink::Preview) == 1);
	CHECK(countFills(drawing, Ink::Handle) == 2);
	CHECK(covered(drawing, {1500.0, 1000.0}, Ink::Preview));
	CHECK(covered(drawing, {5100.0, 110.0}, Ink::Handle));

	overlay.previewWalls = {Wall{WallId{Uuid{0, 4}}, {0, 4000}, {0, 1000}, 200, 2800}}; // the west wall being dragged
	const PlanDrawing dragged = buildPlanDrawing(room(), LevelId(), overlay);
	CHECK(countFills(dragged, Ink::Preview) == 1);
	CHECK_FALSE(covered(dragged, {0.0, 500.0}, Ink::Preview));
}

TEST_CASE("only the given level is drawn, and the bounds hold everything")
{
	Project project = room();
	const LevelId ground{Uuid{0, 100}};
	const LevelId upper{Uuid{0, 101}};
	project.levels = {Level{ground, "", 2800, 250}, Level{upper, "", 2800, 250}};
	for (Wall& wall : project.walls)
	{
		wall.level = ground;
	}
	project.walls.push_back(Wall{WallId{Uuid{0, 6}}, {0, 0}, {3000, 0}, 100, 2800});
	project.walls.back().level = upper;
	const PlanDrawing top = buildPlanDrawing(project, upper);
	CHECK(countFills(top, Ink::Cut) == 1);
	CHECK_FALSE(covered(top, {5000.0, 2000.0}));

	const PlanDrawing drawing = buildPlanDrawing(project, ground);
	CHECK(drawing.min.y_mm < -1335.0);
	CHECK(drawing.min.x_mm < -835.0);
	CHECK(drawing.max.x_mm > 5100.0);
	CHECK(drawing.max.y_mm > 4100.0);
	CHECK(buildPlanDrawing(Project{}, LevelId()).empty());
}

TEST_CASE("the SVG is a white sheet at scale, north up, with every element in its ink")
{
	PlanDrawing marks;
	marks.labels.push_back(Label{{0.0, 1000.0}, 0.0, 150.0, "N", Ink::Label});
	marks.labels.push_back(Label{{0.0, 0.0}, 0.0, 150.0, "S & <n>", Ink::Label});
	marks.min = PlanPoint{0.0, 0.0};
	marks.max = PlanPoint{0.0, 1000.0};
	const std::string sheet = writeSvg(marks, SvgOptions{100.0, 1000.0});
	CHECK(sheet.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"20mm\" height=\"30mm\" "
		"viewBox=\"0 0 2000 3000\">", 0) == 0);
	CHECK(sheet.find("<text x=\"1000\" y=\"1000\" font-size=\"208.3\" fill=\"#111111\"") != std::string::npos); // north above
	CHECK(sheet.find("<text x=\"1000\" y=\"2000\"") != std::string::npos);
	CHECK(sheet.find(">S &amp; &lt;n&gt;</text>") != std::string::npos);
	CHECK(sheet.find("</svg>\n") == sheet.size() - 7);

	const PlanDrawing drawing = buildPlanDrawing(room(), LevelId());
	const std::string svg = writeSvg(drawing);
	const auto closed = static_cast<std::size_t>(std::count_if(drawing.strokes.begin(), drawing.strokes.end(), [](const Stroke& s) { return s.closed; }));
	CHECK(occurrences(svg, "<polygon points=") == drawing.fills.size() + closed);
	CHECK(occurrences(svg, "<polyline points=") == drawing.strokes.size() - closed);
	CHECK(occurrences(svg, "<text ") == drawing.labels.size());
	CHECK(occurrences(svg, "stroke-dasharray=\"120 80\"") == 1);
	CHECK(occurrences(svg, "fill=\"#262626\"") == drawing.fills.size());
	CHECK(occurrences(svg, "transform=\"rotate(-90 ") >= 2); // the east and west figures
	CHECK(writeSvg(drawing) == svg); // deterministic
	CHECK(writeSvg(PlanDrawing{}).find("viewBox=\"0 0 2000 2000\"") != std::string::npos);
}
