#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Cad/OpeningCatalogue.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Geometry/CarpentryBuilder.h"
#include "F101/Geometry/WallJoins.h"
#include "F101/Scene/SceneCompiler.h"

#include <cmath>
#include <set>

using namespace f101;
using namespace f101::cad;

namespace
{
	Opening familyOpening(const OpeningFamily& family)
	{
		Opening opening{OpeningId{Uuid{0, 10}}, family.kind, 3000, family.widthMm, family.heightMm, family.sillMm};
		REQUIRE(applyOpeningFamily(opening, family.id).ok());
		return opening;
	}

	Wall hostWall()
	{
		return Wall{WallId{Uuid{0, 20}}, {0, 0}, {10000, 0}, 150, 3000};
	}

	std::size_t paneCount(const geometry::MeshData& mesh)
	{
		for (const auto& section : mesh.sections)
		{
			if (section.slot == "glass") { return section.indices.size() / 36; }
		}
		return 0;
	}
}

TEST_CASE("the opening catalogue has twelve distinct door families and twelve window families")
{
	std::set<std::string_view> ids;
	std::set<std::string_view> names;
	int doors = 0;
	int windows = 0;
	for (const auto& family : openingCatalogue())
	{
		CHECK(ids.insert(family.id).second);
		CHECK(names.insert(family.name).second);
		CHECK(findOpeningFamily(family.id) == &family);
		family.kind == OpeningKind::Door ? ++doors : ++windows;
		Wall wall = hostWall();
		wall.openings = {familyOpening(family)};
		CHECK(validateWall(wall).ok());
	}
	CHECK(doors == 12);
	CHECK(windows == 12);
	CHECK(findOpeningFamily("unknown") == nullptr);
}

TEST_CASE("all opening families regenerate valid meshes and individual panes at different sizes and openness")
{
	for (const auto& family : openingCatalogue())
	{
		CAPTURE(family.id);
		for (const std::int64_t width : {300, 900, 2400})
		{
			for (const int percent : {0, 45, 100})
			{
				Wall wall = hostWall();
				Opening opening = familyOpening(family);
				opening.widthMm = width;
				opening.openPercent = family.operation == OpeningOperation::Fixed ? 0 : percent;
				const auto mesh = geometry::buildOpeningMesh(wall, geometry::plainFootprint(wall), opening);
				CHECK(mesh.triangleCount() > 0);
				CHECK(mesh.positions.size() == mesh.normals.size());
				CHECK(mesh.positions.size() == mesh.uvs.size());
				for (const auto& p : mesh.positions)
				{
					CHECK(std::isfinite(p.x));
					CHECK(std::isfinite(p.y));
					CHECK(std::isfinite(p.z));
					CHECK(p.z >= opening.sillMm);
					CHECK(p.z <= opening.sillMm + opening.heightMm);
				}
				const int leaves = family.operation == OpeningOperation::DoubleHinged ||
					(family.kind == OpeningKind::Window && family.operation == OpeningOperation::Sliding) ? 2 : 1;
				CHECK(paneCount(mesh) == (family.design == OpeningDesign::Glazed ? static_cast<std::size_t>(family.columns * family.rows * leaves) : 0));
				for (const auto& section : mesh.sections)
				{
					for (std::size_t i = 0; i < section.indices.size(); i += 3)
					{
						const auto& a = mesh.positions[section.indices[i]];
						const auto& b = mesh.positions[section.indices[i + 1]];
						const auto& c = mesh.positions[section.indices[i + 2]];
						const double x = (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y);
						const double y = (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z);
						const double z = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
						CHECK(x * x + y * y + z * z > 0.0);
					}
				}
			}
		}
	}
}

