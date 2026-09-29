#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Geometry/CarpentryBuilder.h"
#include "F101/Geometry/WallJoins.h"
#include "F101/Scene/SceneCompiler.h"

#include <cmath>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::cad;

// Types of doors and windows (ADR-0014).
namespace
{
	constexpr double Sin45 = 0.70710678118654752;

	Ray3 down(double x, double y)
	{
		return Ray3{x, y, 20000.0, 0.0, 0.0, -1.0};
	}

	std::size_t triangles(const geometry::MeshData& mesh, const std::string& slot)
	{
		for (const geometry::MeshSection& section : mesh.sections)
		{
			if (section.slot == slot)
			{
				return section.indices.size() / 3;
			}
		}
		return 0;
	}

	geometry::Bounds3 sectionBounds(const geometry::MeshData& mesh, const std::string& slot)
	{
		geometry::Bounds3 bounds{{1e300, 1e300, 1e300}, {-1e300, -1e300, -1e300}};
		for (const geometry::MeshSection& section : mesh.sections)
		{
			if (section.slot == slot)
			{
				for (const std::uint32_t index : section.indices)
				{
					const geometry::Vec3& p = mesh.positions[index];
					bounds.min = geometry::Vec3{std::min(bounds.min.x, p.x), std::min(bounds.min.y, p.y), std::min(bounds.min.z, p.z)};
					bounds.max = geometry::Vec3{std::max(bounds.max.x, p.x), std::max(bounds.max.y, p.y), std::max(bounds.max.z, p.z)};
				}
			}
		}
		return bounds;
	}

	// A 4 m wall along +x, 120 mm thick: its left face looks north (+y).
	Wall eastWall()
	{
		return Wall{WallId{Uuid{0, 1}}, {0, 0}, {4000, 0}, 120, 2800};
	}

	Opening door(OpeningOperation operation, OpeningEnd hinge, WallSide opensTo, int openPercent, std::int64_t width = 900)
	{
		return Opening{OpeningId{Uuid{0, 9}}, OpeningKind::Door, 1000, width, 2100, 0, {}, operation, hinge, opensTo, openPercent};
	}

	Opening window(OpeningOperation operation, OpeningEnd hinge, WallSide opensTo, int openPercent)
	{
		return Opening{OpeningId{Uuid{0, 9}}, OpeningKind::Window, 1000, 1200, 1200, 900, {}, operation, hinge, opensTo, openPercent};
	}

	geometry::MeshData carpentry(const Opening& opening)
	{
		const Wall wall = eastWall();
		return geometry::buildOpeningMesh(wall, geometry::plainFootprint(wall), opening);
	}

	Status withOpenings(std::vector<Opening> openings)
	{
		Wall wall = eastWall();
		wall.openings = std::move(openings);
		return validateWall(wall);
	}

	// A host wall along x with a wall meeting it in a T at x = 2000 (100 mm thick), joints checked.
	struct HostWithTee
	{
		Document document{makeSequentialUuidSource()};
		WallId host = document.addWall({0, 0}, {6000, 0}, 120, 2800).value();
		WallId tee = document.addWall({2000, 0}, {2000, 3000}, 100, 2800).value();

		HostWithTee()
		{
			document.setWallObstructions(geometry::makeWallObstructions());
		}

		const Opening& opening(const OpeningId& id) const
		{
			return *document.project().findWall(host)->findOpening(id);
		}
	};
}

TEST_CASE("how an opening opens suits its kind; openness is a percentage; fixed windows do not open")
{
	CHECK(operationName(OpeningOperation::Hinged) == "hinged");
	CHECK(operationName(OpeningOperation::DoubleHinged) == "double");
	CHECK(operationName(OpeningOperation::Sliding) == "sliding");
	CHECK(operationName(OpeningOperation::Fixed) == "fixed");

	CHECK(withOpenings({door(OpeningOperation::DoubleHinged, OpeningEnd::Start, WallSide::Left, 30)}).ok());
	CHECK(withOpenings({window(OpeningOperation::Sliding, OpeningEnd::End, WallSide::Right, 100)}).ok());
	CHECK(withOpenings({door(OpeningOperation::Fixed, OpeningEnd::Start, WallSide::Left, 0)}).error().message == "A door cannot be fixed");
	CHECK(withOpenings({door(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 101)}).error().message ==
		"The door is open from 0 to 100 %");
	CHECK(withOpenings({window(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, -1)}).error().code == ErrorCode::OutOfRange);
	CHECK(withOpenings({window(OpeningOperation::Fixed, OpeningEnd::Start, WallSide::Left, 10)}).error().message == "A fixed window does not open");
}

