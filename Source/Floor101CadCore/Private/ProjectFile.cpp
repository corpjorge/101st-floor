#include "F101/Cad/ProjectFile.h"

#include "F101/Cad/Document.h"
#include "F101/Cad/Units.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <tuple>
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

namespace f101::cad
{
	namespace
	{
		using Json = nlohmann::ordered_json;

		// UTF-8 text of a path for messages. path::string() converts to the ANSI code page and would abort (no exceptions)
		// on characters it cannot represent.
		std::string displayPath(const std::filesystem::path& path)
		{
			const std::u8string text = path.u8string();
			return std::string(text.begin(), text.end());
		}

		Error formatError(const std::string& message)
		{
			return Error{ErrorCode::ParseError, "Invalid project file: " + message};
		}

		Json toJson(const Point2D& point)
		{
			return Json::array({point.x_mm, point.y_mm});
		}

		// Reads an integer millimetre value within the model range. Floats and strings are rejected.
		Result<std::int64_t> readMillimeters(const Json& value, const std::string& what)
		{
			if (value.is_number_unsigned())
			{
				const std::uint64_t magnitude = value.get<std::uint64_t>();
				if (magnitude > static_cast<std::uint64_t>(MaxAbsMillimeters))
				{
					return Error{ErrorCode::OutOfRange, "Invalid project file: " + what + " is outside the model range"};
				}
				return static_cast<std::int64_t>(magnitude);
			}
			if (value.is_number_integer())
			{
				const std::int64_t millimeters = value.get<std::int64_t>();
				if (millimeters < -MaxAbsMillimeters || millimeters > MaxAbsMillimeters)
				{
					return Error{ErrorCode::OutOfRange, "Invalid project file: " + what + " is outside the model range"};
				}
				return millimeters;
			}
			return formatError(what + " must be an integer number of millimetres");
		}

		Result<std::int64_t> readMillimetersField(const Json& object, const char* key, const std::string& where)
		{
			const auto field = object.find(key);
			if (field == object.end())
			{
				return formatError(where + " has no '" + key + "'");
			}
			return readMillimeters(*field, where + "." + key);
		}

		Result<Point2D> readPoint(const Json& object, const char* key, const std::string& where)
		{
			const auto field = object.find(key);
			if (field == object.end() || !field->is_array() || field->size() != 2)
			{
				return formatError(where + "." + key + " must be [x_mm, y_mm]");
			}
			const Result<std::int64_t> x = readMillimeters((*field)[0], where + "." + key + "[0]");
			if (!x)
			{
				return x.error();
			}
			const Result<std::int64_t> y = readMillimeters((*field)[1], where + "." + key + "[1]");
			if (!y)
			{
				return y.error();
			}
			return Point2D{x.value(), y.value()};
		}

		Result<Opening> readOpening(const Json& object, const std::string& where)
		{
			if (!object.is_object())
			{
				return formatError(where + " must be an object");
			}
			const auto idField = object.find("id");
			if (idField == object.end() || !idField->is_string())
			{
				return formatError(where + ".id must be a UUID string");
			}
			const Result<Uuid> uuid = parseUuid(idField->get_ref<const std::string&>());
			if (!uuid)
			{
				return formatError(where + ".id: " + uuid.error().message);
			}
			const auto kindField = object.find("kind");
			if (kindField == object.end() || !kindField->is_string() ||
				(kindField->get_ref<const std::string&>() != kindName(OpeningKind::Door) &&
					kindField->get_ref<const std::string&>() != kindName(OpeningKind::Window)))
			{
				return formatError(where + ".kind must be \"door\" or \"window\"");
			}
			Opening opening;
			opening.id = OpeningId{uuid.value()};
			opening.kind = kindField->get_ref<const std::string&>() == kindName(OpeningKind::Door) ? OpeningKind::Door : OpeningKind::Window;
			for (const auto& [key, target] : {std::pair<const char*, std::int64_t*>{"offsetMm", &opening.offsetMm},
					 {"widthMm", &opening.widthMm}, {"heightMm", &opening.heightMm}, {"sillMm", &opening.sillMm}})
			{
				const Result<std::int64_t> value = readMillimetersField(object, key, where);
				if (!value)
				{
					return value.error();
				}
				*target = value.value();
			}
			if (const auto finish = object.find("finish"); finish != object.end()) // schema 5
			{
				if (!finish->is_string())
				{
					return formatError(where + ".finish must be a string");
				}
				opening.finish = finish->get<std::string>();
			}
			// Schema 6 (ADR-0014). Older files had closed doors and fixed windows: that is what their openings become.
			opening.operation = opening.kind == OpeningKind::Door ? OpeningOperation::Hinged : OpeningOperation::Fixed;
			if (const auto operation = object.find("operation"); operation != object.end())
			{
				bool known = false;
				for (const OpeningOperation candidate : {OpeningOperation::Hinged, OpeningOperation::DoubleHinged,
						 OpeningOperation::Sliding, OpeningOperation::Fixed})
				{
					if (operation->is_string() && operation->get_ref<const std::string&>() == operationName(candidate))
					{
						opening.operation = candidate;
						known = true;
					}
				}
				if (!known)
				{
					return formatError(where + ".operation must be \"hinged\", \"double\", \"sliding\" or \"fixed\"");
				}
			}
			if (const auto hinge = object.find("hinge"); hinge != object.end())
			{
				if (!hinge->is_string() || (*hinge != "start" && *hinge != "end"))
				{
					return formatError(where + ".hinge must be \"start\" or \"end\"");
				}
				opening.hinge = *hinge == "start" ? OpeningEnd::Start : OpeningEnd::End;
			}
			if (const auto opensTo = object.find("opensTo"); opensTo != object.end())
			{
				if (!opensTo->is_string() || (*opensTo != "left" && *opensTo != "right"))
				{
					return formatError(where + ".opensTo must be \"left\" or \"right\"");
				}
				opening.opensTo = *opensTo == "left" ? WallSide::Left : WallSide::Right;
			}
			if (const auto open = object.find("openPercent"); open != object.end())
			{
				if (!open->is_number_integer() || open->get<std::int64_t>() < 0 || open->get<std::int64_t>() > 100)
				{
					return formatError(where + ".openPercent must be an integer from 0 to 100");
				}
				opening.openPercent = static_cast<int>(open->get<std::int64_t>());
			}
			return opening;
		}

