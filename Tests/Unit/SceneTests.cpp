#include "doctest.h"

#include "F101/Cad/Document.h"
#include "F101/Scene/SceneCompiler.h"

#include <utility>
#include <vector>

using namespace f101;
using namespace f101::scene;

namespace
{
	struct ReferenceProject
	{
		cad::Document document{cad::makeSequentialUuidSource()};
		cad::WallId wallId = document.addWall({0, 0}, {3550, 0}, 120, 2400).value();
	};
}

TEST_CASE_FIXTURE(ReferenceProject, "the reference project compiles to one wall node")
{
	const Scene scene = compileScene(document.project());
	REQUIRE(scene.nodes.size() == 1);
	const SceneNode& node = scene.nodes[0];
	CHECK(node.id.value == "00000000-0000-0000-0000-000000000002/body"); // id 1 is the ground level
	CHECK(node.id == wallBodyNodeId(wallId));
	CHECK(node.sourceKind == SourceKind::Wall);
	CHECK(node.sourceId == "00000000-0000-0000-0000-000000000002");
	CHECK(node.level == document.project().levels[0].id);
	CHECK(node.name == "Wall 00000000");
	CHECK(node.mesh.vertexCount() == 24);
	CHECK(node.contentHash == hashMesh(node.mesh));
	CHECK(scene.findNode(node.id) == &node);
	CHECK(scene.findNode(SceneNodeId{"missing"}) == nullptr);
}

TEST_CASE_FIXTURE(ReferenceProject, "scene geometry stays in millimetres")
{
	const geometry::Bounds3 b = geometry::computeBounds(compileScene(document.project()).nodes[0].mesh);
	CHECK(b.max.x - b.min.x == 3550.0);
	CHECK(b.max.y - b.min.y == 120.0);
	CHECK(b.max.z - b.min.z == 2400.0);
}

TEST_CASE_FIXTURE(ReferenceProject, "node ids are deterministic; the hash follows the geometry")
{
	const Scene first = compileScene(document.project());
	const Scene again = compileScene(document.project());
	CHECK(again.nodes[0].id == first.nodes[0].id);
	CHECK(again.nodes[0].contentHash == first.nodes[0].contentHash);

	REQUIRE(document.setWallEnd(wallId, {4200, 0}).ok());
	const Scene edited = compileScene(document.project());
	CHECK(edited.nodes[0].id == first.nodes[0].id);
	CHECK(edited.nodes[0].contentHash != first.nodes[0].contentHash);
	CHECK(geometry::computeBounds(edited.nodes[0].mesh).max.x == 4200.0);

	REQUIRE(document.setWallEnd(wallId, {3550, 0}).ok());
	CHECK(compileScene(document.project()).nodes[0].contentHash == first.nodes[0].contentHash);
}

TEST_CASE_FIXTURE(ReferenceProject, "the view overlay styles nodes, adds previews joined to the project, and markers")
{
	ViewOverlay overlay;
	overlay.selectedWall = wallId;
	overlay.previewWalls = {cad::Wall{cad::WallId{}, {3550, 0}, {3550, 3000}, 120, 2400}}; // a new wall: no id yet
	overlay.markers = {{0, 0}, {3550, 0}};

	const Scene scene = compileScene(document.project(), overlay);
	REQUIRE(scene.nodes.size() == 4);
	CHECK(scene.nodes[0].style == NodeStyle::Selected);
	CHECK(scene.nodes[1].id.value == "preview/wall/0");
	CHECK(scene.nodes[1].style == NodeStyle::Preview);
	CHECK(geometry::computeBounds(scene.nodes[1].mesh).max.y == 3000.0);
	// The preview is joined like a real wall: the L corner is mitred in both.
	CHECK(geometry::computeBounds(scene.nodes[0].mesh).max.x == doctest::Approx(3610.0));
	CHECK(geometry::computeBounds(scene.nodes[1].mesh).min.y == doctest::Approx(-60.0));
	CHECK(scene.nodes[2].id.value == "marker/0");
	CHECK(scene.nodes[3].style == NodeStyle::Handle);
	const geometry::Bounds3 marker = geometry::computeBounds(scene.nodes[3].mesh);
	CHECK(marker.min.x == 3430.0);
	CHECK(marker.max.x == 3670.0);
	CHECK(marker.max.z == 2500.0);

	// The project scene is unchanged by the overlay.
	CHECK(compileScene(document.project()).nodes.size() == 1);
	CHECK(compileScene(document.project()).nodes[0].style == NodeStyle::Normal);
}

