#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Geometry/CarpentryBuilder.h"
#include "F101/Geometry/WallGeometryBuilder.h"
#include "F101/Geometry/WallJoins.h"
#include "F101/Scene/FinishTextures.h"
#include "F101/Scene/SceneCompiler.h"

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

	// Bounds of one section, in mm.
	geometry::Bounds3 sectionBounds(const geometry::MeshData& mesh, const std::string& slot)
	{
		for (const geometry::MeshSection& section : mesh.sections)
		{
			if (section.slot == slot)
			{
				geometry::Bounds3 bounds{{1e300, 1e300, 1e300}, {-1e300, -1e300, -1e300}};
				for (const std::uint32_t index : section.indices)
				{
					const geometry::Vec3& p = mesh.positions[index];
					bounds.min = geometry::Vec3{std::min(bounds.min.x, p.x), std::min(bounds.min.y, p.y), std::min(bounds.min.z, p.z)};
					bounds.max = geometry::Vec3{std::max(bounds.max.x, p.x), std::max(bounds.max.y, p.y), std::max(bounds.max.z, p.z)};
				}
				return bounds;
			}
		}
		FAIL("no section " << slot);
		return {};
	}

	Wall eastWall(std::int64_t length = 4000)
	{
		return Wall{WallId{Uuid{0, 1}}, {0, 0}, {length, 0}, 120, 2800};
	}

	// A host wall along x with, optionally, a wall meeting it in a T at x = 2000 (100 mm thick, going north).
	struct HostWithTee
	{
		Document document{makeSequentialUuidSource()};
		WallId host{Uuid{0, 100}};
		WallId tee{Uuid{0, 101}};

		HostWithTee()
		{
			// Legacy loaded projects may contain unsplit hosts; opening edits must still check their junctions.
			Project project = document.project();
			Wall a{host, {0, 0}, {6000, 0}, 120, 2800};
			Wall b{tee, {2000, 0}, {2000, 3000}, 100, 2800};
			a.level = b.level = project.levels.front().id;
			project.walls = {a, b};
			REQUIRE(document.replaceProject(project).ok());
			document.setWallObstructions(geometry::makeWallObstructions());
		}
	};
}

TEST_CASE("carpentry and glazing finishes: defaults, glass is see-through, opening finishes are validated")
{
	CHECK(findFinish(DefaultCarpentryFinish)->allows(Surface::Carpentry));
	CHECK(findFinish(DefaultGlazingFinish)->allows(Surface::Glazing));
	CHECK(findFinish(HardwareFinish)->allows(Surface::Carpentry));
	CHECK(findFinish("glass-clear")->opacity < 0.5);
	CHECK(findFinish("brick-red")->opacity == 1.0);
	CHECK(scene::buildFinishLook("glass-clear", 8).opacity == findFinish("glass-clear")->opacity);
	CHECK(validateFinish("wood-oak", Surface::Carpentry).ok());
	CHECK(validateFinish("paint-sage", Surface::Carpentry).error().message == "The finish 'paint-sage' is not for doors and window frames");

	Wall wall = eastWall();
	wall.openings.push_back(Opening{OpeningId{Uuid{0, 9}}, OpeningKind::Door, 500, 900, 2100, 0, "wood-walnut"});
	CHECK(validateWall(wall).ok());
	wall.openings[0].finish = "tile-grey";
	CHECK(validateWall(wall).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("an opening's finish is an undoable change, written as schema 5 only when set")
{
	Document document{makeSequentialUuidSource()};
	const WallId wall = document.addWall({0, 0}, {4000, 0}, 120, 2800).value();
	const OpeningId door = document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Door, 500, 900, 2100, 0}).value();
	CHECK(writeProjectJson(document.project()).find("\"finish\"") == std::string::npos);

	Opening oak = *document.project().findWall(wall)->findOpening(door);
	oak.finish = "wood-oak";
	REQUIRE(document.execute(ChangeOpeningCommand{wall, oak}).ok());
	const std::string json = writeProjectJson(document.project());
	CHECK(json.find("\"finish\": \"wood-oak\"") != std::string::npos);
	const Result<Project> loaded = readProjectJson(json);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls == document.project().walls);
	REQUIRE(document.undo().ok());
	CHECK(document.project().findWall(wall)->findOpening(door)->finish.empty());
}