		// A UUID string field.
		Result<Uuid> readUuidField(const Json& object, const char* key, const std::string& where)
		{
			const auto field = object.find(key);
			if (field == object.end() || !field->is_string())
			{
				return formatError(where + "." + key + " must be a UUID string");
			}
			const Result<Uuid> uuid = parseUuid(field->get_ref<const std::string&>());
			if (!uuid)
			{
				return formatError(where + "." + key + ": " + uuid.error().message);
			}
			return uuid;
		}

		Result<Level> readLevel(const Json& object, std::size_t index)
		{
			const std::string where = "levels[" + std::to_string(index) + "]";
			if (!object.is_object())
			{
				return formatError(where + " must be an object");
			}
			const Result<Uuid> uuid = readUuidField(object, "id", where);
			if (!uuid)
			{
				return uuid.error();
			}
			Level level;
			level.id = LevelId{uuid.value()};
			if (const auto name = object.find("name"); name != object.end())
			{
				if (!name->is_string())
				{
					return formatError(where + ".name must be a string");
				}
				level.name = name->get<std::string>();
			}
			for (const auto& [key, target] : {std::pair<const char*, std::int64_t*>{"heightMm", &level.heightMm},
					 {"slabThicknessMm", &level.slabThicknessMm}})
			{
				const Result<std::int64_t> value = readMillimetersField(object, key, where);
				if (!value)
				{
					return value.error();
				}
				*target = value.value();
			}
			return level;
		}

		// An optional string field; `out` is left empty when it is missing.
		Status readOptionalString(const Json& object, const char* key, const std::string& where, std::string& out)
		{
			const auto field = object.find(key);
			if (field == object.end())
			{
				return Status::success();
			}
			if (!field->is_string())
			{
				return formatError(where + "." + key + " must be a string");
			}
			out = field->get<std::string>();
			return Status::success();
		}

		Result<RoomFinish> readRoomFinish(const Json& object, std::size_t index)
		{
			const std::string where = "roomFinishes[" + std::to_string(index) + "]";
			if (!object.is_object())
			{
				return formatError(where + " must be an object");
			}
			RoomFinish finish;
			for (const auto& [key, target] : {std::pair<const char*, std::string*>{"room", &finish.room}, {"floor", &finish.floor},
					 {"ceiling", &finish.ceiling}})
			{
				if (Status read = readOptionalString(object, key, where, *target); !read)
				{
					return read.error();
				}
			}
			return finish;
		}

