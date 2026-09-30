#include "F101/Cad/Asset.h"

#include "F101/Cad/Units.h"

namespace f101::cad
{
	double millimetersPerUnit(ModelUnit unit)
	{
		switch (unit)
		{
		case ModelUnit::Centimeter: return 10.0;
		case ModelUnit::Millimeter: return 1.0;
		case ModelUnit::Inch: return 25.4;
		default: return 1000.0;
		}
	}

	std::string assetRef(const AssetId& id)
	{
		return std::string(AssetRefPrefix) + toString(id);
	}

	std::optional<AssetId> parseAssetRef(std::string_view text)
	{
		if (text.substr(0, AssetRefPrefix.size()) != AssetRefPrefix)
		{
			return std::nullopt;
		}
		const Result<Uuid> uuid = parseUuid(text.substr(AssetRefPrefix.size()));
		if (!uuid || uuid.value().isNil())
		{
			return std::nullopt;
		}
		return AssetId{uuid.value()};
	}

	bool isAssetRef(std::string_view text)
	{
		return parseAssetRef(text).has_value();
	}

	std::string modelMaterialFinish(const AssetId& id, std::size_t material)
	{
		return assetRef(id) + "#" + std::to_string(material);
	}

	std::string assetFolder(const AssetId& id)
	{
		return "assets/" + toString(id);
	}

	Status validateAssetPath(std::string_view file)
	{
		if (file.empty() || file.size() > 400)
		{
			return Error{ErrorCode::InvalidArgument, "An asset file needs a path of 1 to 400 characters"};
		}
		std::size_t start = 0;
		while (start <= file.size())
		{
			const std::size_t end = file.find('/', start);
			const std::string_view part = file.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
			if (part.empty() || part == "." || part == "..")
			{
				return Error{ErrorCode::InvalidArgument, "Asset path '" + std::string(file) + "' must stay inside the project's folder"};
			}
			for (const char c : part)
			{
				if (c == '\\' || c == ':' || static_cast<unsigned char>(c) < 0x20)
				{
					return Error{ErrorCode::InvalidArgument, "Asset path '" + std::string(file) + "' has a character it cannot have"};
				}
			}
			if (end == std::string_view::npos)
			{
				break;
			}
			start = end + 1;
		}
		return Status::success();
	}

	Status validateAsset(const Asset& asset)
	{
		if (asset.id.isNil())
		{
			return Error{ErrorCode::InvalidArgument, "Asset id is nil"};
		}
		if (asset.name.empty() || asset.name.size() > MaxAssetNameBytes)
		{
			return Error{ErrorCode::OutOfRange, "An asset needs a name of 1 to " + std::to_string(MaxAssetNameBytes) + " bytes"};
		}
		if (Status path = validateAssetPath(asset.file); !path)
		{
			return path;
		}
		const std::string folder = assetFolder(asset.id) + "/";
		if (asset.file.compare(0, folder.size(), folder) != 0 || asset.file.size() == folder.size())
		{
			return Error{ErrorCode::InvalidArgument, "Asset file '" + asset.file + "' must be inside " + folder};
		}
		if (asset.kind == AssetKind::Texture)
		{
			const TextureAsset& texture = asset.texture;
			if (texture.widthMm <= 0 || texture.widthMm > MaxAbsMillimeters || texture.heightMm <= 0 || texture.heightMm > MaxAbsMillimeters)
			{
				return Error{ErrorCode::OutOfRange, "A texture repeats every 1 mm or more"};
			}
			if (texture.roughnessPercent < 0 || texture.roughnessPercent > 100)
			{
				return Error{ErrorCode::OutOfRange, "Roughness goes from 0 to 100 %"};
			}
			if ((texture.surfaces & AllTextureSurfaces) == 0 || (texture.surfaces & ~AllTextureSurfaces) != 0)
			{
				return Error{ErrorCode::InvalidArgument, "A texture needs at least one surface to go on (not glass)"};
			}
			return Status::success();
		}
		const ModelAsset& model = asset.model;
		if (model.frontQuarterTurns < 0 || model.frontQuarterTurns > 3)
		{
			return Error{ErrorCode::OutOfRange, "A model's front turns 0 to 3 quarter turns"};
		}
		if (model.widthMm < 0 || model.depthMm < 0 || model.heightMm < 0 || model.widthMm > MaxAbsMillimeters ||
			model.depthMm > MaxAbsMillimeters || model.heightMm > MaxAbsMillimeters || model.triangles < 0)
		{
			return Error{ErrorCode::OutOfRange, "A model's size is outside the model range"};
		}
		return Status::success();
	}
}
