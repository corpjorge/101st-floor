#include "doctest.h"

#include "F101/Assets/AssetImport.h"
#include "F101/Assets/ImageInfo.h"
#include "F101/Assets/ModelLoader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::assets;

// Readers and importer of imported assets (ADR-0017): image headers, OBJ, glTF and GLB, normalization, copying into the
// project, the model cache.
namespace
{
	using Bytes = std::vector<std::uint8_t>;

	struct TempFolder
	{
		std::filesystem::path path;

		TempFolder()
		{
			std::random_device random;
			path = std::filesystem::temp_directory_path() / ("f101-assets-" + std::to_string(random()) + "-" + std::to_string(random()));
			std::filesystem::create_directories(path);
		}

		~TempFolder()
		{
			std::error_code error;
			std::filesystem::remove_all(path, error);
		}
	};

	void write(const std::filesystem::path& file, const Bytes& bytes)
	{
		std::filesystem::create_directories(file.parent_path());
		std::ofstream out(file, std::ios::binary);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}

	void write(const std::filesystem::path& file, const std::string& text)
	{
		write(file, Bytes(text.begin(), text.end()));
	}

	Bytes pngHeader(std::uint32_t width, std::uint32_t height)
	{
		Bytes bytes{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
		for (const std::uint32_t value : {width, height})
		{
			for (int shift = 24; shift >= 0; shift -= 8)
			{
				bytes.push_back(static_cast<std::uint8_t>(value >> shift));
			}
		}
		bytes.insert(bytes.end(), {8, 6, 0, 0, 0, 0, 0, 0, 0}); // depth, colour type, ..., (fake) CRC
		return bytes;
	}

	Bytes jpegHeader(std::uint16_t width, std::uint16_t height)
	{
		Bytes bytes{0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0};
		bytes.insert(bytes.end(), {0xFF, 0xC0, 0x00, 0x11, 0x08, static_cast<std::uint8_t>(height >> 8), static_cast<std::uint8_t>(height),
			static_cast<std::uint8_t>(width >> 8), static_cast<std::uint8_t>(width), 3, 1, 0x22, 0, 2, 0x11, 1, 3, 0x11, 1});
		bytes.insert(bytes.end(), {0xFF, 0xD9});
		return bytes;
	}

	std::string base64(const Bytes& bytes)
	{
		static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string text;
		for (std::size_t i = 0; i < bytes.size(); i += 3)
		{
			const std::uint32_t chunk = (std::uint32_t{bytes[i]} << 16) | (i + 1 < bytes.size() ? std::uint32_t{bytes[i + 1]} << 8 : 0) |
				(i + 2 < bytes.size() ? std::uint32_t{bytes[i + 2]} : 0);
			text += alphabet[(chunk >> 18) & 63];
			text += alphabet[(chunk >> 12) & 63];
			text += i + 1 < bytes.size() ? alphabet[(chunk >> 6) & 63] : '=';
			text += i + 2 < bytes.size() ? alphabet[chunk & 63] : '=';
		}
		return text;
	}

	template <typename T>
	void append(Bytes& bytes, std::initializer_list<T> values)
	{
		for (const T value : values)
		{
			const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
			bytes.insert(bytes.end(), raw, raw + sizeof(T));
		}
	}

	// A triangle (positions, normals, uvs, u16 indices) at node translation (1, 0, 0) turned 90 degrees about Y, with a
	// grey material, roughness 0.4 and an embedded texture; and a second primitive without a material.
	struct GltfFixture
	{
		Bytes buffer;
		std::string json;

		explicit GltfFixture(const std::string& bufferUri, const std::string& imageSource = "")
		{
			append<float>(buffer, {0, 0, 0, 1, 0, 0, 0, 1, 0});        // 0: positions (36 bytes)
			append<float>(buffer, {0, 0, 1, 0, 0, 1, 0, 0, 1});        // 36: normals (36)
			append<float>(buffer, {0, 0, 1, 0, 0, 1});                 // 72: uvs (24)
			append<std::uint16_t>(buffer, {0, 1, 2, 0});               // 96: indices (6 + 2 padding)
			const std::string uri = bufferUri.empty() ? "" : R"("uri":")" + bufferUri + R"(",)";
			const std::string image = imageSource.empty() ? R"({"uri":"data:image/png;base64,)" + base64(pngHeader(4, 4)) + R"("})"
				: imageSource;
			json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
				R"("nodes":[{"mesh":0,"translation":[1,0,0],"rotation":[0,0.70710678118654757,0,0.70710678118654757]}],)"
				R"("meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0},)"
				R"({"attributes":{"POSITION":0},"mode":4}]}],)"
				R"("materials":[{"name":"Grey","pbrMetallicRoughness":{"baseColorFactor":[0.21586,0.21586,0.21586,1],)"
				R"("roughnessFactor":0.4,"baseColorTexture":{"index":0}}}],)"
				R"("textures":[{"source":0}],"images":[)" + image + "],"
				R"("buffers":[{)" + uri + R"("byteLength":104}],)"
				R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},)"
				R"({"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":6}],)"
				R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},)"
				R"({"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},)"
				R"({"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},)"
				R"({"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}]})";
		}

		Bytes gltf() const
		{
			return Bytes(json.begin(), json.end());
		}

		Bytes glb() const
		{
			std::string chunk = json;
			while (chunk.size() % 4 != 0)
			{
				chunk += ' ';
			}
			Bytes bytes;
			append<std::uint32_t>(bytes, {0x46546C67, 2, static_cast<std::uint32_t>(12 + 8 + chunk.size() + 8 + buffer.size())});
			append<std::uint32_t>(bytes, {static_cast<std::uint32_t>(chunk.size()), 0x4E4F534A});
			bytes.insert(bytes.end(), chunk.begin(), chunk.end());
			append<std::uint32_t>(bytes, {static_cast<std::uint32_t>(buffer.size()), 0x004E4942});
			bytes.insert(bytes.end(), buffer.begin(), buffer.end());
			return bytes;
		}
	};

	const ModelFileReader NoFiles = [](std::string_view) { return std::optional<Bytes>(); };

	constexpr const char* ChairObj = "# a quad and a triangle\n"
		"mtllib chair.mtl\n"
		"o Chair\n"
		"v 0 0 0\nv 1 0 0\nv 1 0 1\nv 0 0 1\n"
		"vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
		"vn 0 1 0\n"
		"usemtl Seat\n"
		"f 1/1/1 2/2/1 3/3/1 4/4/1\n"
		"usemtl Legs\n"
		"f -4 -3 -2\n";

	std::string chairMtl(const std::string& legsTexture)
	{
		return "newmtl Seat\nKd 0.8 0.2 0.2\nmap_Kd -s 1 1 1 textures/seat.png\nNs 250\n\n"
			"newmtl Legs\nKd 0.2158605 0.2158605 0.2158605\nPr 0.3\nmap_Kd " + legsTexture + "\n";
	}
}

