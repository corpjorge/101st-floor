#include "doctest.h"

#include "F101/Cad/Document.h"
#include "F101/Scene/SceneCompiler.h"

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
	CHECK(node.id.value == "00000000-0000-0000-0000-000000000001/body");
	CHECK(node.id == wallBodyNodeId(wallId));
	CHECK(node.sourceKind == SourceKind::Wall);
	CHECK(node.sourceId == "00000000-0000-0000-0000-000000000001");
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

TEST_CASE_FIXTURE(ReferenceProject, "removing a wall removes its node")
{
	REQUIRE(document.removeWall(wallId).ok());
	CHECK(compileScene(document.project()).nodes.empty());
}