TEST_CASE("a sliding door takes a pocket as wide as itself on its hinge side, inside the wall and clear of other openings")
{
	CHECK(occupiedSpan(door(OpeningOperation::Sliding, OpeningEnd::Start, WallSide::Left, 0, 800)) == AxisSpan{200, 1800});
	CHECK(occupiedSpan(door(OpeningOperation::Sliding, OpeningEnd::End, WallSide::Left, 0, 800)) == AxisSpan{1000, 2600});
	CHECK(occupiedSpan(door(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 0, 800)) == AxisSpan{1000, 1800});
	CHECK(occupiedSpan(window(OpeningOperation::Sliding, OpeningEnd::Start, WallSide::Left, 0)) == AxisSpan{1000, 2200});

	Opening pocket = door(OpeningOperation::Sliding, OpeningEnd::Start, WallSide::Left, 0, 800);
	pocket.offsetMm = 300;
	CHECK(withOpenings({pocket}).error().message == "The sliding door needs 800 mm of wall beside it for its pocket");

	Opening before = window(OpeningOperation::Fixed, OpeningEnd::Start, WallSide::Left, 0);
	before.id = OpeningId{Uuid{0, 8}};
	before.offsetMm = 1000;
	before.widthMm = 1000;
	pocket.offsetMm = 2200; // its pocket 1400..2200 over the window
	CHECK(withOpenings({before, pocket}).error().message == "The sliding door's pocket overlaps another opening");
	pocket.offsetMm = 2800;
	CHECK(withOpenings({before, pocket}).ok());

	Opening pocketEnd = door(OpeningOperation::Sliding, OpeningEnd::End, WallSide::Left, 0, 800);
	pocketEnd.offsetMm = 500; // pocket 1300..2100
	Opening after = before;
	after.offsetMm = 1500;
	after.widthMm = 600;
	CHECK(withOpenings({pocketEnd, after}).error().message == "The window is in the pocket of a sliding door");
}

TEST_CASE_FIXTURE(HostWithTee, "a pocket that would run into a joined wall is refused with its own message")
{
	const OpeningId id = document.addOpening(host, Opening{OpeningId{}, OpeningKind::Door, 2300, 800, 2100, 0}).value();
	Opening sliding = opening(id);
	sliding.operation = OpeningOperation::Sliding; // pocket 1500..2300, over the T at 1950..2050
	CHECK(document.execute(ChangeOpeningCommand{host, sliding}).error().message ==
		"The sliding door's pocket runs into a wall joined to this one (1950 to 2050 mm along it)");
	sliding.hinge = OpeningEnd::End; // pocket 3100..3900
	CHECK(document.execute(ChangeOpeningCommand{host, sliding}).ok());
	// Changing only how open it is shown moves nothing.
	sliding.openPercent = 60;
	CHECK(document.execute(ChangeOpeningCommand{host, sliding}).ok());
}

