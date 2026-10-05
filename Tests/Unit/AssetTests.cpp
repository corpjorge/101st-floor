#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Geometry/ModelMesh.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Scene/FinishTextures.h"
#include "F101/Scene/SceneCompiler.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::cad;

// Imported assets in the model (ADR-0017): references, commands, project.json schema 8, the scene and the editor.
namespace
{
	Ray3 down(double x, double y)
	{
		return Ray3{x, y, 10000.0, 0.0, 0.0, -1.0};
	}

	Asset texture(std::uint64_t n, std::uint8_t surfaces = AllTextureSurfaces)
	{
		Asset asset;
		asset.id = AssetId{Uuid{0xa55e7, n}};
		asset.kind = AssetKind::Texture;
		asset.name = "Parquet " + std::to_string(n);
		asset.file = assetFolder(asset.id) + "/parquet.png";
		asset.texture.widthMm = 1200;
		asset.texture.heightMm = 800;
		asset.texture.surfaces = surfaces;
		asset.texture.pixelWidth = 512;
		asset.texture.pixelHeight = 341;
		return asset;
	}

	Asset model(std::uint64_t n, bool againstWall = false)
	{
		Asset asset;
		asset.id = AssetId{Uuid{0x40de1, n}};
		asset.kind = AssetKind::Model;
		asset.name = "Armchair";
		asset.file = assetFolder(asset.id) + "/armchair.glb";
		asset.model.widthMm = 800;
		asset.model.depthMm = 700;
		asset.model.heightMm = 900;
		asset.model.triangles = 1234;
		asset.model.againstWall = againstWall;
		return asset;
	}

	struct Library
	{
		Document document{makeSequentialUuidSource()};
		EditController editor{document, EditSettings{}};
		Asset parquet = texture(1);
		Asset floorOnly = texture(2, static_cast<std::uint8_t>(Surface::Floor));
		Asset chair = model(3);
		WallId wall = document.addWall({0, 0}, {6000, 0}, 200, 2600).value();

		Library()
		{
			for (const Asset& asset : {parquet, floorOnly, chair})
			{
				REQUIRE(document.execute(AddAssetCommand{asset}).ok());
			}
		}
	};
}

TEST_CASE("asset references, folders and paths stay inside the project")
{
	const AssetId id{Uuid{0x1234, 0x5678}};
	CHECK(assetRef(id) == "asset:00000000-0000-1234-0000-000000005678");
	CHECK(parseAssetRef(assetRef(id)) == id);
	CHECK_FALSE(parseAssetRef("brick-red").has_value());
	CHECK_FALSE(parseAssetRef("asset:not-a-uuid").has_value());
	CHECK_FALSE(parseAssetRef("asset:00000000-0000-0000-0000-000000000000").has_value()); // nil
	CHECK_FALSE(isAssetRef(modelMaterialFinish(id, 2)));
	CHECK(modelMaterialFinish(id, 2) == assetRef(id) + "#2");
	CHECK(assetFolder(id) == "assets/00000000-0000-1234-0000-000000005678");

	CHECK(validateAssetPath("assets/x/sofa.glb").ok());
	CHECK(validateAssetPath("assets/x/tex/wood grain ñ.png").ok());
	for (const char* bad : {"", "/assets/x.png", "assets/../x.png", "assets//x.png", "assets/./x.png", "C:/x.png",
			 "assets\\x.png", "assets/x/"})
	{
		CAPTURE(bad);
		CHECK_FALSE(validateAssetPath(bad).ok());
	}

	Asset asset = texture(1);
	CHECK(validateAsset(asset).ok());
	asset.file = "assets/someone-else/parquet.png";
	CHECK_FALSE(validateAsset(asset).ok()); // outside its own folder
	asset = texture(1);
	asset.name.clear();
	CHECK_FALSE(validateAsset(asset).ok());
	asset = texture(1);
	asset.texture.surfaces = static_cast<std::uint8_t>(Surface::Glazing);
	CHECK_FALSE(validateAsset(asset).ok());
	asset = texture(1);
	asset.texture.roughnessPercent = 101;
	CHECK_FALSE(validateAsset(asset).ok());
	asset = texture(1);
	asset.texture.widthMm = 0;
	CHECK_FALSE(validateAsset(asset).ok());
	Asset chair = model(1);
	CHECK(validateAsset(chair).ok());
	chair.model.frontQuarterTurns = 4;
	CHECK_FALSE(validateAsset(chair).ok());
	CHECK(millimetersPerUnit(ModelUnit::Inch) == doctest::Approx(25.4));
	// From the built-in library (ADR-0019): its id is lower-case letters, digits and hyphens.
	chair = model(1);
	chair.library = "chair-windsor-2";
	CHECK(validateAsset(chair).ok());
	for (const char* bad : {"Chair", "chair windsor", "silla_1", "sillón"})
	{
		CAPTURE(bad);
		chair.library = bad;
		CHECK_FALSE(validateAsset(chair).ok());
	}
	CHECK(isLibraryId(std::string(64, 'a')));
	CHECK_FALSE(isLibraryId(std::string(65, 'a')));
}

