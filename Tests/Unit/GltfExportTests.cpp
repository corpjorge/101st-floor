#include "doctest.h"

#include "F101/Assets/AssetImport.h"
#include "F101/Assets/Files.h"
#include "F101/Assets/ImageInfo.h"
#include "F101/Assets/ModelReader.h"
#include "F101/Cad/Document.h"
#include "F101/Exporters/Gltf.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Scene/SceneCompiler.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <vector>

#if defined(_MSC_VER)
	#pragma warning(push, 0)
#endif
#include "nlohmann/json.hpp"
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

using namespace f101;
using namespace f101::exporters;
using Json = nlohmann::json;

// glTF export for presentation tools such as Twinmotion (ADR-0020).
namespace
{
	struct TempFolder
	{
		std::filesystem::path path;

		TempFolder()
		{
			std::random_device random;
			path = std::filesystem::temp_directory_path() / ("f101-gltf-" + std::to_string(random()) + "-" + std::to_string(random()));
			std::filesystem::create_directories(path);
		}

		~TempFolder()
		{
			std::error_code error;
			std::filesystem::remove_all(path, error);
		}
	};

	const std::filesystem::path Samples = assets::pathFromUtf8(F101_TEST_DATA_DIR) / "Assets";

	GltfTexts spanish()
	{
		GltfTexts texts;
		texts.groundFloor = "Planta baja";
		texts.levelNumber = "Planta {n}";
		texts.walls = "Muros";
		texts.floors = "Suelos";
		texts.slabs = "Forjados";
		texts.openings = "Puertas y ventanas";
		texts.furniture = "Muebles";
		texts.wall = "Muro";
		texts.joint = "Unión";
		texts.floor = "Suelo";
		texts.slab = "Forjado";
		texts.baseboard = "Rodapié";
		texts.door = "Puerta";
		texts.window = "Ventana";
		texts.columns = "Pilares";
		texts.services = "Instalaciones";
		texts.column = "Pilar";
		texts.gas = "Gas";
		texts.water = "Agua";
		texts.finishNames = {{"brick-red", "Ladrillo visto"}, {"paint-sage", "Pintura verde salvia"}};
		texts.furnitureNames = {{"sofa", "Sofá"}};
		return texts;
	}

	// A 4 x 3 m room with a door and a window, painted walls, a tiled floor and a sofa.
	struct House
	{
		cad::Document document{cad::makeSequentialUuidSource()};
		std::vector<cad::WallId> walls;
		cad::OpeningId door;
		cad::OpeningId window;
		cad::FurnitureId sofa;

		House()
		{
			for (const auto& [start, end] : std::vector<std::pair<cad::Point2D, cad::Point2D>>{
					 {{0, 0}, {4000, 0}}, {{4000, 0}, {4000, 3000}}, {{4000, 3000}, {0, 3000}}, {{0, 3000}, {0, 0}}})
			{
				walls.push_back(document.addWall(start, end, 120, 2800).value());
			}
			door = document.addOpening(walls[0], cad::Opening{cad::OpeningId{}, cad::OpeningKind::Door, 1500, 900, 2100, 0}).value();
			window = document.addOpening(walls[2], cad::Opening{cad::OpeningId{}, cad::OpeningKind::Window, 1000, 1200, 1200, 900}).value();
			REQUIRE(document.execute(cad::SetWallFinishCommand{walls[0], cad::WallSide::Left, "brick-red"}).ok());
			REQUIRE(document.execute(cad::SetWallFinishCommand{walls[1], cad::WallSide::Left, "paint-sage"}).ok());
			const std::vector<geometry::Room> rooms = geometry::detectRooms(document.project());
			REQUIRE(rooms.size() == 1);
			REQUIRE(document.execute(cad::SetRoomFinishCommand{rooms[0].key, "tile-grey", ""}).ok());
			sofa = document.addFurniture(cad::makeFurniture("sofa", {2000, 1500})).value();
		}

		const cad::Project& project() const { return document.project(); }
	};

