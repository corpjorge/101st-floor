#include "doctest.h"

#include "F101/Assets/Files.h"
#include "F101/Assets/Library.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/ProjectFile.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>

using namespace f101;
using namespace f101::assets;

// The built-in library (ADR-0019): manifests, loading with the files checked, adding items to a project.
namespace
{
	struct LibraryFolder
	{
		std::filesystem::path path;

		LibraryFolder()
		{
			std::random_device random;
			path = std::filesystem::temp_directory_path() / ("f101-library-" + std::to_string(random()) + "-" + std::to_string(random()));
			std::filesystem::create_directories(path);
		}

		~LibraryFolder()
		{
			std::error_code error;
			std::filesystem::remove_all(path, error);
		}
	};

	void write(const std::filesystem::path& file, const std::string& text)
	{
		std::filesystem::create_directories(file.parent_path());
		std::ofstream(file, std::ios::binary) << text;
	}

	const std::filesystem::path Samples = pathFromUtf8(F101_TEST_DATA_DIR) / "Assets";

	constexpr const char* FurnitureManifest = R"({
		"version": 1,
		"furniture": [
			{"id": "armchair-club", "name": "Sillón club", "category": "sofas", "file": "armchair.glb",
			 "thumbnail": "armchair.png", "againstWall": true, "credit": "101st Floor"},
			{"id": "lamp-arc", "name": "Lámpara de pie", "category": "lighting", "file": "lamp.obj", "unit": "cm"}
		]
	})";

	constexpr const char* TextureManifest = R"({
		"version": 1,
		"textures": [
			{"id": "parquet-light", "name": "Parquet claro", "category": "wood", "file": "parquet.png", "widthMm": 600,
			 "heightMm": 600, "roughnessPercent": 45, "surfaces": ["floors"], "credit": "CC0"}
		]
	})";

	// A library with a furniture folder (armchair, lamp) and a textures folder (parquet), from the test samples.
	void fillLibrary(const std::filesystem::path& folder)
	{
		for (const char* file : {"armchair.glb", "lamp.obj", "lamp.mtl", "lamp_shade.png"})
		{
			std::filesystem::create_directories(folder / "furniture");
			std::filesystem::copy_file(Samples / file, folder / "furniture" / file);
		}
		std::filesystem::copy_file(Samples / "parquet.png", folder / "furniture" / "armchair.png"); // a picture will do
		std::filesystem::create_directories(folder / "textures");
		std::filesystem::copy_file(Samples / "parquet.png", folder / "textures" / "parquet.png");
		write(folder / "furniture" / "manifest.json", FurnitureManifest);
		write(folder / "textures" / "manifest.json", TextureManifest);
	}
}