TEST_CASE("image headers: PNG and JPEG with their size; anything else refused")
{
	const Bytes png = pngHeader(640, 480);
	const Result<ImageInfo> pngInfo = readImageInfo(png.data(), png.size());
	REQUIRE(pngInfo.ok());
	CHECK(pngInfo.value().format == ImageFormat::Png);
	CHECK(pngInfo.value().width == 640);
	CHECK(pngInfo.value().height == 480);
	const Bytes jpeg = jpegHeader(1024, 768);
	const Result<ImageInfo> jpegInfo = readImageInfo(jpeg.data(), jpeg.size());
	REQUIRE(jpegInfo.ok());
	CHECK(jpegInfo.value().format == ImageFormat::Jpeg);
	CHECK(jpegInfo.value().width == 1024);
	CHECK(jpegInfo.value().height == 768);
	CHECK(imageExtension(ImageFormat::Jpeg) == "jpg");

	const Bytes text{'h', 'e', 'l', 'l', 'o'};
	CHECK(readImageInfo(text.data(), text.size()).error().code == ErrorCode::InvalidArgument);
	const Bytes huge = pngHeader(20000, 10);
	CHECK(readImageInfo(huge.data(), huge.size()).error().code == ErrorCode::OutOfRange);
	const Bytes cut(png.begin(), png.begin() + 12);
	CHECK(readImageInfo(cut.data(), cut.size()).error().code == ErrorCode::ParseError);
	const Bytes empty = pngHeader(0, 5);
	CHECK_FALSE(readImageInfo(empty.data(), empty.size()).ok());
	const Bytes noFrame{0xFF, 0xD8, 0xFF, 0xD9};
	CHECK_FALSE(readImageInfo(noFrame.data(), noFrame.size()).ok());
}