TEST_CASE_FIXTURE(Library, "imported textures are finishes of the surfaces they allow; models are furniture types")
{
	const std::string parquetRef = assetRef(parquet.id);
	CHECK(validateFinishIn(document.project(), parquetRef, Surface::Wall).ok());
	CHECK(validateFinishIn(document.project(), assetRef(floorOnly.id), Surface::Floor).ok());
	CHECK(validateFinishIn(document.project(), assetRef(floorOnly.id), Surface::Wall).error().code == ErrorCode::InvalidArgument);
	CHECK(validateFinishIn(document.project(), assetRef(chair.id), Surface::Wall).error().code == ErrorCode::NotFound);
	CHECK(validateFinishIn(document.project(), "asset:00000000-0000-0000-0000-0000000000ff", Surface::Wall).error().code ==
		ErrorCode::NotFound);
	CHECK(validateFinish(parquetRef, Surface::Wall).ok()); // without the project: only the form

	REQUIRE(document.execute(SetWallFinishCommand{wall, WallSide::Left, parquetRef}).ok());
	CHECK(document.project().walls[0].finishLeft == parquetRef);
	CHECK(document.execute(SetWallFinishCommand{wall, WallSide::Right, assetRef(floorOnly.id)}).error().code ==
		ErrorCode::InvalidArgument);
	CHECK_FALSE(document.execute(SetRoomFinishCommand{"room-x", assetRef(chair.id), {}}).ok());
	REQUIRE(document.execute(SetRoomFinishCommand{"room-x", assetRef(floorOnly.id), {}}).ok());

	const std::string chairRef = assetRef(chair.id); // the spec views the type it was asked for
	const std::optional<FurnitureType> spec = furnitureSpec(document.project(), chairRef);
	REQUIRE(spec.has_value());
	CHECK(spec->widthMm == 800);
	CHECK(spec->maxHeightMm == MaxModelSizeMm);
	CHECK_FALSE(spec->againstWall);
	CHECK_FALSE(furnitureSpec(document.project(), assetRef(parquet.id)).has_value());
	CHECK(furnitureSpec(document.project(), "sofa")->widthMm == 2000);

	Furniture piece = makeFurniture(*spec, {2000, 2000});
	CHECK(piece.type == assetRef(chair.id));
	CHECK(piece.heightMm == 900);
	const FurnitureId placed = document.addFurniture(piece).value();
	Furniture stretched = *document.project().findFurniture(placed);
	stretched.widthMm = 50'000;
	REQUIRE(document.execute(ChangeFurnitureCommand{stretched}).ok());
	stretched.widthMm = 5;
	CHECK(document.execute(ChangeFurnitureCommand{stretched}).error().code == ErrorCode::OutOfRange);
	Furniture ghost = piece;
	ghost.type = "asset:00000000-0000-0000-0000-0000000000ff";
	CHECK(document.addFurniture(ghost).error().code == ErrorCode::NotFound);
	Furniture painted = piece;
	painted.finish = parquetRef;
	CHECK(document.addFurniture(painted).ok());
	CHECK(assetUses(document.project(), parquet.id) == 2);
	CHECK(assetUses(document.project(), chair.id) == 2);
}

