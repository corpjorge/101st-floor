#include "F101/Exporters/Gltf.h"

#include "F101/Assets/Files.h"
#include "F101/Assets/ImageInfo.h"
#include "F101/Exporters/Png.h"
#include "F101/Scene/SceneCompiler.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>

// Third-party header: its warnings are not ours. Exceptions are disabled, so nlohmann/json turns throws into abort(); this
// file only builds values and dumps them with invalid UTF-8 replaced, which never throws.
#if defined(_MSC_VER)
	#pragma warning(push, 0)
#endif
#include "nlohmann/json.hpp"
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

namespace f101::exporters
{
	namespace
	{
		using Json = nlohmann::json;

		constexpr std::uint32_t GlbMagic = 0x46546C67;  // "glTF"
		constexpr std::uint32_t GlbVersion = 2;
		constexpr std::uint32_t JsonChunkType = 0x4E4F534A; // "JSON"
		constexpr std::uint32_t BinChunkType = 0x004E4942;  // "BIN\0"
		constexpr int ArrayBuffer = 34962;
		constexpr int ElementArrayBuffer = 34963;
		constexpr int FloatComponent = 5126;
		constexpr int UnsignedShortComponent = 5123;
		constexpr int UnsignedIntComponent = 5125;

		double srgbToLinear(std::uint8_t value)
		{
			const double c = value / 255.0;
			return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
		}

		std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes)
		{
			std::uint64_t hash = 14695981039346656037ull;
			for (const std::uint8_t byte : bytes)
			{
				hash = (hash ^ byte) * 1099511628211ull;
			}
			return hash;
		}

		void putLittleEndian(std::vector<std::uint8_t>& out, std::uint32_t value)
		{
			for (int shift = 0; shift < 32; shift += 8)
			{
				out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
			}
		}

		std::string shortId(std::string_view id)
		{
			return std::string(id.substr(0, 8));
		}

		// Scene space (mm, Z up) to glTF (metres, Y up): a rotation, so triangles keep their winding. Adding 0 turns -0 into 0.
		std::array<float, 3> toGltf(const geometry::Vec3& p, double scale)
		{
			return {static_cast<float>(p.x * scale + 0.0), static_cast<float>(p.z * scale + 0.0), static_cast<float>(-p.y * scale + 0.0)};
		}

		enum class Group
		{
			Walls,
			Floors,
			Slabs,
			Openings,
			Furniture,
			Columns,
			Services,
			Count,
		};

		Group groupOf(scene::SourceKind kind)
		{
			switch (kind)
			{
			case scene::SourceKind::Room: return Group::Floors;
			case scene::SourceKind::Slab: return Group::Slabs;
			case scene::SourceKind::Opening: return Group::Openings;
			case scene::SourceKind::Furniture: return Group::Furniture;
			case scene::SourceKind::Column: return Group::Columns;
			case scene::SourceKind::Service: return Group::Services;
			default: return Group::Walls;
			}
		}

		std::string_view sourceName(scene::SourceKind kind)
		{
			switch (kind)
			{
			case scene::SourceKind::Wall: return "wall";
			case scene::SourceKind::Joint: return "joint";
			case scene::SourceKind::Room: return "floor";
			case scene::SourceKind::Slab: return "slab";
			case scene::SourceKind::Opening: return "opening";
			case scene::SourceKind::Furniture: return "furniture";
			case scene::SourceKind::Column: return "column";
			case scene::SourceKind::Service: return "service";
			default: return "marker";
			}
		}

		std::string replaceAll(std::string text, std::string_view from, std::string_view to)
		{
			for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
			{
				text.replace(at, from.size(), to);
			}
			return text;
		}

		// The binary chunk: every view starts on a 4-byte boundary, as glTF requires of accessors.
		class Writer
		{
		public:
			Json views = Json::array();
			Json accessors = Json::array();
			std::vector<std::uint8_t> bin;

