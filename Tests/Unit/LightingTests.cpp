#include "doctest.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Cad/Picking.h"
#include "F101/Geometry/FurnitureBuilder.h"
#include "F101/Geometry/ModelMesh.h"
#include "F101/Scene/SceneCompiler.h"
#include "F101/Exporters/Gltf.h"
#include <algorithm>
#include <cstring>
#include <string>

using namespace f101;
using namespace f101::cad;

namespace
{
	Project litProject()
	{
		Project project;
		project.levels.push_back(Level{LevelId{Uuid{0, 1}}, "Ground", 2800, 250});
		auto fixture = makeFurniture("table", {1000, 2000}, 9000);
		fixture.id = FurnitureId{Uuid{0, 2}};
		fixture.level = project.levels.front().id;
		fixture.elevationMm = 1700;
		fixture.light = FurnitureLight{100, 200, -20, 900, 3500, {255, 224, 185}, true};
		project.furniture.push_back(fixture);
		return project;
	}
}

TEST_CASE("Fixture light and furniture elevation survive saving and legacy projects stay unlit")
{
	const auto project = litProject();
	REQUIRE(validateProject(project));
	const auto serialized = writeProjectJson(project);
	const auto read = readProjectJson(serialized);
	REQUIRE(read);
	CHECK(read.value().furniture == project.furniture);
	CHECK(writeProjectJson(read.value()) == serialized);
	auto legacy = project;
	legacy.furniture.front().light.reset();
	legacy.furniture.front().elevationMm = 0;
	std::string old = writeProjectJson(legacy);
	const auto version = old.find("\"schemaVersion\": 17");
	REQUIRE(version != std::string::npos);
	old.replace(version, std::strlen("\"schemaVersion\": 17"), "\"schemaVersion\": 15");
	const auto restored = readProjectJson(old);
	REQUIRE(restored);
	CHECK_FALSE(restored.value().furniture.front().light);
	CHECK(restored.value().furniture.front().elevationMm == 0);
	for (const std::string bad : {std::string("\"lumens\": -1"), std::string("\"lumens\": 100001"), std::string("\"lumens\": 2.5")})
	{
		auto invalid = serialized;
		invalid.replace(invalid.find("\"lumens\": 900"), std::strlen("\"lumens\": 900"), bad);
		CHECK_FALSE(readProjectJson(invalid));
	}
	auto invalid = project;
	invalid.furniture.front().elevationMm = -1;
	CHECK_FALSE(validateProject(invalid));
	invalid = project;
	invalid.furniture.front().light->radiusMm = 0;
	CHECK_FALSE(validateProject(invalid));
}

TEST_CASE("Fixture movement rotation elevation and levels drive the light position")
{
	auto project = litProject();
	auto scene = scene::compileScene(project);
	REQUIRE(scene.lights.size() == 1);
	CHECK(scene.lights[0].position.x == doctest::Approx(800));
	CHECK(scene.lights[0].position.y == doctest::Approx(2100));
	CHECK(scene.lights[0].position.z == doctest::Approx(1690));
	CHECK(scene.lights[0].lumens == 900);
	const auto id = scene.lights[0].id;
	project.furniture[0].position.x_mm += 500;
	CHECK(scene::compileScene(project).lights[0].position.x == doctest::Approx(1300));
	project.levels.push_back(Level{LevelId{Uuid{0, 3}}, "Upper", 3000, 250});
	project.furniture[0].level = project.levels.back().id;
	scene = scene::compileScene(project);
	CHECK(scene.lights[0].id == id);
	CHECK(scene.lights[0].position.z == doctest::Approx(4490));
	scene::ViewOverlay view;
	view.activeLevel = project.levels.front().id;
	CHECK(scene::compileScene(project, view).lights.empty());
	view.showAllLevels = true;
	CHECK(scene::compileScene(project, view).lights.size() == 1);
	project.furniture[0].light->enabled = false;
	CHECK(scene::compileScene(project).lights.empty());
}

TEST_CASE("Elevated native and placeholder geometry have the same base height")
{
	const auto fixture = litProject().furniture[0];
	for (const auto& mesh : {geometry::buildFurnitureMesh(fixture), geometry::buildPlaceholderFurnitureMesh(fixture)})
	{
		REQUIRE_FALSE(mesh.positions.empty());
		const auto lowest = std::min_element(mesh.positions.begin(), mesh.positions.end(), [](auto a, auto b) { return a.z < b.z; });
		CHECK(lowest->z == doctest::Approx(1710));
	}
}

TEST_CASE("Raised fixtures can be picked at their real height without blocking a floor-level ray")
{
	const auto project = litProject();
	const auto level = project.levels.front().id;
	CHECK(pickFurniture(project, Ray3{1000, 0, 1900, 0, 1, 0}, level).has_value());
	CHECK_FALSE(pickFurniture(project, Ray3{1000, 0, 100, 0, 1, 0}, level));
}

TEST_CASE("Deleting a luminaire removes its light and undo restores it with editable flux")
{
	Document document(makeSequentialUuidSource(100));
	REQUIRE(document.replaceProject(litProject()));
	EditController editor(document, EditSettings{});
	const auto fixture = document.project().furniture[0].id;
	REQUIRE(editor.selectFurniture(fixture));
	REQUIRE(editor.setFurnitureLightLumens(1200));
	CHECK(scene::compileScene(document.project()).lights[0].lumens == 1200);
	CHECK_FALSE(editor.setFurnitureLightLumens(-1));
	REQUIRE(editor.setFurnitureLightEnabled(false));
	CHECK(scene::compileScene(document.project()).lights.empty());
	REQUIRE(document.undo());
	REQUIRE(document.execute(DeleteFurnitureCommand{fixture}));
	CHECK(scene::compileScene(document.project()).lights.empty());
	REQUIRE(document.undo());
	CHECK(scene::compileScene(document.project()).lights.size() == 1);
	REQUIRE(editor.selectFurniture(fixture));
	REQUIRE(editor.setFurnitureElevation("1800 mm"));
	CHECK(scene::compileScene(document.project()).lights[0].position.z == doctest::Approx(1790));
}

TEST_CASE("GLB carries point flux as candela and positions in glTF metres")
{
	const auto project = litProject();
	const auto exported = exporters::writeGlb(scene::compileScene(project), project,
		[](std::string_view) { return exporters::GltfMaterialLook{}; }, {});
	REQUIRE(exported.bytes.size() > 20);
	std::uint32_t length = 0;
	std::memcpy(&length, exported.bytes.data() + 12, sizeof(length));
	const std::string json(reinterpret_cast<const char*>(exported.bytes.data() + 20), length);
	CHECK(json.find("KHR_lights_punctual") != std::string::npos);
	CHECK(json.find("\"type\":\"point\"") != std::string::npos);
	CHECK(json.find("71.6197") != std::string::npos); // 900 lm / 4pi
	CHECK(json.find("1.69") != std::string::npos);
}