TEST_CASE("door carpentry: frame in the hole, a closed leaf and a handle on each face")
{
	Wall wall = eastWall();
	const Opening door{OpeningId{Uuid{0, 9}}, OpeningKind::Door, 1000, 900, 2100, 0};
	const geometry::MeshData mesh = geometry::buildOpeningMesh(wall, geometry::plainFootprint(wall), door);
	CHECK(triangles(mesh, geometry::carpentry_slots::Frame) == 3 * 12); // two jambs and the head
	CHECK(triangles(mesh, geometry::carpentry_slots::Leaf) == 12);
	CHECK(triangles(mesh, geometry::carpentry_slots::Hardware) == 2 * 12);
	CHECK(triangles(mesh, geometry::carpentry_slots::Glass) == 0);

	const geometry::Bounds3 frame = sectionBounds(mesh, geometry::carpentry_slots::Frame);
	CHECK(frame.min.x == 1000.0);
	CHECK(frame.max.x == 1900.0);
	CHECK(frame.min.z == 0.0);
	CHECK(frame.max.z == 2100.0);
	CHECK(frame.max.y == doctest::Approx(35.0)); // 70 mm deep, centred in the 120 mm wall
	const geometry::Bounds3 leaf = sectionBounds(mesh, geometry::carpentry_slots::Leaf);
	CHECK(leaf.min.x == doctest::Approx(1053.0)); // inside the 50 mm jamb, 3 mm clearance
	CHECK(leaf.max.x == doctest::Approx(1847.0));
	CHECK(leaf.min.z == doctest::Approx(10.0));   // gap under the leaf
	CHECK(leaf.max.z == doctest::Approx(2047.0)); // under the head
	CHECK(leaf.max.y == doctest::Approx(20.0));   // 40 mm leaf
	const geometry::Bounds3 handles = sectionBounds(mesh, geometry::carpentry_slots::Hardware);
	CHECK(handles.min.z == doctest::Approx(1000.0));
	CHECK(handles.max.y == doctest::Approx(50.0)); // 30 mm off each face of the leaf
	CHECK(handles.min.y == doctest::Approx(-50.0));
}

TEST_CASE("fixed window carpentry: frame with a bottom rail around a pane; wide ones get a mullion")
{
	Wall wall = eastWall();
	Opening window{OpeningId{Uuid{0, 9}}, OpeningKind::Window, 1000, 1200, 1200, 900, {}, OpeningOperation::Fixed};
	geometry::MeshData mesh = geometry::buildOpeningMesh(wall, geometry::plainFootprint(wall), window);
	CHECK(triangles(mesh, geometry::carpentry_slots::Frame) == 4 * 12);
	CHECK(triangles(mesh, geometry::carpentry_slots::Glass) == 12);
	CHECK(triangles(mesh, geometry::carpentry_slots::Leaf) == 0);
	const geometry::Bounds3 glass = sectionBounds(mesh, geometry::carpentry_slots::Glass);
	CHECK(glass.min.z == doctest::Approx(950.0));
	CHECK(glass.max.z == doctest::Approx(2050.0));
	CHECK(glass.max.y == doctest::Approx(3.0)); // 6 mm pane

	window.widthMm = 1500;
	mesh = geometry::buildOpeningMesh(wall, geometry::plainFootprint(wall), window);
	CHECK(triangles(mesh, geometry::carpentry_slots::Frame) == 5 * 12);
}

TEST_CASE("the scene has a carpentry node per opening, with its finishes, on its level, styled like its wall")
{
	Document document{makeSequentialUuidSource()};
	const LevelId upper = document.addLevel().value();
	const WallId wall = document.addWall({0, 0}, {4000, 0}, 120, 2800, upper).value();
	const OpeningId window =
		document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Window, 1000, 1200, 1200, 900, "aluminium-anthracite"}).value();
	const scene::Scene scene = scene::compileScene(document.project());
	const scene::SceneNode* node = scene.findNode(scene::SceneNodeId{toString(wall) + "/opening/" + toString(window)});
	REQUIRE(node != nullptr);
	CHECK(node->sourceKind == scene::SourceKind::Opening);
	CHECK(node->sourceId == toString(window));
	CHECK(node->level == upper);
	CHECK(geometry::computeBounds(node->mesh).min.z == doctest::Approx(2800.0 + 900.0)); // lifted to its level
	for (std::size_t i = 0; i < node->mesh.sections.size(); ++i)
	{
		CHECK(node->finishes[i] == (node->mesh.sections[i].slot == "glass" ? "glass-clear" : "aluminium-anthracite"));
	}
	CHECK(node->contentHash == scene::hashNode(node->mesh, node->finishes));

	scene::ViewOverlay overlay;
	overlay.selectedWall = wall;
	const scene::Scene selected = scene::compileScene(document.project(), overlay);
	CHECK(selected.findNode(node->id)->style == scene::NodeStyle::Selected);
}

