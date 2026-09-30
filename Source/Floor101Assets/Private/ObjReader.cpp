#include "F101/Assets/ModelReader.h"

#include "ModelCommon.h"

#include <array>
#include <map>
#include <string>

namespace f101::assets
{
	using namespace detail;

	namespace
	{
		Error parseError(std::size_t line, const std::string& message)
		{
			return Error{ErrorCode::ParseError, "OBJ line " + std::to_string(line) + ": " + message};
		}

		// The rest of a line after its keyword, trimmed (names and paths may contain spaces).
		std::string_view restOf(std::string_view line, std::string_view keyword)
		{
			std::string_view rest = line.substr(line.find(keyword) + keyword.size());
			while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t'))
			{
				rest.remove_prefix(1);
			}
			while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' || rest.back() == '\r'))
			{
				rest.remove_suffix(1);
			}
			return rest;
		}

		// The file a map_Kd line names, after its options (-o u v w, -s u v w, -bm f, -clamp on...).
		std::string textureOf(const std::vector<std::string_view>& parts)
		{
			std::size_t at = 1;
			while (at < parts.size() && parts[at].size() > 1 && parts[at].front() == '-')
			{
				const std::string_view option = parts[at++];
				const bool vector = option == "-o" || option == "-s" || option == "-t";
				const std::size_t arguments = vector ? 3 : option == "-mm" ? 2 : 1;
				for (std::size_t taken = 0; taken < arguments && at < parts.size(); ++taken)
				{
					double number = 0.0;
					if (vector && taken > 0 && !toDouble(parts[at], number))
					{
						break; // -s u only: the next token is not a number
					}
					++at;
				}
			}
			std::string file;
			for (; at < parts.size(); ++at)
			{
				file += (file.empty() ? "" : " ") + std::string(parts[at]);
			}
			return normalizeReference(file, false);
		}

		// Material libraries: newmtl, Kd, map_Kd, Ns (specular exponent) and Pr (PBR roughness).
		void readMtl(std::string_view text, std::map<std::string, ModelMaterial>& library)
		{
			ModelMaterial* current = nullptr;
			bool roughnessGiven = false;
			bool colourGiven = false;
			std::size_t start = 0;
			while (start < text.size())
			{
				std::size_t end = text.find('\n', start);
				if (end == std::string_view::npos)
				{
					end = text.size();
				}
				const std::string_view line = text.substr(start, end - start);
				start = end + 1;
				const std::vector<std::string_view> parts = splitWhitespace(line);
				if (parts.empty() || parts[0].front() == '#')
				{
					continue;
				}
				if (parts[0] == "newmtl")
				{
					const std::string name(restOf(line, "newmtl"));
					current = &library[name];
					current->name = name;
					roughnessGiven = false;
					colourGiven = false;
					continue;
				}
				if (!current)
				{
					continue;
				}
				double values[3] = {0.0, 0.0, 0.0};
				if (parts[0] == "Kd" && parts.size() >= 4 && toDouble(parts[1], values[0]) && toDouble(parts[2], values[1]) &&
					toDouble(parts[3], values[2]))
				{
					current->color = linearToSrgb(values[0], values[1], values[2]);
					colourGiven = true;
				}
				else if (parts[0] == "map_Kd" && parts.size() >= 2)
				{
					current->texture = textureOf(parts);
					if (!colourGiven)
					{
						current->color = cad::Rgb8{255, 255, 255}; // a texture without Kd shows as it is
					}
				}
				else if (parts[0] == "Pr" && parts.size() >= 2 && toDouble(parts[1], values[0]))
				{
					current->roughness = std::clamp(values[0], 0.05, 1.0);
					roughnessGiven = true;
				}
				else if (parts[0] == "Ns" && parts.size() >= 2 && toDouble(parts[1], values[0]) && !roughnessGiven)
				{
					// Exporters write Ns = (1 - roughness)^2 * 1000.
					current->roughness = std::clamp(1.0 - std::sqrt(std::clamp(values[0], 0.0, 1000.0) / 1000.0), 0.05, 1.0);
				}
			}
		}
	}

