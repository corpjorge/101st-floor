#include "F101/Assets/ModelReader.h"

#include "F101/Assets/ImageInfo.h"
#include "ModelCommon.h"

#include <array>
#include <cstring>
#include <map>
#include <set>
#include <string>

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
	using namespace detail;

	namespace
	{
		using Json = nlohmann::json;
		using Matrix = std::array<double, 16>; // column-major, as glTF

		constexpr Matrix Identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

		Error gltfError(const std::string& message)
		{
			return Error{ErrorCode::ParseError, "glTF: " + message};
		}

		const Json* member(const Json& object, const char* key)
		{
			if (!object.is_object())
			{
				return nullptr;
			}
			const auto found = object.find(key);
			return found == object.end() ? nullptr : &*found;
		}

		const Json* element(const Json* array, std::size_t index)
		{
			return array && array->is_array() && index < array->size() ? &(*array)[index] : nullptr;
		}

		bool unsignedOf(const Json* value, std::uint64_t& out)
		{
			if (value && value->is_number_unsigned())
			{
				out = value->get<std::uint64_t>();
				return true;
			}
			if (value && value->is_number_integer() && value->get<std::int64_t>() >= 0)
			{
				out = static_cast<std::uint64_t>(value->get<std::int64_t>());
				return true;
			}
			return false;
		}

		double numberOr(const Json* value, double fallback)
		{
			return value && value->is_number() ? value->get<double>() : fallback;
		}

		std::string stringOr(const Json* value, const std::string& fallback)
		{
			return value && value->is_string() ? value->get<std::string>() : fallback;
		}

		std::optional<std::vector<std::uint8_t>> decodeBase64(std::string_view text)
		{
			auto value = [](char c) -> int
			{
				if (c >= 'A' && c <= 'Z') { return c - 'A'; }
				if (c >= 'a' && c <= 'z') { return c - 'a' + 26; }
				if (c >= '0' && c <= '9') { return c - '0' + 52; }
				if (c == '+' || c == '-') { return 62; }
				if (c == '/' || c == '_') { return 63; }
				return -1;
			};
			std::vector<std::uint8_t> bytes;
			bytes.reserve(text.size() * 3 / 4);
			std::uint32_t buffer = 0;
			int bits = 0;
			for (const char c : text)
			{
				if (c == '=' || c == '\n' || c == '\r' || c == ' ')
				{
					continue;
				}
				const int v = value(c);
				if (v < 0)
				{
					return std::nullopt;
				}
				buffer = (buffer << 6) | static_cast<std::uint32_t>(v);
				bits += 6;
				if (bits >= 8)
				{
					bits -= 8;
					bytes.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
				}
			}
			return bytes;
		}

		// The bytes of a data: URI ("data:<type>;base64,<data>"); nullopt if it is not one.
		std::optional<std::vector<std::uint8_t>> dataUri(const std::string& uri)
		{
			if (uri.rfind("data:", 0) != 0)
			{
				return std::nullopt;
			}
			const std::size_t comma = uri.find(',');
			if (comma == std::string::npos || uri.substr(0, comma).find(";base64") == std::string::npos)
			{
				return std::vector<std::uint8_t>{}; // not base64: treated as empty (invalid) below
			}
			return decodeBase64(std::string_view(uri).substr(comma + 1));
		}

		Matrix multiply(const Matrix& a, const Matrix& b)
		{
			Matrix m{};
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					double sum = 0.0;
					for (int k = 0; k < 4; ++k)
					{
						sum += a[static_cast<std::size_t>(k * 4 + row)] * b[static_cast<std::size_t>(column * 4 + k)];
					}
					m[static_cast<std::size_t>(column * 4 + row)] = sum;
				}
			}
			return m;
		}

		Matrix localMatrix(const Json& node)
		{
			if (const Json* matrix = member(node, "matrix"); matrix && matrix->is_array() && matrix->size() == 16)
			{
				Matrix m{};
				for (std::size_t i = 0; i < 16; ++i)
				{
					m[i] = numberOr(&(*matrix)[i], Identity[i]);
				}
				return m;
			}
			auto vector = [&node](const char* key, std::array<double, 4> fallback, std::size_t size)
			{
				const Json* value = member(node, key);
				if (value && value->is_array() && value->size() == size)
				{
					for (std::size_t i = 0; i < size; ++i)
					{
						fallback[i] = numberOr(&(*value)[i], fallback[i]);
					}
				}
				return fallback;
			};
			const std::array<double, 4> t = vector("translation", {0, 0, 0, 0}, 3);
			const std::array<double, 4> q = vector("rotation", {0, 0, 0, 1}, 4);
			const std::array<double, 4> s = vector("scale", {1, 1, 1, 0}, 3);
			const double x = q[0], y = q[1], z = q[2], w = q[3];
			// Rotation matrix of the quaternion, columns scaled; then the translation.
			return Matrix{(1 - 2 * (y * y + z * z)) * s[0], 2 * (x * y + z * w) * s[0], 2 * (x * z - y * w) * s[0], 0,
				2 * (x * y - z * w) * s[1], (1 - 2 * (x * x + z * z)) * s[1], 2 * (y * z + x * w) * s[1], 0,
				2 * (x * z + y * w) * s[2], 2 * (y * z - x * w) * s[2], (1 - 2 * (x * x + y * y)) * s[2], 0,
				t[0], t[1], t[2], 1};
		}

		struct Reader
		{
			const Json& root;
			std::vector<std::vector<std::uint8_t>> buffers;
			RawModel model;
			std::map<std::size_t, std::string> embeddedNames; // image index -> file name
			std::optional<std::uint32_t> defaultMaterial;

			// An accessor's elements as doubles (count x components), with every read inside its buffer.
			Result<std::vector<double>> accessor(std::uint64_t index, int components, std::size_t& count)
			{
				const Json* accessor = element(member(root, "accessors"), static_cast<std::size_t>(index));
				if (!accessor)
				{
					return gltfError("accessor " + std::to_string(index) + " does not exist");
				}
				if (member(*accessor, "sparse"))
				{
					return gltfError("sparse accessors are not supported");
				}
				const std::string type = stringOr(member(*accessor, "type"), "");
				const int typeComponents = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : 0;
				std::uint64_t componentType = 0;
				std::uint64_t elements = 0;
				std::uint64_t viewIndex = 0;
				std::uint64_t offset = 0;
				if (typeComponents != components || !unsignedOf(member(*accessor, "componentType"), componentType) ||
					!unsignedOf(member(*accessor, "count"), elements) || !unsignedOf(member(*accessor, "bufferView"), viewIndex))
				{
					return gltfError("accessor " + std::to_string(index) + " is not a " + std::to_string(components) +
						"-component accessor with data");
				}
				(void)unsignedOf(member(*accessor, "byteOffset"), offset);
				const std::size_t componentSize = componentType == 5120 || componentType == 5121 ? 1
					: componentType == 5122 || componentType == 5123 ? 2 : componentType == 5125 || componentType == 5126 ? 4 : 0;
				if (componentSize == 0)
				{
					return gltfError("accessor " + std::to_string(index) + " has an unknown component type");
				}
				const Json* view = element(member(root, "bufferViews"), static_cast<std::size_t>(viewIndex));
				std::uint64_t bufferIndex = 0;
				std::uint64_t viewOffset = 0;
				std::uint64_t viewLength = 0;
				std::uint64_t stride = 0;
				if (!view || !unsignedOf(member(*view, "buffer"), bufferIndex) || !unsignedOf(member(*view, "byteLength"), viewLength) ||
					bufferIndex >= buffers.size())
				{
					return gltfError("buffer view " + std::to_string(viewIndex) + " is invalid");
				}
				(void)unsignedOf(member(*view, "byteOffset"), viewOffset);
				(void)unsignedOf(member(*view, "byteStride"), stride);
				const std::uint64_t elementSize = componentSize * static_cast<std::uint64_t>(components);
				if (stride == 0)
				{
					stride = elementSize;
				}
				const std::vector<std::uint8_t>& buffer = buffers[static_cast<std::size_t>(bufferIndex)];
				if (elements > (1ull << 26) || viewOffset + viewLength > buffer.size() || stride < elementSize ||
					(elements > 0 && offset + stride * (elements - 1) + elementSize > viewLength))
				{
					return gltfError("accessor " + std::to_string(index) + " reads outside its buffer");
				}
				const bool normalized = member(*accessor, "normalized") && member(*accessor, "normalized")->is_boolean() &&
					member(*accessor, "normalized")->get<bool>();
				count = static_cast<std::size_t>(elements);
				std::vector<double> values(count * static_cast<std::size_t>(components));
				const std::uint8_t* base = buffer.data() + viewOffset + offset;
				for (std::size_t e = 0; e < count; ++e)
				{
					const std::uint8_t* at = base + e * stride;
					for (int c = 0; c < components; ++c)
					{
						const std::uint8_t* p = at + static_cast<std::size_t>(c) * componentSize;
						double v = 0.0;
						switch (componentType)
						{
						case 5120: { std::int8_t x; std::memcpy(&x, p, 1); v = normalized ? std::max(x / 127.0, -1.0) : x; break; }
						case 5121: { std::uint8_t x; std::memcpy(&x, p, 1); v = normalized ? x / 255.0 : x; break; }
						case 5122: { std::int16_t x; std::memcpy(&x, p, 2); v = normalized ? std::max(x / 32767.0, -1.0) : x; break; }
						case 5123: { std::uint16_t x; std::memcpy(&x, p, 2); v = normalized ? x / 65535.0 : x; break; }
						case 5125: { std::uint32_t x; std::memcpy(&x, p, 4); v = x; break; }
						default: { float x; std::memcpy(&x, p, 4); v = x; break; }
						}
						values[e * static_cast<std::size_t>(components) + static_cast<std::size_t>(c)] = v;
					}
				}
				return values;
			}

			// The file name of an embedded image (its bytes kept for the importer), or the reference of an external one.
			std::string imageReference(std::uint64_t imageIndex)
			{
				const Json* image = element(member(root, "images"), static_cast<std::size_t>(imageIndex));
				if (!image)
				{
					return {};
				}
				const std::string uri = stringOr(member(*image, "uri"), "");
				std::optional<std::vector<std::uint8_t>> bytes;
				if (!uri.empty())
				{
					bytes = dataUri(uri);
					if (!bytes)
					{
						return normalizeReference(uri, true); // a file next to the model
					}
				}
				else if (std::uint64_t view = 0; unsignedOf(member(*image, "bufferView"), view))
				{
					const Json* bufferView = element(member(root, "bufferViews"), static_cast<std::size_t>(view));
					std::uint64_t buffer = 0;
					std::uint64_t offset = 0;
					std::uint64_t length = 0;
					if (bufferView && unsignedOf(member(*bufferView, "buffer"), buffer) && unsignedOf(member(*bufferView, "byteLength"), length) &&
						buffer < buffers.size())
					{
						(void)unsignedOf(member(*bufferView, "byteOffset"), offset);
						if (offset + length <= buffers[static_cast<std::size_t>(buffer)].size())
						{
							const auto begin = buffers[static_cast<std::size_t>(buffer)].begin() + static_cast<std::ptrdiff_t>(offset);
							bytes = std::vector<std::uint8_t>(begin, begin + static_cast<std::ptrdiff_t>(length));
						}
					}
				}
				if (!bytes)
				{
					return {};
				}
				const Result<ImageInfo> info = readImageInfo(bytes->data(), bytes->size());
				if (!info)
				{
					model.warnings.push_back("Embedded image " + std::to_string(imageIndex) + ": " + info.error().message);
					return {};
				}
				if (const auto named = embeddedNames.find(static_cast<std::size_t>(imageIndex)); named != embeddedNames.end())
				{
					return named->second;
				}
				const std::string name = "embedded_" + std::to_string(imageIndex) + "." + std::string(imageExtension(info.value().format));
				embeddedNames[static_cast<std::size_t>(imageIndex)] = name;
				model.embeddedImages.push_back(EmbeddedImage{name, std::move(*bytes)});
				return name;
			}

			void readMaterials()
			{
				const Json* materials = member(root, "materials");
				for (std::size_t i = 0; materials && materials->is_array() && i < materials->size(); ++i)
				{
					const Json& material = (*materials)[i];
					ModelMaterial entry;
					entry.name = stringOr(member(material, "name"), "material " + std::to_string(i));
					const Json* pbr = member(material, "pbrMetallicRoughness");
					if (const Json* factor = pbr ? member(*pbr, "baseColorFactor") : nullptr; factor && factor->is_array() && factor->size() == 4)
					{
						entry.color = linearToSrgb(numberOr(&(*factor)[0], 1.0), numberOr(&(*factor)[1], 1.0), numberOr(&(*factor)[2], 1.0));
					}
					else
					{
						entry.color = cad::Rgb8{255, 255, 255}; // glTF's default factor: the texture alone, or white
					}
					entry.roughness = std::clamp(numberOr(pbr ? member(*pbr, "roughnessFactor") : nullptr, 1.0), 0.05, 1.0);
					const Json* textureInfo = pbr ? member(*pbr, "baseColorTexture") : nullptr;
					std::uint64_t textureIndex = 0;
					std::uint64_t source = 0;
					if (textureInfo && unsignedOf(member(*textureInfo, "index"), textureIndex))
					{
						const Json* texture = element(member(root, "textures"), static_cast<std::size_t>(textureIndex));
						if (texture && unsignedOf(member(*texture, "source"), source))
						{
							entry.texture = imageReference(source);
						}
					}
					model.materialList.push_back(std::move(entry));
				}
			}

			Status readPrimitive(const Json& primitive, const Matrix& world)
			{
				std::uint64_t mode = 4;
				(void)unsignedOf(member(primitive, "mode"), mode);
				if (mode < 4 || mode > 6)
				{
					model.warnings.push_back("Points and lines are left out");
					return Status::success();
				}
				const Json* attributes = member(primitive, "attributes");
				std::uint64_t positionIndex = 0;
				if (!attributes || !unsignedOf(member(*attributes, "POSITION"), positionIndex))
				{
					return gltfError("a primitive has no POSITION");
				}
				std::size_t vertices = 0;
				Result<std::vector<double>> positions = accessor(positionIndex, 3, vertices);
				if (!positions)
				{
					return positions.error();
				}
				std::vector<double> normals;
				std::vector<double> uvs;
				std::size_t count = 0;
				if (std::uint64_t index = 0; unsignedOf(member(*attributes, "NORMAL"), index))
				{
					Result<std::vector<double>> read = accessor(index, 3, count);
					if (!read)
					{
						return read.error();
					}
					if (count == vertices)
					{
						normals = std::move(read).value();
					}
				}
				if (std::uint64_t index = 0; unsignedOf(member(*attributes, "TEXCOORD_0"), index))
				{
					Result<std::vector<double>> read = accessor(index, 2, count);
					if (!read)
					{
						return read.error();
					}
					if (count == vertices)
					{
						uvs = std::move(read).value();
					}
				}
				std::vector<std::uint32_t> indices;
				if (std::uint64_t index = 0; unsignedOf(member(primitive, "indices"), index))
				{
					Result<std::vector<double>> read = accessor(index, 1, count);
					if (!read)
					{
						return read.error();
					}
					for (const double value : read.value())
					{
						if (value < 0.0 || value >= static_cast<double>(vertices))
						{
							return gltfError("an index refers to a vertex that does not exist");
						}
						indices.push_back(static_cast<std::uint32_t>(value));
					}
				}
				else
				{
					for (std::size_t i = 0; i < vertices; ++i)
					{
						indices.push_back(static_cast<std::uint32_t>(i));
					}
				}
				std::uint32_t material = 0;
				if (std::uint64_t index = 0; unsignedOf(member(primitive, "material"), index) && index < model.materialList.size())
				{
					material = static_cast<std::uint32_t>(index);
				}
				else
				{
					if (!defaultMaterial)
					{
						defaultMaterial = static_cast<std::uint32_t>(model.materialList.size());
						model.materialList.push_back(ModelMaterial{"default", cad::Rgb8{204, 204, 204}, 0.8, {}});
					}
					material = *defaultMaterial;
				}

				// Positions by the world matrix; normals by its inverse transpose (cofactors); a mirror flips the winding.
				const Matrix& m = world;
				const double c00 = m[5] * m[10] - m[9] * m[6], c01 = m[9] * m[2] - m[1] * m[10], c02 = m[1] * m[6] - m[5] * m[2];
				const double c10 = m[8] * m[6] - m[4] * m[10], c11 = m[0] * m[10] - m[8] * m[2], c12 = m[4] * m[2] - m[0] * m[6];
				const double c20 = m[4] * m[9] - m[8] * m[5], c21 = m[8] * m[1] - m[0] * m[9], c22 = m[0] * m[5] - m[4] * m[1];
				const double determinant = m[0] * c00 + m[4] * c01 + m[8] * c02;
				auto position = [&](std::uint32_t v)
				{
					const double* p = &positions.value()[static_cast<std::size_t>(v) * 3];
					return geometry::Vec3{m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
						m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
				};
				auto normal = [&](std::uint32_t v)
				{
					const double* n = &normals[static_cast<std::size_t>(v) * 3];
					const double sign = determinant < 0.0 ? -1.0 : 1.0;
					return normalized(geometry::Vec3{sign * (c00 * n[0] + c01 * n[1] + c02 * n[2]),
						sign * (c10 * n[0] + c11 * n[1] + c12 * n[2]), sign * (c20 * n[0] + c21 * n[1] + c22 * n[2])});
				};
				auto addTriangle = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) -> Status
				{
					if (model.triangleCount() >= MaxModelTriangles)
					{
						return Error{ErrorCode::OutOfRange, "The model has more than " + std::to_string(MaxModelTriangles) + " triangles"};
					}
					if (determinant < 0.0)
					{
						std::swap(b, c);
					}
					const geometry::Vec3 flat = faceNormal(position(a), position(b), position(c));
					for (const std::uint32_t v : {a, b, c})
					{
						model.positions.push_back(position(v));
						model.normals.push_back(normals.empty() ? flat : normal(v));
						model.uvs.push_back(uvs.empty() ? geometry::Vec2{}
							: geometry::Vec2{uvs[static_cast<std::size_t>(v) * 2], uvs[static_cast<std::size_t>(v) * 2 + 1]});
					}
					model.materials.push_back(material);
					return Status::success();
				};
				for (std::size_t i = 0; i + 2 < indices.size(); i += mode == 4 ? 3 : 1)
				{
					Status added = mode == 4 ? addTriangle(indices[i], indices[i + 1], indices[i + 2])
						: mode == 5 ? (i % 2 == 0 ? addTriangle(indices[i], indices[i + 1], indices[i + 2])
							: addTriangle(indices[i], indices[i + 2], indices[i + 1]))
						: addTriangle(indices[i + 1], indices[i + 2], indices[0]);
					if (!added)
					{
						return added;
					}
				}
				return Status::success();
			}

			Status readNode(std::uint64_t index, const Matrix& parent, std::set<std::uint64_t>& path)
			{
				const Json* node = element(member(root, "nodes"), static_cast<std::size_t>(index));
				if (!node || path.count(index) || path.size() > 64)
				{
					return gltfError("the node tree is invalid");
				}
				path.insert(index);
				const Matrix world = multiply(parent, localMatrix(*node));
				if (std::uint64_t meshIndex = 0; unsignedOf(member(*node, "mesh"), meshIndex))
				{
					const Json* mesh = element(member(root, "meshes"), static_cast<std::size_t>(meshIndex));
					const Json* primitives = mesh ? member(*mesh, "primitives") : nullptr;
					if (!primitives || !primitives->is_array())
					{
						return gltfError("mesh " + std::to_string(meshIndex) + " is invalid");
					}
					for (const Json& primitive : *primitives)
					{
						if (Status read = readPrimitive(primitive, world); !read)
						{
							return read;
						}
					}
				}
				if (const Json* children = member(*node, "children"); children && children->is_array())
				{
					for (const Json& child : *children)
					{
						std::uint64_t childIndex = 0;
						if (!unsignedOf(&child, childIndex))
						{
							return gltfError("the node tree is invalid");
						}
						if (Status read = readNode(childIndex, world, path); !read)
						{
							return read;
						}
					}
				}
				path.erase(index);
				return Status::success();
			}
		};
	}

	Result<RawModel> readGltf(const std::vector<std::uint8_t>& bytes, const ModelFileReader& files)
	{
		// GLB: a 12-byte header, a JSON chunk and an optional binary chunk (the first buffer).
		std::string_view jsonText(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		std::optional<std::vector<std::uint8_t>> binaryChunk;
		auto read32 = [&bytes](std::size_t at)
		{
			return std::uint32_t{bytes[at]} | (std::uint32_t{bytes[at + 1]} << 8) | (std::uint32_t{bytes[at + 2]} << 16) |
				(std::uint32_t{bytes[at + 3]} << 24);
		};
		if (bytes.size() >= 4 && read32(0) == 0x46546C67)
		{
			if (bytes.size() < 20 || read32(4) != 2 || read32(8) > bytes.size() || read32(16) != 0x4E4F534A)
			{
				return gltfError("not a version 2 binary glTF (.glb)");
			}
			const std::size_t jsonLength = read32(12);
			if (20 + jsonLength > bytes.size())
			{
				return gltfError("the .glb JSON chunk is cut short");
			}
			jsonText = jsonText.substr(20, jsonLength);
			const std::size_t next = 20 + ((jsonLength + 3) & ~std::size_t{3});
			if (next + 8 <= bytes.size() && read32(next + 4) == 0x004E4942)
			{
				const std::size_t binaryLength = read32(next);
				if (next + 8 + binaryLength > bytes.size())
				{
					return gltfError("the .glb binary chunk is cut short");
				}
				binaryChunk = std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(next + 8),
					bytes.begin() + static_cast<std::ptrdiff_t>(next + 8 + binaryLength));
			}
		}
		const Json root = Json::parse(jsonText.begin(), jsonText.end(), nullptr, /*allow_exceptions*/ false);
		if (root.is_discarded() || !root.is_object())
		{
			return gltfError("the JSON is not valid");
		}
		const Json* asset = member(root, "asset");
		const std::string version = asset ? stringOr(member(*asset, "version"), "") : "";
		if (version.rfind("2.", 0) != 0)
		{
			return gltfError("only glTF 2.0 is supported (this file says \"" + version + "\")");
		}
		if (const Json* required = member(root, "extensionsRequired"); required && required->is_array() && !required->empty())
		{
			std::string names;
			for (const Json& name : *required)
			{
				names += (names.empty() ? "" : ", ") + stringOr(&name, "?");
			}
			return Error{ErrorCode::InvalidArgument, "The model needs glTF extensions 101st Floor cannot read yet: " + names};
		}

		Reader reader{root, {}, {}, {}, {}};
		const Json* buffers = member(root, "buffers");
		for (std::size_t i = 0; buffers && buffers->is_array() && i < buffers->size(); ++i)
		{
			const std::string uri = stringOr(member((*buffers)[i], "uri"), "");
			std::optional<std::vector<std::uint8_t>> data;
			if (uri.empty())
			{
				data = i == 0 ? binaryChunk : std::nullopt;
			}
			else
			{
				data = dataUri(uri);
				if (!data)
				{
					data = files(normalizeReference(uri, true));
					if (!data)
					{
						return Error{ErrorCode::NotFound, "The model's buffer '" + normalizeReference(uri, true) + "' was not found"};
					}
				}
			}
			if (!data)
			{
				return gltfError("buffer " + std::to_string(i) + " has no data");
			}
			reader.buffers.push_back(std::move(*data));
		}
		reader.readMaterials();

		// The default scene's root nodes; without scenes, every node no other node has as a child.
		std::vector<std::uint64_t> roots;
		std::uint64_t sceneIndex = 0;
		(void)unsignedOf(member(root, "scene"), sceneIndex);
		if (const Json* scene = element(member(root, "scenes"), static_cast<std::size_t>(sceneIndex)))
		{
			if (const Json* nodes = member(*scene, "nodes"); nodes && nodes->is_array())
			{
				for (const Json& node : *nodes)
				{
					std::uint64_t index = 0;
					if (unsignedOf(&node, index))
					{
						roots.push_back(index);
					}
				}
			}
		}
		else if (const Json* nodes = member(root, "nodes"); nodes && nodes->is_array())
		{
			std::set<std::uint64_t> children;
			for (const Json& node : *nodes)
			{
				if (const Json* list = member(node, "children"); list && list->is_array())
				{
					for (const Json& child : *list)
					{
						std::uint64_t index = 0;
						if (unsignedOf(&child, index))
						{
							children.insert(index);
						}
					}
				}
			}
			for (std::uint64_t i = 0; i < nodes->size(); ++i)
			{
				if (!children.count(i))
				{
					roots.push_back(i);
				}
			}
		}
		for (const std::uint64_t node : roots)
		{
			std::set<std::uint64_t> path;
			if (Status read = reader.readNode(node, Identity, path); !read)
			{
				return read.error();
			}
		}
		if (reader.model.triangleCount() == 0)
		{
			return gltfError("the model has no triangles");
		}
		return std::move(reader.model);
	}
}