TEST_CASE("OBJ: polygons as fans, negative indices, flat normals where none, v flipped, MTL colours, textures and roughness, CRLF")
{
	const std::string mtl = chairMtl("-bm 0.5 ../wood grain.png");
	const ModelFileReader files = [&mtl](std::string_view reference) -> std::optional<Bytes>
	{
		return reference == "chair.mtl" ? std::optional<Bytes>(Bytes(mtl.begin(), mtl.end())) : std::nullopt;
	};
	const Result<RawModel> read = readObj(ChairObj, files);
	REQUIRE(read.ok());
	const RawModel& model = read.value();
	CHECK(model.triangleCount() == 3);
	CHECK(model.materials == std::vector<std::uint32_t>{0, 0, 1});
	REQUIRE(model.materialList.size() == 2);
	const ModelMaterial& seat = model.materialList[0];
	CHECK(seat.name == "Seat");
	CHECK(seat.color.r == 231);
	CHECK(std::abs(int{seat.color.g} - 124) <= 1);
	CHECK(seat.roughness == doctest::Approx(0.5));
	CHECK(seat.texture == "textures/seat.png");
	const ModelMaterial& legs = model.materialList[1];
	CHECK(legs.color.r == 128);
	CHECK(legs.roughness == doctest::Approx(0.3));
	CHECK(legs.texture == "../wood grain.png");
	// The quad: its given normal; its first corner's v flipped.
	CHECK(model.normals[0].y == doctest::Approx(1.0));
	CHECK(model.uvs[0].v == doctest::Approx(1.0));
	CHECK(model.positions[4].x == 1.0); // second triangle of the fan: 1, 3, 4
	CHECK(model.positions[4].z == 1.0);
	// The triangle has no normals: flat, from its winding.
	CHECK(model.normals[6].y == doctest::Approx(-1.0));

	// Windows line endings (a Git checkout with CRLF) read the same.
	auto crlf = [](std::string text)
	{
		for (std::size_t at = text.find('\n'); at != std::string::npos; at = text.find('\n', at + 2))
		{
			text.insert(at, 1, '\r');
		}
		return text;
	};
	const std::string crlfMtl = crlf(mtl);
	const Result<RawModel> windows = readObj(crlf(ChairObj), [&crlfMtl](std::string_view reference) -> std::optional<Bytes>
	{
		return reference == "chair.mtl" ? std::optional<Bytes>(Bytes(crlfMtl.begin(), crlfMtl.end())) : std::nullopt;
	});
	REQUIRE(windows.ok());
	CHECK(windows.value().triangleCount() == 3);
	CHECK(windows.value().materialList[0].texture == "textures/seat.png");
	CHECK(windows.value().materialList[1].texture == "../wood grain.png");
	CHECK(windows.value().materialList[1].roughness == doctest::Approx(0.3));

	const Result<RawModel> alone = readObj(ChairObj, NoFiles);
	REQUIRE(alone.ok());
	CHECK(alone.value().warnings.size() >= 2); // library and materials not found
	CHECK(alone.value().materialList[0].color.r == 204);

	CHECK(readObj("v 0 0 0\nf 1 2 3\n", NoFiles).error().code == ErrorCode::ParseError);
	CHECK(readObj("v 0 0\n", NoFiles).error().message.find("OBJ line 1") == 0);
	CHECK(readObj("v 0 0 0\nv 1 0 0\n", NoFiles).error().message == "The OBJ file has no faces");
	CHECK(readObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 0\n", NoFiles).error().code == ErrorCode::ParseError);
}

TEST_CASE("glTF: node transforms, indices, normals, UVs, materials and embedded images; the same from a GLB")
{
	const GltfFixture fixture("data:application/octet-stream;base64," + base64(GltfFixture("").buffer));
	const Result<RawModel> read = readGltf(fixture.gltf(), NoFiles);
	REQUIRE(read.ok());
	const RawModel& model = read.value();
	CHECK(model.triangleCount() == 2);
	// (1, 0, 0) turned 90 degrees about Y is (0, 0, -1); then moved by (1, 0, 0).
	CHECK(model.positions[1].x == doctest::Approx(1.0));
	CHECK(model.positions[1].z == doctest::Approx(-1.0));
	CHECK(model.normals[0].x == doctest::Approx(1.0)); // +z turned to +x
	CHECK(model.uvs[1].u == doctest::Approx(1.0));
	REQUIRE(model.materialList.size() == 2);
	CHECK(model.materialList[0].name == "Grey");
	CHECK(model.materialList[0].color.g == 128);
	CHECK(model.materialList[0].roughness == doctest::Approx(0.4));
	CHECK(model.materialList[0].texture == "embedded_0.png");
	CHECK(model.materialList[1].name == "default");
	CHECK(model.materials == std::vector<std::uint32_t>{0, 1});
	REQUIRE(model.embeddedImages.size() == 1);
	CHECK(model.embeddedImages[0].name == "embedded_0.png");
	CHECK(model.embeddedImages[0].bytes == pngHeader(4, 4));

	const GltfFixture binary("");
	const Result<RawModel> fromGlb = readGltf(binary.glb(), NoFiles);
	REQUIRE(fromGlb.ok());
	CHECK(fromGlb.value().positions.size() == model.positions.size());
	CHECK(fromGlb.value().positions[1].z == doctest::Approx(-1.0));

	// A buffer and an image as files next to the model ("%20" decoded).
	const GltfFixture external("mesh%20data.bin", R"({"uri":"tex/wood.png"})");
	int asked = 0;
	const ModelFileReader files = [&](std::string_view reference) -> std::optional<Bytes>
	{
		++asked;
		return reference == "mesh data.bin" ? std::optional<Bytes>(external.buffer) : std::nullopt;
	};
	const Result<RawModel> withFiles = readGltf(external.gltf(), files);
	REQUIRE(withFiles.ok());
	CHECK(asked == 1);
	CHECK(withFiles.value().materialList[0].texture == "tex/wood.png");
	CHECK(withFiles.value().embeddedImages.empty());
	CHECK(readGltf(external.gltf(), NoFiles).error().message == "The model's buffer 'mesh data.bin' was not found");
}

TEST_CASE("glTF: triangle strips and fans, mirrored nodes, and what is refused")
{
	auto strip = [](int mode)
	{
		Bytes buffer;
		append<float>(buffer, {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0});
		const std::string json = R"({"asset":{"version":"2.0"},"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":)" +
			std::to_string(mode) + R"(}]}],"buffers":[{"uri":"data:application/octet-stream;base64,)" + base64(buffer) +
			R"(","byteLength":48}],"bufferViews":[{"buffer":0,"byteLength":48}],)"
			R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"}]})";
		return readGltf(Bytes(json.begin(), json.end()), NoFiles);
	};
	const Result<RawModel> stripped = strip(5);
	REQUIRE(stripped.ok());
	CHECK(stripped.value().triangleCount() == 2);
	for (std::size_t t = 0; t < 2; ++t)
	{
		CHECK(stripped.value().normals[t * 3].z == doctest::Approx(1.0)); // both counter-clockwise from +z
	}
	const Result<RawModel> fanned = strip(6);
	REQUIRE(fanned.ok());
	CHECK(fanned.value().triangleCount() == 2);
	CHECK(fanned.value().positions[2].x == 0.0); // (v1, v2, v0)
	CHECK(strip(1).error().message == "glTF: the model has no triangles"); // lines only

	auto edited = [](const std::string& from, const std::string& to)
	{
		GltfFixture fixture("data:application/octet-stream;base64," + base64(GltfFixture("").buffer));
		fixture.json.replace(fixture.json.find(from), from.size(), to);
		return readGltf(fixture.gltf(), NoFiles);
	};
	CHECK(edited(R"("scene":0,)", R"("extensionsRequired":["KHR_draco_mesh_compression"],"scene":0,)").error().message ==
		"The model needs glTF extensions 101st Floor cannot read yet: KHR_draco_mesh_compression");
	CHECK(edited(R"("version":"2.0")", R"("version":"1.0")").error().message.find("only glTF 2.0") != std::string::npos);
	CHECK(edited(R"("count":3,"type":"SCALAR"})", R"("count":3,"type":"SCALAR","sparse":{}})").error().message ==
		"glTF: sparse accessors are not supported");
	CHECK(edited(R"("byteOffset":96,"byteLength":6})", R"("byteOffset":96,"byteLength":4})").error().message.find("outside") !=
		std::string::npos);
	CHECK(edited(R"({"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"})",
			  R"({"bufferView":0,"componentType":5126,"count":2,"type":"VEC3"})").error().message ==
		"glTF: an index refers to a vertex that does not exist");
	// A mirrored node (negative scale) keeps its faces facing out: the winding is flipped and the normals follow.
	const Result<RawModel> mirrored = edited(R"("rotation":[0,0.70710678118654757,0,0.70710678118654757])", R"("scale":[-1,1,1])");
	REQUIRE(mirrored.ok());
	CHECK(mirrored.value().positions[1].y == doctest::Approx(1.0)); // (0, 1, 0) second: the winding flipped
	for (std::size_t t = 0; t < 2; ++t)
	{
		CHECK(mirrored.value().normals[t * 3].z == doctest::Approx(1.0)); // the given normal and the flat one
	}
	CHECK(edited(R"("nodes":[{"mesh":0,)", R"("nodes":[{"children":[0],"mesh":0,)").error().message == "glTF: the node tree is invalid");
	CHECK(readGltf(Bytes{'{', 'x'}, NoFiles).error().message == "glTF: the JSON is not valid");
}