TEST_CASE("schema 6 writes how every opening opens; older openings are read as hinged doors and fixed windows")
{
	Document document{makeSequentialUuidSource()};
	const WallId wall = document.addWall({0, 0}, {6000, 0}, 120, 2800).value();
	REQUIRE(document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Door, 500, 800, 2100, 0, {}, OpeningOperation::Sliding,
		OpeningEnd::End, WallSide::Right, 40}).ok());
	REQUIRE(document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Window, 3000, 1200, 1200, 900, {}, OpeningOperation::DoubleHinged}).ok());
	const std::string json = writeProjectJson(document.project());
	CHECK(json.find("\"schemaVersion\": 6") != std::string::npos);
	for (const char* field : {"\"operation\": \"sliding\"", "\"hinge\": \"end\"", "\"opensTo\": \"right\"", "\"openPercent\": 40",
			 "\"operation\": \"double\""})
	{
		CHECK(json.find(field) != std::string::npos);
	}
	CHECK(json.find("\"sillMm\": 0,") < json.find("\"operation\": \"sliding\"")); // after the dimensions
	const Result<Project> loaded = readProjectJson(json);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls == document.project().walls);

	const std::string legacy = R"({"format":"101st-floor-project","schemaVersion":5,"units":"mm","levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],)"
		R"("walls":[{"id":"00000000-0000-0000-0000-000000000001","level":"00000000-0000-0000-0000-000000000064","start":[0,0],"end":[6000,0],"thicknessMm":120,"heightMm":2800,)"
		R"("openings":[{"id":"00000000-0000-0000-0000-000000000002","kind":"door","offsetMm":500,"widthMm":900,"heightMm":2100,"sillMm":0},)"
		R"({"id":"00000000-0000-0000-0000-000000000003","kind":"window","offsetMm":3000,"widthMm":1200,"heightMm":1200,"sillMm":900}]}]})";
	const Result<Project> old = readProjectJson(legacy);
	REQUIRE(old.ok());
	const std::vector<Opening>& openings = old.value().walls[0].openings;
	CHECK(openings[0].operation == OpeningOperation::Hinged);
	CHECK(openings[1].operation == OpeningOperation::Fixed);
	CHECK(openings[1].openPercent == 0);

	auto withField = [&legacy](const std::string& field)
	{
		std::string json = legacy;
		json.replace(json.find(R"("sillMm":0})"), 11, R"("sillMm":0,)" + field + "}");
		return readProjectJson(json);
	};
	CHECK(withField(R"("operation":"sliding","hinge":"end","opensTo":"right","openPercent":25)").ok());
	CHECK(withField(R"("operation":"revolving")").error().code == ErrorCode::ParseError);
	CHECK(withField(R"("hinge":"middle")").error().code == ErrorCode::ParseError);
	CHECK(withField(R"("opensTo":1)").error().code == ErrorCode::ParseError);
	CHECK(withField(R"("openPercent":150)").error().code == ErrorCode::ParseError);
	CHECK(withField(R"("openPercent":12.5)").error().code == ErrorCode::ParseError);
	CHECK(withField(R"("operation":"fixed")").error().message == "Invalid project file: A door cannot be fixed");
}

TEST_CASE("a hinged door leaf turns about its hinges towards the face it opens to, handles with it")
{
	// Closed: as in MVP 6, whatever the hinge side.
	geometry::Bounds3 leaf = sectionBounds(carpentry(door(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 0)), "leaf");
	CHECK(leaf.min.x == doctest::Approx(1053.0));
	CHECK(leaf.max.x == doctest::Approx(1847.0));
	CHECK(leaf.min.y == doctest::Approx(-20.0));
	CHECK(leaf.max.y == doctest::Approx(20.0));

	// Fully open towards the north face, hinged at the start jamb: 794 mm out of the wall from its face, 40 mm thick.
	const geometry::MeshData open = carpentry(door(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 100));
	leaf = sectionBounds(open, "leaf");
	CHECK(leaf.min.x == doctest::Approx(1053.0));
	CHECK(leaf.max.x == doctest::Approx(1093.0));
	CHECK(leaf.min.y == doctest::Approx(20.0));
	CHECK(leaf.max.y == doctest::Approx(814.0));
	CHECK(sectionBounds(open, "hardware").max.y == doctest::Approx(754.0)); // 60 mm from the free edge

	leaf = sectionBounds(carpentry(door(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Right, 100)), "leaf");
	CHECK(leaf.min.y == doctest::Approx(-814.0));
	CHECK(leaf.max.y == doctest::Approx(-20.0));

	leaf = sectionBounds(carpentry(door(OpeningOperation::Hinged, OpeningEnd::End, WallSide::Left, 100)), "leaf");
	CHECK(leaf.min.x == doctest::Approx(1807.0));
	CHECK(leaf.max.x == doctest::Approx(1847.0));
	CHECK(leaf.max.y == doctest::Approx(814.0));

	// Half open: 45 degrees.
	leaf = sectionBounds(carpentry(door(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 50)), "leaf");
	CHECK(leaf.max.y == doctest::Approx(20.0 + 794.0 * Sin45));
	CHECK(leaf.max.x == doctest::Approx(1053.0 + 794.0 * Sin45 + 40.0 * Sin45));
}