		// Schema 7 (ADR-0016). The type, sizes and finish are checked against the catalogue by validateProject.
		Result<Furniture> readFurniture(const Json& object, std::size_t index)
		{
			const std::string where = "furniture[" + std::to_string(index) + "]";
			if (!object.is_object())
			{
				return formatError(where + " must be an object");
			}
			const Result<Uuid> uuid = readUuidField(object, "id", where);
			if (!uuid)
			{
				return uuid.error();
			}
			const Result<Uuid> level = readUuidField(object, "level", where);
			if (!level)
			{
				return level.error();
			}
			const auto type = object.find("type");
			if (type == object.end() || !type->is_string())
			{
				return formatError(where + ".type must be a string");
			}
			const Result<Point2D> position = readPoint(object, "position", where);
			if (!position)
			{
				return position.error();
			}
			Furniture furniture;
			furniture.id = FurnitureId{uuid.value()};
			furniture.level = LevelId{level.value()};
			furniture.type = type->get<std::string>();
			furniture.position = position.value();
			if (const auto rotation = object.find("rotationCentidegrees"); rotation != object.end())
			{
				if (!rotation->is_number_integer() || rotation->get<std::int64_t>() < 0 ||
					rotation->get<std::int64_t>() >= FullTurnCentidegrees)
				{
					return formatError(where + ".rotationCentidegrees must be an integer from 0 to 35999");
				}
				furniture.rotationCentidegrees = rotation->get<std::int64_t>();
			}
			for (const auto& [key, target] : {std::pair<const char*, std::int64_t*>{"widthMm", &furniture.widthMm},
					 {"depthMm", &furniture.depthMm}, {"heightMm", &furniture.heightMm}})
			{
				const Result<std::int64_t> value = readMillimetersField(object, key, where);
				if (!value)
				{
					return value.error();
				}
				*target = value.value();
			}
			if (Status read = readOptionalString(object, "finish", where, furniture.finish); !read)
			{
				return read.error();
			}
			return furniture;
		}

		// Schema 10 (ADR-0021). Sizes, shape and finish are checked by validateProject.
		Result<Column> readColumn(const Json& object, std::size_t index)
		{
			const std::string where = "columns[" + std::to_string(index) + "]";
			if (!object.is_object())
			{
				return formatError(where + " must be an object");
			}
			const Result<Uuid> uuid = readUuidField(object, "id", where);
			if (!uuid)
			{
				return uuid.error();
			}
			const Result<Uuid> level = readUuidField(object, "level", where);
			if (!level)
			{
				return level.error();
			}
			const Result<Point2D> position = readPoint(object, "position", where);
			if (!position)
			{
				return position.error();
			}
			const auto shape = object.find("shape");
			const std::optional<ColumnShape> parsed = shape != object.end() && shape->is_string()
				? parseColumnShape(shape->get_ref<const std::string&>()) : std::nullopt;
			if (!parsed)
			{
				return formatError(where + ".shape must be \"rectangle\" or \"round\"");
			}
			Column column;
			column.id = ColumnId{uuid.value()};
			column.level = LevelId{level.value()};
			column.position = position.value();
			column.shape = *parsed;
			for (const auto& [key, target] : {std::pair<const char*, std::int64_t*>{"widthMm", &column.widthMm}, {"depthMm", &column.depthMm}})
			{
				const Result<std::int64_t> value = readMillimetersField(object, key, where);
				if (!value)
				{
					return value.error();
				}
				*target = value.value();
			}
			if (const auto rotation = object.find("rotationCentidegrees"); rotation != object.end())
			{
				if (!rotation->is_number_integer() || rotation->get<std::int64_t>() < 0 || rotation->get<std::int64_t>() >= FullTurnCentidegrees)
				{
					return formatError(where + ".rotationCentidegrees must be an integer from 0 to 35999");
				}
				column.rotationCentidegrees = rotation->get<std::int64_t>();
			}
			if (Status read = readOptionalString(object, "finish", where, column.finish); !read)
			{
				return read.error();
			}
			return column;
		}

		// Schema 10 (ADR-0021): points are [x_mm, y_mm, z_mm], z over the level's floor.
		Result<ServiceRun> readServiceRun(const Json& object, std::size_t index)
		{
			const std::string where = "services[" + std::to_string(index) + "]";
			if (!object.is_object())
			{
				return formatError(where + " must be an object");
			}
			const Result<Uuid> uuid = readUuidField(object, "id", where);
			if (!uuid)
			{
				return uuid.error();
			}
			const Result<Uuid> level = readUuidField(object, "level", where);
			if (!level)
			{
				return level.error();
			}
			const auto kind = object.find("kind");
			const std::optional<ServiceKind> parsed = kind != object.end() && kind->is_string()
				? parseServiceKind(kind->get_ref<const std::string&>()) : std::nullopt;
			if (!parsed)
			{
				return formatError(where + ".kind must be \"electrical\", \"water\", \"sewage\" or \"gas\"");
			}
			const auto points = object.find("points");
			if (points == object.end() || !points->is_array())
			{
				return formatError(where + ".points must be an array");
			}
			ServiceRun run;
			run.id = ServiceRunId{uuid.value()};
			run.level = LevelId{level.value()};
			run.kind = *parsed;
			for (std::size_t p = 0; p < points->size(); ++p)
			{
				const Json& point = (*points)[p];
				const std::string at = where + ".points[" + std::to_string(p) + "]";
				if (!point.is_array() || point.size() != 3)
				{
					return formatError(at + " must be [x_mm, y_mm, z_mm]");
				}
				ServicePoint read;
				for (const auto& [axis, target] : {std::pair<std::size_t, std::int64_t*>{0, &read.x_mm}, {1, &read.y_mm}, {2, &read.z_mm}})
				{
					const Result<std::int64_t> value = readMillimeters(point[axis], at + "[" + std::to_string(axis) + "]");
					if (!value)
					{
						return value.error();
					}
					*target = value.value();
				}
				run.points.push_back(read);
			}
			return run;
		}