TEST_CASE("a model is normalized to millimetres, z up, its front to -y, centred on its footprint and on the floor")
{
	const std::string mtl = chairMtl("wood.png");
	const ModelFileReader files = [&mtl](std::string_view) { return std::optional<Bytes>(Bytes(mtl.begin(), mtl.end())); };
	const RawModel raw = readObj(ChairObj, files).value();
	const LoadedModel metres = normalizeModel(raw, ModelSettings{});
	CHECK(metres.mesh.widthMm == doctest::Approx(1000.0));
	CHECK(metres.mesh.depthMm == doctest::Approx(1000.0)); // Y up: the model's z becomes depth
	CHECK(metres.mesh.heightMm == doctest::Approx(0.0));
	CHECK(metres.mesh.mesh.positions[0].x == doctest::Approx(-500.0));
	CHECK(metres.mesh.mesh.positions[0].y == doctest::Approx(500.0)); // native z = 0 is the back (+y) once turned
	CHECK(metres.mesh.mesh.normals[0].z == doctest::Approx(1.0));      // native +y is up
	REQUIRE(metres.mesh.mesh.sections.size() == 2);
	CHECK(metres.mesh.mesh.sections[0].slot == "m0");
	CHECK(metres.mesh.mesh.sections[0].indices.size() == 6);
	CHECK(metres.triangles == 3);

	const LoadedModel zUp = normalizeModel(raw, ModelSettings{cad::ModelUnit::Centimeter, cad::ModelUpAxis::Z, 1});
	// Z up keeps the quad standing in the x-z plane (1 cm by 1 cm); a quarter turn sends its width to the depth.
	CHECK(zUp.mesh.widthMm == doctest::Approx(0.0));
	CHECK(zUp.mesh.depthMm == doctest::Approx(10.0));
	CHECK(zUp.mesh.heightMm == doctest::Approx(10.0));
	CHECK(zUp.mesh.mesh.normals[0].x == doctest::Approx(-1.0)); // +y turned a quarter counter-clockwise is -x

	RawModel big = raw;
	for (geometry::Vec3& p : big.positions)
	{
		p = geometry::Vec3{p.x * 200.0, p.y * 200.0, p.z * 200.0};
	}
	CHECK(guessUnit(raw) == cad::ModelUnit::Meter);
	CHECK(guessUnit(big) == cad::ModelUnit::Centimeter); // 200 m would not be furniture: 2 m in centimetres
	for (geometry::Vec3& p : big.positions)
	{
		p = geometry::Vec3{p.x * 100.0, p.y * 100.0, p.z * 100.0};
	}
	CHECK(guessUnit(big) == cad::ModelUnit::Millimeter);
}

