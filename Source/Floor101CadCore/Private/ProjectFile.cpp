#include "F101/Cad/ProjectFile.h"

#include "F101/Cad/Document.h"
#include "F101/Cad/Units.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

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
					openings.push_back(std::move(item));
				}
				entry["openings"] = std::move(openings);
			}
			walls.push_back(std::move(entry));
		}
		Json root = Json::object();
		root["format"] = std::string(ProjectFormatName);
		root["schemaVersion"] = ProjectSchemaVersion;
		root["units"] = "mm";
		root["levels"] = std::move(levels);
		root["walls"] = std::move(walls);
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