			int addView(const void* data, std::size_t size, int target)
			{
				while (bin.size() % 4 != 0)
				{
					bin.push_back(0);
				}
				const std::size_t offset = bin.size();
				const auto* bytes = static_cast<const std::uint8_t*>(data);
				bin.insert(bin.end(), bytes, bytes + size);
				Json view{{"buffer", 0}, {"byteOffset", offset}, {"byteLength", size}};
				if (target != 0)
				{
					view["target"] = target;
				}
				views.push_back(std::move(view));
				return static_cast<int>(views.size()) - 1;
			}

			int addAccessor(int view, int componentType, std::size_t count, const char* type)
			{
				accessors.push_back(Json{{"bufferView", view}, {"componentType", componentType}, {"count", count}, {"type", type}});
				return static_cast<int>(accessors.size()) - 1;
			}
		};

		struct Material
		{
			GltfMaterialLook look;
			int image = -1; // index into the images, or none
		};
	}

	GltfExport writeGlb(const scene::Scene& scene, const cad::Project& project, const GltfMaterialSource& materials,
		const GltfTexts& texts)
	{
		GltfExport result;
		Writer writer;
		constexpr double MetresPerMm = 0.001;

		// Materials and images, in order of first use.
		std::map<std::string, int, std::less<>> materialOf;
		std::vector<Material> materialList;
		std::map<std::pair<std::uint64_t, std::size_t>, int> imageOf; // content hash and size -> image
		Json images = Json::array();
		const auto materialIndex = [&](const std::string& finish) -> int
		{
			if (const auto found = materialOf.find(finish); found != materialOf.end())
			{
				return found->second;
			}
			Material material{materials ? materials(finish) : GltfMaterialLook{}, -1};
			if (material.look.name.empty())
			{
				material.look.name = finish;
			}
			if (!material.look.warning.empty())
			{
				result.warnings.push_back(material.look.warning);
			}
			std::vector<std::uint8_t> encoded = !material.look.pixels.rgba.empty() ? encodePng(material.look.pixels)
				: material.look.encodedImage;
			if (!encoded.empty())
			{
				const Result<assets::ImageInfo> info = assets::readImageInfo(encoded.data(), encoded.size());
				if (!info)
				{
					result.warnings.push_back("The image of '" + material.look.name + "' is not a PNG or JPEG: " + info.error().message);
				}
				else
				{
					const auto key = std::make_pair(fnv1a(encoded), encoded.size());
					if (const auto found = imageOf.find(key); found != imageOf.end())
					{
						material.image = found->second;
					}
					else
					{
						const int view = writer.addView(encoded.data(), encoded.size(), 0);
						images.push_back(Json{{"bufferView", view},
							{"mimeType", info.value().format == assets::ImageFormat::Png ? "image/png" : "image/jpeg"}});
						material.image = static_cast<int>(images.size()) - 1;
						imageOf.emplace(key, material.image);
					}
				}
			}
			materialList.push_back(std::move(material));
			const int index = static_cast<int>(materialList.size()) - 1;
			materialOf.emplace(finish, index);
			return index;
		};

		// Lookups for the names: what each opening is, and each piece of furniture's type.
		std::map<std::string, cad::OpeningKind, std::less<>> openingKinds;
		for (const cad::Wall& wall : project.walls)
		{
			for (const cad::Opening& opening : wall.openings)
			{
				openingKinds.emplace(cad::toString(opening.id), opening.kind);
			}
		}
		std::map<std::string, std::string, std::less<>> furnitureTypes;
		for (const cad::Furniture& piece : project.furniture)
		{
			furnitureTypes.emplace(cad::toString(piece.id), piece.type);
		}
		std::map<std::string, cad::ServiceKind, std::less<>> serviceKinds;
		for (const cad::ServiceRun& run : project.services)
		{
			serviceKinds.emplace(cad::toString(run.id), run.kind);
		}
		const auto serviceName = [&texts](cad::ServiceKind kind) -> const std::string&
		{
			switch (kind)
			{
			case cad::ServiceKind::Water: return texts.water;
			case cad::ServiceKind::Sewage: return texts.sewage;
			case cad::ServiceKind::Gas: return texts.gas;
			default: return texts.electrical;
			}
		};
		const auto furnitureName = [&](std::string_view type) -> std::string
		{
			if (const std::optional<cad::AssetId> asset = cad::parseAssetRef(type))
			{
				if (const cad::Asset* found = project.findAsset(*asset))
				{
					return found->name;
				}
			}
			const auto named = texts.furnitureNames.find(type);
			return named != texts.furnitureNames.end() ? named->second : std::string(type);
		};
		std::set<std::string> usedNames;
		const auto uniqueName = [&usedNames](std::string name)
		{
			std::string candidate = name;
			for (int n = 2; !usedNames.insert(candidate).second; ++n)
			{
				candidate = name + " " + std::to_string(n);
			}
			return candidate;
		};
		const auto nodeName = [&](const scene::SceneNode& node) -> std::string
		{
			const std::string id = shortId(node.sourceId);
			switch (node.sourceKind)
			{
			case scene::SourceKind::Wall: return texts.wall + " " + id;
			case scene::SourceKind::Joint: return texts.joint + " " + id;
			case scene::SourceKind::Room: return texts.floor + " " + id;
			case scene::SourceKind::Slab: return texts.slab + " " + id;
			case scene::SourceKind::Opening:
			{
				const auto kind = openingKinds.find(node.sourceId);
				return (kind != openingKinds.end() && kind->second == cad::OpeningKind::Window ? texts.window : texts.door) + " " + id;
			}
			case scene::SourceKind::Furniture:
			{
				const auto type = furnitureTypes.find(node.sourceId);
				return (type != furnitureTypes.end() ? furnitureName(type->second) : texts.furniture) + " " + id;
			}
			case scene::SourceKind::Column: return texts.column + " " + id;
			case scene::SourceKind::Service:
			{
				const auto kind = serviceKinds.find(node.sourceId);
				return (kind != serviceKinds.end() ? serviceName(kind->second) : texts.services) + " " + id;
			}
			default: return node.name;
			}
		};

		// One glTF node and mesh per scene node with triangles, bucketed by level (index into project.levels, or -1)
		// and group.
		Json nodes = Json::array();
		Json meshes = Json::array();
		std::map<std::pair<int, int>, std::vector<int>> buckets;
		for (const scene::SceneNode& node : scene.nodes)
		{
			const geometry::MeshData& mesh = node.mesh;
			if (node.sourceKind == scene::SourceKind::Marker || mesh.triangleCount() == 0)
			{
				continue;
			}
			const bool hasNormals = mesh.normals.size() == mesh.positions.size();
			const bool hasUvs = mesh.uvs.size() == mesh.positions.size();
			Json primitives = Json::array();
			for (std::size_t s = 0; s < mesh.sections.size(); ++s)
			{
				const geometry::MeshSection& section = mesh.sections[s];
				if (section.indices.size() < 3)
				{
					continue;
				}
				const int material = materialIndex(s < node.finishes.size() ? node.finishes[s] : std::string());
				const GltfMaterialLook& look = materialList[static_cast<std::size_t>(material)].look;
				const double uScale = 1000.0 / std::max(look.repeatWidthMm, 1.0);
				const double vScale = (look.flipImage ? -1000.0 : 1000.0) / std::max(look.repeatHeightMm, 1.0);

				// The section's own vertices, in order of first use.
				std::vector<std::uint32_t> remap(mesh.positions.size(), std::numeric_limits<std::uint32_t>::max());
				std::vector<float> positions;
				std::vector<float> normals;
				std::vector<float> uvs;
				std::vector<std::uint32_t> indices;
				indices.reserve(section.indices.size());
				std::array<float, 3> low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
				std::array<float, 3> high{-low[0], -low[1], -low[2]};
				for (const std::uint32_t original : section.indices)
				{
					if (original >= mesh.positions.size())
					{
						continue;
					}
					if (remap[original] == std::numeric_limits<std::uint32_t>::max())
					{
						remap[original] = static_cast<std::uint32_t>(positions.size() / 3);
						const std::array<float, 3> p = toGltf(mesh.positions[original], MetresPerMm);
						for (int axis = 0; axis < 3; ++axis)
						{
							positions.push_back(p[static_cast<std::size_t>(axis)]);
							low[static_cast<std::size_t>(axis)] = std::min(low[static_cast<std::size_t>(axis)], p[static_cast<std::size_t>(axis)]);
							high[static_cast<std::size_t>(axis)] = std::max(high[static_cast<std::size_t>(axis)], p[static_cast<std::size_t>(axis)]);
						}
						if (hasNormals)
						{
							const std::array<float, 3> n = toGltf(mesh.normals[original], 1.0);
							normals.insert(normals.end(), n.begin(), n.end());
						}
						if (hasUvs)
						{
							uvs.push_back(static_cast<float>(mesh.uvs[original].u * uScale));
							uvs.push_back(static_cast<float>(mesh.uvs[original].v * vScale));
						}
					}
					indices.push_back(remap[original]);
				}
				const std::size_t vertexCount = positions.size() / 3;
				if (indices.size() < 3 || indices.size() % 3 != 0)
				{
					continue;
				}
				Json attributes;
				const int positionView = writer.addView(positions.data(), positions.size() * sizeof(float), ArrayBuffer);
				const int position = writer.addAccessor(positionView, FloatComponent, vertexCount, "VEC3");
				writer.accessors[static_cast<std::size_t>(position)]["min"] = Json::array({low[0], low[1], low[2]});
				writer.accessors[static_cast<std::size_t>(position)]["max"] = Json::array({high[0], high[1], high[2]});
				attributes["POSITION"] = position;
				if (hasNormals)
				{
					const int view = writer.addView(normals.data(), normals.size() * sizeof(float), ArrayBuffer);
					attributes["NORMAL"] = writer.addAccessor(view, FloatComponent, vertexCount, "VEC3");
				}
				if (hasUvs)
				{
					const int view = writer.addView(uvs.data(), uvs.size() * sizeof(float), ArrayBuffer);
					attributes["TEXCOORD_0"] = writer.addAccessor(view, FloatComponent, vertexCount, "VEC2");
				}
				int indexAccessor = -1;
				if (vertexCount <= 65535)
				{
					std::vector<std::uint16_t> shortIndices(indices.begin(), indices.end());
					const int view = writer.addView(shortIndices.data(), shortIndices.size() * sizeof(std::uint16_t), ElementArrayBuffer);
					indexAccessor = writer.addAccessor(view, UnsignedShortComponent, shortIndices.size(), "SCALAR");
				}
				else
				{
					const int view = writer.addView(indices.data(), indices.size() * sizeof(std::uint32_t), ElementArrayBuffer);
					indexAccessor = writer.addAccessor(view, UnsignedIntComponent, indices.size(), "SCALAR");
				}
				primitives.push_back(Json{{"attributes", std::move(attributes)}, {"indices", indexAccessor}, {"material", material}, {"mode", 4}});
				result.triangles += indices.size() / 3;
			}
			if (primitives.empty())
			{
				continue;
			}
			const std::string name = uniqueName(nodeName(node));
			meshes.push_back(Json{{"name", name}, {"primitives", std::move(primitives)}});
			nodes.push_back(Json{{"name", name}, {"mesh", static_cast<int>(meshes.size()) - 1},
				{"extras", Json{{"f101NodeId", node.id.value}, {"f101Source", sourceName(node.sourceKind)}}}});
			const std::optional<std::size_t> level = project.levelIndex(node.level);
			buckets[{level ? static_cast<int>(*level) : -1, static_cast<int>(groupOf(node.sourceKind))}].push_back(static_cast<int>(nodes.size()) - 1);
			++result.objects;
		}

		// Groups under their levels, levels under the root.
		const std::array<const std::string*, static_cast<std::size_t>(Group::Count)> groupNames{&texts.walls, &texts.floors,
			&texts.slabs, &texts.openings, &texts.furniture, &texts.columns, &texts.services};
		const auto levelName = [&](std::size_t index)
		{
			const cad::Level& level = project.levels[index];
			if (!level.name.empty())
			{
				return level.name;
			}
			return index == 0 ? texts.groundFloor : replaceAll(texts.levelNumber, "{n}", std::to_string(index));
		};
		Json rootChildren = Json::array();
		for (int level = -1; level < static_cast<int>(project.levels.size()); ++level)
		{
			Json groups = Json::array();
			for (int group = 0; group < static_cast<int>(Group::Count); ++group)
			{
				const auto bucket = buckets.find({level, group});
				if (bucket == buckets.end())
				{
					continue;
				}
				nodes.push_back(Json{{"name", uniqueName(*groupNames[static_cast<std::size_t>(group)] +
					(level >= 0 && project.levels.size() > 1 ? " (" + levelName(static_cast<std::size_t>(level)) + ")" : std::string()))},
					{"children", bucket->second}});
				groups.push_back(static_cast<int>(nodes.size()) - 1);
			}
			if (groups.empty())
			{
				continue;
			}
			if (level < 0)
			{
				for (const Json& group : groups)
				{
					rootChildren.push_back(group);
				}
				continue;
			}
			nodes.push_back(Json{{"name", uniqueName(levelName(static_cast<std::size_t>(level)))}, {"children", std::move(groups)},
				{"extras", Json{{"f101LevelId", cad::toString(project.levels[static_cast<std::size_t>(level)].id)}}}});
			rootChildren.push_back(static_cast<int>(nodes.size()) - 1);
		}
		nodes.push_back(Json{{"name", uniqueName(texts.root)}, {"children", std::move(rootChildren)}});
		const int root = static_cast<int>(nodes.size()) - 1;

		// Materials: colour (linear) times the image, roughness, never metallic; glass blended.
		Json materialJson = Json::array();
		Json textures = Json::array();
		std::vector<std::string> finishOf(materialList.size());
		for (const auto& [finish, index] : materialOf)
		{
			finishOf[static_cast<std::size_t>(index)] = finish;
		}
		std::set<std::string> usedMaterialNames;
		for (std::size_t i = 0; i < materialList.size(); ++i)
		{
			const Material& material = materialList[i];
			const GltfMaterialLook& look = material.look;
			Json pbr{{"baseColorFactor", Json::array({srgbToLinear(look.color.r), srgbToLinear(look.color.g), srgbToLinear(look.color.b),
						std::clamp(look.opacity, 0.0, 1.0)})},
				{"metallicFactor", 0.0}, {"roughnessFactor", std::clamp(look.roughness, 0.0, 1.0)}};
			if (material.image >= 0)
			{
				textures.push_back(Json{{"sampler", 0}, {"source", material.image}});
				pbr["baseColorTexture"] = Json{{"index", static_cast<int>(textures.size()) - 1}};
			}
			std::string name = look.name;
			for (int n = 2; !usedMaterialNames.insert(name).second; ++n)
			{
				name = look.name + " " + std::to_string(n);
			}
			Json entry{{"name", name}, {"pbrMetallicRoughness", std::move(pbr)}, {"extras", Json{{"f101FinishId", finishOf[i]}}}};
			if (look.opacity < 1.0)
			{
				entry["alphaMode"] = "BLEND";
			}
			materialJson.push_back(std::move(entry));
		}
		result.materials = materialList.size();
		result.images = images.size();

		Json gltf{
			{"asset", Json{{"version", "2.0"}, {"generator", "101st Floor"}}},
			{"scene", 0},
			{"scenes", Json::array({Json{{"name", texts.root}, {"nodes", Json::array({root})}}})},
			{"nodes", std::move(nodes)},
			{"meshes", std::move(meshes)},
			{"materials", std::move(materialJson)},
			{"accessors", std::move(writer.accessors)},
			{"bufferViews", std::move(writer.views)},
		};
		if (!images.empty())
		{
			gltf["images"] = std::move(images);
			gltf["textures"] = std::move(textures);
			// Linear filtering with mipmaps, repeating: finishes tile.
			gltf["samplers"] = Json::array({Json{{"magFilter", 9729}, {"minFilter", 9987}, {"wrapS", 10497}, {"wrapT", 10497}}});
		}
		while (writer.bin.size() % 4 != 0)
		{
			writer.bin.push_back(0);
		}
		gltf["buffers"] = Json::array({Json{{"byteLength", writer.bin.size()}}});

		std::string json = gltf.dump(-1, ' ', false, Json::error_handler_t::replace);
		while (json.size() % 4 != 0)
		{
			json.push_back(' ');
		}
		std::vector<std::uint8_t>& bytes = result.bytes;
		bytes.reserve(28 + json.size() + writer.bin.size());
		putLittleEndian(bytes, GlbMagic);
		putLittleEndian(bytes, GlbVersion);
		putLittleEndian(bytes, static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + writer.bin.size()));
		putLittleEndian(bytes, static_cast<std::uint32_t>(json.size()));
		putLittleEndian(bytes, JsonChunkType);
		bytes.insert(bytes.end(), json.begin(), json.end());
		putLittleEndian(bytes, static_cast<std::uint32_t>(writer.bin.size()));
		putLittleEndian(bytes, BinChunkType);
		bytes.insert(bytes.end(), writer.bin.begin(), writer.bin.end());
		return result;
	}

	GltfMaterialSource projectMaterials(const cad::Project& project, const std::filesystem::path& projectFolder,
		assets::ModelCache& models, const GltfTexts& texts, int textureSize)
	{
		return [&project, projectFolder, &models, names = texts.finishNames, textureSize](std::string_view finishId) -> GltfMaterialLook
		{
			GltfMaterialLook look;
			const std::size_t hash = finishId.find('#');
			const std::optional<cad::AssetId> assetId = cad::parseAssetRef(finishId.substr(0, hash));
			const cad::Asset* asset = assetId ? project.findAsset(*assetId) : nullptr;
			const auto readImage = [&look](const std::filesystem::path& file, const std::string& owner)
			{
				Result<std::vector<std::uint8_t>> bytes = assets::readFileBytes(file);
				if (!bytes)
				{
					look.warning = "The image of '" + owner + "' cannot be read: " + bytes.error().message;
					return;
				}
				look.encodedImage = std::move(bytes).value();
			};

			if (asset && asset->kind == cad::AssetKind::Texture && hash == std::string_view::npos)
			{
				// An imported texture: its file, its repeat and roughness, flipped for surfaces (ADR-0017).
				look.name = asset->name;
				look.roughness = static_cast<double>(asset->texture.roughnessPercent) / 100.0;
				look.repeatWidthMm = static_cast<double>(asset->texture.widthMm);
				look.repeatHeightMm = static_cast<double>(asset->texture.heightMm);
				look.flipImage = true;
				readImage(assets::assetFilePath(projectFolder, *asset), asset->name);
				return look;
			}
			if (asset && asset->kind == cad::AssetKind::Model && hash != std::string_view::npos)
			{
				// A material of an imported model: its colour, roughness and image, addressed by the model's own UVs.
				std::size_t index = 0;
				const std::string_view digits = finishId.substr(hash + 1);
				const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), index);
				const assets::LoadedModel* model = models.get(projectFolder, *asset);
				if (parsed.ec == std::errc() && model && index < model->materials.size())
				{
					const assets::ModelMaterial& material = model->materials[index];
					look.name = asset->name + " - " + (material.name.empty() ? "material " + std::to_string(index) : material.name);
					look.color = material.color;
					look.roughness = material.roughness;
					if (!material.texture.empty())
					{
						readImage(assets::assetFilePath(projectFolder, *asset).parent_path() / assets::pathFromUtf8(material.texture), look.name);
					}
					return look;
				}
			}

			// A catalogue finish, synthesised like the application's (an unknown id: magenta).
			const scene::FinishLook synthesised = scene::buildFinishLook(finishId, textureSize);
			const auto named = names.find(finishId);
			look.name = named != names.end() ? named->second : std::string(finishId);
			if (!cad::findFinish(finishId))
			{
				look.warning = "Unknown finish '" + std::string(finishId) + "': exported as magenta";
			}
			look.roughness = synthesised.roughness;
			look.opacity = synthesised.opacity;
			look.repeatWidthMm = synthesised.texture.repeatWidthMm;
			look.repeatHeightMm = synthesised.texture.repeatHeightMm;
			const scene::TextureImage& top = synthesised.texture.mips.front();
			const scene::TextureImage& smallest = synthesised.texture.mips.back();
			bool uniform = true;
			for (std::size_t i = 4; uniform && i < top.rgba.size(); ++i)
			{
				uniform = top.rgba[i] == top.rgba[i % 4];
			}
			if (look.opacity < 1.0 || uniform)
			{
				// Glass is its tint (the application's glass has no texture); a single colour needs no image.
				const scene::TextureImage& source = look.opacity < 1.0 ? smallest : top;
				look.color = cad::Rgb8{source.rgba[0], source.rgba[1], source.rgba[2]};
				return look;
			}
			look.pixels = top;
			return look;
		};
	}

	Result<GltfExport> exportProjectGlb(const cad::Project& project, const std::filesystem::path& projectFolder,
		assets::ModelCache& models, const GltfTexts& texts, int textureSize)
	{
		const scene::ModelProvider provider = [&models, &projectFolder](const cad::Asset& asset) -> const geometry::ModelMesh*
		{
			const assets::LoadedModel* model = models.get(projectFolder, asset);
			return model ? &model->mesh : nullptr;
		};
		const scene::Scene scene = scene::compileScene(project, provider);
		const bool empty = std::none_of(scene.nodes.begin(), scene.nodes.end(),
			[](const scene::SceneNode& node) { return node.mesh.triangleCount() > 0; });
		if (empty)
		{
			return Error{ErrorCode::InvalidArgument, "Nothing to export: the project has no walls, floors or furniture"};
		}
		return writeGlb(scene, project, projectMaterials(project, projectFolder, models, texts, textureSize), texts);
	}

	Status saveGlbFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
	{
		std::error_code error;
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path(), error);
			if (error)
			{
				return Error{ErrorCode::InvalidArgument, "Cannot create the folder " + assets::utf8Of(path.parent_path()) + ": " + error.message()};
			}
		}
		std::filesystem::path temporary = path;
		temporary += ".tmp";
		{
			std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
			if (!out)
			{
				return Error{ErrorCode::InvalidArgument, "Cannot write " + assets::utf8Of(temporary)};
			}
			out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			if (!out)
			{
				return Error{ErrorCode::InvalidArgument, "Failed while writing " + assets::utf8Of(temporary)};
			}
		}
		std::filesystem::rename(temporary, path, error);
		if (error)
		{
			std::filesystem::remove(temporary, error);
			return Error{ErrorCode::InvalidArgument, "Cannot replace " + assets::utf8Of(path) + ": close it in any program using it"};
		}
		return Status::success();
	}
}