TEST_CASE("importing copies a texture into the project and sizes it by its proportions; bad files leave nothing")
{
	TempFolder source;
	TempFolder project;
	write(source.path / "parquet.png", pngHeader(512, 256));
	const cad::AssetId id{cad::Uuid{7, 1}};
	const Result<ImportedAsset> imported = importAsset(source.path / "parquet.png", project.path, id);
	REQUIRE(imported.ok());
	const cad::Asset& asset = imported.value().asset;
	CHECK(asset.kind == cad::AssetKind::Texture);
	CHECK(asset.name == "parquet");
	CHECK(asset.file == cad::assetFolder(id) + "/parquet.png");
	CHECK(asset.texture.widthMm == 1000);
	CHECK(asset.texture.heightMm == 500);
	CHECK(asset.texture.pixelWidth == 512);
	CHECK(asset.texture.surfaces == cad::AllTextureSurfaces);
	CHECK(std::filesystem::file_size(assetFilePath(project.path, asset)) == pngHeader(512, 256).size());

	// Unicode names, JPEG.
	const std::filesystem::path stone = source.path / pathFromUtf8("piedra ñ.JPG");
	write(stone, jpegHeader(300, 600));
	const Result<ImportedAsset> jpeg = importAsset(stone, project.path, cad::AssetId{cad::Uuid{7, 2}});
	REQUIRE(jpeg.ok());
	CHECK(jpeg.value().asset.name == "piedra ñ");
	CHECK(jpeg.value().asset.texture.heightMm == 2000);
	CHECK(std::filesystem::exists(assetFilePath(project.path, jpeg.value().asset)));

	write(source.path / "fake.png", std::string("not an image"));
	const cad::AssetId fakeId{cad::Uuid{7, 3}};
	CHECK(importAsset(source.path / "fake.png", project.path, fakeId).error().message == "Not a PNG or JPEG image");
	CHECK_FALSE(std::filesystem::exists(project.path / pathFromUtf8(cad::assetFolder(fakeId))));
	write(source.path / "song.mp3", std::string("x"));
	CHECK(importAsset(source.path / "song.mp3", project.path, fakeId).error().code == ErrorCode::InvalidArgument);
	CHECK(importAsset(source.path / "missing.png", project.path, fakeId).error().code == ErrorCode::NotFound);
}

