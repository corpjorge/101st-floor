#include "doctest.h"

#include "F101/Cad/ProjectFile.h"
#include "F101/Cad/Units.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace f101;
using namespace f101::cad;

namespace
{
	const LevelId Ground{Uuid{0, 100}};
	const LevelId Upper{Uuid{0, 101}};

	// Two levels, a wall on each.
	Project sampleProject()
	{
		Project project;
		project.levels.push_back(Level{Ground, "Planta baja \xc3\xb1", 2800, 250});
		project.levels.push_back(Level{Upper, "", 3000, 300});
		Wall first{WallId{Uuid{0x0123'4567'89ab'cdefull, 0x8edc'ba98'7654'3210ull}}, {0, 0}, {3550, 0}, 120, 2400};
		first.level = Ground;
		Wall second{WallId{Uuid{0, 2}}, {-999'999'999, 5}, {999'999'999, -7}, 1, MaxAbsMillimeters};
		second.level = Upper;
		project.walls = {first, second};
		return project;
	}

	Project emptyProject()
	{
		Project project;
		project.levels.push_back(Level{Ground});
		return project;
	}

	ErrorCode readError(const std::string& json)
	{
		const Result<Project> result = readProjectJson(json);
		REQUIRE_MESSAGE(!result.ok(), "unexpectedly parsed: " << json);
		return result.error().code;
	}

	std::string minimalJson(const std::string& wallsJson)
	{
		return R"({"format":"101st-floor-project","schemaVersion":1,"units":"mm","walls":)" + wallsJson + "}";
	}
}

TEST_CASE("writes the documented, deterministic format")
{
	Project project = emptyProject();
	project.levels.front().name = "Planta baja";
	Wall wall{WallId{Uuid{0, 1}}, {0, 0}, {3550, 0}, 120, 2400};
	wall.level = Ground;
	project.walls.push_back(wall);
	CHECK(writeProjectJson(project) ==
		"{\n"
		"  \"format\": \"101st-floor-project\",\n"
		"  \"schemaVersion\": 8,\n"
		"  \"units\": \"mm\",\n"
		"  \"levels\": [\n"
		"    {\n"
		"      \"id\": \"00000000-0000-0000-0000-000000000064\",\n"
		"      \"name\": \"Planta baja\",\n"
		"      \"heightMm\": 2800,\n"
		"      \"slabThicknessMm\": 250\n"
		"    }\n"
		"  ],\n"
		"  \"walls\": [\n"
		"    {\n"
		"      \"id\": \"00000000-0000-0000-0000-000000000001\",\n"
		"      \"level\": \"00000000-0000-0000-0000-000000000064\",\n"
		"      \"start\": [\n        0,\n        0\n      ],\n"
		"      \"end\": [\n        3550,\n        0\n      ],\n"
		"      \"thicknessMm\": 120,\n"
		"      \"heightMm\": 2400\n"
		"    }\n"
		"  ]\n"
		"}\n");
}

TEST_CASE("round trip keeps every millimetre and id exactly")
{
	const Project original = sampleProject();
	const Result<Project> loaded = readProjectJson(writeProjectJson(original));
	REQUIRE(loaded.ok());
	CHECK(loaded.value().levels == original.levels);
	REQUIRE(loaded.value().walls.size() == original.walls.size());
	for (std::size_t i = 0; i < original.walls.size(); ++i)
	{
		CHECK(loaded.value().walls[i] == original.walls[i]);
	}
	CHECK(writeProjectJson(loaded.value()) == writeProjectJson(original));
}

TEST_CASE("a project with one level and no walls is valid; one without levels is not")
{
	const Result<Project> loaded = readProjectJson(writeProjectJson(emptyProject()));
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls.empty());
	CHECK(loaded.value().levels.size() == 1);
	CHECK(readError(writeProjectJson(Project{})) == ErrorCode::InvalidArgument);
}

TEST_CASE("unknown fields are ignored")
{
	const std::string json = R"({"format":"101st-floor-project","schemaVersion":1,"units":"mm","comment":"x",)"
		R"("walls":[{"id":"00000000-0000-0000-0000-000000000001","start":[0,0],"end":[10,0],"thicknessMm":1,"heightMm":1,"tag":7}]})";
	CHECK(readProjectJson(json).ok());
}

TEST_CASE("rejects files that are not valid 101st Floor projects")
{
	CHECK(readError("") == ErrorCode::ParseError);
	CHECK(readError("{not json") == ErrorCode::ParseError);
	CHECK(readError("[]") == ErrorCode::ParseError);
	CHECK(readError(R"({"format":"other","schemaVersion":1,"units":"mm","walls":[]})") == ErrorCode::ParseError);
	CHECK(readError(R"({"format":"101st-floor-project","units":"mm","walls":[]})") == ErrorCode::ParseError);
	CHECK(readError(R"({"format":"101st-floor-project","schemaVersion":1,"units":"cm","walls":[]})") == ErrorCode::ParseError);
	CHECK(readError(R"({"format":"101st-floor-project","schemaVersion":1,"units":"mm","walls":{}})") == ErrorCode::ParseError);
}

