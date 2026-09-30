#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Finishes.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Imported assets (ADR-0017): image textures and 3D models the user brings into a project. The project keeps a record
// of each one; its files live in the project's folder, under assets/<uuid>/. A texture is used as a finish and a model as
// a type of furniture, both named by the asset's reference "asset:<uuid>".
namespace f101::cad
{
	enum class AssetKind
	{
		Texture, // a PNG or JPEG image
		Model,   // a glTF 2.0 (.gltf, .glb) or OBJ model, with its materials and textures
	};

	enum class ModelUnit
	{
		Meter,
		Centimeter,
		Millimeter,
		Inch,
	};

	enum class ModelUpAxis
	{
		Y, // glTF, and OBJ as most tools export it
		Z,
	};

	// Millimetres in one unit of a model's coordinates.
	FLOOR101CADCORE_API double millimetersPerUnit(ModelUnit unit);

	inline constexpr std::uint8_t AllTextureSurfaces = static_cast<std::uint8_t>(Surface::Wall) |
		static_cast<std::uint8_t>(Surface::Floor) | static_cast<std::uint8_t>(Surface::Ceiling) |
		static_cast<std::uint8_t>(Surface::Carpentry) | static_cast<std::uint8_t>(Surface::Furniture);

	// A texture: the image repeats every widthMm along a surface and every heightMm across it.
	struct TextureAsset
	{
		std::int64_t widthMm = 1000;
		std::int64_t heightMm = 1000;
		int roughnessPercent = 70; // 0 mirror .. 100 matte
		std::uint8_t surfaces = AllTextureSurfaces; // Surface flags it may be applied to (not glazing)
		int pixelWidth = 0; // of the image, for information
		int pixelHeight = 0;

		friend bool operator==(const TextureAsset&, const TextureAsset&) = default;
	};

	// A model: how to read its coordinates, and its size once read (x across its front, y front to back, z up).
	struct ModelAsset
	{
		ModelUnit unit = ModelUnit::Meter;
		ModelUpAxis up = ModelUpAxis::Y;
		int frontQuarterTurns = 0; // [0, 3]: turns it counter-clockwise, seen from above, so its front faces -y
		bool againstWall = false;  // placed with the mouse near a wall, its back goes against it
		std::int64_t widthMm = 0;  // its size read with these settings (set on import and when they change)
		std::int64_t depthMm = 0;
		std::int64_t heightMm = 0;
		std::int64_t triangles = 0; // for information

		friend bool operator==(const ModelAsset&, const ModelAsset&) = default;
	};

	struct Asset
	{
		AssetId id;
		AssetKind kind = AssetKind::Texture;
		std::string name;
		std::string file; // the imported file, relative to the project's folder: assets/<uuid>/<file name>
		TextureAsset texture; // for textures
		ModelAsset model;     // for models
		// The built-in library item it was added from (MVP 11, ADR-0019): lower-case letters, digits and hyphens; empty for
		// the user's own imports. The same item is added once per project.
		std::string library;

		friend bool operator==(const Asset&, const Asset&) = default;
	};

	inline constexpr std::string_view AssetRefPrefix = "asset:";
	inline constexpr std::size_t MaxAssetNameBytes = 128;
	// Sizes a model can be placed at, per axis.
	inline constexpr std::int64_t MinModelSizeMm = 10;
	inline constexpr std::int64_t MaxModelSizeMm = 100'000;

	// "asset:<uuid>": the finish id of a texture and the furniture type of a model.
	FLOOR101CADCORE_API std::string assetRef(const AssetId& id);
	// The asset a reference names; nullopt if the text is not "asset:<uuid>".
	FLOOR101CADCORE_API std::optional<AssetId> parseAssetRef(std::string_view text);
	FLOOR101CADCORE_API bool isAssetRef(std::string_view text);
	// Finish of one material of a model ("asset:<uuid>#<index>"): given by the model, never chosen by the user.
	FLOOR101CADCORE_API std::string modelMaterialFinish(const AssetId& id, std::size_t material);
	// The folder of an asset's files, relative to the project's folder: "assets/<uuid>".
	FLOOR101CADCORE_API std::string assetFolder(const AssetId& id);

	// A path inside the project's folder: relative, '/'-separated, without empty, "." or ".." parts, drive letters or
	// backslashes. Project files never point outside their folder.
	FLOOR101CADCORE_API Status validateAssetPath(std::string_view file);
	// A non-nil id, a name, its file inside its own folder (assetFolder), and settings in range: a texture's repeat in the
	// model range, roughness 0-100 and at least one surface (not glazing); a model's quarter turns 0-3; a library id, if
	// any, as isLibraryId.
	FLOOR101CADCORE_API Status validateAsset(const Asset& asset);
	// 1 to 64 lower-case letters, digits and hyphens: the ids of the built-in library's items (ADR-0019).
	FLOOR101CADCORE_API bool isLibraryId(std::string_view id);
}