TEST_CASE("importing a model brings its material libraries, buffers and textures, and embedded images as files")
{
	TempFolder source;
	TempFolder elsewhere;
	TempFolder project;
	write(elsewhere.path / "wood.png", pngHeader(8, 8));
	const std::filesystem::path chair = source.path / "chair" / "chair.obj";
	write(chair, std::string(ChairObj));
	const std::u8string wood = (elsewhere.path / "wood.png").generic_u8string(); // an absolute path, as some exporters write
	write(chair.parent_path() / "chair.mtl", chairMtl(std::string(wood.begin(), wood.end())));
	write(chair.parent_path() / "textures" / "seat.png", pngHeader(16, 16));
	const cad::AssetId id{cad::Uuid{8, 1}};
	const Result<ImportedAsset> imported = importAsset(chair, project.path, id);
	REQUIRE(imported.ok());
	const cad::Asset& asset = imported.value().asset;
	CHECK(asset.kind == cad::AssetKind::Model);
	CHECK(asset.name == "chair");
	CHECK(asset.model.unit == cad::ModelUnit::Meter);
	CHECK(asset.model.widthMm == 1000);
	CHECK(asset.model.depthMm == 1000);
	CHECK(asset.model.triangles == 3);
	const std::filesystem::path folder = project.path / pathFromUtf8(cad::assetFolder(id));
	CHECK(std::filesystem::exists(folder / "chair.obj"));
	CHECK(std::filesystem::exists(folder / "chair.mtl"));
	CHECK(std::filesystem::exists(folder / "textures" / "seat.png"));
	CHECK(std::filesystem::exists(folder / "wood.png")); // named by an absolute path: brought by name

	const Result<LoadedModel> loaded = loadModelFile(folder / "chair.obj", ModelSettings{});
	REQUIRE(loaded.ok());
	CHECK(loaded.value().materials[0].texture == "textures/seat.png");
	CHECK(loaded.value().materials[1].texture == "wood.png");
	CHECK(loaded.value().warnings.empty());

	// A GLB with its image inside: the image is written out next to it.
	const std::filesystem::path armchair = source.path / "armchair.glb";
	write(armchair, GltfFixture("").glb());
	const cad::AssetId glbId{cad::Uuid{8, 2}};
	const Result<ImportedAsset> glb = importAsset(armchair, project.path, glbId);
	REQUIRE(glb.ok());
	const std::filesystem::path glbFolder = project.path / pathFromUtf8(cad::assetFolder(glbId));
	CHECK(std::filesystem::exists(glbFolder / "embedded_0.png"));
	CHECK(loadModelFile(glbFolder / "armchair.glb", ModelSettings{}).value().materials[0].texture == "embedded_0.png");

	// A .gltf with its buffer next to it; a missing texture is a warning, not an error.
	const std::filesystem::path lamp = source.path / "lamp" / "lamp.gltf";
	const GltfFixture external("lamp.bin", R"({"uri":"missing.png"})");
	write(lamp, external.gltf());
	write(lamp.parent_path() / "lamp.bin", external.buffer);
	const Result<ImportedAsset> gltf = importAsset(lamp, project.path, cad::AssetId{cad::Uuid{8, 3}});
	REQUIRE(gltf.ok());
	CHECK(std::filesystem::exists(project.path / pathFromUtf8(cad::assetFolder(cad::AssetId{cad::Uuid{8, 3}}) + "/lamp.bin")));
	CHECK(gltf.value().warnings.size() == 1);

	// Models in centimetres are guessed; a broken model leaves nothing behind.
	std::string big;
	for (const char* line : {"v 0 0 0\n", "v 200 0 0\n", "v 0 90 0\n", "f 1 2 3\n"})
	{
		big += line;
	}
	write(source.path / "sofa.obj", big);
	const Result<ImportedAsset> sofa = importAsset(source.path / "sofa.obj", project.path, cad::AssetId{cad::Uuid{8, 4}});
	REQUIRE(sofa.ok());
	CHECK(sofa.value().asset.model.unit == cad::ModelUnit::Centimeter);
	CHECK(sofa.value().asset.model.widthMm == 2000);
	CHECK(sofa.value().asset.model.heightMm == 900);
	write(source.path / "broken.gltf", std::string("{"));
	const cad::AssetId brokenId{cad::Uuid{8, 5}};
	CHECK_FALSE(importAsset(source.path / "broken.gltf", project.path, brokenId).ok());
	CHECK_FALSE(std::filesystem::exists(project.path / pathFromUtf8(cad::assetFolder(brokenId))));
}

