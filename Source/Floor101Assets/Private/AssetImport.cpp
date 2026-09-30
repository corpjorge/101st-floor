#include "F101/Assets/AssetImport.h"

#include "F101/Assets/ImageInfo.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <system_error>

namespace f101::assets
{
	namespace
	{
		std::string lowerExtension(const std::filesystem::path& file)
		{
			std::string extension = utf8Of(file.extension());
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });
			return extension;
		}

		// A name of at most MaxAssetNameBytes, cut on a UTF-8 character boundary.
		std::string assetName(const std::filesystem::path& source)
		{
			std::string name = utf8Of(source.stem());
			if (name.size() > cad::MaxAssetNameBytes)
			{
				std::size_t cut = cad::MaxAssetNameBytes;
				while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80)
				{
					--cut;
				}
				name.resize(cut);
			}
			return name.empty() ? std::string("asset") : name;
		}

		Status copyFile(const std::filesystem::path& from, const std::filesystem::path& to)
		{
			std::error_code error;
			std::filesystem::create_directories(to.parent_path(), error);
			std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, error);
			if (error)
			{
				return Error{ErrorCode::InvalidArgument, "Cannot copy " + utf8Of(from) + " to " + utf8Of(to) + ": " + error.message()};
			}
			return Status::success();
		}

		Status writeFile(const std::filesystem::path& to, const std::vector<std::uint8_t>& bytes)
		{
			std::error_code error;
			std::filesystem::create_directories(to.parent_path(), error);
			std::ofstream out(to, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			return out ? Status::success() : Status(Error{ErrorCode::InvalidArgument, "Cannot write " + utf8Of(to)});
		}

		// Where a file the model uses goes in the asset's folder: its path relative to the model's folder when it is
		// inside it, else its name at the top.
		std::filesystem::path destinationOf(const std::filesystem::path& file, const std::filesystem::path& sourceFolder,
			const std::filesystem::path& destination)
		{
			const std::filesystem::path relative = file.lexically_relative(sourceFolder);
			const std::u8string text = relative.generic_u8string();
			const std::string inside = insideReference(std::string(text.begin(), text.end()));
			return inside.empty() ? destination / file.filename() : destination / pathFromUtf8(inside);
		}

		Result<ImportedAsset> importModel(const std::filesystem::path& source, const std::filesystem::path& destination,
			ImportedAsset imported)
		{
			Result<ReadModel> read = readModelFile(source, true);
			if (!read)
			{
				return read.error();
			}
			const std::filesystem::path sourceFolder = source.parent_path();
			if (Status copied = copyFile(source, destination / source.filename()); !copied)
			{
				return copied.error();
			}
			std::set<std::filesystem::path> copied;
			for (const std::filesystem::path& file : read.value().filesRead)
			{
				if (copied.insert(file).second)
				{
					if (Status done = copyFile(file, destinationOf(file, sourceFolder, destination)); !done)
					{
						return done.error();
					}
				}
			}
			for (const ModelMaterial& material : read.value().raw.materialList)
			{
				const std::filesystem::path texture = material.texture.empty() ? std::filesystem::path()
					: resolveReference(sourceFolder, material.texture, true);
				if (!texture.empty() && copied.insert(texture).second)
				{
					if (Status done = copyFile(texture, destinationOf(texture, sourceFolder, destination)); !done)
					{
						return done.error();
					}
				}
			}
			for (const EmbeddedImage& image : read.value().raw.embeddedImages)
			{
				if (Status done = writeFile(destination / pathFromUtf8(image.name), image.bytes); !done)
				{
					return done.error();
				}
			}

			cad::ModelAsset& model = imported.asset.model;
			model.unit = modelFormatOf(source) == ModelFormat::Obj ? guessUnit(read.value().raw) : cad::ModelUnit::Meter;
			model.up = cad::ModelUpAxis::Y;
			Result<LoadedModel> loaded = loadModelFile(destination / source.filename(), ModelSettings{model.unit, model.up, 0});
			if (!loaded)
			{
				return loaded.error();
			}
			model.widthMm = std::llround(loaded.value().mesh.widthMm);
			model.depthMm = std::llround(loaded.value().mesh.depthMm);
			model.heightMm = std::llround(loaded.value().mesh.heightMm);
			model.triangles = static_cast<std::int64_t>(loaded.value().triangles);
			imported.warnings = loaded.value().warnings;
			return imported;
		}
	}

	std::optional<cad::AssetKind> assetKindOf(const std::filesystem::path& file)
	{
		const std::string extension = lowerExtension(file);
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
		{
			return cad::AssetKind::Texture;
		}
		if (modelFormatOf(file))
		{
			return cad::AssetKind::Model;
		}
		return std::nullopt;
	}

	Result<ImportedAsset> importAsset(const std::filesystem::path& source, const std::filesystem::path& projectFolder,
		const cad::AssetId& id)
	{
		const std::optional<cad::AssetKind> kind = assetKindOf(source);
		if (!kind)
		{
			return Error{ErrorCode::InvalidArgument, "Import PNG or JPEG textures, or glTF (.gltf, .glb) or OBJ models"};
		}
		std::error_code error;
		if (!std::filesystem::is_regular_file(source, error))
		{
			return Error{ErrorCode::NotFound, "Cannot open " + utf8Of(source)};
		}
		const std::string folder = cad::assetFolder(id);
		const std::filesystem::path destination = projectFolder / pathFromUtf8(folder);
		std::filesystem::create_directories(destination, error);
		if (error)
		{
			return Error{ErrorCode::InvalidArgument, "Cannot create " + utf8Of(destination) + ": " + error.message()};
		}

		ImportedAsset imported;
		cad::Asset& asset = imported.asset;
		asset.id = id;
		asset.kind = *kind;
		asset.name = assetName(source);
		asset.file = folder + "/" + utf8Of(source.filename());
		Result<ImportedAsset> result = imported;
		if (*kind == cad::AssetKind::Texture)
		{
			Result<std::vector<std::uint8_t>> bytes = readFileBytes(source, 256ull << 20);
			const Result<ImageInfo> info = bytes ? readImageInfo(bytes.value().data(), bytes.value().size())
				: Result<ImageInfo>(bytes.error());
			if (!info)
			{
				result = info.error();
			}
			else if (Status copied = writeFile(destination / source.filename(), bytes.value()); !copied)
			{
				result = copied.error();
			}
			else
			{
				asset.texture.pixelWidth = info.value().width;
				asset.texture.pixelHeight = info.value().height;
				asset.texture.widthMm = 1000;
				asset.texture.heightMm = std::max<std::int64_t>(1, std::llround(1000.0 * info.value().height / info.value().width));
				result = imported;
			}
		}
		else
		{
			result = importModel(source, destination, imported);
		}
		if (result)
		{
			if (Status valid = cad::validateAsset(result.value().asset); !valid)
			{
				result = valid.error();
			}
		}
		if (!result)
		{
			std::filesystem::remove_all(destination, error); // nothing left behind
		}
		return result;
	}

	std::filesystem::path assetFilePath(const std::filesystem::path& projectFolder, const cad::Asset& asset)
	{
		return projectFolder / pathFromUtf8(asset.file);
	}

	Result<cad::ModelAsset> measureModel(const std::filesystem::path& projectFolder, const cad::Asset& asset,
		const cad::ModelAsset& settings)
	{
		Result<LoadedModel> loaded = loadModelFile(assetFilePath(projectFolder, asset),
			ModelSettings{settings.unit, settings.up, settings.frontQuarterTurns});
		if (!loaded)
		{
			return loaded.error();
		}
		cad::ModelAsset measured = settings;
		measured.widthMm = std::llround(loaded.value().mesh.widthMm);
		measured.depthMm = std::llround(loaded.value().mesh.depthMm);
		measured.heightMm = std::llround(loaded.value().mesh.heightMm);
		measured.triangles = static_cast<std::int64_t>(loaded.value().triangles);
		return measured;
	}

	Status copyAssetFiles(const cad::Project& project, const std::filesystem::path& from, const std::filesystem::path& to)
	{
		std::error_code error;
		if (project.assets.empty() || std::filesystem::equivalent(from, to, error))
		{
			return Status::success();
		}
		for (const cad::Asset& asset : project.assets)
		{
			const std::filesystem::path source = from / pathFromUtf8(cad::assetFolder(asset.id));
			const std::filesystem::path target = to / pathFromUtf8(cad::assetFolder(asset.id));
			if (std::filesystem::exists(target, error) || !std::filesystem::is_directory(source, error))
			{
				continue;
			}
			std::filesystem::create_directories(target.parent_path(), error);
			std::filesystem::copy(source, target, std::filesystem::copy_options::recursive, error);
			if (error)
			{
				return Error{ErrorCode::InvalidArgument, "Cannot copy the files of '" + asset.name + "' to " + utf8Of(target) + ": " +
					error.message()};
			}
		}
		return Status::success();
	}

	const LoadedModel* ModelCache::get(const std::filesystem::path& projectFolder, const cad::Asset& asset)
	{
		const std::filesystem::path file = assetFilePath(projectFolder, asset);
		std::error_code error;
		const auto written = std::filesystem::last_write_time(file, error);
		const std::string key = utf8Of(file) + "|" + std::to_string(static_cast<int>(asset.model.unit)) + "|" +
			std::to_string(static_cast<int>(asset.model.up)) + "|" + std::to_string(asset.model.frontQuarterTurns) + "|" +
			(error ? std::string("missing") : std::to_string(written.time_since_epoch().count()));
		Entry& entry = entries[asset.id];
		if (entry.key != key)
		{
			entry = Entry{key, std::nullopt, {}};
			Result<LoadedModel> loaded = loadModelFile(file, ModelSettings{asset.model.unit, asset.model.up, asset.model.frontQuarterTurns});
			if (loaded)
			{
				entry.model = std::move(loaded).value();
			}
			else
			{
				entry.error = loaded.error().message;
			}
		}
		return entry.model ? &*entry.model : nullptr;
	}

	std::string ModelCache::error(const cad::AssetId& asset) const
	{
		const auto found = entries.find(asset);
		return found == entries.end() ? std::string() : found->second.error;
	}
}