	std::uint32_t littleEndian(const std::vector<std::uint8_t>& bytes, std::size_t at)
	{
		return bytes[at] | (static_cast<std::uint32_t>(bytes[at + 1]) << 8) | (static_cast<std::uint32_t>(bytes[at + 2]) << 16) |
			(static_cast<std::uint32_t>(bytes[at + 3]) << 24);
	}

	struct Glb
	{
		Json json;
		std::vector<std::uint8_t> bin;

		std::vector<std::uint8_t> view(int index) const
		{
			const Json& v = json["bufferViews"][static_cast<std::size_t>(index)];
			const std::size_t offset = v.value("byteOffset", std::size_t{0});
			const std::size_t length = v["byteLength"].get<std::size_t>();
			REQUIRE(offset + length <= bin.size());
			return std::vector<std::uint8_t>(bin.begin() + static_cast<std::ptrdiff_t>(offset),
				bin.begin() + static_cast<std::ptrdiff_t>(offset + length));
		}

		std::vector<float> floats(int accessor) const
		{
			const Json& a = json["accessors"][static_cast<std::size_t>(accessor)];
			REQUIRE(a["componentType"] == 5126);
			const std::vector<std::uint8_t> bytes = view(a["bufferView"].get<int>());
			std::vector<float> values(bytes.size() / sizeof(float));
			std::memcpy(values.data(), bytes.data(), values.size() * sizeof(float));
			return values;
		}

		// Material by its finish id (extras.f101FinishId), or null.
		const Json* material(std::string_view finish) const
		{
			for (const Json& m : json["materials"])
			{
				if (m["extras"]["f101FinishId"] == std::string(finish))
				{
					return &m;
				}
			}
			return nullptr;
		}

		std::vector<std::uint8_t> imageOf(const Json& material) const
		{
			const int texture = material["pbrMetallicRoughness"]["baseColorTexture"]["index"].get<int>();
			const int image = json["textures"][static_cast<std::size_t>(texture)]["source"].get<int>();
			return view(json["images"][static_cast<std::size_t>(image)]["bufferView"].get<int>());
		}
	};

	// The header and both chunks, as the GLB specification lays them out.
	Glb parseGlb(const std::vector<std::uint8_t>& bytes)
	{
		REQUIRE(bytes.size() >= 28);
		CHECK(littleEndian(bytes, 0) == 0x46546C67u); // "glTF"
		CHECK(littleEndian(bytes, 4) == 2u);
		CHECK(littleEndian(bytes, 8) == bytes.size());
		const std::uint32_t jsonLength = littleEndian(bytes, 12);
		CHECK(littleEndian(bytes, 16) == 0x4E4F534Au); // "JSON"
		CHECK(jsonLength % 4 == 0);
		REQUIRE(20 + jsonLength + 8 <= bytes.size());
		Glb glb;
		glb.json = Json::parse(bytes.begin() + 20, bytes.begin() + 20 + jsonLength, nullptr, false);
		REQUIRE_FALSE(glb.json.is_discarded());
		const std::size_t bin = 20 + jsonLength;
		const std::uint32_t binLength = littleEndian(bytes, bin);
		CHECK(littleEndian(bytes, bin + 4) == 0x004E4942u); // "BIN\0"
		CHECK(binLength % 4 == 0);
		REQUIRE(bin + 8 + binLength == bytes.size());
		glb.bin.assign(bytes.begin() + static_cast<std::ptrdiff_t>(bin + 8), bytes.end());
		CHECK(glb.json["buffers"][0]["byteLength"] == binLength);
		return glb;
	}

	double linear(std::uint8_t value)
	{
		const double c = value / 255.0;
		return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
	}

	std::size_t trianglesOf(const scene::Scene& scene)
	{
		std::size_t count = 0;
		for (const scene::SceneNode& node : scene.nodes)
		{
			count += node.mesh.triangleCount();
		}
		return count;
	}
}

