#pragma once

#include "F101/Assets/AssetImport.h"
#include "F101/Cad/Asset.h"
#include "F101/Cad/Finishes.h"
#include "F101/Cad/Furniture.h"
#include "F101/Cad/Project.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// The built-in library (ADR-0019): furniture models and textures shipped with the application in its Library folder,
// each subfolder described by a manifest.json. Using an item adds it to the project as an asset, copied next to the
// project like an import (so the project keeps working without the library), that remembers the item's id: the same item
// is added once per project.
namespace f101::assets
{
	struct LibraryFurniture
	{
		std::string id;   // cad::isLibraryId
		std::string name; // shown as is
		cad::FurnitureCategory category = cad::FurnitureCategory::Decor;
		std::string file;      // the model, relative to the library folder
		std::string thumbnail; // a PNG picture of it, relative to the library folder
		cad::ModelAsset model; // how to read it (unit, up axis, front, against a wall); its size is measured when added
		std::string credit;    // author and licence
	};

	struct LibraryTexture
	{
		std::string id;
		std::string name;
		cad::FinishCategory category = cad::FinishCategory::Tiles;
		std::string file;          // the image (also its thumbnail), relative to the library folder
		cad::TextureAsset texture; // repeat, roughness and surfaces
		std::string credit;
	};

	struct Library
	{
		std::vector<LibraryFurniture> furniture;
		std::vector<LibraryTexture> textures;

		FLOOR101ASSETS_API const LibraryFurniture* findFurniture(std::string_view id) const;
		FLOOR101ASSETS_API const LibraryTexture* findTexture(std::string_view id) const;
	};

	// One manifest: {"version": 1, "furniture": [...], "textures": [...]} (either list may be missing). Its paths are
	// relative to the manifest's folder: `folder` (relative to the library folder, '/'-separated) is put in front.
	FLOOR101ASSETS_API Result<Library> readLibraryManifest(std::string_view json, std::string_view folder);
	// Every <subfolder>/manifest.json of `libraryFolder`, with its files checked on disk; ids unique across them. No
	// folder: an empty library.
	FLOOR101ASSETS_API Result<Library> loadLibrary(const std::filesystem::path& libraryFolder);

	// Adds a library item to a project's folder as the asset `id`: importAsset, then the item's name, settings and library
	// id (a model is measured with its settings). On failure nothing is left behind.
	FLOOR101ASSETS_API Result<ImportedAsset> importLibraryFurniture(const LibraryFurniture& item,
		const std::filesystem::path& libraryFolder, const std::filesystem::path& projectFolder, const cad::AssetId& id);
	FLOOR101ASSETS_API Result<ImportedAsset> importLibraryTexture(const LibraryTexture& item,
		const std::filesystem::path& libraryFolder, const std::filesystem::path& projectFolder, const cad::AssetId& id);
	// The project's asset added from that library item, or null.
	FLOOR101ASSETS_API const cad::Asset* findLibraryAsset(const cad::Project& project, std::string_view libraryId);
}