TEST_CASE("a project from a newer schema version is refused, not misread")
{
	CHECK(readError(R"({"format":"101st-floor-project","schemaVersion":)" + std::to_string(ProjectSchemaVersion + 1) +
		R"(,"units":"mm","walls":[]})") == ErrorCode::InvalidArgument);
}

TEST_CASE("millimetres must be integers inside the model range")
{
	const std::string id = R"("id":"00000000-0000-0000-0000-000000000001")";
	CHECK(readError(minimalJson("[{" + id + R"(,"start":[0.5,0],"end":[10,0],"thicknessMm":1,"heightMm":1}])")) == ErrorCode::ParseError);
	CHECK(readError(minimalJson("[{" + id + R"(,"start":["0",0],"end":[10,0],"thicknessMm":1,"heightMm":1}])")) == ErrorCode::ParseError);
	CHECK(readError(minimalJson("[{" + id + R"(,"start":[0],"end":[10,0],"thicknessMm":1,"heightMm":1}])")) == ErrorCode::ParseError);
	CHECK(readError(minimalJson("[{" + id + R"(,"start":[0,0],"end":[1000000001,0],"thicknessMm":1,"heightMm":1}])")) == ErrorCode::OutOfRange);
	CHECK(readError(minimalJson("[{" + id + R"(,"start":[0,0],"end":[10,0],"heightMm":1}])")) == ErrorCode::ParseError);
}

TEST_CASE("walls must be valid and uniquely identified")
{
	CHECK(readError(minimalJson(R"([{"id":"nope","start":[0,0],"end":[10,0],"thicknessMm":1,"heightMm":1}])")) == ErrorCode::ParseError);
	const std::string wall = R"({"id":"00000000-0000-0000-0000-000000000001","start":[0,0],"end":[10,0],"thicknessMm":1,"heightMm":1})";
	CHECK(readError(minimalJson("[" + wall + "," + wall + "]")) == ErrorCode::InvalidArgument);
	CHECK(readError(minimalJson(R"([{"id":"00000000-0000-0000-0000-000000000001","start":[0,0],"end":[0,0],"thicknessMm":1,"heightMm":1}])")) == ErrorCode::InvalidArgument);
	CHECK(readError(minimalJson(R"([{"id":"00000000-0000-0000-0000-000000000001","start":[0,0],"end":[10,0],"thicknessMm":0,"heightMm":1}])")) == ErrorCode::OutOfRange);
}

TEST_CASE("save and load a file, including a non-ASCII path")
{
	const std::filesystem::path folder = std::filesystem::temp_directory_path() / u8"101st-floor-tests" / u8"proyecto año ñandú";
	const std::filesystem::path file = folder / u8"project.json";
	std::error_code ignored; // non-throwing overloads: exceptions are disabled
	std::filesystem::remove_all(folder.parent_path(), ignored);

	REQUIRE(saveProjectFile(file, sampleProject()).ok());
	CHECK(std::filesystem::exists(file));
	CHECK_FALSE(std::filesystem::exists(std::filesystem::path(file).concat(".tmp")));

	const Result<Project> loaded = loadProjectFile(file);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls.size() == 2);

	// Saving again replaces the file.
	REQUIRE(saveProjectFile(file, emptyProject()).ok());
	CHECK(loadProjectFile(file).value().walls.empty());

	CHECK(loadProjectFile(folder / u8"missing.json").error().code == ErrorCode::NotFound);
	std::filesystem::remove_all(folder.parent_path(), ignored);
}

TEST_CASE("openings are written and read back exactly")
{
	Project project = emptyProject();
	Wall wall{WallId{Uuid{0, 1}}, {0, 0}, {4000, 0}, 120, 2400};
	wall.level = Ground;
	wall.openings.push_back(Opening{OpeningId{Uuid{0, 7}}, OpeningKind::Door, 300, 900, 2100, 0});
	wall.openings.push_back(Opening{OpeningId{Uuid{0, 8}}, OpeningKind::Window, 2000, 1200, 1200, 900});
	project.walls.push_back(wall);
	const std::string json = writeProjectJson(project);
	CHECK(json.find("\"kind\": \"door\"") != std::string::npos);
	CHECK(json.find("\"sillMm\": 900") != std::string::npos);
	const Result<Project> loaded = readProjectJson(json);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls == project.walls);

	// A wall without openings writes no "openings" key.
	CHECK(writeProjectJson(sampleProject()).find("openings") == std::string::npos);
}