TEST_CASE("a project becomes a binary glTF: a root, its level, a group per kind, one named object per scene node")
{
	House house;
	TempFolder folder;
	assets::ModelCache models;
	const Result<GltfExport> exported = exportProjectGlb(house.project(), folder.path, models, spanish());
	REQUIRE(exported.ok());
	const Glb glb = parseGlb(exported.value().bytes);
	CHECK(glb.json["asset"]["version"] == "2.0");
	CHECK(glb.json["asset"]["generator"] == "101st Floor");
	CHECK(exported.value().warnings.empty());

	const Json& nodes = glb.json["nodes"];
	const Json& root = nodes[glb.json["scenes"][0]["nodes"][0].get<std::size_t>()];
	CHECK(root["name"] == "101st Floor");
	REQUIRE(root["children"].size() == 1);
	const Json& level = nodes[root["children"][0].get<std::size_t>()];
	CHECK(level["name"] == "Planta baja");
	CHECK(level["extras"]["f101LevelId"] == cad::toString(house.project().levels[0].id));
	std::vector<std::string> groups;
	for (const Json& child : level["children"])
	{
		groups.push_back(nodes[child.get<std::size_t>()]["name"].get<std::string>());
	}
	CHECK(groups == std::vector<std::string>{"Muros", "Suelos", "Forjados", "Puertas y ventanas", "Muebles"});

	// Every scene node with triangles is an object with a stable, unique name and its id in extras.
	const scene::Scene scene = scene::compileScene(house.project());
	std::set<std::string> sceneIds;
	for (const scene::SceneNode& node : scene.nodes)
	{
		if (node.mesh.triangleCount() > 0)
		{
			sceneIds.insert(node.id.value);
		}
	}
	std::set<std::string> exportedIds;
	std::set<std::string> names;
	for (const Json& node : nodes)
	{
		if (node.contains("mesh"))
		{
			exportedIds.insert(node["extras"]["f101NodeId"].get<std::string>());
			CHECK(names.insert(node["name"].get<std::string>()).second);
		}
	}
	CHECK(exportedIds == sceneIds);
	CHECK(exported.value().objects == sceneIds.size());
	CHECK(exported.value().triangles == trianglesOf(scene));
	CHECK(names.count("Muro " + cad::toString(house.walls[0]).substr(0, 8)) == 1);
	CHECK(names.count("Puerta " + cad::toString(house.door).substr(0, 8)) == 1);
	CHECK(names.count("Ventana " + cad::toString(house.window).substr(0, 8)) == 1);
	CHECK(names.count("Sofá " + cad::toString(house.sofa).substr(0, 8)) == 1);

	// Deterministic: the same project gives the same bytes.
	CHECK(exportProjectGlb(house.project(), folder.path, models, spanish()).value().bytes == exported.value().bytes);
}

