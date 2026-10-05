#include <doctest/doctest.h>

#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Geometry/Baseboards.h"
#include "F101/Geometry/PolygonGeometry.h"
#include "F101/Scene/SceneCompiler.h"

#include <algorithm>
#include <cmath>

using namespace f101;
using namespace f101::cad;

namespace
{
	Project roomProject(const Document& document)
	{
		Project project = document.project();
		project.walls = {
			Wall{WallId{Uuid{0, 100}}, {0, 0}, {4000, 0}, 120, 2800},
			Wall{WallId{Uuid{0, 101}}, {4000, 0}, {4000, 3000}, 120, 2800},
			Wall{WallId{Uuid{0, 102}}, {4000, 3000}, {0, 3000}, 120, 2800},
			Wall{WallId{Uuid{0, 103}}, {0, 3000}, {0, 0}, 120, 2800}};
		for (Wall& wall : project.walls) wall.level = project.levels.front().id;
		return project;
	}

	double volume(const geometry::MeshData& mesh)
	{
		double total = 0.0;
		for (const auto& section : mesh.sections)
		{
			for (std::size_t i = 0; i < section.indices.size(); i += 3)
			{
				const auto a = mesh.positions[section.indices[i]], b = mesh.positions[section.indices[i + 1]], c = mesh.positions[section.indices[i + 2]];
				total += (a.x * (b.y * c.z - b.z * c.y) + a.y * (b.z * c.x - b.x * c.z) + a.z * (b.x * c.y - b.y * c.x)) / 6.0;
			}
		}
		return total;
	}
}

TEST_CASE("baseboard settings are independent reversible edits and disabling retains their configuration")
{
	Document document{makeSequentialUuidSource()};
	EditController editor{document, EditSettings{}};
	CHECK_FALSE(editor.setSelectedRoomBaseboard(true).ok());
	REQUIRE(editor.selectRoom("room-a").ok());
	REQUIRE(editor.setSelectedRoomBaseboard(false).ok());
	CHECK(document.project().roomBaseboards.empty());
	CHECK_FALSE(document.canUndo());
	std::vector<std::string> changed;
	document.subscribe([&](const ChangeSet& changes) { changed = changes.changedRoomBaseboards; });
	REQUIRE(editor.setSelectedRoomName("Cocina").ok());
	REQUIRE(editor.setSelectedRoomFloor("tile-grey").ok());
	REQUIRE(editor.setSelectedRoomBaseboard(true).ok());
	CHECK(changed == std::vector<std::string>{"room-a"});
	REQUIRE(editor.setSelectedBaseboardHeight("15 cm").ok());
	REQUIRE(editor.setSelectedBaseboardThickness("18 mm").ok());
	REQUIRE(editor.setSelectedBaseboardFinish("wood-walnut").ok());
	const auto original = *document.project().findRoomBaseboard("room-a");
	CHECK(original.heightMm == 150);
	CHECK(original.thicknessMm == 18);
	REQUIRE(editor.setSelectedRoomBaseboard(false).ok());
	CHECK_FALSE(document.project().findRoomBaseboard("room-a")->enabled);
	REQUIRE(editor.undo().ok());
	CHECK(*document.project().findRoomBaseboard("room-a") == original);
	REQUIRE(editor.redo().ok());
	REQUIRE(editor.setSelectedRoomBaseboard(true).ok());
	CHECK(*document.project().findRoomBaseboard("room-a") == original);
	CHECK(document.project().findRoomName("room-a")->name == "Cocina");
	CHECK(document.project().findRoomFinish("room-a")->floor == "tile-grey");
	CHECK(document.execute(SetRoomBaseboardCommand{original}).value().empty());
	const auto decoded = readProjectJson(writeProjectJson(document.project()));
	REQUIRE(decoded.ok());
	CHECK(decoded.value().roomBaseboards == document.project().roomBaseboards);
}

