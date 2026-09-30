#pragma once

#include "F101/Assets/ModelLoader.h"
#include "F101/Cad/Project.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Importing assets into a project (ADR-0017): the files are copied into the project's folder (assets/<uuid>/), so a
// project folder carries everything it needs, and the project records each asset (cad::Asset).
namespace f101::assets
{
	// Texture for .png, .jpg and .jpeg; model for .gltf, .glb and .obj; nullopt for anything else.
	FLOOR101ASSETS_API std::optional<cad::AssetKind> assetKindOf(const std::filesystem::path& file);

	struct ImportedAsset
	{
		cad::Asset asset;
		std::vector<std::string> warnings; // e.g. a texture of the model that was not found
	};

	// Checks `source` (a readable image, or a model with triangles), copies it into projectFolder/assets/<id>/ with the
	// files it needs (a model's material libraries, buffers and textures, keeping their relative paths when they are
	// inside its folder; embedded images written out as files) and describes it: named after the file; a texture 1 m wide
	// and as tall as its proportions give, for every surface; a model in metres (glTF) or in the unit that gives it a
	// plausible size (OBJ), Y up, with its size. On failure nothing is left behind.
	FLOOR101ASSETS_API Result<ImportedAsset> importAsset(const std::filesystem::path& source,
		const std::filesystem::path& projectFolder, const cad::AssetId& id);

	// The asset's file on disk.
	FLOOR101ASSETS_API std::filesystem::path assetFilePath(const std::filesystem::path& projectFolder, const cad::Asset& asset);

	// A model's settings with the size and triangles it has when read with them (to change units, up axis or front).
	FLOOR101ASSETS_API Result<cad::ModelAsset> measureModel(const std::filesystem::path& projectFolder, const cad::Asset& asset,
		const cad::ModelAsset& settings);

	// Copies each asset's folder from one project folder to another (saving the project somewhere else). Folders already
	// there are kept (an asset's files never change).
	FLOOR101ASSETS_API Status copyAssetFiles(const cad::Project& project, const std::filesystem::path& from,
		const std::filesystem::path& to);

	// Models read from their files, once per file and settings, for the scene (scene::ModelProvider).
	class ModelCache
	{
	public:
		// The asset's model; null if it cannot be read (see error()).
		FLOOR101ASSETS_API const LoadedModel* get(const std::filesystem::path& projectFolder, const cad::Asset& asset);
		// Why the asset's model could not be read; empty if it could (or was never asked for).
		FLOOR101ASSETS_API std::string error(const cad::AssetId& asset) const;
		void clear() { entries.clear(); }

	private:
		struct Entry
		{
			std::string key; // file, settings and last write time
			std::optional<LoadedModel> model;
			std::string error;
		};
		std::map<cad::AssetId, Entry> entries;
	};
}