TEST_CASE("a library manifest lists furniture and textures with their categories, files and settings")
{
	const Result<Library> read = readLibraryManifest(FurnitureManifest, "furniture");
	REQUIRE(read.ok());
	const Library& library = read.value();
	REQUIRE(library.furniture.size() == 2);
	const LibraryFurniture& armchair = library.furniture[0];
	CHECK(armchair.id == "armchair-club");
	CHECK(armchair.name == "Sillón club");
	CHECK(armchair.category == cad::FurnitureCategory::Sofas);
	CHECK(armchair.file == "furniture/armchair.glb");
	CHECK(armchair.thumbnail == "furniture/armchair.png");
	CHECK(armchair.model.againstWall);
	CHECK(armchair.model.unit == cad::ModelUnit::Meter);
	CHECK(library.furniture[1].model.unit == cad::ModelUnit::Centimeter);
	CHECK(library.findFurniture("lamp-arc") == &library.furniture[1]);
	CHECK(library.findFurniture("nope") == nullptr);

	const Library textures = readLibraryManifest(TextureManifest, "textures").value();
	REQUIRE(textures.textures.size() == 1);
	CHECK(textures.textures[0].category == cad::FinishCategory::Wood);
	CHECK(textures.textures[0].texture.widthMm == 600);
	CHECK(textures.textures[0].texture.roughnessPercent == 45);
	CHECK(textures.textures[0].texture.surfaces == static_cast<std::uint8_t>(cad::Surface::Floor));

	auto error = [](const std::string& json) { return readLibraryManifest(json, "x").error().message; };
	CHECK(error("{") == "Library manifest: the JSON is not valid");
	CHECK(error(R"({"version": 2})") == "Library manifest: version must be 1");
	CHECK(error(R"({"version": 1, "furniture": [{"id": "Bad Id", "name": "a", "category": "sofas", "file": "a.glb"}]})").find(
			  "lower-case") != std::string::npos);
	CHECK(error(R"({"version": 1, "furniture": [{"id": "a", "name": "a", "category": "cars", "file": "a.glb"}]})").find(
			  "not a furniture category") != std::string::npos);
	CHECK(error(R"({"version": 1, "furniture": [{"id": "a", "name": "a", "category": "sofas", "file": "a.fbx"}]})").find(
			  "does not take") != std::string::npos);
	CHECK(error(R"({"version": 1, "furniture": [{"id": "a", "name": "a", "category": "sofas", "file": "../a.glb"}]})").find(
			  "inside") != std::string::npos);
	CHECK(error(R"({"version": 1, "textures": [{"id": "a", "name": "a", "category": "wood", "file": "a.png"},
		{"id": "a", "name": "b", "category": "wood", "file": "b.png"}]})") == "Library manifest: the id 'a' is repeated");
	CHECK(error(R"({"version": 1, "textures": [{"id": "a", "name": "a", "category": "wood", "file": "a.png", "surfaces": ["glass"]}]})")
			  .find("surfaces") != std::string::npos);
}

TEST_CASE("the library folder is read manifest by manifest, with every file checked")
{
	LibraryFolder folder;
	CHECK(loadLibrary(folder.path / "none").value().furniture.empty()); // no library: empty
	fillLibrary(folder.path);
	const Result<Library> loaded = loadLibrary(folder.path);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().furniture.size() == 2);
	CHECK(loaded.value().textures.size() == 1);
	CHECK(loaded.value().textures[0].file == "textures/parquet.png");

	std::filesystem::remove(folder.path / "furniture" / "armchair.png");
	const Result<Library> missing = loadLibrary(folder.path);
	REQUIRE_FALSE(missing.ok());
	CHECK(missing.error().message == "Library item 'armchair-club': furniture/armchair.png is missing");
}

TEST_CASE("the shipped library: every category furnished, every model readable at a plausible size, every texture known")
{
	const std::filesystem::path folder = pathFromUtf8(F101_LIBRARY_DIR);
	const Result<Library> loaded = loadLibrary(folder);
	REQUIRE(loaded.ok());
	const Library& library = loaded.value();
	CHECK(library.furniture.size() >= 50);
	CHECK(library.textures.size() >= 14);
	// Each piece added to a project as the application does it (its embedded textures written out), then read back.
	LibraryFolder project;
	std::map<cad::FurnitureCategory, int> byCategory;
	std::uint64_t next = 1;
	int textured = 0;
	for (const LibraryFurniture& item : library.furniture)
	{
		CAPTURE(item.id);
		++byCategory[item.category];
		CHECK_FALSE(item.thumbnail.empty());
		const Result<ImportedAsset> imported = importLibraryFurniture(item, folder, project.path, cad::AssetId{cad::Uuid{0, next++}});
		REQUIRE(imported.ok());
		const std::string warning = imported.value().warnings.empty() ? std::string() : imported.value().warnings.front();
		CHECK(warning == "");
		const cad::ModelAsset& model = imported.value().asset.model;
		CHECK(model.triangles < 20000);
		for (const std::int64_t size : {model.widthMm, model.depthMm, model.heightMm})
		{
			CHECK(size >= 5);
			CHECK(size <= 3000);
		}
		const bool standing = model.heightMm >= 300 || item.category == cad::FurnitureCategory::Decor; // rugs lie flat
		CHECK(standing);
		const Result<LoadedModel> read = loadModelFile(assetFilePath(project.path, imported.value().asset), ModelSettings{});
		REQUIRE(read.ok());
		CHECK(read.value().warnings.empty());
		textured += std::any_of(read.value().materials.begin(), read.value().materials.end(),
			[](const ModelMaterial& material) { return !material.texture.empty(); }) ? 1 : 0;
	}
	CHECK(textured >= 30); // wood and fabric carry their embedded textures; lacquer, metal and ceramic are plain colours
	for (const cad::FurnitureCategory category : {cad::FurnitureCategory::Beds, cad::FurnitureCategory::Seating,
			 cad::FurnitureCategory::Sofas, cad::FurnitureCategory::Tables, cad::FurnitureCategory::Storage,
			 cad::FurnitureCategory::Kitchen, cad::FurnitureCategory::Bathroom, cad::FurnitureCategory::Lighting,
			 cad::FurnitureCategory::Decor})
	{
		CHECK(byCategory[category] >= 3);
	}
	for (const LibraryTexture& item : library.textures)
	{
		CAPTURE(item.id);
		CHECK_FALSE(item.credit.empty());
		CHECK(item.texture.widthMm >= 500);
	}
}

