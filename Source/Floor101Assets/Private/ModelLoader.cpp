#include "F101/Assets/ModelLoader.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

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

		// Model units to furniture's axes (before scaling): z up, front towards -y.
		geometry::Vec3 toFurnitureAxes(const geometry::Vec3& v, cad::ModelUpAxis up, int turns)
		{
			geometry::Vec3 r = up == cad::ModelUpAxis::Y ? geometry::Vec3{v.x, -v.z, v.y} : v;
			for (int turn = 0; turn < turns; ++turn)
			{
				r = geometry::Vec3{-r.y, r.x, r.z};
			}
			return r;
		}
	}

	std::optional<ModelFormat> modelFormatOf(const std::filesystem::path& file)
	{
		const std::string extension = lowerExtension(file);
		if (extension == ".gltf" || extension == ".glb")
		{
			return ModelFormat::Gltf;
		}
		if (extension == ".obj")
		{
			return ModelFormat::Obj;
		}
		return std::nullopt;
	}

	LoadedModel normalizeModel(const RawModel& raw, const ModelSettings& settings)
	{
		LoadedModel loaded;
		loaded.materials = raw.materialList;
		loaded.triangles = raw.triangleCount();
		loaded.warnings = raw.warnings;
		const double scale = cad::millimetersPerUnit(settings.unit);
		const int turns = ((settings.frontQuarterTurns % 4) + 4) % 4;

		std::vector<geometry::Vec3> positions;
		positions.reserve(raw.positions.size());
		geometry::Vec3 low{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
			std::numeric_limits<double>::infinity()};
		geometry::Vec3 high{-low.x, -low.y, -low.z};
		for (const geometry::Vec3& p : raw.positions)
		{
			const geometry::Vec3 v = toFurnitureAxes(geometry::Vec3{p.x * scale, p.y * scale, p.z * scale}, settings.up, turns);
			positions.push_back(v);
			low = geometry::Vec3{std::min(low.x, v.x), std::min(low.y, v.y), std::min(low.z, v.z)};
			high = geometry::Vec3{std::max(high.x, v.x), std::max(high.y, v.y), std::max(high.z, v.z)};
		}
		if (positions.empty())
		{
			return loaded;
		}
		const geometry::Vec3 centre{(low.x + high.x) / 2.0, (low.y + high.y) / 2.0, low.z};
		geometry::MeshData& mesh = loaded.mesh.mesh;
		mesh.positions.reserve(positions.size());
		for (const geometry::Vec3& v : positions)
		{
			mesh.positions.push_back(geometry::Vec3{v.x - centre.x, v.y - centre.y, v.z - centre.z});
		}
		for (const geometry::Vec3& n : raw.normals)
		{
			mesh.normals.push_back(toFurnitureAxes(n, settings.up, turns));
		}
		mesh.uvs = raw.uvs;
		loaded.mesh.widthMm = high.x - low.x;
		loaded.mesh.depthMm = high.y - low.y;
		loaded.mesh.heightMm = high.z - low.z;

		// One section per material with triangles, in material order.
		std::map<std::uint32_t, std::vector<std::uint32_t>> sections;
		for (std::size_t triangle = 0; triangle < raw.materials.size(); ++triangle)
		{
			std::vector<std::uint32_t>& indices = sections[raw.materials[triangle]];
			for (std::uint32_t corner = 0; corner < 3; ++corner)
			{
				indices.push_back(static_cast<std::uint32_t>(triangle * 3 + corner));
			}
		}
		for (auto& [material, indices] : sections)
		{
			mesh.sections.push_back(geometry::MeshSection{geometry::modelSectionName(material), std::move(indices)});
		}
		return loaded;
	}

	cad::ModelUnit guessUnit(const RawModel& raw)
	{
		double extent = 0.0;
		if (!raw.positions.empty())
		{
			geometry::Vec3 low = raw.positions.front();
			geometry::Vec3 high = low;
			for (const geometry::Vec3& p : raw.positions)
			{
				low = geometry::Vec3{std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
				high = geometry::Vec3{std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
			}
			extent = std::max({high.x - low.x, high.y - low.y, high.z - low.z});
		}
		constexpr double MaxPlausibleMm = 30'000.0;
		for (const cad::ModelUnit unit : {cad::ModelUnit::Meter, cad::ModelUnit::Centimeter})
		{
			if (extent * cad::millimetersPerUnit(unit) <= MaxPlausibleMm)
			{
				return unit;
			}
		}
		return cad::ModelUnit::Millimeter;
	}

	Result<ReadModel> readModelFile(const std::filesystem::path& file, bool allowOutside)
	{
		const std::optional<ModelFormat> format = modelFormatOf(file);
		if (!format)
		{
			return Error{ErrorCode::InvalidArgument, "Import glTF (.gltf, .glb) or OBJ models"};
		}
		Result<std::vector<std::uint8_t>> bytes = readFileBytes(file);
		if (!bytes)
		{
			return bytes.error();
		}
		ReadModel read;
		const std::filesystem::path folder = file.parent_path();
		const ModelFileReader files = [&folder, &read, allowOutside](std::string_view reference)
			-> std::optional<std::vector<std::uint8_t>>
		{
			const std::filesystem::path found = resolveReference(folder, reference, allowOutside);
			if (found.empty())
			{
				return std::nullopt;
			}
			Result<std::vector<std::uint8_t>> content = readFileBytes(found);
			if (!content)
			{
				return std::nullopt;
			}
			read.filesRead.push_back(found);
			return std::move(content).value();
		};
		Result<RawModel> raw = *format == ModelFormat::Gltf ? readGltf(bytes.value(), files)
			: readObj(std::string_view(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size()), files);
		if (!raw)
		{
			return raw.error();
		}
		read.raw = std::move(raw).value();
		return read;
	}

	Result<LoadedModel> loadModelFile(const std::filesystem::path& file, const ModelSettings& settings)
	{
		Result<ReadModel> read = readModelFile(file);
		if (!read)
		{
			return read.error();
		}
		LoadedModel loaded = normalizeModel(read.value().raw, settings);
		const std::filesystem::path folder = file.parent_path();
		for (ModelMaterial& material : loaded.materials)
		{
			if (material.texture.empty())
			{
				continue;
			}
			const std::filesystem::path found = resolveReference(folder, material.texture);
			if (found.empty())
			{
				loaded.warnings.push_back("Texture '" + material.texture + "' of material '" + material.name + "' not found");
				material.texture.clear();
				continue;
			}
			const std::filesystem::path relative = found.lexically_relative(folder);
			const std::u8string text = (relative.empty() ? found.filename() : relative).generic_u8string();
			material.texture = std::string(text.begin(), text.end());
		}
		return loaded;
	}
}
