#pragma once

#include "F101/Assets/AssetImport.h"
#include "F101/Cad/Export.h"
#include "F101/Cad/Finishes.h"
#include "F101/Cad/Project.h"
#include "F101/Cad/Result.h"
#include "F101/Scene/FinishTextures.h"
#include "F101/Scene/Scene.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#ifndef FLOOR101EXPORTERS_API
	#define FLOOR101EXPORTERS_API
#endif

// glTF 2.0 export (ADR-0020): the project as a presentation tool (Twinmotion, Blender...) imports it, written against the
// neutral scene (ADR-0003) like every consumer. Binary glTF (.glb): one file with the geometry, the materials and their
// images. Metres, Y up: a scene point (x, y, z) in mm is (x, z, -y) / 1000.
namespace f101::exporters
{
	// Names shown in the presentation tool (the application passes them in its language, like drawing::DrawingTexts).
	struct GltfTexts
	{
		std::string root = "101st Floor";
		std::string groundFloor = "Ground floor";
		std::string levelNumber = "Level {n}"; // {n}: 1, 2... counted from the ground floor
		std::string walls = "Walls";
		std::string floors = "Floors";
		std::string slabs = "Slabs";
		std::string openings = "Doors and windows";
		std::string furniture = "Furniture";
		std::string columns = "Columns";
		std::string services = "Building services";
		std::string wall = "Wall";
		std::string joint = "Joint";
		std::string floor = "Floor";
		std::string slab = "Slab";
		std::string door = "Door";
		std::string window = "Window";
		std::string column = "Column";
		std::string electrical = "Electrical"; // runs of each kind (ADR-0021)
		std::string water = "Water supply";
		std::string sewage = "Sewage";
		std::string gas = "Gas";
		// Catalogue finishes and furniture types by id; an id without a name is shown as is. Imported assets show their
		// own names.
		std::map<std::string, std::string, std::less<>> finishNames;
		std::map<std::string, std::string, std::less<>> furnitureNames;
	};

	// How a finish looks in a file that knows nothing of the catalogue: base colour times an image that covers
	// repeatWidthMm x repeatHeightMm of the surface (the mesh UVs are in metres, so the writer scales them by
	// 1000 / repeat), roughness and opacity. Metallic is always 0, like the application's material.
	struct GltfMaterialLook
	{
		std::string name;
		cad::Rgb8 color{255, 255, 255}; // sRGB; multiplies the image
		double roughness = 0.85;
		double opacity = 1.0; // less than 1: blended (glass)
		// The image, if any: pixels to encode as PNG, or a PNG or JPEG file as it is.
		scene::TextureImage pixels;
		std::vector<std::uint8_t> encodedImage;
		double repeatWidthMm = 1000.0;
		double repeatHeightMm = 1000.0;
		// Surface textures from image files: the image's top row goes up a wall and north on a floor
		// (scene::textureFromImage with flipVertically), which the writer does by mirroring v.
		bool flipImage = false;
		std::string warning; // e.g. an image that could not be read (the material keeps its colour)
	};

	using GltfMaterialSource = std::function<GltfMaterialLook(std::string_view finishId)>;

	struct GltfExport
	{
		std::vector<std::uint8_t> bytes; // the .glb file
		std::size_t objects = 0;         // scene nodes with geometry
		std::size_t materials = 0;
		std::size_t images = 0;
		std::size_t triangles = 0;
		std::vector<std::string> warnings;
	};

	// Writes `scene` as binary glTF 2.0: a root node, one node per level of `project` (bottom to top) with a group per
	// kind (walls and joints, floors, slabs, doors and windows, furniture, columns, building services), and one node per
	// scene node with a mesh of one primitive per section. Node names are stable and unique ("Wall 3f2a9c1e", "Sofa
	// 7117ade1", "Gas 0c4e81d2"); the scene node id,
	// finish id and level id go in `extras` (f101NodeId, f101FinishId, f101LevelId) so a tool can match them on reimport.
	// One material per finish id in order of first use; images are shared when their bytes are equal. Deterministic.
	FLOOR101EXPORTERS_API GltfExport writeGlb(const scene::Scene& scene, const cad::Project& project,
		const GltfMaterialSource& materials, const GltfTexts& texts);

	// The looks of the project's finishes, as the application shows them: catalogue finishes synthesised
	// (scene::buildFinishLook, `textureSize` pixels; a uniform one as a plain colour, glass as its tint and opacity),
	// imported textures as their files, and the materials of imported models from `models` (colour, roughness, image).
	FLOOR101EXPORTERS_API GltfMaterialSource projectMaterials(const cad::Project& project,
		const std::filesystem::path& projectFolder, assets::ModelCache& models, const GltfTexts& texts, int textureSize = 512);

	// The whole project (every level, with slabs; no view state) compiled with its models and written by writeGlb.
	// An error if there is nothing to export.
	FLOOR101EXPORTERS_API Result<GltfExport> exportProjectGlb(const cad::Project& project,
		const std::filesystem::path& projectFolder, assets::ModelCache& models, const GltfTexts& texts, int textureSize = 512);

	// Writes the file (through a temporary file next to it, so a failure leaves the old one), creating its folder.
	FLOOR101EXPORTERS_API Status saveGlbFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes);
}