		constexpr std::pair<Surface, const char*> SurfaceNames[] = {{Surface::Wall, "walls"}, {Surface::Floor, "floors"},
			{Surface::Ceiling, "ceilings"}, {Surface::Carpentry, "carpentry"}, {Surface::Furniture, "furniture"}};
		constexpr std::pair<ModelUnit, const char*> UnitNames[] = {{ModelUnit::Meter, "m"}, {ModelUnit::Centimeter, "cm"},
			{ModelUnit::Millimeter, "mm"}, {ModelUnit::Inch, "in"}};

		// An integer field within [low, high]; `out` keeps its value when the field is missing.
		Status readInteger(const Json& object, const char* key, const std::string& where, std::int64_t low, std::int64_t high,
			std::int64_t& out)
		{
			const auto field = object.find(key);
			if (field == object.end())
			{
				return Status::success();
			}
			if (!field->is_number_integer() || field->get<std::int64_t>() < low || field->get<std::int64_t>() > high)
			{
				return formatError(where + "." + key + " must be an integer from " + std::to_string(low) + " to " +
					std::to_string(high));
			}
			out = field->get<std::int64_t>();
			return Status::success();
		}

		// Schema 8 (ADR-0017). Paths, ranges and ids are checked by validateProject.
		Result<Asset> readAsset(const Json& object, std::size_t index)
		{
			const std::string where = "assets[" + std::to_string(index) + "]";
			if (!object.is_object())
			{
				return formatError(where + " must be an object");
			}
			const Result<Uuid> uuid = readUuidField(object, "id", where);
			if (!uuid)
			{
				return uuid.error();
			}
			Asset asset;
			asset.id = AssetId{uuid.value()};
			const auto kind = object.find("kind");
			if (kind == object.end() || !kind->is_string() || (*kind != "texture" && *kind != "model"))
			{
				return formatError(where + ".kind must be \"texture\" or \"model\"");
			}
			asset.kind = *kind == "texture" ? AssetKind::Texture : AssetKind::Model;
			for (const auto& [key, target] : {std::pair<const char*, std::string*>{"name", &asset.name}, {"file", &asset.file},
					 {"library", &asset.library}}) // library: schema 9, optional
			{
				if (Status read = readOptionalString(object, key, where, *target); !read)
				{
					return read.error();
				}
			}
			if (asset.kind == AssetKind::Texture)
			{
				const auto texture = object.find("texture");
				if (texture == object.end() || !texture->is_object())
				{
					return formatError(where + ".texture must be an object");
				}
				const std::string at = where + ".texture";
				std::int64_t roughness = asset.texture.roughnessPercent;
				std::int64_t pixelWidth = 0;
				std::int64_t pixelHeight = 0;
				for (const auto& [key, target] : {std::pair<const char*, std::int64_t*>{"widthMm", &asset.texture.widthMm},
						 {"heightMm", &asset.texture.heightMm}})
				{
					const Result<std::int64_t> value = readMillimetersField(*texture, key, at);
					if (!value)
					{
						return value.error();
					}
					*target = value.value();
				}
				using Bounded = std::tuple<const char*, std::int64_t*, std::int64_t>;
				for (const auto& [key, target, high] : {Bounded{"roughnessPercent", &roughness, 100},
						 Bounded{"pixelWidth", &pixelWidth, 1 << 20}, Bounded{"pixelHeight", &pixelHeight, 1 << 20}})
				{
					if (Status read = readInteger(*texture, key, at, 0, high, *target); !read)
					{
						return read.error();
					}
				}
				asset.texture.roughnessPercent = static_cast<int>(roughness);
				asset.texture.pixelWidth = static_cast<int>(pixelWidth);
				asset.texture.pixelHeight = static_cast<int>(pixelHeight);
				const auto surfaces = texture->find("surfaces");
				if (surfaces == texture->end() || !surfaces->is_array())
				{
					return formatError(at + ".surfaces must be an array");
				}
				asset.texture.surfaces = 0;
				for (const Json& name : *surfaces)
				{
					const auto known = std::find_if(std::begin(SurfaceNames), std::end(SurfaceNames),
						[&name](const auto& entry) { return name.is_string() && name == entry.second; });
					if (known == std::end(SurfaceNames))
					{
						return formatError(at + ".surfaces: \"walls\", \"floors\", \"ceilings\", \"carpentry\" or \"furniture\"");
					}
					asset.texture.surfaces |= static_cast<std::uint8_t>(known->first);
				}
				return asset;
			}
			const auto model = object.find("model");
			if (model == object.end() || !model->is_object())
			{
				return formatError(where + ".model must be an object");
			}
			const std::string at = where + ".model";
			const auto unit = model->find("unit");
			const auto knownUnit = std::find_if(std::begin(UnitNames), std::end(UnitNames),
				[&unit, &model](const auto& entry) { return unit != model->end() && unit->is_string() && *unit == entry.second; });
			if (knownUnit == std::end(UnitNames))
			{
				return formatError(at + ".unit must be \"m\", \"cm\", \"mm\" or \"in\"");
			}
			asset.model.unit = knownUnit->first;
			const auto up = model->find("up");
			if (up == model->end() || !up->is_string() || (*up != "y" && *up != "z"))
			{
				return formatError(at + ".up must be \"y\" or \"z\"");
			}
			asset.model.up = *up == "y" ? ModelUpAxis::Y : ModelUpAxis::Z;
			std::int64_t turns = 0;
			if (Status read = readInteger(*model, "frontQuarterTurns", at, 0, 3, turns); !read)
			{
				return read.error();
			}
			asset.model.frontQuarterTurns = static_cast<int>(turns);
			if (const auto against = model->find("againstWall"); against != model->end())
			{
				if (!against->is_boolean())
				{
					return formatError(at + ".againstWall must be true or false");
				}
				asset.model.againstWall = against->get<bool>();
			}
			for (const auto& [key, target] : {std::pair<const char*, std::int64_t*>{"widthMm", &asset.model.widthMm},
					 {"depthMm", &asset.model.depthMm}, {"heightMm", &asset.model.heightMm}})
			{
				const Result<std::int64_t> value = readMillimetersField(*model, key, at);
				if (!value)
				{
					return value.error();
				}
				*target = value.value();
			}
			if (Status read = readInteger(*model, "triangles", at, 0, MaxAbsMillimeters, asset.model.triangles); !read)
			{
				return read.error();
			}
			return asset;
		}

