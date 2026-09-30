#include "F101/Assets/Library.h"

#include "F101/Assets/Files.h"

#include <algorithm>
#include <set>
#include <system_error>
#include <utility>

// Third-party header: its warnings are not ours. Exceptions are disabled, so nlohmann/json turns throws into abort();
// this file only uses the non-throwing parse and checks every type before reading a value.
#if defined(_MSC_VER)
	#pragma warning(push, 0)
#endif
#include "nlohmann/json.hpp"
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

namespace f101::assets
{
	namespace
	{
		using Json = nlohmann::json;

		Error manifestError(const std::string& message)
		{
			return Error{ErrorCode::ParseError, "Library manifest: " + message};
		}

		// A string member; `required` ones must be non-empty.
		Status readText(const Json& object, const char* key, const std::string& where, bool required, std::string& out)
		{
			const auto found = object.find(key);
			if (found == object.end())
			{
				return required ? Status(manifestError(where + "." + key + " is missing")) : Status::success();
			}
			if (!found->is_string() || (required && found->get_ref<const std::string&>().empty()))
			{
				return manifestError(where + "." + key + " must be a non-empty text");
			}
			out = found->get<std::string>();
			return Status::success();
		}

		Status readWhole(const Json& object, const char* key, const std::string& where, std::int64_t low, std::int64_t high,
			std::int64_t& out)
		{
			const auto found = object.find(key);
			if (found == object.end())
			{
				return Status::success(); // keeps the default
			}
			if (!found->is_number_integer() || found->get<std::int64_t>() < low || found->get<std::int64_t>() > high)
			{
				return manifestError(where + "." + key + " must be a whole number from " + std::to_string(low) + " to " +
					std::to_string(high));
			}
			out = found->get<std::int64_t>();
			return Status::success();
		}