TEST_CASE("double doors and windows: two leaves or sashes hinged at both jambs, meeting in the middle")
{
	const geometry::MeshData doors = carpentry(door(OpeningOperation::DoubleHinged, OpeningEnd::Start, WallSide::Left, 100, 1600));
	CHECK(triangles(doors, "leaf") == 2 * 12);
	CHECK(triangles(doors, "hardware") == 4 * 12);
	const geometry::Bounds3 leaves = sectionBounds(doors, "leaf");
	CHECK(leaves.min.x == doctest::Approx(1053.0));
	CHECK(leaves.max.x == doctest::Approx(2547.0));
	CHECK(leaves.max.y == doctest::Approx(20.0 + (1800.0 - 1.5 - 1053.0)));

	const geometry::MeshData sashes = carpentry(window(OpeningOperation::DoubleHinged, OpeningEnd::Start, WallSide::Left, 0));
	CHECK(triangles(sashes, "leaf") == 2 * 4 * 12);
	CHECK(triangles(sashes, "glass") == 2 * 12);
	CHECK(triangles(sashes, "hardware") == 12); // one handle, on the face it opens to
	CHECK(sectionBounds(sashes, "hardware").max.y == doctest::Approx(25.0 + 25.0));
	CHECK(triangles(sashes, "frame") == 4 * 12); // no fixed mullion
}

TEST_CASE("a hinged window sash carries its pane and turns like a door leaf")
{
	const geometry::MeshData closed = carpentry(window(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 0));
	CHECK(triangles(closed, "leaf") == 4 * 12);
	const geometry::Bounds3 glassClosed = sectionBounds(closed, "glass");
	CHECK(glassClosed.max.y == doctest::Approx(3.0));
	CHECK(glassClosed.min.x == doctest::Approx(1053.0 + 50.0));

	const geometry::MeshData open = carpentry(window(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 100));
	const geometry::Bounds3 sash = sectionBounds(open, "leaf");
	CHECK(sash.min.x == doctest::Approx(1053.0));
	CHECK(sash.max.x == doctest::Approx(1103.0)); // 50 mm sash, now across the wall
	CHECK(sash.max.y == doctest::Approx(25.0 + (2147.0 - 1053.0)));
	CHECK(sectionBounds(open, "glass").min.y > 25.0);
}

TEST_CASE("a sliding door goes into its pocket; a sliding window moves one sash over the other")
{
	geometry::Bounds3 leaf = sectionBounds(carpentry(door(OpeningOperation::Sliding, OpeningEnd::Start, WallSide::Left, 100)), "leaf");
	CHECK(leaf.min.x == doctest::Approx(1053.0 - 794.0));
	CHECK(leaf.max.x == doctest::Approx(1053.0));
	CHECK(leaf.max.y == doctest::Approx(20.0)); // inside the wall
	leaf = sectionBounds(carpentry(door(OpeningOperation::Sliding, OpeningEnd::End, WallSide::Left, 50)), "leaf");
	CHECK(leaf.min.x == doctest::Approx(1053.0 + 397.0));

	// 1100 mm inside the frame: two 575 mm sashes overlapping 50 mm; the one on the north track travels 525 mm.
	const geometry::MeshData closed = carpentry(window(OpeningOperation::Sliding, OpeningEnd::Start, WallSide::Left, 0));
	CHECK(triangles(closed, "leaf") == 2 * 4 * 12);
	CHECK(sectionBounds(closed, "glass").max.x == doctest::Approx(2150.0 - 50.0));
	const geometry::MeshData open = carpentry(window(OpeningOperation::Sliding, OpeningEnd::Start, WallSide::Left, 100));
	const geometry::Bounds3 panes = sectionBounds(open, "glass");
	CHECK(panes.max.x == doctest::Approx(1625.0 - 50.0));
	CHECK(panes.max.y == doctest::Approx(2.0 + 31.0 / 2.0 + 3.0)); // the moving pane, on the north track
	CHECK(panes.min.y == doctest::Approx(-2.0 - 31.0 / 2.0 - 3.0));
}