		// `withLevel`: schema 3, where every wall names its level.
		Result<Wall> readWall(const Json& object, std::size_t index, bool withLevel)
		{
			const std::string where = "walls[" + std::to_string(index) + "]";
			if (!object.is_object())
			{
				return formatError(where + " must be an object");
			}
			const auto idField = object.find("id");
			if (idField == object.end() || !idField->is_string())
			{
				return formatError(where + ".id must be a UUID string");
			}
			const Result<Uuid> uuid = parseUuid(idField->get_ref<const std::string&>());
			if (!uuid)
			{
				return formatError(where + ".id: " + uuid.error().message);
			}
			const Result<Point2D> start = readPoint(object, "start", where);
			if (!start)
			{
				return start.error();
			}
			const Result<Point2D> end = readPoint(object, "end", where);
			if (!end)
			{
				return end.error();
			}
			const Result<std::int64_t> thickness = readMillimetersField(object, "thicknessMm", where);
			if (!thickness)
			{
				return thickness.error();
			}
			const Result<std::int64_t> height = readMillimetersField(object, "heightMm", where);
			if (!height)
			{
				return height.error();
			}
			Wall wall{WallId{uuid.value()}, start.value(), end.value(), thickness.value(), height.value()};
			if (withLevel)
			{
				const Result<Uuid> level = readUuidField(object, "level", where);
				if (!level)
				{
					return level.error();
				}
				wall.level = LevelId{level.value()};
			}
			if (const auto openings = object.find("openings"); openings != object.end()) // schema 2
			{
				if (!openings->is_array())
				{
					return formatError(where + ".openings must be an array");
				}
				for (std::size_t item = 0; item < openings->size(); ++item)
				{
					Result<Opening> opening = readOpening((*openings)[item], where + ".openings[" + std::to_string(item) + "]");
					if (!opening)
					{
						return opening.error();
					}
					wall.openings.push_back(std::move(opening).value());
				}
			}
			if (const auto finishes = object.find("finishes"); finishes != object.end()) // schema 4
			{
				if (!finishes->is_object())
				{
					return formatError(where + ".finishes must be an object");
				}
				for (const auto& [key, side] : {std::pair<const char*, WallSide>{"left", WallSide::Left}, {"right", WallSide::Right}})
				{
					if (Status read = readOptionalString(*finishes, key, where + ".finishes", wall.finish(side)); !read)
					{
						return read.error();
					}
				}
			}
			return wall;
		}
	}