TEST_CASE("invalid baseboards leave model and history unchanged")
{
	Document document{makeSequentialUuidSource()};
	const auto before = writeProjectJson(document.project());
	CHECK_FALSE(document.execute(SetRoomBaseboardCommand{{"", true, 100, 12, ""}}).ok());
	CHECK_FALSE(document.execute(SetRoomBaseboardCommand{{"a", true, 0, 12, ""}}).ok());
	CHECK_FALSE(document.execute(SetRoomBaseboardCommand{{"a", true, 1001, 12, ""}}).ok());
	CHECK_FALSE(document.execute(SetRoomBaseboardCommand{{"a", true, 100, -1, ""}}).ok());
	CHECK_FALSE(document.execute(SetRoomBaseboardCommand{{"a", true, 100, 101, ""}}).ok());
	CHECK_FALSE(document.execute(SetRoomBaseboardCommand{{"a", true, 100, 12, "glass-clear"}}).ok());
	CHECK(writeProjectJson(document.project()) == before);
	CHECK_FALSE(document.canUndo());
}

TEST_CASE("legacy projects have no baseboards and malformed persisted settings are rejected")
{
	const std::string prefix = R"({"format":"101st-floor-project","schemaVersion":12,"units":"mm","levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[],"roomBaseboards":)";
	for (const auto entry : {R"({})", R"([{"room":"a","enabled":"yes","heightMm":100,"thicknessMm":12}])",
		R"([{"room":"a","enabled":true,"heightMm":100.5,"thicknessMm":12}])",
		R"([{"room":"a","enabled":true,"heightMm":100,"thicknessMm":0}])",
		R"([{"room":"a","enabled":true,"heightMm":100,"thicknessMm":12,"finish":"missing"}])",
		R"([{"room":"a","enabled":true,"heightMm":100,"thicknessMm":12},{"room":"a","enabled":false,"heightMm":100,"thicknessMm":12}])"})
	{
		CHECK_FALSE(readProjectJson(prefix + entry + "}").ok());
	}
	const auto legacy = readProjectJson(R"({"format":"101st-floor-project","schemaVersion":11,"units":"mm","levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[]})");
	REQUIRE(legacy.ok());
	CHECK(legacy.value().roomBaseboards.empty());
}

TEST_CASE("baseboards rise with their level and an opening on a reversed wall still cuts correctly")
{
	Document document{makeSequentialUuidSource()};
	Project project = roomProject(document);
	std::swap(project.walls[0].start, project.walls[0].end);
	const auto room = geometry::detectRooms(project).front();
	const RoomBaseboard setting{room.key, true, 100, 12, ""};
	const double whole = volume(geometry::buildBaseboardMesh(room, project, setting));
	project.walls[0].openings = {Opening{OpeningId{Uuid{0, 150}}, OpeningKind::Door, 1000, 900, 2000, 0}};
	CHECK(volume(geometry::buildBaseboardMesh(room, project, setting)) == doctest::Approx(whole - 900.0 * 12.0 * 100.0));
	const LevelId upper{Uuid{0, 200}};
	project.levels.push_back(Level{upper, "Upper", 2800, 250});
	for (auto& wall : project.walls) wall.level = upper;
	project.roomBaseboards.push_back(setting);
	const auto compiled = scene::compileScene(project);
	const auto node = std::find_if(compiled.nodes.begin(), compiled.nodes.end(), [](const auto& n) { return n.sourceKind == scene::SourceKind::Baseboard; });
	REQUIRE(node != compiled.nodes.end());
	CHECK(geometry::computeBounds(node->mesh).min.z == 2810.0);
	CHECK(geometry::computeBounds(node->mesh).max.z == 2910.0);
}

TEST_CASE("baseboards form a mitred ring of exact thickness resting on the floor")
{
	Document document{makeSequentialUuidSource()};
	const Project project = roomProject(document);
	const auto room = geometry::detectRooms(project).front();
	const auto mesh = geometry::buildBaseboardMesh(room, project, {room.key, true, 100, 12, ""});
	REQUIRE_FALSE(mesh.positions.empty());
	const auto bounds = geometry::computeBounds(mesh);
	CHECK(bounds.min.x == 60.0);
	CHECK(bounds.max.x == 3940.0);
	CHECK(bounds.min.z == geometry::FloorThicknessMm);
	CHECK(bounds.max.z == geometry::FloorThicknessMm + 100.0);
	CHECK(volume(mesh) == doctest::Approx((3880.0 * 2880.0 - 3856.0 * 2856.0) * 100.0));
	for (const auto& n : mesh.normals) CHECK(std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z) == doctest::Approx(1.0));
	CHECK(geometry::detectRooms(project).front().areaMm2 == room.areaMm2);
	CHECK(geometry::buildBaseboardMesh(room, project, {room.key, false}).positions.empty());
}