TEST_CASE("models are measured with new settings, cached per file and settings, and asset folders follow a project")
{
	TempFolder source;
	TempFolder project;
	TempFolder copy;
	write(source.path / "armchair.glb", GltfFixture("").glb());
	const Result<ImportedAsset> imported = importAsset(source.path / "armchair.glb", project.path, cad::AssetId{cad::Uuid{9, 1}});
	REQUIRE(imported.ok());
	cad::Asset asset = imported.value().asset;

	cad::ModelAsset settings = asset.model;
	settings.unit = cad::ModelUnit::Centimeter;
	settings.up = cad::ModelUpAxis::Z;
	const Result<cad::ModelAsset> measured = measureModel(project.path, asset, settings);
	REQUIRE(measured.ok());
	CHECK(measured.value().unit == cad::ModelUnit::Centimeter);
	CHECK(measured.value().widthMm * 100 == asset.model.widthMm);

	ModelCache cache;
	const LoadedModel* first = cache.get(project.path, asset);
	REQUIRE(first != nullptr);
	CHECK(cache.get(project.path, asset) == first); // cached
	const double width = first->mesh.widthMm;
	asset.model.unit = cad::ModelUnit::Centimeter;
	const LoadedModel* again = cache.get(project.path, asset);
	REQUIRE(again != nullptr);
	CHECK(again->mesh.widthMm == doctest::Approx(width / 100.0));
	CHECK(cache.error(asset.id).empty());

	cad::Project owner;
	owner.assets = {asset};
	REQUIRE(copyAssetFiles(owner, project.path, copy.path).ok());
	CHECK(std::filesystem::exists(assetFilePath(copy.path, asset)));
	CHECK(std::filesystem::exists(copy.path / pathFromUtf8(cad::assetFolder(asset.id)) / "embedded_0.png"));
	CHECK(copyAssetFiles(owner, project.path, project.path).ok()); // same folder: nothing to do

	std::filesystem::remove_all(project.path / "assets");
	cache.clear();
	CHECK(cache.get(project.path, asset) == nullptr);
	CHECK(cache.error(asset.id).find("Cannot open") == 0);
}