TEST_CASE("wall obstructions: mitred ends, T joints and crossings, merged in whole millimetres")
{
	Project project;
	const Wall host = eastWall(6000);
	project.walls.push_back(host);
	// L at the end: the north wall from (6000, 0).
	project.walls.push_back(Wall{WallId{Uuid{0, 2}}, {6000, 0}, {6000, 3000}, 120, 2800});
	// T at x = 2000 (100 mm thick) and a wall crossing at x = 4000 (100 mm thick).
	project.walls.push_back(Wall{WallId{Uuid{0, 3}}, {2000, 0}, {2000, 3000}, 100, 2800});
	project.walls.push_back(Wall{WallId{Uuid{0, 4}}, {4000, -1500}, {4000, 1500}, 100, 2800});
	const std::vector<AxisSpan> spans = geometry::wallObstructions(project, host);
	REQUIRE(spans.size() == 3);
	CHECK(spans[0] == AxisSpan{1950, 2050}); // the T wall's width
	CHECK(spans[1] == AxisSpan{3950, 4050}); // the crossing
	CHECK(spans[2] == AxisSpan{5940, 6000}); // where the inner face ends at the mitre
	CHECK(geometry::wallObstructions(Project{{}, {host}}, host).empty()); // a wall on its own
}

TEST_CASE_FIXTURE(HostWithTee, "openings added or moved into a joint are refused; a finish change never is")
{
	CHECK(document.addOpening(host, Opening{OpeningId{}, OpeningKind::Door, 1500, 900, 2100, 0}).error().message ==
		"The door runs into a wall joined to this one (1950 to 2050 mm along it)");
	const OpeningId door = document.addOpening(host, Opening{OpeningId{}, OpeningKind::Door, 500, 900, 2100, 0}).value();
	Opening moved = *document.project().findWall(host)->findOpening(door);
	moved.offsetMm = 1800;
	CHECK(document.execute(ChangeOpeningCommand{host, moved}).error().code == ErrorCode::OutOfRange);
	moved.offsetMm = 2100;
	CHECK(document.execute(ChangeOpeningCommand{host, moved}).ok());

	// A geometry edit cannot split through the door; the rejected move leaves both the door and finish edits intact.
	CHECK_FALSE(document.execute(MoveWallEndpointCommand{tee, WallEndpoint::Start, {2500, 0}}).ok());
	Opening refinished = *document.project().findWall(host)->findOpening(door);
	refinished.finish = "wood-oak";
	CHECK(document.execute(ChangeOpeningCommand{host, refinished}).ok());
}

TEST_CASE("an opening reaching a mitre after a wall change is trimmed there, in the wall and its carpentry")
{
	Project project;
	Wall host = eastWall(4000);
	host.openings.push_back(Opening{OpeningId{Uuid{0, 9}}, OpeningKind::Window, 3000, 1000, 1200, 900});
	project.walls.push_back(host);
	project.walls.push_back(Wall{WallId{Uuid{0, 2}}, {4000, 0}, {4000, 3000}, 120, 2800});
	const geometry::WallJoins joins = geometry::solveWallJoins(project);
	const geometry::MeshData carpentry = geometry::buildOpeningMesh(host, joins.footprints[0], host.openings[0]);
	CHECK(geometry::computeBounds(carpentry).max.x == doctest::Approx(3940.0)); // the inner face ends at 3940
	const geometry::MeshData wall = geometry::buildWallMesh(host, joins.footprints[0]);
	const geometry::Bounds3 reveals = sectionBounds(wall, geometry::wall_slots::Reveal);
	CHECK(reveals.max.x == doctest::Approx(3940.0));
}

TEST_CASE_FIXTURE(HostWithTee, "the door tool offers the free stretch nearest to the pointer, clear of joints")
{
	EditController editor{document, EditSettings{}};
	editor.setMode(EditMode::Door);
	editor.pointerMoved(down(1900.0, 30.0)); // centred on the pointer the door would cross the T at 1950..2050
	REQUIRE(editor.openingPlacement().has_value());
	CHECK(editor.openingPlacement()->opening.offsetMm == 1050); // pushed back to end at the T, on the grid
	editor.pointerMoved(down(2200.0, 30.0));
	CHECK(editor.openingPlacement()->opening.offsetMm == 2050); // just after the T
}

