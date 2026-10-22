#include "doctest.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/OpeningCatalogue.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Geometry/RoofBuilder.h"
#include "F101/Geometry/PolygonGeometry.h"
#include "F101/Scene/SceneCompiler.h"
#include "F101/Scene/FinishTextures.h"
#include "F101/Exporters/Gltf.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace f101;
using namespace f101::cad;

namespace
{
	bool insideOrOnEdge(const std::vector<PlanPoint>& polygon, PlanPoint p)
	{
		if (geometry::containsPoint(polygon, p)) return true;
		for (std::size_t i = 0; i < polygon.size(); ++i)
		{
			const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
			const double dx = b.x_mm - a.x_mm, dy = b.y_mm - a.y_mm;
			const double t = std::clamp(((p.x_mm - a.x_mm) * dx + (p.y_mm - a.y_mm) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
			if (std::hypot(p.x_mm - a.x_mm - t * dx, p.y_mm - a.y_mm - t * dy) < .001) return true;
		}
		return false;
	}
	Project coveredRoom()
	{
		Document doc{makeSequentialUuidSource()};
		REQUIRE(doc.addWall({0, 0}, {4000, 0}, 120, 2800));
		REQUIRE(doc.addWall({4000, 0}, {4000, 5000}, 120, 2800));
		REQUIRE(doc.addWall({4000, 5000}, {0, 5000}, 120, 2800));
		REQUIRE(doc.addWall({0, 5000}, {0, 0}, 120, 2800));
		auto p = doc.project();
		p.roomRoofs.push_back({geometry::detectRooms(p).front().key, 300, 0});
		Opening door;
		door.id = OpeningId{Uuid{0, 100}};
		door.widthMm = 2000; door.heightMm = 2300; door.offsetMm = 500;
		REQUIRE(applyOpeningFamily(door, "door-double-glass"));
		door.glazing = "glass-reflective";
		p.walls.front().openings.push_back(door);
		return p;
	}
}

TEST_CASE("Translucent roofs and independent glazing survive save/open, with strict validation")
{
	const auto p = coveredRoom();
	REQUIRE(validateProject(p));
	const auto text = writeProjectJson(p);
	const auto opened = readProjectJson(text);
	REQUIRE(opened);
	CHECK(opened.value().roomRoofs == p.roomRoofs);
	CHECK(opened.value().walls == p.walls);
	CHECK(writeProjectJson(opened.value()) == text);
	for (const auto rise : {-1, 1501})
	{
		auto bad = p; bad.roomRoofs[0].riseMm = rise; CHECK_FALSE(validateProject(bad));
	}
	auto bad = p; bad.roomRoofs.push_back(bad.roomRoofs.front()); CHECK_FALSE(validateProject(bad));
	bad = p; bad.roomRoofs.front().fallQuarterTurns = 4; CHECK_FALSE(validateProject(bad));
	bad = p; bad.walls.front().openings.front().glazing = "wood-oak"; CHECK_FALSE(validateProject(bad));
	for (const auto& mutation : {nlohmann::json("north"), nlohmann::json(1.5), nlohmann::json(true)})
	{
		auto root = nlohmann::json::parse(text); root["roomRoofs"][0]["fallQuarterTurns"] = mutation;
		CHECK_FALSE(readProjectJson(root.dump()));
	}
	auto legacy = nlohmann::json::parse(text);
	legacy["schemaVersion"] = 16; legacy.erase("roomRoofs"); legacy["walls"][0]["openings"][0].erase("glazing");
	const auto old = readProjectJson(legacy.dump()); REQUIRE(old);
	CHECK(old.value().roomRoofs.empty()); CHECK(old.value().walls.front().openings.front().glazing.empty());
}

TEST_CASE("Room roof edits are transactional and preserve their direction through removal undo")
{
	Document doc{makeSequentialUuidSource()}; REQUIRE(doc.replaceProject(coveredRoom()));
	EditController editor(doc, {}); const auto key = doc.project().roomRoofs.front().room;
	REQUIRE(editor.selectRoom(key));
	REQUIRE(editor.setSelectedRoofRise("450 mm")); CHECK(doc.project().roomRoofs.front().riseMm == 450);
	REQUIRE(editor.rotateSelectedRoof()); CHECK(doc.project().roomRoofs.front().fallQuarterTurns == 1);
	REQUIRE(editor.setSelectedRoomRoof(false)); CHECK(doc.project().roomRoofs.empty());
	REQUIRE(doc.undo()); REQUIRE(doc.project().findRoomRoof(key)); CHECK(doc.project().findRoomRoof(key)->fallQuarterTurns == 1);
	REQUIRE(doc.redo()); CHECK(doc.project().roomRoofs.empty());
	REQUIRE(editor.setSelectedRoomRoof(true)); CHECK(doc.project().roomRoofs.front().riseMm == 200);
	const auto before = writeProjectJson(doc.project()); CHECK_FALSE(editor.setSelectedRoofRise("-1 mm"));
	CHECK(writeProjectJson(doc.project()) == before);
}

TEST_CASE("Corrugated sheets follow all fall directions and clip concave rooms without zero triangles")
{
	geometry::Room room; room.key = "roof";
	room.outline = {{0, 0}, {4000, 0}, {4000, 5000}, {2500, 5000}, {2500, 2200}, {0, 2200}};
	for (int direction = 0; direction < 4; ++direction)
	{
		const auto mesh = geometry::buildRoofMesh(room, 2800, {room.key, 300, direction});
		REQUIRE(mesh.sections.size() == 2);
		const auto bounds = geometry::computeBounds(mesh);
		CHECK(bounds.min.z == doctest::Approx(2710)); CHECK(bounds.max.z > 3100); CHECK(bounds.max.z <= 3127.001);
		for (const auto& p : mesh.positions)
		{
			CHECK(std::isfinite(p.z)); CHECK(p.x >= -.001); CHECK(p.x <= 4000.001); CHECK(p.y >= -.001); CHECK(p.y <= 5000.001);
		}
		for (const auto& section : mesh.sections)
			for (std::size_t t = 0; t < section.indices.size(); t += 3)
			{
				const auto a = mesh.positions[section.indices[t]], b = mesh.positions[section.indices[t + 1]], c = mesh.positions[section.indices[t + 2]];
				const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
				const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
				CHECK(nx * nx + ny * ny + nz * nz > 1e-10);
				const auto normal = mesh.normals[section.indices[t]];
				CHECK(nx * normal.x + ny * normal.y + nz * normal.z > 0);
				CHECK(insideOrOnEdge(room.outline, {(a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3}));
			}
	}
}

TEST_CASE("Scene replaces only the top storey's slab with translucent sheets and exports the coating")
{
	auto p = coveredRoom(); const auto key = p.roomRoofs.front().room;
	const auto scene = scene::compileScene(p);
	const auto* roof = scene.findNode({key + "/slab"}); REQUIRE(roof);
	CHECK(roof->finishes == std::vector<std::string>{"polycarbonate-opal", "lacquer-white"});
	CHECK(geometry::computeBounds(roof->mesh).min.z >= 2710);
	const auto* door = scene.findNode({toString(p.walls.front().id) + "/opening/" + toString(p.walls.front().openings.front().id)}); REQUIRE(door);
	CHECK(std::find(door->finishes.begin(), door->finishes.end(), "glass-reflective") != door->finishes.end());
	const auto look = scene::buildFinishLook("glass-reflective", 4); CHECK(look.opacity < 1); CHECK(look.metallic > .5);
	assets::ModelCache models;
	const auto glb = exporters::exportProjectGlb(p, {}, models, {}); REQUIRE(glb);
	std::uint32_t length = 0; std::memcpy(&length, glb.value().bytes.data() + 12, 4);
	const auto root = nlohmann::json::parse(glb.value().bytes.begin() + 20, glb.value().bytes.begin() + 20 + length);
	bool reflected = false;
	for (const auto& material : root["materials"])
		if (material["extras"]["f101FinishId"] == "glass-reflective")
		{ CHECK(material["alphaMode"] == "BLEND"); CHECK(material["pbrMetallicRoughness"]["metallicFactor"] == .65); reflected = true; }
	CHECK(reflected);
	p.levels.push_back({LevelId{Uuid{0, 200}}, "Upper", 2800, 250});
	const auto stacked = scene::compileScene(p); roof = stacked.findNode({key + "/slab"}); REQUIRE(roof);
	CHECK(geometry::computeBounds(roof->mesh).max.z == 2800);
	CHECK(std::find(roof->finishes.begin(), roof->finishes.end(), "polycarbonate-opal") == roof->finishes.end());
}