TEST_CASE_FIXTURE(Library, "assets are imported, changed and deleted as undoable commands; one in use stays")
{
	CHECK(document.undoLabel() == "Import asset");
	CHECK(document.project().assets.size() == 3);
	CHECK(document.execute(AddAssetCommand{parquet}).error().message.find("Duplicate asset id") == 0);
	Asset nil = texture(9);
	nil.id = AssetId{};
	CHECK_FALSE(document.execute(AddAssetCommand{nil}).ok());

	std::vector<ChangeSet> seen;
	document.subscribe([&seen](const ChangeSet& changes) { seen.push_back(changes); });
	Asset renamed = parquet;
	renamed.name = "Roble claro";
	renamed.texture.widthMm = 600;
	REQUIRE(document.execute(ChangeAssetCommand{renamed}).ok());
	CHECK(document.undoLabel() == "Change asset");
	CHECK(document.project().findAsset(parquet.id)->name == "Roble claro");
	REQUIRE(seen.size() == 1);
	CHECK(seen[0].modifiedAssets == std::vector<AssetId>{parquet.id});
	Asset moved = parquet;
	moved.file = assetFolder(parquet.id) + "/other.png";
	CHECK_FALSE(document.execute(ChangeAssetCommand{moved}).ok()); // its file stays

	// A texture used on a wall cannot stop allowing walls; nor can it be deleted while used.
	REQUIRE(document.execute(SetWallFinishCommand{wall, WallSide::Left, assetRef(parquet.id)}).ok());
	Asset narrowed = renamed;
	narrowed.texture.surfaces = static_cast<std::uint8_t>(Surface::Floor);
	CHECK_FALSE(document.execute(ChangeAssetCommand{narrowed}).ok());
	const Result<Transaction> refused = document.execute(DeleteAssetCommand{parquet.id});
	REQUIRE_FALSE(refused.ok());
	CHECK(refused.error().message == "'Roble claro' is in use (1 surface or piece of furniture)");

	REQUIRE(document.undo().ok()); // the wall finish
	REQUIRE(document.execute(DeleteAssetCommand{parquet.id}).ok());
	CHECK(document.undoLabel() == "Delete asset");
	CHECK_FALSE(document.project().findAsset(parquet.id));
	CHECK(seen.back().removedAssets == std::vector<AssetId>{parquet.id});
	REQUIRE(document.undo().ok());
	REQUIRE(document.project().findAsset(parquet.id));
	CHECK(document.project().assets[0].id == parquet.id); // back in its place
	CHECK(document.execute(DeleteAssetCommand{AssetId{Uuid{1, 1}}}).error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(Library, "project.json saves the asset library (schema 8; the library id, 9) and refuses files outside the project")
{
	Asset turned = chair;
	turned.model.unit = ModelUnit::Centimeter;
	turned.model.up = ModelUpAxis::Z;
	turned.model.frontQuarterTurns = 3;
	turned.model.againstWall = true;
	REQUIRE(document.execute(ChangeAssetCommand{turned}).ok());
	REQUIRE(document.execute(SetWallFinishCommand{wall, WallSide::Left, assetRef(parquet.id)}).ok());
	const std::string json = writeProjectJson(document.project());
	CHECK(json.find("\"schemaVersion\": 12") != std::string::npos);
	CHECK(json.find("\"kind\": \"texture\"") != std::string::npos);
	CHECK(json.find("\"surfaces\": [\n          \"floors\"\n        ]") != std::string::npos);
	CHECK(json.find("\"unit\": \"cm\"") != std::string::npos);
	CHECK(json.find("\"up\": \"z\"") != std::string::npos);
	const Result<Project> loaded = readProjectJson(json);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().assets == document.project().assets);
	CHECK(writeProjectJson(loaded.value()) == json);
	Document empty{makeSequentialUuidSource()};
	CHECK(writeProjectJson(empty.project()).find("\"assets\"") == std::string::npos);

	auto withAssets = [](const std::string& assets)
	{
		return R"({"format":"101st-floor-project","schemaVersion":8,"units":"mm",)"
			R"("levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[],"assets":)" +
			assets + "}";
	};
	const std::string id = "00000000-0000-0000-0000-0000000000aa";
	const std::string good = R"([{"id":")" + id + R"(","kind":"texture","name":"T","file":"assets/)" + id +
		R"(/t.png","texture":{"widthMm":1000,"heightMm":1000,"surfaces":["walls"]}}])";
	REQUIRE(readProjectJson(withAssets(good)).ok());
	CHECK(readProjectJson(withAssets(good)).value().assets[0].texture.roughnessPercent == 70);
	for (const std::string& file : {std::string("assets/") + id + "/../../../Windows/win.ini", std::string("C:/secret.png"),
			 std::string("assets/other/t.png")})
	{
		CAPTURE(file);
		std::string bad = good;
		bad.replace(bad.find("assets/" + id + "/t.png"), ("assets/" + id + "/t.png").size(), file);
		CHECK_FALSE(readProjectJson(withAssets(bad)).ok());
	}
	std::string surfaces = good;
	surfaces.replace(surfaces.find("[\"walls\"]"), 9, "[\"glass\"]");
	CHECK(readProjectJson(withAssets(surfaces)).error().code == ErrorCode::ParseError);
	CHECK(readProjectJson(withAssets(R"([{"id":")" + id + R"(","kind":"sound","name":"S","file":"assets/)" + id + R"(/s.wav"}])"))
			  .error().code == ErrorCode::ParseError);
	// A wall painted with a texture the project does not have.
	const std::string orphan = R"({"format":"101st-floor-project","schemaVersion":8,"units":"mm",)"
		R"("levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],)"
		R"("walls":[{"id":"00000000-0000-0000-0000-000000000001","level":"00000000-0000-0000-0000-000000000064",)"
		R"("start":[0,0],"end":[1000,0],"thicknessMm":100,"heightMm":2500,"finishes":{"left":"asset:)" + id + R"("}}]})";
	CHECK(readProjectJson(orphan).error().code == ErrorCode::NotFound);
}