TEST_CASE_FIXTURE(HostWithTee, "a selected opening is moved by dragging it and resized by its edge handles")
{
	const OpeningId window = document.addOpening(host, Opening{OpeningId{}, OpeningKind::Window, 3000, 1200, 1200, 900}).value();
	EditController editor{document, EditSettings{}};
	REQUIRE(editor.selectOpening(host, window).ok());

	// Grab the hole and move it 830 mm east: it snaps to the grid, previews, and applies once on release.
	REQUIRE(editor.pointerPressed(down(3600.0, 30.0)).ok());
	CHECK(editor.isDragging());
	editor.pointerMoved(down(4430.0, 30.0));
	REQUIRE(editor.view().previewWalls.size() == 1);
	CHECK(editor.view().previewWalls[0].openings[0].offsetMm == 3850);
	CHECK(editor.view().handles == std::vector<Point2D>{{3850, 0}, {5050, 0}});
	CHECK(document.project().findWall(host)->findOpening(window)->offsetMm == 3000);
	REQUIRE(editor.pointerReleased(down(4430.0, 30.0)).ok());
	CHECK(document.project().findWall(host)->findOpening(window)->offsetMm == 3850);
	CHECK(document.undoLabel() == "Change opening");

	// Dragged against the T, it stops just after it.
	REQUIRE(editor.pointerPressed(down(4400.0, 30.0)).ok());
	editor.pointerMoved(down(2300.0, 30.0));
	REQUIRE(editor.pointerReleased(down(2300.0, 30.0)).ok());
	CHECK(document.project().findWall(host)->findOpening(window)->offsetMm == 2050);

	// The end handle widens it on the grid; the start handle cannot pass the T nor leave less than 300 mm.
	REQUIRE(editor.pointerPressed(down(3250.0, 0.0)).ok()); // the handle at the end edge (3250)
	editor.pointerMoved(down(3712.0, 0.0));
	REQUIRE(editor.pointerReleased(down(3712.0, 0.0)).ok());
	CHECK(document.project().findWall(host)->findOpening(window)->widthMm == 1650);
	REQUIRE(editor.pointerPressed(down(2050.0, 0.0)).ok()); // the start handle
	editor.pointerMoved(down(1000.0, 0.0));
	REQUIRE(editor.pointerReleased(down(1000.0, 0.0)).ok());
	CHECK(document.project().findWall(host)->findOpening(window)->offsetMm == 2050);

	// A click without moving changes nothing; Esc drops a drag.
	const std::string_view before = document.undoLabel();
	REQUIRE(editor.pointerPressed(down(2500.0, 30.0)).ok());
	REQUIRE(editor.pointerReleased(down(2500.0, 30.0)).ok());
	CHECK(document.undoLabel() == before);
	REQUIRE(editor.pointerPressed(down(2500.0, 30.0)).ok());
	editor.pointerMoved(down(4000.0, 30.0));
	editor.cancel();
	CHECK_FALSE(editor.isDragging());
	CHECK(editor.setOpeningFinish("wood-walnut").ok());
	CHECK(document.project().findWall(host)->findOpening(window)->finish == "wood-walnut");
}

TEST_CASE_FIXTURE(HostWithTee, "pressing well inside the hole moves it, even when the ray goes on into an edge handle")
{
	const OpeningId window = document.addOpening(host, Opening{OpeningId{}, OpeningKind::Window, 3000, 1200, 1200, 900}).value();
	EditController editor{document, EditSettings{}};
	REQUIRE(editor.selectOpening(host, window).ok());

	// Low and almost along the wall: it meets the face at 3600 mm and, inside the wall, the post of the end edge (4200).
	const Ray3 oblique{-2000.0, -1000.0, 3000.0, 5600.0, 940.0, -1500.0};
	REQUIRE(intersectPost(oblique, Point2D{4200, 0}, 240.0, 2500.0).has_value());
	REQUIRE(editor.pointerPressed(oblique).ok());
	CHECK(editor.isDraggingOpening());
	editor.pointerMoved(down(3800.0, 30.0));
	REQUIRE(editor.view().previewWalls.size() == 1);
	CHECK(editor.view().previewWalls[0].openings[0].offsetMm == 3200); // moved, not resized
	CHECK(editor.view().previewWalls[0].openings[0].widthMm == 1200);
	editor.cancel();

	// Near an edge, its handle still wins.
	REQUIRE(editor.pointerPressed(down(4180.0, 30.0)).ok());
	editor.pointerMoved(down(4400.0, 30.0));
	REQUIRE(editor.view().previewWalls.size() == 1);
	CHECK(editor.view().previewWalls[0].openings[0].offsetMm == 3000);
	CHECK(editor.view().previewWalls[0].openings[0].widthMm == 1400);
}