TEST_CASE("opening families persist in schema fifteen and older openings retain their original carpentry")
{
	Document document{makeSequentialUuidSource()};
	Project project = document.project();
	Wall wall = hostWall();
	wall.level = project.levels.front().id;
	project.walls = {wall};
	for (const auto& family : openingCatalogue())
	{
		project.walls.front().openings = {familyOpening(family)};
		const std::string json = writeProjectJson(project);
		const auto restored = readProjectJson(json);
		REQUIRE(restored.ok());
		CHECK(writeProjectJson(restored.value()) == json);
		const auto scene = scene::compileScene(restored.value());
		CHECK_FALSE(scene.nodes.empty());
		CHECK_FALSE(drawing::buildPlanDrawing(restored.value(), wall.level).strokes.empty());
	}
	Opening legacy{OpeningId{Uuid{0, 10}}, OpeningKind::Door, 1000, 900, 2100, 0};
	project.walls.front().openings = {legacy};
	std::string old = writeProjectJson(project);
	const auto at = old.find("\"schemaVersion\": 15");
	REQUIRE(at != std::string::npos);
	old.replace(at, std::string("\"schemaVersion\": 15").size(), "\"schemaVersion\": 14");
	const auto restored = readProjectJson(old);
	REQUIRE(restored.ok());
	CHECK(restored.value().walls.front().openings.front() == legacy);
	CHECK(old.find("\"family\"") == std::string::npos);
	project.walls.front().openings.front().family = "unknown";
	CHECK_FALSE(readProjectJson(writeProjectJson(project)).ok());
	project.walls.front().openings.front().family = "window-fixed";
	CHECK_FALSE(readProjectJson(writeProjectJson(project)).ok());
}

TEST_CASE("catalogue selection follows preview and placement and edits existing openings in one undoable step")
{
	Document document{makeSequentialUuidSource()};
	const auto wall = document.addWall({0, 0}, {10000, 0}, 150, 3000).value();
	EditController editor{document, EditSettings{}};
	editor.setMode(EditMode::Door);
	REQUIRE(editor.setNewOpeningFamily(OpeningKind::Door, "door-glass-grid").ok());
	REQUIRE(editor.setNewOpeningWidth("110 cm").ok());
	REQUIRE(editor.setNewOpeningHeight("220 cm").ok());
	CHECK_FALSE(editor.setNewOpeningWidth("-1 m").ok());
	CHECK_FALSE(editor.setNewOpeningSill("10 cm").ok());
	CHECK_FALSE(editor.setNewOpeningFamily(OpeningKind::Door, "window-fixed").ok());
	const Ray3 ray{5000.0, 30.0, 20000.0, 0.0, 0.0, -1.0};
	editor.pointerMoved(ray);
	REQUIRE(editor.openingPlacement().has_value());
	CHECK(editor.openingPlacement()->opening.family == "door-glass-grid");
	REQUIRE(editor.pointerPressed(ray).ok());
	const Opening placed = document.project().findWall(wall)->openings.front();
	CHECK(placed.family == "door-glass-grid");
	CHECK(placed.widthMm == 1100);
	CHECK(placed.heightMm == 2200);
	editor.setMode(EditMode::Select);
	REQUIRE(editor.selectOpening(wall, placed.id).ok());
	REQUIRE(editor.setOpeningFamily("door-double-panels").ok());
	const Opening changed = document.project().findWall(wall)->openings.front();
	CHECK(changed.widthMm == placed.widthMm);
	CHECK(changed.operation == OpeningOperation::DoubleHinged);
	REQUIRE(document.undo().ok());
	CHECK(*document.project().findWall(wall)->findOpening(placed.id) == placed);
	REQUIRE(document.redo().ok());
	CHECK(document.project().findWall(wall)->findOpening(placed.id)->family == "door-double-panels");
	REQUIRE(editor.setOpeningOperation(OpeningOperation::Hinged).ok());
	CHECK(document.project().findWall(wall)->findOpening(placed.id)->family.empty());
	editor.setMode(EditMode::Window);
	REQUIRE(editor.setNewOpeningFamily(OpeningKind::Window, "window-fixed-grid").ok());
	REQUIRE(editor.setNewOpeningSill("80 cm").ok());
	CHECK(editor.settings().window.sillMm == 800);
	CHECK(editor.settings().door.family == "door-glass-grid");
}

TEST_CASE("a family change that needs an obstructed pocket fails without changing the opening or undo history")
{
	Document document{makeSequentialUuidSource()};
	const auto wall = document.addWall({0, 0}, {4000, 0}, 150, 2800).value();
	const auto id = document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Door, 0, 900, 2100, 0}).value();
	EditController editor{document, EditSettings{}};
	REQUIRE(editor.selectOpening(wall, id).ok());
	const auto before = writeProjectJson(document.project());
	const auto undo = document.undoLabel();
	CHECK_FALSE(editor.setOpeningFamily("door-pocket-glass").ok());
	CHECK(writeProjectJson(document.project()) == before);
	CHECK(document.undoLabel() == undo);
}