	Result<RawModel> readObj(std::string_view text, const ModelFileReader& files)
	{
		RawModel model;
		std::vector<geometry::Vec3> positions;
		std::vector<geometry::Vec2> uvs;
		std::vector<geometry::Vec3> normals;
		std::map<std::string, ModelMaterial> library;
		std::map<std::string, std::uint32_t> used; // material name -> index in model.materialList
		std::optional<std::uint32_t> material;

		auto materialNamed = [&](const std::string& name)
		{
			if (const auto found = used.find(name); found != used.end())
			{
				return found->second;
			}
			ModelMaterial entry;
			if (const auto defined = library.find(name); defined != library.end())
			{
				entry = defined->second;
			}
			else if (name != "default")
			{
				model.warnings.push_back("Material '" + name + "' is not in any material library");
			}
			entry.name = name;
			const auto index = static_cast<std::uint32_t>(model.materialList.size());
			model.materialList.push_back(entry);
			used[name] = index;
			return index;
		};

		std::size_t lineNumber = 0;
		std::size_t start = 0;
		while (start < text.size())
		{
			std::size_t end = text.find('\n', start);
			if (end == std::string_view::npos)
			{
				end = text.size();
			}
			const std::string_view line = text.substr(start, end - start);
			start = end + 1;
			++lineNumber;
			const std::vector<std::string_view> parts = splitWhitespace(line);
			if (parts.empty() || parts[0].front() == '#')
			{
				continue;
			}
			const std::string_view keyword = parts[0];
			if (keyword == "v" || keyword == "vn")
			{
				geometry::Vec3 v;
				if (parts.size() < 4 || !toDouble(parts[1], v.x) || !toDouble(parts[2], v.y) || !toDouble(parts[3], v.z))
				{
					return parseError(lineNumber, "a vertex needs three numbers");
				}
				(keyword == "v" ? positions : normals).push_back(keyword == "v" ? v : normalized(v));
			}
			else if (keyword == "vt")
			{
				geometry::Vec2 uv;
				if (parts.size() < 2 || !toDouble(parts[1], uv.u) || (parts.size() > 2 && !toDouble(parts[2], uv.v)))
				{
					return parseError(lineNumber, "a texture coordinate needs numbers");
				}
				uv.v = 1.0 - uv.v; // OBJ's v runs up the image
				uvs.push_back(uv);
			}
			else if (keyword == "mtllib")
			{
				// The rest of the line is one file name (it may have spaces) or several separated by spaces.
				std::vector<std::string> names{std::string(restOf(line, "mtllib"))};
				for (std::size_t i = 1; parts.size() > 2 && i < parts.size(); ++i)
				{
					names.emplace_back(parts[i]);
				}
				bool found = false;
				for (std::size_t i = 0; i < names.size(); ++i)
				{
					if (const std::optional<std::vector<std::uint8_t>> bytes = files(normalizeReference(names[i], false)))
					{
						readMtl(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), library);
						found = true;
						if (i == 0)
						{
							break;
						}
					}
				}
				if (!found)
				{
					model.warnings.push_back("Material library '" + std::string(restOf(line, "mtllib")) + "' not found");
				}
			}
			else if (keyword == "usemtl")
			{
				material = materialNamed(std::string(restOf(line, "usemtl")));
			}
			else if (keyword == "f")
			{
				if (parts.size() < 4)
				{
					return parseError(lineNumber, "a face needs at least three corners");
				}
				struct Corner
				{
					long long position = -1;
					long long uv = -1;
					long long normal = -1;
				};
				std::vector<Corner> corners;
				for (std::size_t i = 1; i < parts.size(); ++i)
				{
					Corner corner;
					std::array<std::string_view, 3> fields;
					std::size_t field = 0;
					std::size_t from = 0;
					const std::string_view token = parts[i];
					while (field < 3)
					{
						const std::size_t slash = token.find('/', from);
						fields[field++] = token.substr(from, slash == std::string_view::npos ? std::string_view::npos : slash - from);
						if (slash == std::string_view::npos)
						{
							break;
						}
						from = slash + 1;
					}
					auto resolve = [&](std::string_view value, std::size_t count, long long& out) -> bool
					{
						if (value.empty())
						{
							return true; // not given
						}
						long long index = 0;
						if (!toInteger(value, index) || index == 0)
						{
							return false;
						}
						index = index > 0 ? index - 1 : static_cast<long long>(count) + index;
						if (index < 0 || index >= static_cast<long long>(count))
						{
							return false;
						}
						out = index;
						return true;
					};
					if (!resolve(fields[0], positions.size(), corner.position) || corner.position < 0 ||
						(field > 1 && !resolve(fields[1], uvs.size(), corner.uv)) || (field > 2 && !resolve(fields[2], normals.size(), corner.normal)))
					{
						return parseError(lineNumber, "a face refers to a vertex that does not exist");
					}
					corners.push_back(corner);
				}
				if (!material)
				{
					material = materialNamed("default");
				}
				for (std::size_t i = 1; i + 1 < corners.size(); ++i)
				{
					if (model.triangleCount() >= MaxModelTriangles)
					{
						return Error{ErrorCode::OutOfRange, "The model has more than " + std::to_string(MaxModelTriangles) + " triangles"};
					}
					const std::array<Corner, 3> triangle{corners[0], corners[i], corners[i + 1]};
					const bool withNormals = triangle[0].normal >= 0 && triangle[1].normal >= 0 && triangle[2].normal >= 0;
					const geometry::Vec3 flat = faceNormal(positions[static_cast<std::size_t>(triangle[0].position)],
						positions[static_cast<std::size_t>(triangle[1].position)], positions[static_cast<std::size_t>(triangle[2].position)]);
					for (const Corner& corner : triangle)
					{
						model.positions.push_back(positions[static_cast<std::size_t>(corner.position)]);
						model.normals.push_back(withNormals ? normals[static_cast<std::size_t>(corner.normal)] : flat);
						model.uvs.push_back(corner.uv >= 0 ? uvs[static_cast<std::size_t>(corner.uv)] : geometry::Vec2{});
					}
					model.materials.push_back(*material);
				}
			}
		}
		if (model.triangleCount() == 0)
		{
			return Error{ErrorCode::ParseError, "The OBJ file has no faces"};
		}
		return model;
	}
}