TEST_CASE("the swing symbol of a door lies on the floor, on the side it opens to; windows and sliding doors have none")
{
	const Wall wall = eastWall();
	auto symbol = [&wall](const Opening& opening) { return geometry::buildOpeningSymbolMesh(wall, geometry::plainFootprint(wall), opening); };

	const geometry::MeshData north = symbol(door(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 0));
	REQUIRE(north.sections.size() == 1);
	CHECK(north.sections[0].slot == "swing");
	CHECK(triangles(north, "swing") == 2 * (16 + 1));
	const geometry::Bounds3 bounds = sectionBounds(north, "swing");
	CHECK(bounds.min.z == doctest::Approx(geometry::FloorThicknessMm + geometry::SwingLiftMm));
	CHECK(bounds.max.z == doctest::Approx(geometry::FloorThicknessMm + geometry::SwingLiftMm));
	CHECK(bounds.min.x == doctest::Approx(1053.0 - 10.0));
	CHECK(bounds.max.x == doctest::Approx(1053.0 + 794.0 + 10.0));
	CHECK(bounds.min.y == doctest::Approx(60.0)); // from the north face
	CHECK(bounds.max.y == doctest::Approx(60.0 + 794.0 + 10.0));

	const geometry::Bounds3 south = sectionBounds(symbol(door(OpeningOperation::Hinged, OpeningEnd::End, WallSide::Right, 0)), "swing");
	CHECK(south.max.y == doctest::Approx(-60.0));
	CHECK(south.max.x == doctest::Approx(1847.0 + 10.0));

	CHECK(triangles(symbol(door(OpeningOperation::DoubleHinged, OpeningEnd::Start, WallSide::Left, 0, 1600)), "swing") == 4 * (16 + 1));
	CHECK(symbol(door(OpeningOperation::Sliding, OpeningEnd::Start, WallSide::Left, 0)).sections.empty());
	CHECK(symbol(window(OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 0)).sections.empty());
}

TEST_CASE("door swings are editing symbols: in the edited view on its levels, never for exporters")
{
	Document document{makeSequentialUuidSource()};
	const LevelId ground = document.project().levels.front().id;
	const LevelId upper = document.addLevel().value();
	const WallId below = document.addWall({0, 0}, {4000, 0}, 120, 2800, ground).value();
	const WallId above = document.addWall({0, 0}, {4000, 0}, 120, 2800, upper).value();
	const OpeningId lowDoor = document.addOpening(below, Opening{OpeningId{}, OpeningKind::Door, 1000, 900, 2100, 0}).value();
	const OpeningId highDoor = document.addOpening(above, Opening{OpeningId{}, OpeningKind::Door, 1000, 900, 2100, 0}).value();
	REQUIRE(document.addOpening(above, Opening{OpeningId{}, OpeningKind::Window, 2500, 1000, 1200, 900}).ok());
	const scene::SceneNodeId lowSwing{toString(below) + "/opening/" + toString(lowDoor) + "/swing"};
	const scene::SceneNodeId highSwing{toString(above) + "/opening/" + toString(highDoor) + "/swing"};

	CHECK(scene::compileScene(document.project()).findNode(lowSwing) == nullptr);

	scene::ViewOverlay overlay;
	overlay.activeLevel = upper;
	const scene::Scene view = scene::compileScene(document.project(), overlay);
	CHECK(view.findNode(lowSwing) == nullptr); // the level below is only context
	const scene::SceneNode* swing = view.findNode(highSwing);
	REQUIRE(swing != nullptr);
	CHECK(swing->style == scene::NodeStyle::Symbol);
	CHECK(swing->sourceKind == scene::SourceKind::Opening);
	CHECK(swing->level == upper);
	CHECK(geometry::computeBounds(swing->mesh).min.z == doctest::Approx(2800.0 + geometry::FloorThicknessMm + geometry::SwingLiftMm));
	CHECK(&view.nodes.back() == swing); // after everything else

	overlay.activeLevel = ground;
	CHECK(scene::compileScene(document.project(), overlay).findNode(lowSwing) != nullptr);
}