TEST_CASE("a model is stretched to its piece on each axis, turned, placed on the floor, with normals that follow")
{
	geometry::ModelMesh model;
	model.widthMm = 1000;
	model.depthMm = 500;
	model.heightMm = 2000;
	// One slanted triangle through the model's box corners; its normal leans towards +x and +z.
	model.mesh.positions = {{-500, -250, 0}, {500, -250, 2000}, {500, 250, 2000}};
	model.mesh.normals = std::vector<geometry::Vec3>(3, geometry::Vec3{-0.8944271909999159, 0.0, 0.4472135954999579});
	model.mesh.uvs = std::vector<geometry::Vec2>(3, geometry::Vec2{0.25, 0.75});
	model.mesh.sections = {{geometry::modelSectionName(0), {0, 1, 2}}};

	Furniture piece;
	piece.type = "asset:00000000-0000-0000-0000-000000000001";
	piece.position = {3000, 1000};
	piece.rotationCentidegrees = 9000;
	piece.widthMm = 2000; // twice as wide
	piece.depthMm = 500;
	piece.heightMm = 1000; // half as tall
	const geometry::MeshData mesh = geometry::buildModelFurnitureMesh(piece, model);
	// Turned 90 degrees: the model's x runs north, its y (towards the back) west.
	CHECK(mesh.positions[0].x == doctest::Approx(3000.0 + 250.0));
	CHECK(mesh.positions[0].y == doctest::Approx(1000.0 - 1000.0));
	CHECK(mesh.positions[0].z == doctest::Approx(geometry::FloorThicknessMm));
	CHECK(mesh.positions[1].y == doctest::Approx(1000.0 + 1000.0));
	CHECK(mesh.positions[1].z == doctest::Approx(geometry::FloorThicknessMm + 1000.0));
	CHECK(mesh.uvs[0].u == 0.25);
	// Stretched twice as wide and half as tall: the slope flattens, so the normal turns up (and north once turned).
	const geometry::Vec3 n = mesh.normals[0];
	CHECK(n.x == doctest::Approx(0.0).epsilon(1e-9));
	CHECK(n.y < 0.0);
	CHECK(n.z == doctest::Approx(0.894427).epsilon(1e-6)); // (-2, 0, 1) stretched to (-0.5, 0, 1)
	CHECK(std::hypot(n.x, n.y, n.z) == doctest::Approx(1.0));

	const geometry::MeshData box = geometry::buildPlaceholderFurnitureMesh(piece);
	CHECK(box.triangleCount() == 12);
	CHECK(box.sections[0].slot == "body");
	CHECK(geometry::modelMaterialIndex("m12") == std::optional<std::size_t>(12));
	CHECK_FALSE(geometry::modelMaterialIndex("body").has_value());
	CHECK_FALSE(geometry::modelMaterialIndex("m").has_value());
	CHECK_FALSE(geometry::modelMaterialIndex("m1x").has_value());
}

