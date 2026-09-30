#pragma once

#include "F101/Assets/Files.h"
#include "F101/Cad/Finishes.h"
#include "F101/Geometry/MeshData.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Imported models (ADR-0017): glTF 2.0 (.gltf with its buffers, .glb) and Wavefront OBJ (with its MTL libraries), read
// into triangles in the model's own units and axes. Written for this project: no third-party importer.
namespace f101::assets
{
	struct ModelMaterial
	{
		std::string name;
		cad::Rgb8 color{204, 204, 204}; // base colour (sRGB)
		double roughness = 0.8;         // 0 mirror .. 1 matte
		std::string texture;            // base colour image as the model names it ("textures/wood.png"); empty: none

		friend bool operator==(const ModelMaterial&, const ModelMaterial&) = default;
	};

	// An image stored inside the model (glTF buffer or data: URI), named "embedded_<index>.<png|jpg>" by the reader; the
	// importer writes it next to the model so textures are always files.
	struct EmbeddedImage
	{
		std::string name;
		std::vector<std::uint8_t> bytes;
	};

	// Triangles as read: three corners each, counter-clockwise seen from the side their normal points to.
	struct RawModel
	{
		std::vector<geometry::Vec3> positions; // per corner, in model units and axes
		std::vector<geometry::Vec3> normals;   // per corner, unit length (flat where the file has none)
		std::vector<geometry::Vec2> uvs;       // per corner, image convention: (0, 0) the image's top left
		std::vector<std::uint32_t> materials;  // per triangle, into `materialList`
		std::vector<ModelMaterial> materialList;
		std::vector<EmbeddedImage> embeddedImages;
		std::vector<std::string> warnings;

		std::size_t triangleCount() const { return materials.size(); }
	};

	// Most triangles a model may have.
	inline constexpr std::size_t MaxModelTriangles = 2'000'000;

	// The bytes of a file the model refers to (a material library, a buffer), named as the model names it
	// (normalizeReference); nullopt if it does not exist.
	using ModelFileReader = std::function<std::optional<std::vector<std::uint8_t>>(std::string_view reference)>;

	// Wavefront OBJ: v, vt, vn, f (polygons as fans, negative indices), usemtl, mtllib (Kd, map_Kd, Ns or Pr). Texture v
	// runs up in OBJ and is turned to the image convention.
	FLOOR101ASSETS_API Result<RawModel> readObj(std::string_view text, const ModelFileReader& files);

	// glTF 2.0, JSON (.gltf) or binary (.glb): the default scene's node tree (matrices or translation, rotation and scale),
	// triangle, strip and fan primitives with POSITION, NORMAL and TEXCOORD_0 (float, or normalized bytes and shorts), and
	// materials (baseColorFactor, baseColorTexture, roughnessFactor). Buffers as files, data: URIs or the GLB chunk. Models
	// that require an extension (e.g. Draco compression) or use sparse accessors are refused with the reason.
	FLOOR101ASSETS_API Result<RawModel> readGltf(const std::vector<std::uint8_t>& bytes, const ModelFileReader& files);
}
