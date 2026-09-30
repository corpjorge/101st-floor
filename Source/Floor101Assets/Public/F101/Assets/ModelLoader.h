#pragma once

#include "F101/Assets/ModelReader.h"
#include "F101/Cad/Asset.h"
#include "F101/Geometry/ModelMesh.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Imported models (ADR-0017): from a file to a model in the frame of furniture.
namespace f101::assets
{
	struct ModelSettings
	{
		cad::ModelUnit unit = cad::ModelUnit::Meter;
		cad::ModelUpAxis up = cad::ModelUpAxis::Y;
		int frontQuarterTurns = 0;
	};

	struct LoadedModel
	{
		geometry::ModelMesh mesh;
		// One per material of the model (section "m<index>"). Textures are paths relative to the model's folder of files
		// that exist; empty when there is none or it was not found (a warning says which).
		std::vector<ModelMaterial> materials;
		std::size_t triangles = 0;
		std::vector<std::string> warnings;
	};

	enum class ModelFormat
	{
		Gltf, // .gltf and .glb
		Obj,
	};

	// By the file's extension; nullopt for any other file.
	FLOOR101ASSETS_API std::optional<ModelFormat> modelFormatOf(const std::filesystem::path& file);

	// The model in furniture's frame: millimetres (unit), z up (a Y-up model's (x, y, z) is (x, -z, y), so its front,
	// +z in glTF, faces -y), turned counter-clockwise by frontQuarterTurns, centred on its footprint and standing on z = 0.
	// One section per material that has triangles.
	FLOOR101ASSETS_API LoadedModel normalizeModel(const RawModel& raw, const ModelSettings& settings);

	// A unit that gives a piece of furniture a plausible size: metres unless that makes it over 30 m, then centimetres,
	// then millimetres.
	FLOOR101ASSETS_API cad::ModelUnit guessUnit(const RawModel& raw);

	// A model file as read, and the files it read beside it (material libraries, buffers).
	struct ReadModel
	{
		RawModel raw;
		std::vector<std::filesystem::path> filesRead;
	};

	// Reads a .gltf, .glb or .obj file; the files it names are looked for relative to its folder, or by their name in it
	// (resolveReference; `allowOutside` when importing).
	FLOOR101ASSETS_API Result<ReadModel> readModelFile(const std::filesystem::path& file, bool allowOutside = false);

	// readModelFile and normalizeModel, with each material's texture found in the model's folder.
	FLOOR101ASSETS_API Result<LoadedModel> loadModelFile(const std::filesystem::path& file, const ModelSettings& settings);
}