		// A file of the manifest's folder: relative, inside it, with one of the extensions.
		Result<std::string> readFile(const Json& object, const char* key, const std::string& where, std::string_view folder,
			std::initializer_list<std::string_view> extensions, bool required)
		{
			std::string file;
			if (Status read = readText(object, key, where, required, file); !read)
			{
				return read.error();
			}
			if (file.empty())
			{
				return std::string();
			}
			if (Status path = cad::validateAssetPath(file); !path)
			{
				return manifestError(where + "." + key + ": " + path.error().message);
			}
			std::string extension = file.substr(file.find_last_of('.') == std::string::npos ? file.size() : file.find_last_of('.') + 1);
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });
			if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end())
			{
				return manifestError(where + "." + key + " '" + file + "' has a type the library does not take");
			}
			return folder.empty() ? file : std::string(folder) + "/" + file;
		}

		Status readItemBasics(const Json& object, const std::string& where, std::string& id, std::string& name, std::string& credit)
		{
			if (Status read = readText(object, "id", where, true, id); !read)
			{
				return read;
			}
			if (!cad::isLibraryId(id))
			{
				return manifestError(where + ".id '" + id + "' must be lower-case letters, digits and hyphens");
			}
			if (Status read = readText(object, "name", where, true, name); !read)
			{
				return read;
			}
			if (name.size() > cad::MaxAssetNameBytes)
			{
				return manifestError(where + ".name is longer than " + std::to_string(cad::MaxAssetNameBytes) + " bytes");
			}
			return readText(object, "credit", where, false, credit);
		}

		Result<LibraryFurniture> readFurniture(const Json& object, const std::string& where, std::string_view folder)
		{
			if (!object.is_object())
			{
				return manifestError(where + " must be an object");
			}
			LibraryFurniture item;
			if (Status basics = readItemBasics(object, where, item.id, item.name, item.credit); !basics)
			{
				return basics.error();
			}
			std::string category;
			if (Status read = readText(object, "category", where, true, category); !read)
			{
				return read.error();
			}
			const std::optional<cad::FurnitureCategory> parsed = cad::parseFurnitureCategory(category);
			if (!parsed)
			{
				return manifestError(where + ".category '" + category + "' is not a furniture category");
			}
			item.category = *parsed;
			Result<std::string> file = readFile(object, "file", where, folder, {"glb", "gltf", "obj"}, true);
			Result<std::string> thumbnail = readFile(object, "thumbnail", where, folder, {"png"}, false);
			if (!file || !thumbnail)
			{
				return !file ? file.error() : thumbnail.error();
			}
			item.file = file.value();
			item.thumbnail = thumbnail.value();
			std::string unit = "m";
			std::string up = "y";
			if (Status read = readText(object, "unit", where, false, unit); !read)
			{
				return read.error();
			}
			if (Status read = readText(object, "up", where, false, up); !read)
			{
				return read.error();
			}
			constexpr std::pair<cad::ModelUnit, std::string_view> Units[] = {{cad::ModelUnit::Meter, "m"},
				{cad::ModelUnit::Centimeter, "cm"}, {cad::ModelUnit::Millimeter, "mm"}, {cad::ModelUnit::Inch, "in"}};
			const auto knownUnit = std::find_if(std::begin(Units), std::end(Units), [&unit](const auto& entry) { return entry.second == unit; });
			if (knownUnit == std::end(Units) || (up != "y" && up != "z"))
			{
				return manifestError(where + ": unit must be m, cm, mm or in and up y or z");
			}
			item.model.unit = knownUnit->first;
			item.model.up = up == "z" ? cad::ModelUpAxis::Z : cad::ModelUpAxis::Y;
			std::int64_t turns = 0;
			if (Status read = readWhole(object, "frontQuarterTurns", where, 0, 3, turns); !read)
			{
				return read.error();
			}
			item.model.frontQuarterTurns = static_cast<int>(turns);
			if (const auto against = object.find("againstWall"); against != object.end())
			{
				if (!against->is_boolean())
				{
					return manifestError(where + ".againstWall must be true or false");
				}
				item.model.againstWall = against->get<bool>();
			}
			return item;
		}

		Result<LibraryTexture> readTexture(const Json& object, const std::string& where, std::string_view folder)
		{
			if (!object.is_object())
			{
				return manifestError(where + " must be an object");
			}
			LibraryTexture item;
			if (Status basics = readItemBasics(object, where, item.id, item.name, item.credit); !basics)
			{
				return basics.error();
			}
			std::string category;
			if (Status read = readText(object, "category", where, true, category); !read)
			{
				return read.error();
			}
			const std::optional<cad::FinishCategory> parsed = cad::parseFinishCategory(category);
			if (!parsed)
			{
				return manifestError(where + ".category '" + category + "' is not a finish category");
			}
			item.category = *parsed;
			Result<std::string> file = readFile(object, "file", where, folder, {"png", "jpg", "jpeg"}, true);
			if (!file)
			{
				return file.error();
			}
			item.file = file.value();
			std::int64_t roughness = item.texture.roughnessPercent;
			for (const auto& [key, target] : {std::pair<const char*, std::int64_t*>{"widthMm", &item.texture.widthMm},
					 {"heightMm", &item.texture.heightMm}})
			{
				if (Status read = readWhole(object, key, where, cad::MinModelSizeMm, cad::MaxModelSizeMm, *target); !read)
				{
					return read.error();
				}
			}
			if (Status read = readWhole(object, "roughnessPercent", where, 0, 100, roughness); !read)
			{
				return read.error();
			}
			item.texture.roughnessPercent = static_cast<int>(roughness);
			const auto surfaces = object.find("surfaces");
			if (surfaces != object.end())
			{
				constexpr std::pair<cad::Surface, std::string_view> Names[] = {{cad::Surface::Wall, "walls"},
					{cad::Surface::Floor, "floors"}, {cad::Surface::Ceiling, "ceilings"}, {cad::Surface::Carpentry, "carpentry"},
					{cad::Surface::Furniture, "furniture"}};
				if (!surfaces->is_array() || surfaces->empty())
				{
					return manifestError(where + ".surfaces must be a list of walls, floors, ceilings, carpentry, furniture");
				}
				item.texture.surfaces = 0;
				for (const Json& name : *surfaces)
				{
					const auto known = std::find_if(std::begin(Names), std::end(Names),
						[&name](const auto& entry) { return name.is_string() && name.get_ref<const std::string&>() == entry.second; });
					if (known == std::end(Names))
					{
						return manifestError(where + ".surfaces: walls, floors, ceilings, carpentry or furniture");
					}
					item.texture.surfaces |= static_cast<std::uint8_t>(known->first);
				}
			}
			return item;
		}

		void removeAssetFolder(const std::filesystem::path& projectFolder, const cad::AssetId& id)
		{
			std::error_code ignored;
			std::filesystem::remove_all(projectFolder / pathFromUtf8(cad::assetFolder(id)), ignored);
		}

		// The item's name and library id on the imported asset; nothing left behind if it does not validate.
		Result<ImportedAsset> finish(ImportedAsset imported, const std::filesystem::path& projectFolder)
		{
			if (Status valid = cad::validateAsset(imported.asset); !valid)
			{
				removeAssetFolder(projectFolder, imported.asset.id);
				return valid.error();
			}
			return imported;
		}
	}

	const LibraryFurniture* Library::findFurniture(std::string_view id) const
	{
		const auto found = std::find_if(furniture.begin(), furniture.end(), [id](const LibraryFurniture& item) { return item.id == id; });
		return found == furniture.end() ? nullptr : &*found;
	}

	const LibraryTexture* Library::findTexture(std::string_view id) const
	{
		const auto found = std::find_if(textures.begin(), textures.end(), [id](const LibraryTexture& item) { return item.id == id; });
		return found == textures.end() ? nullptr : &*found;
	}

	Result<Library> readLibraryManifest(std::string_view json, std::string_view folder)
	{
		const Json root = Json::parse(json.begin(), json.end(), nullptr, false);
		if (root.is_discarded() || !root.is_object())
		{
			return manifestError("the JSON is not valid");
		}
		const auto version = root.find("version");
		if (version == root.end() || !version->is_number_integer() || version->get<int>() != 1)
		{
			return manifestError("version must be 1");
		}
		Library library;
		std::set<std::string> ids;
		auto add = [&ids](const std::string& id) -> Status
		{
			return ids.insert(id).second ? Status::success() : Status(manifestError("the id '" + id + "' is repeated"));
		};
		for (const auto& [key, isFurniture] : {std::pair<const char*, bool>{"furniture", true}, {"textures", false}})
		{
			const auto list = root.find(key);
			if (list == root.end())
			{
				continue;
			}
			if (!list->is_array())
			{
				return manifestError(std::string(key) + " must be a list");
			}
			for (std::size_t index = 0; index < list->size(); ++index)
			{
				const std::string where = std::string(key) + "[" + std::to_string(index) + "]";
				if (isFurniture)
				{
					Result<LibraryFurniture> item = readFurniture((*list)[index], where, folder);
					if (!item)
					{
						return item.error();
					}
					if (Status unique = add(item.value().id); !unique)
					{
						return unique.error();
					}
					library.furniture.push_back(std::move(item).value());
				}
				else
				{
					Result<LibraryTexture> item = readTexture((*list)[index], where, folder);
					if (!item)
					{
						return item.error();
					}
					if (Status unique = add(item.value().id); !unique)
					{
						return unique.error();
					}
					library.textures.push_back(std::move(item).value());
				}
			}
		}
		return library;
	}

	Result<Library> loadLibrary(const std::filesystem::path& libraryFolder)
	{
		Library library;
		std::error_code error;
		if (!std::filesystem::is_directory(libraryFolder, error))
		{
			return library;
		}
		// Subfolders in name order, so the palette order does not depend on the file system.
		std::vector<std::filesystem::path> manifests;
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(libraryFolder, error))
		{
			if (entry.is_directory(error) && std::filesystem::is_regular_file(entry.path() / "manifest.json", error))
			{
				manifests.push_back(entry.path() / "manifest.json");
			}
		}
		std::sort(manifests.begin(), manifests.end());
		std::set<std::string> ids;
		for (const std::filesystem::path& manifest : manifests)
		{
			const Result<std::vector<std::uint8_t>> bytes = readFileBytes(manifest, 16u << 20);
			if (!bytes)
			{
				return bytes.error();
			}
			const std::string text(bytes.value().begin(), bytes.value().end());
			Result<Library> part = readLibraryManifest(text, utf8Of(manifest.parent_path().filename()));
			if (!part)
			{
				return Error{part.error().code, part.error().message + " (" + utf8Of(manifest) + ")"};
			}
			auto check = [&](const std::string& id, std::initializer_list<const std::string*> files) -> Status
			{
				if (!ids.insert(id).second)
				{
					return manifestError("the id '" + id + "' is repeated (" + utf8Of(manifest) + ")");
				}
				for (const std::string* file : files)
				{
					if (!file->empty() && !std::filesystem::is_regular_file(libraryFolder / pathFromUtf8(*file), error))
					{
						return Error{ErrorCode::NotFound, "Library item '" + id + "': " + *file + " is missing"};
					}
				}
				return Status::success();
			};
			Library items = std::move(part).value();
			for (LibraryFurniture& item : items.furniture)
			{
				if (Status checked = check(item.id, {&item.file, &item.thumbnail}); !checked)
				{
					return checked.error();
				}
				library.furniture.push_back(std::move(item));
			}
			for (LibraryTexture& item : items.textures)
			{
				if (Status checked = check(item.id, {&item.file}); !checked)
				{
					return checked.error();
				}
				library.textures.push_back(std::move(item));
			}
		}
		return library;
	}

	Result<ImportedAsset> importLibraryFurniture(const LibraryFurniture& item, const std::filesystem::path& libraryFolder,
		const std::filesystem::path& projectFolder, const cad::AssetId& id)
	{
		Result<ImportedAsset> imported = importAsset(libraryFolder / pathFromUtf8(item.file), projectFolder, id);
		if (!imported)
		{
			return imported.error();
		}
		ImportedAsset result = std::move(imported).value();
		if (result.asset.kind != cad::AssetKind::Model)
		{
			removeAssetFolder(projectFolder, id);
			return Error{ErrorCode::InvalidArgument, "Library item '" + item.id + "' is not a model"};
		}
		const Result<cad::ModelAsset> measured = measureModel(projectFolder, result.asset, item.model);
		if (!measured)
		{
			removeAssetFolder(projectFolder, id);
			return measured.error();
		}
		result.asset.model = measured.value();
		result.asset.name = item.name;
		result.asset.library = item.id;
		return finish(std::move(result), projectFolder);
	}

	Result<ImportedAsset> importLibraryTexture(const LibraryTexture& item, const std::filesystem::path& libraryFolder,
		const std::filesystem::path& projectFolder, const cad::AssetId& id)
	{
		Result<ImportedAsset> imported = importAsset(libraryFolder / pathFromUtf8(item.file), projectFolder, id);
		if (!imported)
		{
			return imported.error();
		}
		ImportedAsset result = std::move(imported).value();
		const int pixelWidth = result.asset.texture.pixelWidth;
		const int pixelHeight = result.asset.texture.pixelHeight;
		result.asset.texture = item.texture;
		result.asset.texture.pixelWidth = pixelWidth;
		result.asset.texture.pixelHeight = pixelHeight;
		result.asset.name = item.name;
		result.asset.library = item.id;
		return finish(std::move(result), projectFolder);
	}

	const cad::Asset* findLibraryAsset(const cad::Project& project, std::string_view libraryId)
	{
		const auto found = std::find_if(project.assets.begin(), project.assets.end(),
			[libraryId](const cad::Asset& asset) { return !asset.library.empty() && asset.library == libraryId; });
		return found == project.assets.end() ? nullptr : &*found;
	}
}