TEST_CASE_FIXTURE(Library, "the scene shows model furniture with its own materials, a finish of its own, or a box without the model")
{
	geometry::ModelMesh chairMesh;
	chairMesh.widthMm = 800;
	chairMesh.depthMm = 700;
	chairMesh.heightMm = 900;
	chairMesh.mesh.positions = {{-400, -350, 0}, {400, -350, 0}, {0, 350, 900}, {-400, -350, 0}, {400, -350, 0}, {0, 350, 900}};
	chairMesh.mesh.normals = std::vector<geometry::Vec3>(6, geometry::Vec3{0, -1, 0});
	chairMesh.mesh.uvs = std::vector<geometry::Vec2>(6);
	chairMesh.mesh.sections = {{"m0", {0, 1, 2}}, {"m2", {3, 4, 5}}};
	int asked = 0;
	const scene::ModelProvider models = [&](const Asset& asset) -> const geometry::ModelMesh*
	{
		++asked;
		return asset.id == chair.id ? &chairMesh : nullptr;
	};

	const FurnitureId own = document.addFurniture(makeFurniture(*furnitureSpec(document.project(), assetRef(chair.id)), {1000, 1000})).value();
	Furniture painted = makeFurniture(*furnitureSpec(document.project(), assetRef(chair.id)), {3000, 1000});
	painted.finish = assetRef(parquet.id);
	const FurnitureId finished = document.addFurniture(painted).value();

	auto node = [](const scene::Scene& scene, const FurnitureId& id) -> const scene::SceneNode*
	{
		const auto found = std::find_if(scene.nodes.begin(), scene.nodes.end(),
			[&id](const scene::SceneNode& n) { return n.id == scene::furnitureNodeId(id); });
		return found == scene.nodes.end() ? nullptr : &*found;
	};
	const scene::Scene withModels = scene::compileScene(document.project(), models);
	CHECK(asked == 2);
	const scene::SceneNode* first = node(withModels, own);
	REQUIRE(first != nullptr);
	CHECK(first->mesh.triangleCount() == 2);
	CHECK(first->finishes == std::vector<std::string>{modelMaterialFinish(chair.id, 0), modelMaterialFinish(chair.id, 2)});
	CHECK(node(withModels, finished)->finishes == std::vector<std::string>(2, assetRef(parquet.id)));
	const geometry::Bounds3 bounds = geometry::computeBounds(first->mesh);
	CHECK(bounds.min.x == doctest::Approx(600.0));
	CHECK(bounds.max.z == doctest::Approx(geometry::FloorThicknessMm + 900.0));

	const scene::Scene withoutModels = scene::compileScene(document.project());
	const scene::SceneNode* box = node(withoutModels, own);
	REQUIRE(box != nullptr);
	CHECK(box->mesh.triangleCount() == 12);
	CHECK(box->finishes == std::vector<std::string>{std::string(DefaultCarpentryFinish)});

	// The overlay compiles with the models too (the piece being placed is a model).
	scene::ViewOverlay overlay;
	Furniture placing = makeFurniture(*furnitureSpec(document.project(), assetRef(chair.id)), {5000, 1000});
	placing.level = document.project().levels[0].id;
	overlay.previewFurniture = {placing};
	const scene::Scene previewed = scene::compileScene(document.project(), overlay, models);
	const auto preview = std::find_if(previewed.nodes.begin(), previewed.nodes.end(),
		[](const scene::SceneNode& n) { return n.id.value == "preview/furniture/0"; });
	REQUIRE(preview != previewed.nodes.end());
	CHECK(preview->mesh.triangleCount() == 2);
}