TEST_CASE_FIXTURE(ReferenceProject, "a previewed wall replaces the wall with its id; invalid previews are left out")
{
	ViewOverlay overlay;
	overlay.selectedWall = wallId;
	overlay.previewWalls = {cad::Wall{wallId, {0, 0}, {4200, 0}, 120, 2400}, cad::Wall{cad::WallId{}, {0, 0}, {0, 0}, 120, 2400}};
	const Scene scene = compileScene(document.project(), overlay);
	REQUIRE(scene.nodes.size() == 1);
	CHECK(scene.nodes[0].id == wallBodyNodeId(wallId)); // same node, so the same actor is updated in place
	CHECK(scene.nodes[0].style == NodeStyle::Preview);  // the preview wins over the selection
	CHECK(geometry::computeBounds(scene.nodes[0].mesh).max.x == 4200.0);
	CHECK(document.project().walls[0].end == cad::Point2D{3550, 0});
}

TEST_CASE("walls come first, then joint fills, then the floors of the rooms they enclose, then their slabs")
{
	cad::Document document{cad::makeSequentialUuidSource()};
	for (const auto& [start, end] : std::vector<std::pair<cad::Point2D, cad::Point2D>>{
			 {{0, 0}, {4000, 0}}, {{4000, 0}, {4000, 3000}}, {{4000, 3000}, {0, 3000}}, {{0, 3000}, {0, 0}},
			 {{2000, 0}, {2000, 1500}}, {{2000, 1500}, {2000, 3000}}, {{2000, 1500}, {4000, 1500}}})
	{
		REQUIRE(document.addWall(start, end, 120, 2400).ok());
	}
	const Scene scene = compileScene(document.project());
	REQUIRE(scene.nodes.size() == 7 + 1 + 3 + 3);
	const SceneNode& joint = scene.nodes[7];
	CHECK(joint.sourceKind == SourceKind::Joint);
	CHECK(joint.id.value == "00000000-0000-0000-0000-000000000006/joint-end"); // three ends meet at (2000, 1500); id 1 is the level
	CHECK(geometry::computeBounds(joint.mesh).max.z == 2400.0);
	REQUIRE(scene.rooms.size() == 3);
	for (std::size_t i = 0; i < 3; ++i)
	{
		const SceneNode& floor = scene.nodes[8 + i];
		CHECK(floor.sourceKind == SourceKind::Room);
		CHECK(floor.id.value == scene.rooms[i].room.key + "/floor");
		CHECK(floor.sourceId == scene.rooms[i].room.key);
		CHECK(geometry::computeBounds(floor.mesh).max.z == geometry::FloorThicknessMm);
		CHECK(scene.rooms[i].level == document.project().levels[0].id);
		CHECK(scene.rooms[i].elevationMm == 0);
		// The slab over it fills the top band of the level: 2800 mm high, 250 mm thick.
		const SceneNode& slab = scene.nodes[11 + i];
		CHECK(slab.sourceKind == SourceKind::Slab);
		CHECK(slab.id.value == scene.rooms[i].room.key + "/slab");
		CHECK(geometry::computeBounds(slab.mesh).min.z == 2550.0);
		CHECK(geometry::computeBounds(slab.mesh).max.z == 2800.0);
	}
	double total = 0.0;
	for (const SceneRoom& room : scene.rooms)
	{
		total += room.room.areaMm2;
	}
	CHECK(total == doctest::Approx(1880.0 * 2880.0 + 2 * 1880.0 * 1380.0));
	CHECK(compileScene(document.project()).nodes[10].contentHash == scene.nodes[10].contentHash); // deterministic
}

TEST_CASE_FIXTURE(ReferenceProject, "removing a wall removes its node")
{
	REQUIRE(document.removeWall(wallId).ok());
	CHECK(compileScene(document.project()).nodes.empty());
}