	std::string writeProjectJson(const Project& project)
	{
		Json levels = Json::array();
		for (const Level& level : project.levels)
		{
			Json entry = Json::object();
			entry["id"] = toString(level.id);
			if (!level.name.empty())
			{
				entry["name"] = level.name;
			}
			entry["heightMm"] = level.heightMm;
			entry["slabThicknessMm"] = level.slabThicknessMm;
			levels.push_back(std::move(entry));
		}
		Json walls = Json::array();
		for (const Wall& wall : project.walls)
		{
			Json entry = Json::object();
			entry["id"] = toString(wall.id);
			entry["level"] = toString(wall.level);
			entry["start"] = toJson(wall.start);
			entry["end"] = toJson(wall.end);
			entry["thicknessMm"] = wall.thicknessMm;
			entry["heightMm"] = wall.heightMm;
			if (!wall.openings.empty())
			{
				Json openings = Json::array();
				for (const Opening& opening : wall.openings)
				{
					Json item = Json::object();
					item["id"] = toString(opening.id);
					item["kind"] = std::string(kindName(opening.kind));
					item["offsetMm"] = opening.offsetMm;
					item["widthMm"] = opening.widthMm;
					item["heightMm"] = opening.heightMm;
					item["sillMm"] = opening.sillMm;
					item["operation"] = std::string(operationName(opening.operation));
					item["hinge"] = opening.hinge == OpeningEnd::Start ? "start" : "end";
					item["opensTo"] = opening.opensTo == WallSide::Left ? "left" : "right";
					item["openPercent"] = opening.openPercent;
					if (!opening.finish.empty())
					{
						item["finish"] = opening.finish;
					}
					openings.push_back(std::move(item));
				}
				entry["openings"] = std::move(openings);
			}
			if (!wall.finishLeft.empty() || !wall.finishRight.empty())
			{
				Json finishes = Json::object();
				if (!wall.finishLeft.empty())
				{
					finishes["left"] = wall.finishLeft;
				}
				if (!wall.finishRight.empty())
				{
					finishes["right"] = wall.finishRight;
				}
				entry["finishes"] = std::move(finishes);
			}
			walls.push_back(std::move(entry));
		}
		Json root = Json::object();
		root["format"] = std::string(ProjectFormatName);
		root["schemaVersion"] = ProjectSchemaVersion;
		root["units"] = "mm";
		root["levels"] = std::move(levels);
		root["walls"] = std::move(walls);
		if (!project.roomNames.empty())
		{
			Json names = Json::array();
			for (const RoomName& entry : project.roomNames) names.push_back(Json{{"room", entry.room}, {"name", entry.name}});
			root["roomNames"] = std::move(names);
		}
		if (!project.roomFinishes.empty())
		{
			Json rooms = Json::array();
			for (const RoomFinish& finish : project.roomFinishes)
			{
				Json item = Json::object();
				item["room"] = finish.room;
				if (!finish.floor.empty())
				{
					item["floor"] = finish.floor;
				}
				if (!finish.ceiling.empty())
				{
					item["ceiling"] = finish.ceiling;
				}
				rooms.push_back(std::move(item));
			}
			root["roomFinishes"] = std::move(rooms);
		}
		if (!project.furniture.empty())
		{
			Json furniture = Json::array();
			for (const Furniture& piece : project.furniture)
			{
				Json item = Json::object();
				item["id"] = toString(piece.id);
				item["type"] = piece.type;
				item["level"] = toString(piece.level);
				item["position"] = toJson(piece.position);
				item["rotationCentidegrees"] = piece.rotationCentidegrees;
				item["widthMm"] = piece.widthMm;
				item["depthMm"] = piece.depthMm;
				item["heightMm"] = piece.heightMm;
				if (!piece.finish.empty())
				{
					item["finish"] = piece.finish;
				}
				furniture.push_back(std::move(item));
			}
			root["furniture"] = std::move(furniture);
		}
		if (!project.assets.empty())
		{
			Json assets = Json::array();
			for (const Asset& asset : project.assets)
			{
				Json item = Json::object();
				item["id"] = toString(asset.id);
				item["kind"] = asset.kind == AssetKind::Texture ? "texture" : "model";
				item["name"] = asset.name;
				item["file"] = asset.file;
				if (!asset.library.empty())
				{
					item["library"] = asset.library;
				}
				if (asset.kind == AssetKind::Texture)
				{
					Json texture = Json::object();
					texture["widthMm"] = asset.texture.widthMm;
					texture["heightMm"] = asset.texture.heightMm;
					texture["roughnessPercent"] = asset.texture.roughnessPercent;
					Json surfaces = Json::array();
					for (const auto& [surface, name] : SurfaceNames)
					{
						if ((asset.texture.surfaces & static_cast<std::uint8_t>(surface)) != 0)
						{
							surfaces.push_back(name);
						}
					}
					texture["surfaces"] = std::move(surfaces);
					texture["pixelWidth"] = asset.texture.pixelWidth;
					texture["pixelHeight"] = asset.texture.pixelHeight;
					item["texture"] = std::move(texture);
				}
				else
				{
					Json model = Json::object();
					for (const auto& [unit, name] : UnitNames)
					{
						if (unit == asset.model.unit)
						{
							model["unit"] = name;
						}
					}
					model["up"] = asset.model.up == ModelUpAxis::Y ? "y" : "z";
					model["frontQuarterTurns"] = asset.model.frontQuarterTurns;
					model["againstWall"] = asset.model.againstWall;
					model["widthMm"] = asset.model.widthMm;
					model["depthMm"] = asset.model.depthMm;
					model["heightMm"] = asset.model.heightMm;
					model["triangles"] = asset.model.triangles;
					item["model"] = std::move(model);
				}
				assets.push_back(std::move(item));
			}
			root["assets"] = std::move(assets);
		}
		if (!project.columns.empty())
		{
			Json columns = Json::array();
			for (const Column& column : project.columns)
			{
				Json item = Json::object();
				item["id"] = toString(column.id);
				item["level"] = toString(column.level);
				item["position"] = toJson(column.position);
				item["shape"] = std::string(columnShapeId(column.shape));
				item["widthMm"] = column.widthMm;
				item["depthMm"] = column.depthMm;
				item["rotationCentidegrees"] = column.rotationCentidegrees;
				if (!column.finish.empty())
				{
					item["finish"] = column.finish;
				}
				columns.push_back(std::move(item));
			}
			root["columns"] = std::move(columns);
		}
		if (!project.services.empty())
		{
			Json services = Json::array();
			for (const ServiceRun& run : project.services)
			{
				Json item = Json::object();
				item["id"] = toString(run.id);
				item["level"] = toString(run.level);
				item["kind"] = std::string(serviceKindInfo(run.kind).id);
				Json points = Json::array();
				for (const ServicePoint& point : run.points)
				{
					points.push_back(Json::array({point.x_mm, point.y_mm, point.z_mm}));
				}
				item["points"] = std::move(points);
				services.push_back(std::move(item));
			}
			root["services"] = std::move(services);
		}
		return root.dump(2) + "\n";
	}