TEST_CASE("Blender's own exports read as made: a GLB armchair with a packed texture and an OBJ lamp with its MTL")
{
	const std::filesystem::path data = pathFromUtf8(F101_TEST_DATA_DIR) / "Assets";
	auto named = [](const std::vector<ModelMaterial>& materials, const std::string& name) -> const ModelMaterial*
	{
		const auto found = std::find_if(materials.begin(), materials.end(), [&name](const ModelMaterial& m) { return m.name == name; });
		return found == materials.end() ? nullptr : &*found;
	};

	const Result<ReadModel> armchair = readModelFile(data / "armchair.glb");
	REQUIRE(armchair.ok());
	const ModelMaterial* fabric = named(armchair.value().raw.materialList, "Upholstery");
	const ModelMaterial* wood = named(armchair.value().raw.materialList, "Wood");
	REQUIRE(fabric != nullptr);
	REQUIRE(wood != nullptr);
	CHECK(fabric->texture.rfind("embedded_", 0) == 0);
	CHECK(fabric->roughness == doctest::Approx(0.9));
	CHECK(wood->texture.empty());
	CHECK(std::abs(int{wood->color.r} - 137) <= 1); // linear 0.25 as sRGB
	CHECK(armchair.value().raw.embeddedImages.size() == 1);
	const LoadedModel chair = normalizeModel(armchair.value().raw, ModelSettings{});
	CHECK(chair.mesh.widthMm == doctest::Approx(800.0).epsilon(0.001));
	CHECK(chair.mesh.depthMm == doctest::Approx(750.0).epsilon(0.001));
	CHECK(chair.mesh.heightMm == doctest::Approx(900.0).epsilon(0.001));
	// Its back (Blender +y) is at the furniture's back (+y): the highest points are there.
	double backY = 0.0;
	double topZ = -1.0;
	for (const geometry::Vec3& p : chair.mesh.mesh.positions)
	{
		if (p.z > topZ)
		{
			topZ = p.z;
			backY = p.y;
		}
	}
	CHECK(backY > 200.0);

	const Result<LoadedModel> lamp = loadModelFile(data / "lamp.obj", ModelSettings{});
	REQUIRE(lamp.ok());
	CHECK(lamp.value().warnings.empty());
	CHECK(lamp.value().mesh.widthMm == doctest::Approx(500.0).epsilon(0.001));
	CHECK(lamp.value().mesh.heightMm == doctest::Approx(1600.0).epsilon(0.001));
	const ModelMaterial* metal = named(lamp.value().materials, "Metal");
	const ModelMaterial* shade = named(lamp.value().materials, "Shade");
	REQUIRE(metal != nullptr);
	REQUIRE(shade != nullptr);
	CHECK(metal->roughness == doctest::Approx(0.3).epsilon(0.01)); // Blender writes Ns from the roughness
	CHECK(shade->roughness == doctest::Approx(0.8).epsilon(0.01));
	CHECK(shade->texture == "lamp_shade.png");
	CHECK(shade->color.r == 255); // textured without Kd: the texture as it is

	TempFolder project;
	const Result<ImportedAsset> imported = importAsset(data / "armchair.glb", project.path, cad::AssetId{cad::Uuid{10, 1}});
	REQUIRE(imported.ok());
	CHECK(imported.value().asset.model.widthMm == 800);
	CHECK(imported.value().warnings.empty());
	const Result<ImportedAsset> parquet = importAsset(data / "parquet.png", project.path, cad::AssetId{cad::Uuid{10, 2}});
	REQUIRE(parquet.ok());
	CHECK(parquet.value().asset.texture.pixelWidth == 256);
	const Result<ImportedAsset> stone = importAsset(data / "stone.jpg", project.path, cad::AssetId{cad::Uuid{10, 3}});
	REQUIRE(stone.ok());
	CHECK(stone.value().asset.texture.heightMm == 1000);
}