TEST_CASE("the geometry reads back in metres with Y up, every triangle facing where its normal points")
{
	House house;
	TempFolder folder;
	assets::ModelCache models;
	const GltfExport exported = exportProjectGlb(house.project(), folder.path, models, GltfTexts{}).value();
	const Result<assets::RawModel> read = assets::readGltf(exported.bytes, [](std::string_view) { return std::nullopt; });
	REQUIRE(read.ok());
	const assets::RawModel& model = read.value();
	CHECK(model.triangleCount() == exported.triangles);

	// Plan (x east, y north, z up) in mm -> glTF (x, z, -y) in m: the 4 x 3 m room with 120 mm walls, 2.8 m tall.
	geometry::Vec3 low{1e9, 1e9, 1e9};
	geometry::Vec3 high{-1e9, -1e9, -1e9};
	for (const geometry::Vec3& p : model.positions)
	{
		low = geometry::Vec3{std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
		high = geometry::Vec3{std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
	}
	CHECK(low.x == doctest::Approx(-0.06).epsilon(1e-5));
	CHECK(high.x == doctest::Approx(4.06).epsilon(1e-5));
	CHECK(low.z == doctest::Approx(-3.06).epsilon(1e-5));
	CHECK(high.z == doctest::Approx(0.06).epsilon(1e-5));
	CHECK(low.y == doctest::Approx(0.0).epsilon(1e-5));
	CHECK(high.y == doctest::Approx(2.8).epsilon(1e-5));

	// The rotation keeps the winding: counter-clockwise seen from where the normal points.
	std::size_t facing = 0;
	std::size_t counted = 0;
	for (std::size_t t = 0; t < model.triangleCount(); ++t)
	{
		const geometry::Vec3& a = model.positions[t * 3];
		const geometry::Vec3& b = model.positions[t * 3 + 1];
		const geometry::Vec3& c = model.positions[t * 3 + 2];
		const geometry::Vec3 u{b.x - a.x, b.y - a.y, b.z - a.z};
		const geometry::Vec3 v{c.x - a.x, c.y - a.y, c.z - a.z};
		const geometry::Vec3 cross{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
		const double area = std::sqrt(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
		if (area < 1e-9)
		{
			continue;
		}
		const geometry::Vec3& n = model.normals[t * 3];
		++counted;
		facing += (cross.x * n.x + cross.y * n.y + cross.z * n.z) / area > 0.5 ? 1 : 0;
	}
	CHECK(counted > 100);
	CHECK(facing == counted);
}

TEST_CASE("one material per finish: patterned finishes carry their PNG, plain ones a colour, glass blends")
{
	House house;
	TempFolder folder;
	assets::ModelCache models;
	const Glb glb = parseGlb(exportProjectGlb(house.project(), folder.path, models, spanish()).value().bytes);
	std::set<std::string> finishes;
	for (const Json& material : glb.json["materials"])
	{
		CHECK(finishes.insert(material["extras"]["f101FinishId"].get<std::string>()).second);
		CHECK(material["pbrMetallicRoughness"]["metallicFactor"] == 0.0);
	}

	const Json* bricks = glb.material("brick-red");
	REQUIRE(bricks != nullptr);
	CHECK((*bricks)["name"] == "Ladrillo visto");
	CHECK((*bricks)["pbrMetallicRoughness"]["roughnessFactor"].get<double>() == doctest::Approx(cad::findFinish("brick-red")->roughness));
	const std::vector<std::uint8_t> png = glb.imageOf(*bricks);
	const Result<assets::ImageInfo> info = assets::readImageInfo(png.data(), png.size());
	REQUIRE(info.ok());
	CHECK(info.value().format == assets::ImageFormat::Png);
	CHECK(info.value().width == 512);

	const Json* sage = glb.material("paint-sage");
	REQUIRE(sage != nullptr);
	CHECK_FALSE((*sage)["pbrMetallicRoughness"].contains("baseColorTexture"));
	const cad::Rgb8 colour = cad::findFinish("paint-sage")->color;
	CHECK((*sage)["pbrMetallicRoughness"]["baseColorFactor"][0].get<double>() == doctest::Approx(linear(colour.r)));
	CHECK((*sage)["pbrMetallicRoughness"]["baseColorFactor"][2].get<double>() == doctest::Approx(linear(colour.b)));

	const Json* glass = glb.material("glass-clear");
	REQUIRE(glass != nullptr);
	CHECK((*glass)["alphaMode"] == "BLEND");
	CHECK((*glass)["pbrMetallicRoughness"]["baseColorFactor"][3].get<double>() == doctest::Approx(cad::findFinish("glass-clear")->opacity));
	CHECK(glb.material("tile-grey") != nullptr);
	CHECK(glb.json["samplers"][0]["wrapS"] == 10497);
}

TEST_CASE("UVs carry each finish's repeat (metres to repeats), mirrored in v for surface images from files")
{
	scene::Scene scene;
	scene::SceneNode node;
	node.id = scene::SceneNodeId{"test/quad"};
	node.sourceKind = scene::SourceKind::Wall;
	node.sourceId = "0123456789abcdef";
	node.mesh.positions = {{0, 0, 0}, {1000, 0, 0}, {1000, 1000, 0}, {0, 1000, 0}};
	node.mesh.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
	node.mesh.uvs = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
	node.mesh.sections = {{"a", {0, 1, 2, 0, 2, 3}}, {"b", {0, 1, 2, 0, 2, 3}}};
	node.finishes = {"quarter", "flipped"};
	scene.nodes.push_back(node);
	const GltfMaterialSource looks = [](std::string_view finish)
	{
		GltfMaterialLook look;
		look.repeatWidthMm = 250.0;
		look.repeatHeightMm = finish == "quarter" ? 500.0 : 250.0;
		look.flipImage = finish == "flipped";
		return look;
	};
	cad::Project project;
	const GltfExport exported = writeGlb(scene, project, looks, GltfTexts{});
	const Glb glb = parseGlb(exported.bytes);
	CHECK(exported.objects == 1);
	CHECK(exported.triangles == 4);

	// No level in the project: the group hangs from the root; English names by default.
	const Json& nodes = glb.json["nodes"];
	const Json& root = nodes[glb.json["scenes"][0]["nodes"][0].get<std::size_t>()];
	const Json& group = nodes[root["children"][0].get<std::size_t>()];
	CHECK(group["name"] == "Walls");
	CHECK(nodes[group["children"][0].get<std::size_t>()]["name"] == "Wall 01234567");

	const Json& primitives = glb.json["meshes"][0]["primitives"];
	REQUIRE(primitives.size() == 2);
	const std::vector<float> quarter = glb.floats(primitives[0]["attributes"]["TEXCOORD_0"].get<int>());
	const std::vector<float> flipped = glb.floats(primitives[1]["attributes"]["TEXCOORD_0"].get<int>());
	REQUIRE(quarter.size() == 8);
	CHECK(quarter[4] == doctest::Approx(4.0)); // (1 m, 1 m) with 250 x 500 mm repeats
	CHECK(quarter[5] == doctest::Approx(2.0));
	CHECK(flipped[5] == doctest::Approx(-4.0));

	// (1000, 1000, 0) mm -> (1, 0, -1) m.
	const Json& position = glb.json["accessors"][primitives[0]["attributes"]["POSITION"].get<std::size_t>()];
	CHECK(position["min"] == Json::array({0.0, 0.0, -1.0}));
	CHECK(position["max"] == Json::array({1.0, 0.0, 0.0}));
	const std::vector<float> points = glb.floats(primitives[0]["attributes"]["POSITION"].get<int>());
	CHECK(points[6] == doctest::Approx(1.0));
	CHECK(points[8] == doctest::Approx(-1.0));
}

TEST_CASE("imported textures and model materials keep their image files; an unreadable one is reported")
{
	House house;
	TempFolder folder;
	const cad::AssetId parquetId{cad::Uuid{0, 900}};
	const Result<assets::ImportedAsset> parquet = assets::importAsset(Samples / "parquet.png", folder.path, parquetId);
	REQUIRE(parquet.ok());
	REQUIRE(house.document.execute(cad::AddAssetCommand{parquet.value().asset}).ok());
	REQUIRE(house.document.execute(cad::SetWallFinishCommand{house.walls[2], cad::WallSide::Left, cad::assetRef(parquetId)}).ok());
	const cad::AssetId chairId{cad::Uuid{0, 901}};
	const Result<assets::ImportedAsset> chair = assets::importAsset(Samples / "armchair.glb", folder.path, chairId);
	REQUIRE(chair.ok());
	REQUIRE(house.document.execute(cad::AddAssetCommand{chair.value().asset}).ok());
	const std::string chairRef = cad::assetRef(chairId); // the spec's id points into it
	const std::optional<cad::FurnitureType> spec = cad::furnitureSpec(house.project(), chairRef);
	REQUIRE(spec.has_value());
	REQUIRE(house.document.addFurniture(cad::makeFurniture(*spec, {1000, 2000})).ok());

	assets::ModelCache models;
	const GltfExport exported = exportProjectGlb(house.project(), folder.path, models, spanish()).value();
	CHECK(exported.warnings.empty());
	const Glb glb = parseGlb(exported.bytes);

	// The texture: its PNG file as it is, named like the asset.
	const Json* texture = glb.material(cad::assetRef(parquetId));
	REQUIRE(texture != nullptr);
	CHECK((*texture)["name"] == parquet.value().asset.name);
	CHECK(glb.imageOf(*texture) == assets::readFileBytes(assets::assetFilePath(folder.path, parquet.value().asset)).value());

	// The model: a material per material of the file; the textured one embeds the image the importer wrote out.
	std::set<std::vector<std::uint8_t>> modelImages;
	for (const auto& file : std::filesystem::directory_iterator(folder.path / assets::pathFromUtf8(cad::assetFolder(chairId))))
	{
		if (file.path().extension() == ".png" || file.path().extension() == ".jpg")
		{
			modelImages.insert(assets::readFileBytes(file.path()).value());
		}
	}
	REQUIRE_FALSE(modelImages.empty());
	bool textured = false;
	for (std::size_t i = 0; i < 8; ++i)
	{
		if (const Json* material = glb.material(cad::modelMaterialFinish(chairId, i)))
		{
			CHECK((*material)["name"].get<std::string>().rfind(chair.value().asset.name + " - ", 0) == 0);
			if ((*material)["pbrMetallicRoughness"].contains("baseColorTexture"))
			{
				textured = textured || modelImages.count(glb.imageOf(*material)) == 1;
			}
		}
	}
	CHECK(textured);

	// Without its file, the texture exports as a plain material and says why.
	std::filesystem::remove(assets::assetFilePath(folder.path, parquet.value().asset));
	assets::ModelCache again;
	const GltfExport missing = exportProjectGlb(house.project(), folder.path, again, spanish()).value();
	REQUIRE(missing.warnings.size() == 1);
	CHECK(missing.warnings[0].find("cannot be read") != std::string::npos);
	const Glb plain = parseGlb(missing.bytes);
	REQUIRE(plain.material(cad::assetRef(parquetId)) != nullptr);
	CHECK_FALSE((*plain.material(cad::assetRef(parquetId)))["pbrMetallicRoughness"].contains("baseColorTexture"));
}

TEST_CASE("columns and the runs of the building services go in their own groups, named by kind, in their colours")
{
	House house;
	const cad::ColumnId column = house.document.addColumn(cad::makeColumn(cad::ColumnShape::Round, {0, 0}, 350)).value();
	cad::ServiceRun gas;
	gas.kind = cad::ServiceKind::Gas;
	gas.points = {{500, 500, 2400}, {3500, 500, 2400}};
	const cad::ServiceRunId run = house.document.addServiceRun(gas).value();
	TempFolder folder;
	assets::ModelCache models;
	const GltfExport exported = exportProjectGlb(house.project(), folder.path, models, spanish()).value();
	CHECK(exported.warnings.empty());
	const Glb glb = parseGlb(exported.bytes);
	const Json& nodes = glb.json["nodes"];
	const Json& root = nodes[glb.json["scenes"][0]["nodes"][0].get<std::size_t>()];
	const Json& level = nodes[root["children"][0].get<std::size_t>()];
	std::vector<std::string> groups;
	for (const Json& child : level["children"])
	{
		groups.push_back(nodes[child.get<std::size_t>()]["name"].get<std::string>());
	}
	CHECK(groups == std::vector<std::string>{"Muros", "Suelos", "Forjados", "Puertas y ventanas", "Muebles", "Pilares", "Instalaciones"});

	const auto named = [&nodes](const std::string& name) -> const Json*
	{
		for (const Json& node : nodes)
		{
			if (node["name"] == name)
			{
				return &node;
			}
		}
		return nullptr;
	};
	const Json* columnNode = named("Pilar " + cad::toString(column).substr(0, 8));
	REQUIRE(columnNode != nullptr);
	CHECK((*columnNode)["extras"]["f101Source"] == "column");
	const Json* runNode = named("Gas " + cad::toString(run).substr(0, 8));
	REQUIRE(runNode != nullptr);
	CHECK((*runNode)["extras"]["f101Source"] == "service");
	const Json* gold = glb.material("service-gas");
	REQUIRE(gold != nullptr);
	const Json& factor = (*gold)["pbrMetallicRoughness"]["baseColorFactor"];
	CHECK(factor[0].get<double>() == doctest::Approx(linear(212)).epsilon(0.001));
	CHECK(factor[2].get<double>() == doctest::Approx(linear(64)).epsilon(0.001));
	REQUIRE(glb.material("concrete") != nullptr);
}

TEST_CASE("levels above are named and ordered; nothing to export is an error; an unknown finish shows magenta")
{
	House house;
	const cad::LevelId upper = house.document.addLevel().value();
	REQUIRE(house.document.addWall({0, 0}, {4000, 0}, 120, 2800, upper).ok());
	TempFolder folder;
	assets::ModelCache models;
	const Glb glb = parseGlb(exportProjectGlb(house.project(), folder.path, models, spanish()).value().bytes);
	const Json& nodes = glb.json["nodes"];
	const Json& root = nodes[glb.json["scenes"][0]["nodes"][0].get<std::size_t>()];
	REQUIRE(root["children"].size() == 2);
	CHECK(nodes[root["children"][0].get<std::size_t>()]["name"] == "Planta baja");
	const Json& first = nodes[root["children"][1].get<std::size_t>()];
	CHECK(first["name"] == "Planta 1");
	CHECK(nodes[first["children"][0].get<std::size_t>()]["name"] == "Muros (Planta 1)");

	cad::Document empty{cad::makeSequentialUuidSource()};
	const Result<GltfExport> nothing = exportProjectGlb(empty.project(), folder.path, models, spanish());
	REQUIRE_FALSE(nothing.ok());
	CHECK(nothing.error().message.find("Nothing to export") == 0);

	scene::Scene scene;
	scene::SceneNode node;
	node.id = scene::SceneNodeId{"test/triangle"};
	node.mesh.positions = {{0, 0, 0}, {1000, 0, 0}, {0, 1000, 0}};
	node.mesh.sections = {{"a", {0, 1, 2}}};
	node.finishes = {"no-such-finish"};
	scene.nodes.push_back(node);
	const GltfExport magenta = writeGlb(scene, empty.project(), projectMaterials(empty.project(), folder.path, models, GltfTexts{}), GltfTexts{});
	REQUIRE(magenta.warnings.size() == 1);
	CHECK(magenta.warnings[0] == "Unknown finish 'no-such-finish': exported as magenta");
	const Glb magentaGlb = parseGlb(magenta.bytes);
	const Json& factor = magentaGlb.json["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"];
	CHECK(factor[0] == 1.0);
	CHECK(factor[1] == 0.0);
	CHECK(factor[2] == 1.0);
}

TEST_CASE("the file is written whole, creating its folder")
{
	TempFolder folder;
	const std::filesystem::path path = folder.path / "casa ñ" / "casa.glb";
	const std::vector<std::uint8_t> bytes{1, 2, 3, 4, 5};
	REQUIRE(saveGlbFile(path, bytes).ok());
	CHECK(assets::readFileBytes(path).value() == bytes);
	CHECK_FALSE(std::filesystem::exists(folder.path / "casa ñ" / "casa.glb.tmp"));
}

TEST_CASE("GLB includes baseboards with the room name and their own material")
{
	House house;
	const auto room = geometry::detectRooms(house.project()).front();
	REQUIRE(house.document.execute(cad::SetRoomNameCommand{room.key, "Cocina"}).ok());
	REQUIRE(house.document.execute(cad::SetRoomBaseboardCommand{{room.key, true, 100, 12, "wood-walnut"}}).ok());
	TempFolder folder;
	assets::ModelCache models;
	const auto exported = exportProjectGlb(house.project(), folder.path, models, spanish());
	REQUIRE(exported.ok());
	const Glb glb = parseGlb(exported.value().bytes);
	std::size_t count = 0;
	for (const auto& node : glb.json["nodes"])
	{
		if (!node.contains("mesh") || node["extras"]["f101Source"] != "baseboard") continue;
		++count;
		CHECK(node["name"].get<std::string>().find("Rodapié Cocina") == 0);
		const auto& primitive = glb.json["meshes"][node["mesh"].get<std::size_t>()]["primitives"][0];
		CHECK(glb.json["materials"][primitive["material"].get<std::size_t>()]["extras"]["f101FinishId"] == "wood-walnut");
		CHECK(glb.floats(primitive["attributes"]["POSITION"].get<int>()).size() > 0);
	}
	CHECK(count == 1);
}