	Result<Project> readProjectJson(std::string_view json)
	{
		const Json root = Json::parse(json.begin(), json.end(), nullptr, /*allow_exceptions*/ false);
		if (root.is_discarded())
		{
			return formatError("not valid JSON");
		}
		if (!root.is_object())
		{
			return formatError("the root must be an object");
		}
		const auto format = root.find("format");
		if (format == root.end() || !format->is_string() || format->get_ref<const std::string&>() != ProjectFormatName)
		{
			return formatError("not a 101st Floor project (missing \"format\": \"101st-floor-project\")");
		}
		const auto version = root.find("schemaVersion");
		if (version == root.end() || !version->is_number_integer())
		{
			return formatError("missing integer schemaVersion");
		}
		const std::int64_t schemaVersion = version->get<std::int64_t>();
		if (schemaVersion > ProjectSchemaVersion)
		{
			return Error{ErrorCode::InvalidArgument, "Project schemaVersion " + std::to_string(schemaVersion) +
				" is newer than this version of 101st Floor supports (" + std::to_string(ProjectSchemaVersion) + ")"};
		}
		if (schemaVersion < 1)
		{
			return formatError("schemaVersion must be at least 1");
		}
		const auto units = root.find("units");
		if (units == root.end() || !units->is_string() || units->get_ref<const std::string&>() != "mm")
		{
			return formatError("units must be \"mm\"");
		}
		const auto walls = root.find("walls");
		if (walls == root.end() || !walls->is_array())
		{
			return formatError("walls must be an array");
		}

		const bool withLevels = schemaVersion >= 3;
		Project project;
		if (withLevels)
		{
			const auto levels = root.find("levels");
			if (levels == root.end() || !levels->is_array())
			{
				return formatError("levels must be an array");
			}
			for (std::size_t index = 0; index < levels->size(); ++index)
			{
				Result<Level> level = readLevel((*levels)[index], index);
				if (!level)
				{
					return level.error();
				}
				project.levels.push_back(std::move(level).value());
			}
		}
		project.walls.reserve(walls->size());
		for (std::size_t index = 0; index < walls->size(); ++index)
		{
			Result<Wall> wall = readWall((*walls)[index], index, withLevels);
			if (!wall)
			{
				return wall.error();
			}
			project.walls.push_back(std::move(wall).value());
		}
		if (const auto rooms = root.find("roomFinishes"); rooms != root.end()) // schema 4
		{
			if (!rooms->is_array())
			{
				return formatError("roomFinishes must be an array");
			}
			for (std::size_t index = 0; index < rooms->size(); ++index)
			{
				Result<RoomFinish> finish = readRoomFinish((*rooms)[index], index);
				if (!finish)
				{
					return finish.error();
				}
				project.roomFinishes.push_back(std::move(finish).value());
			}
		}
		if (const auto names = root.find("roomNames"); names != root.end()) // schema 11
		{
			if (!names->is_array()) return formatError("roomNames must be an array");
			for (const Json& item : *names)
			{
				if (!item.is_object() || !item.contains("room") || !item["room"].is_string() || !item.contains("name") || !item["name"].is_string())
				{
					return formatError("roomNames entries need room and name strings");
				}
				project.roomNames.push_back({item["room"].get<std::string>(), item["name"].get<std::string>()});
			}
		}
		if (const auto assets = root.find("assets"); assets != root.end()) // schema 8
		{
			if (!assets->is_array())
			{
				return formatError("assets must be an array");
			}
			for (std::size_t index = 0; index < assets->size(); ++index)
			{
				Result<Asset> asset = readAsset((*assets)[index], index);
				if (!asset)
				{
					return asset.error();
				}
				project.assets.push_back(std::move(asset).value());
			}
		}
		if (const auto furniture = root.find("furniture"); furniture != root.end()) // schema 7
		{
			if (!furniture->is_array())
			{
				return formatError("furniture must be an array");
			}
			for (std::size_t index = 0; index < furniture->size(); ++index)
			{
				Result<Furniture> piece = readFurniture((*furniture)[index], index);
				if (!piece)
				{
					return piece.error();
				}
				project.furniture.push_back(std::move(piece).value());
			}
		}
		for (const auto& [key, isColumns] : {std::pair<const char*, bool>{"columns", true}, {"services", false}}) // schema 10
		{
			const auto list = root.find(key);
			if (list == root.end())
			{
				continue;
			}
			if (!list->is_array())
			{
				return formatError(std::string(key) + " must be an array");
			}
			for (std::size_t index = 0; index < list->size(); ++index)
			{
				if (isColumns)
				{
					Result<Column> column = readColumn((*list)[index], index);
					if (!column)
					{
						return column.error();
					}
					project.columns.push_back(std::move(column).value());
				}
				else
				{
					Result<ServiceRun> run = readServiceRun((*list)[index], index);
					if (!run)
					{
						return run.error();
					}
					project.services.push_back(std::move(run).value());
				}
			}
		}
		if (!withLevels)
		{
			// One level as tall as the tallest wall (at least 2 mm, so a slab fits), holding every wall.
			Level level{LevelId{LegacyLevelUuid}};
			level.heightMm = 2;
			for (const Wall& wall : project.walls)
			{
				level.heightMm = std::max(level.heightMm, wall.heightMm);
			}
			if (project.walls.empty())
			{
				level.heightMm = DefaultLevelHeightMm;
			}
			level.slabThicknessMm = std::min(DefaultSlabThicknessMm, level.heightMm / 2);
			project.levels.push_back(level);
			for (Wall& wall : project.walls)
			{
				wall.level = level.id;
			}
		}
		if (Status valid = validateProject(project); !valid)
		{
			return Error{valid.error().code, "Invalid project file: " + valid.error().message};
		}
		return project;
	}

	Status saveProjectFile(const std::filesystem::path& path, const Project& project)
	{
		std::error_code error;
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path(), error);
			if (error)
			{
				return Error{ErrorCode::InvalidArgument, "Cannot create folder " + displayPath(path.parent_path()) + ": " + error.message()};
			}
		}
		std::filesystem::path temporary = path;
		temporary += ".tmp";
		{
			std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
			if (!out)
			{
				return Error{ErrorCode::InvalidArgument, "Cannot write " + displayPath(temporary)};
			}
			const std::string json = writeProjectJson(project);
			out.write(json.data(), static_cast<std::streamsize>(json.size()));
			if (!out)
			{
				return Error{ErrorCode::InvalidArgument, "Failed while writing " + displayPath(temporary)};
			}
		}
		std::filesystem::rename(temporary, path, error);
		if (error)
		{
			std::filesystem::remove(temporary, error);
			return Error{ErrorCode::InvalidArgument, "Cannot replace " + displayPath(path)};
		}
		return Status::success();
	}

	Result<Project> loadProjectFile(const std::filesystem::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in)
		{
			return Error{ErrorCode::NotFound, "Cannot open " + displayPath(path)};
		}
		const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return readProjectJson(json);
	}
}