TEST_CASE_FIXTURE(Library, "the editor places imported models, snaps them to walls when they say so, and paints with textures")
{
	editor.setMode(EditMode::Furniture);
	REQUIRE(editor.setFurnitureType(assetRef(chair.id)).ok());
	CHECK(editor.setFurnitureType(assetRef(parquet.id)).error().code == ErrorCode::NotFound);
	editor.pointerMoved(down(3020.0, 700.0));
	REQUIRE(editor.furniturePlacement().has_value());
	CHECK(editor.furniturePlacement()->position == Point2D{3000, 700}); // free: not against walls
	CHECK(editor.furniturePlacement()->widthMm == 800);

	Asset againstWall = chair;
	againstWall.model.againstWall = true;
	REQUIRE(document.execute(ChangeAssetCommand{againstWall}).ok());
	editor.pointerMoved(down(3020.0, 720.0));
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 18000);
	CHECK(editor.furniturePlacement()->position == Point2D{3000, 450}); // back on the face at y = 100
	REQUIRE(editor.pointerPressed(down(3020.0, 720.0)).ok());
	CHECK(document.project().furniture.back().type == assetRef(chair.id));

	// Pintar with an imported texture: a wall face and a floor; a floor-only texture cannot go on the wall.
	editor.setMode(EditMode::Paint);
	REQUIRE(editor.setPaintFinish(assetRef(parquet.id)).ok());
	CHECK(editor.setPaintFinish(assetRef(chair.id)).error().code == ErrorCode::NotFound);
	CHECK(editor.setPaintFinish("asset:00000000-0000-0000-0000-0000000000ff").error().code == ErrorCode::NotFound);
	REQUIRE(editor.pointerPressed(down(1000.0, 30.0)).ok());
	CHECK(document.project().walls[0].finishLeft == assetRef(parquet.id));
	REQUIRE(editor.setPaintFinish(assetRef(floorOnly.id)).ok()); // allowed: floors
	CHECK_FALSE(editor.pointerPressed(down(1000.0, -30.0)).ok()); // but not on the wall's other face
	CHECK(document.project().walls[0].finishRight.empty());
}

TEST_CASE("an image becomes a tiling texture: power-of-two sides, flipped for surfaces, with every mip")
{
	scene::TextureImage image;
	image.width = 3;
	image.height = 2;
	// Top row red, bottom row blue.
	for (int y = 0; y < 2; ++y)
	{
		for (int x = 0; x < 3; ++x)
		{
			const std::uint8_t r = y == 0 ? 255 : 0;
			const std::uint8_t b = y == 0 ? 0 : 255;
			image.rgba.insert(image.rgba.end(), {r, 0, b, 255});
		}
	}
	const scene::FinishTexture surface = scene::textureFromImage(image, 1200.0, 800.0, true);
	REQUIRE(surface.mips.size() == 3); // 4 x 2, 2 x 1, 1 x 1
	CHECK(surface.mips[0].width == 4);
	CHECK(surface.mips[0].height == 2);
	CHECK(surface.mips.back().width == 1);
	CHECK(surface.repeatWidthMm == 1200.0);
	// Flipped: the image's top (red) is the last row.
	CHECK(surface.mips[0].rgba[0 * 4 + 2] > 200);                   // first row: blue
	CHECK(surface.mips[0].rgba[(1 * 4 + 0) * 4 + 0] > 200);         // last row: red
	const scene::FinishTexture model = scene::textureFromImage(image, 1000.0, 1000.0, false);
	CHECK(model.mips[0].rgba[0] > 200); // not flipped: first row red

	scene::TextureImage big;
	big.width = 5000;
	big.height = 3;
	big.rgba.assign(static_cast<std::size_t>(big.width) * big.height * 4, 128);
	const scene::FinishTexture capped = scene::textureFromImage(big, 1000.0, 1000.0, true, 1024);
	CHECK(capped.mips[0].width == 1024);
	CHECK(capped.mips[0].height == 4);
	CHECK(capped.mips[0].rgba[100] == 128);
}

TEST_CASE("a disabled baseboard retains its imported texture reference until its finish is replaced")
{
	Document document{makeSequentialUuidSource()};
	const Asset finish = texture(55, static_cast<std::uint8_t>(Surface::Carpentry));
	REQUIRE(document.execute(AddAssetCommand{finish}).ok());
	REQUIRE(document.execute(SetRoomBaseboardCommand{{"room-a", false, 100, 12, assetRef(finish.id)}}).ok());
	CHECK(assetUses(document.project(), finish.id) == 1);
	CHECK_FALSE(document.execute(DeleteAssetCommand{finish.id}).ok());
	REQUIRE(document.execute(SetRoomBaseboardCommand{{"room-a", false, 100, 12, "lacquer-white"}}).ok());
	REQUIRE(document.execute(DeleteAssetCommand{finish.id}).ok());
	CHECK(document.project().assets.empty());
}