TEST_CASE("files before schema 3 load onto one level as tall as their tallest wall")
{
	const std::string walls = R"([{"id":"00000000-0000-0000-0000-000000000001","start":[0,0],"end":[3550,0],"thicknessMm":120,"heightMm":2400},)"
		R"({"id":"00000000-0000-0000-0000-000000000002","start":[0,0],"end":[0,3000],"thicknessMm":120,"heightMm":2300,)"
		R"("openings":[{"id":"00000000-0000-0000-0000-000000000003","kind":"door","offsetMm":300,"widthMm":900,"heightMm":2100,"sillMm":0}]}])";
	for (const int version : {1, 2})
	{
		const Result<Project> loaded = readProjectJson(R"({"format":"101st-floor-project","schemaVersion":)" +
			std::to_string(version) + R"(,"units":"mm","walls":)" + walls + "}");
		REQUIRE(loaded.ok());
		REQUIRE(loaded.value().levels.size() == 1);
		const Level& level = loaded.value().levels.front();
		CHECK(level.id == LevelId{LegacyLevelUuid});
		CHECK(toString(level.id) == "f1010000-0000-4000-8000-000000000001");
		CHECK(level.heightMm == 2400);
		CHECK(level.slabThicknessMm == DefaultSlabThicknessMm);
		CHECK(level.name.empty());
		for (const Wall& wall : loaded.value().walls)
		{
			CHECK(wall.level == level.id);
		}
		// Saved again, it is a current schema file with that level.
		CHECK(writeProjectJson(loaded.value()).find("\"schemaVersion\": " + std::to_string(ProjectSchemaVersion)) != std::string::npos);
	}
	const Result<Project> empty = readProjectJson(minimalJson("[]"));
	REQUIRE(empty.ok());
	CHECK(empty.value().levels.front().heightMm == DefaultLevelHeightMm);
}

TEST_CASE("schema 3 needs valid levels and every wall on one of them")
{
	const std::string head = R"({"format":"101st-floor-project","schemaVersion":3,"units":"mm",)";
	const std::string level = R"({"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250})";
	const std::string wall = R"({"id":"00000000-0000-0000-0000-000000000001","start":[0,0],"end":[10,0],"thicknessMm":1,"heightMm":1)";
	const std::string onGround = R"(,"level":"00000000-0000-0000-0000-000000000064"})";
	CHECK(readProjectJson(head + R"("levels":[)" + level + R"(],"walls":[)" + wall + onGround + "]}").ok());
	CHECK(readError(head + R"("walls":[]})") == ErrorCode::ParseError);                               // no levels key
	CHECK(readError(head + R"("levels":[],"walls":[]})") == ErrorCode::InvalidArgument);              // no level
	CHECK(readError(head + R"("levels":[)" + level + R"(],"walls":[)" + wall + "}]}") == ErrorCode::ParseError); // wall without level
	CHECK(readError(head + R"("levels":[)" + level + R"(],"walls":[)" + wall +
		R"(,"level":"00000000-0000-0000-0000-000000000065"}]})") == ErrorCode::InvalidArgument);          // unknown level
	CHECK(readError(head + R"("levels":[)" + level + "," + level + R"(],"walls":[]})") == ErrorCode::InvalidArgument); // duplicate
	CHECK(readError(head + R"("levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":2800}],"walls":[]})") ==
		ErrorCode::OutOfRange); // slab as thick as the level
	CHECK(readError(head + R"("levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800.5,"slabThicknessMm":250}],"walls":[]})") ==
		ErrorCode::ParseError);
	CHECK(readError(head + R"("levels":[{"id":"00000000-0000-0000-0000-000000000064","name":7,"heightMm":2800,"slabThicknessMm":250}],"walls":[]})") ==
		ErrorCode::ParseError);
}

TEST_CASE("invalid openings are refused")
{
	const std::string prefix = R"([{"id":"00000000-0000-0000-0000-000000000001","start":[0,0],"end":[3000,0],"thicknessMm":120,"heightMm":2400,"openings":[)";
	auto opening = [](const std::string& fields) { return R"({"id":"00000000-0000-0000-0000-000000000009",)" + fields + "}"; };
	CHECK(readError(minimalJson(prefix + opening(R"("kind":"hatch","offsetMm":0,"widthMm":900,"heightMm":2100,"sillMm":0)") + "]}]")) == ErrorCode::ParseError);
	CHECK(readError(minimalJson(prefix + opening(R"("kind":"door","offsetMm":2500,"widthMm":900,"heightMm":2100,"sillMm":0)") + "]}]")) == ErrorCode::OutOfRange); // past the end
	CHECK(readError(minimalJson(prefix + opening(R"("kind":"door","offsetMm":0,"widthMm":900,"heightMm":2400,"sillMm":0)") + "]}]")) == ErrorCode::OutOfRange);  // no lintel
	CHECK(readError(minimalJson(prefix + opening(R"("kind":"door","offsetMm":0,"widthMm":900,"heightMm":2100,"sillMm":100)") + "]}]")) == ErrorCode::OutOfRange); // door off the floor
	CHECK(readError(minimalJson(prefix + opening(R"("kind":"window","offsetMm":0,"widthMm":900.5,"heightMm":1000,"sillMm":900)") + "]}]")) == ErrorCode::ParseError);
}