TEST_CASE("a library item joins the project as an asset with its name, settings and library id, once")
{
	LibraryFolder library;
	LibraryFolder project;
	fillLibrary(library.path);
	const Library loaded = loadLibrary(library.path).value();

	const cad::AssetId chairId{cad::Uuid{0, 41}};
	const Result<ImportedAsset> chair = importLibraryFurniture(*loaded.findFurniture("armchair-club"), library.path, project.path, chairId);
	REQUIRE(chair.ok());
	const cad::Asset& asset = chair.value().asset;
	CHECK(asset.name == "Sillón club");
	CHECK(asset.library == "armchair-club");
	CHECK(asset.kind == cad::AssetKind::Model);
	CHECK(asset.model.againstWall);
	CHECK(asset.model.widthMm == 800); // the Blender armchair, 80 x 75 x 90 cm
	CHECK(asset.model.heightMm == 900);
	CHECK(std::filesystem::is_regular_file(assetFilePath(project.path, asset)));

	// The lamp in centimetres: a hundredth of its metre size.
	const Result<ImportedAsset> lamp =
		importLibraryFurniture(*loaded.findFurniture("lamp-arc"), library.path, project.path, cad::AssetId{cad::Uuid{0, 42}});
	REQUIRE(lamp.ok());
	CHECK(lamp.value().asset.model.unit == cad::ModelUnit::Centimeter);
	CHECK(lamp.value().asset.model.heightMm == 16);

	const Result<ImportedAsset> parquet =
		importLibraryTexture(*loaded.findTexture("parquet-light"), library.path, project.path, cad::AssetId{cad::Uuid{0, 43}});
	REQUIRE(parquet.ok());
	CHECK(parquet.value().asset.texture.widthMm == 600);
	CHECK(parquet.value().asset.texture.pixelWidth == 256);
	CHECK(parquet.value().asset.texture.surfaces == static_cast<std::uint8_t>(cad::Surface::Floor));
	CHECK(parquet.value().asset.library == "parquet-light");

	// In the project, the item is found by its library id; the file keeps it (schema 9).
	cad::Document document{cad::makeSequentialUuidSource()};
	REQUIRE(document.execute(cad::AddAssetCommand{asset}).ok());
	CHECK(findLibraryAsset(document.project(), "armchair-club") == &document.project().assets.front());
	CHECK(findLibraryAsset(document.project(), "lamp-arc") == nullptr);
	const std::string json = cad::writeProjectJson(document.project());
	CHECK(json.find("\"library\": \"armchair-club\"") != std::string::npos);
	CHECK(cad::readProjectJson(json).value().assets.front().library == "armchair-club");
}