TEST_CASE("doors interrupt baseboards; high windows do not and low windows cut only the affected height")
{
	Document document{makeSequentialUuidSource()};
	Project project = roomProject(document);
	const auto room = geometry::detectRooms(project).front();
	const RoomBaseboard setting{room.key, true, 100, 12, ""};
	const double whole = volume(geometry::buildBaseboardMesh(room, project, setting));
	project.walls[0].openings = {Opening{OpeningId{Uuid{0, 150}}, OpeningKind::Door, 1000, 900, 2000, 0}};
	const auto door = geometry::buildBaseboardMesh(room, project, setting);
	CHECK(volume(door) == doctest::Approx(whole - 900.0 * 12.0 * 100.0));
	for (const auto& p : door.positions) CHECK_FALSE((p.x > 1000.0 && p.x < 1900.0 && p.y < 72.0));
	project.walls[0].openings[0].kind = OpeningKind::Window;
	project.walls[0].openings[0].sillMm = 800;
	CHECK(volume(geometry::buildBaseboardMesh(room, project, setting)) == doctest::Approx(whole));
	project.walls[0].openings[0].sillMm = 60;
	CHECK(volume(geometry::buildBaseboardMesh(room, project, setting)) == doctest::Approx(whole - 900.0 * 12.0 * 50.0));
}

TEST_CASE("baseboards follow angled walls and concave room corners")
{
	Document document{makeSequentialUuidSource()};
	Project project = roomProject(document);
	const Point2D corners[] = {{0, 0}, {3000, 3000}, {0, 6000}, {-3000, 3000}};
	for (std::size_t i = 0; i < 4; ++i) { project.walls[i].start = corners[i]; project.walls[i].end = corners[(i + 1) % 4]; }
	const auto room = geometry::detectRooms(project).front();
	const RoomBaseboard setting{room.key, true, 100, 12, ""};
	const double whole = volume(geometry::buildBaseboardMesh(room, project, setting));
	project.walls[0].openings = {Opening{OpeningId{Uuid{0, 150}}, OpeningKind::Door, 1000, 900, 2000, 0}};
	CHECK(volume(geometry::buildBaseboardMesh(room, project, setting)) == doctest::Approx(whole - 900.0 * 12.0 * 100.0));
	geometry::Room concave;
	concave.outline = {{0, 0}, {4000, 0}, {4000, 1500}, {2000, 1500}, {2000, 3000}, {0, 3000}};
	const auto mesh = geometry::buildBaseboardMesh(concave, {}, setting);
	REQUIRE_FALSE(mesh.positions.empty());
	CHECK(volume(mesh) > 0.0);
	for (const auto& p : mesh.positions) CHECK_FALSE((p.x > 2000.0 && p.y > 1500.0));
}

TEST_CASE("room division inherits baseboards atomically and scene includes the finish")
{
	Document document{makeSequentialUuidSource()};
	document.setRoomBaseboardTransfer(geometry::makeRoomBaseboardTransfer());
	Project project = roomProject(document);
	project.roomBaseboards.push_back({geometry::detectRooms(project).front().key, true, 120, 18, "wood-walnut"});
	REQUIRE(document.replaceProject(project).ok());
	REQUIRE(document.addWall({2000, 0}, {2000, 3000}, 120, 2800).ok());
	const auto rooms = geometry::detectRooms(document.project());
	REQUIRE(rooms.size() == 2);
	for (const auto& room : rooms)
	{
		const auto* setting = document.project().findRoomBaseboard(room.key);
		REQUIRE(setting != nullptr);
		CHECK(setting->heightMm == 120);
		CHECK(setting->finish == "wood-walnut");
	}
	const auto scene = scene::compileScene(document.project());
	CHECK(std::count_if(scene.nodes.begin(), scene.nodes.end(), [](const auto& node) { return node.sourceKind == scene::SourceKind::Baseboard; }) == 2);
	for (const auto& node : scene.nodes)
	{
		if (node.sourceKind == scene::SourceKind::Baseboard)
		{
			CHECK(node.finishes == std::vector<std::string>{"wood-walnut"});
			CHECK(geometry::computeBounds(node.mesh).max.z == 130.0);
		}
	}
	REQUIRE(document.undo().ok());
	CHECK(document.project().roomBaseboards == project.roomBaseboards);
	REQUIRE(document.redo().ok());
	CHECK(geometry::detectRooms(document.project()).size() == 2);
}