TEST_CASE_FIXTURE(HostWithTee, "the door tool opens towards the side the pointer is on; Space moves the hinges")
{
	EditController editor{document, EditSettings{}};
	editor.setMode(EditMode::Door);
	editor.pointerMoved(down(3500.0, 40.0));
	REQUIRE(editor.openingPlacement().has_value());
	CHECK(editor.openingPlacement()->opening.opensTo == WallSide::Left);
	CHECK(editor.openingPlacement()->opening.hinge == OpeningEnd::Start);
	editor.pointerMoved(down(3500.0, -40.0));
	CHECK(editor.openingPlacement()->opening.opensTo == WallSide::Right);
	CHECK(editor.flip().ok());
	CHECK(editor.openingPlacement()->opening.hinge == OpeningEnd::End);
	REQUIRE(editor.pointerPressed(down(3500.0, -40.0)).ok());
	const Opening& placed = document.project().findWall(host)->openings.front();
	CHECK(placed.opensTo == WallSide::Right);
	CHECK(placed.hinge == OpeningEnd::End);
	CHECK(placed.operation == OpeningOperation::Hinged);

	editor.setMode(EditMode::Window);
	editor.pointerMoved(down(5000.0, 40.0));
	CHECK(editor.openingPlacement()->opening.operation == OpeningOperation::DoubleHinged); // the window tool's default
	editor.setMode(EditMode::Select);
	CHECK(editor.flip().error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(HostWithTee, "the selected opening changes how it opens, its hinges, its side and how open it is shown")
{
	const OpeningId id = document.addOpening(host, Opening{OpeningId{}, OpeningKind::Door, 2300, 800, 2100, 0}).value();
	EditController editor{document, EditSettings{}};
	REQUIRE(editor.selectOpening(host, id).ok());

	CHECK(editor.flip().ok());
	CHECK(opening(id).hinge == OpeningEnd::End);
	CHECK(editor.flipOpeningHinge().ok());
	CHECK(opening(id).hinge == OpeningEnd::Start);
	CHECK(editor.flipOpeningSide().ok());
	CHECK(opening(id).opensTo == WallSide::Right);
	CHECK(document.undoLabel() == "Change opening");

	CHECK(editor.setOpeningOpenPercent("35 %").ok());
	CHECK(opening(id).openPercent == 35);
	CHECK(editor.setOpeningOpenPercent("100").ok());
	for (const char* bad : {"120", "abc", "", "-5", "12.5"})
	{
		CHECK(editor.setOpeningOpenPercent(bad).error().code == ErrorCode::InvalidArgument);
	}
	CHECK(opening(id).openPercent == 100);

	CHECK(editor.setOpeningOperation(OpeningOperation::Fixed).error().message == "A door cannot be fixed");
	// Sliding with its hinges at the start: the pocket would reach the T.
	CHECK(editor.setOpeningOperation(OpeningOperation::Sliding).error().code == ErrorCode::OutOfRange);
	CHECK(opening(id).operation == OpeningOperation::Hinged);
	REQUIRE(editor.flipOpeningHinge().ok());
	CHECK(editor.setOpeningOperation(OpeningOperation::Sliding).ok());

	const OpeningId pane = document.addOpening(host, Opening{OpeningId{}, OpeningKind::Window, 4500, 1000, 1200, 900, {},
		OpeningOperation::Hinged, OpeningEnd::Start, WallSide::Left, 40}).value();
	REQUIRE(editor.selectOpening(host, pane).ok());
	CHECK(editor.setOpeningOperation(OpeningOperation::Fixed).ok());
	CHECK(opening(pane).operation == OpeningOperation::Fixed);
	CHECK(opening(pane).openPercent == 0);
}

TEST_CASE_FIXTURE(HostWithTee, "placing and dragging a sliding door keeps its pocket clear of joints and other openings")
{
	EditSettings settings;
	settings.door.operation = OpeningOperation::Sliding; // pocket before it (hinge at the start)
	EditController editor{document, settings};
	editor.setMode(EditMode::Door);
	// Centred on 2600 the door would start at 2150; its pocket (900 mm before it) must clear the T at 1950..2050.
	editor.pointerMoved(down(2600.0, 30.0));
	REQUIRE(editor.openingPlacement().has_value());
	CHECK(editor.openingPlacement()->opening.offsetMm == 2950);
	REQUIRE(editor.pointerPressed(down(2600.0, 30.0)).ok());
	const OpeningId id = document.project().findWall(host)->openings.front().id;

	editor.setMode(EditMode::Select);
	REQUIRE(editor.selectOpening(host, id).ok());
	REQUIRE(editor.pointerPressed(down(3400.0, 30.0)).ok()); // well inside the hole (2950..3850)
	editor.pointerMoved(down(4400.0, 30.0));
	CHECK(editor.view().previewWalls.at(0).openings.at(0).offsetMm == 3950);
	editor.pointerMoved(down(2600.0, 30.0)); // would put it at 2150, its pocket over the T: it stays at 2950
	CHECK(editor.view().previewWalls.empty()); // back where it is: nothing to preview
	REQUIRE(editor.pointerReleased(down(2600.0, 30.0)).ok());
	CHECK(opening(id).offsetMm == 2950);
}
