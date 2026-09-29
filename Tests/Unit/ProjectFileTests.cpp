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
	Project sampleProject()
	{
		Project project;
		project.walls.push_back(Wall{WallId{Uuid{0x0123'4567'89ab'cdefull, 0x8edc'ba98'7654'3210ull}}, {0, 0}, {3550, 0}, 120, 2400});
		project.walls.push_back(Wall{WallId{Uuid{0, 2}}, {-999'999'999, 5}, {999'999'999, -7}, 1, MaxAbsMillimeters});
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
	Project project;
	project.walls.push_back(Wall{WallId{Uuid{0, 1}}, {0, 0}, {3550, 0}, 120, 2400});
	CHECK(writeProjectJson(project) ==
		"{\n"
		"  \"format\": \"101st-floor-project\",\n"
		"  \"schemaVersion\": 2,\n"
		"  \"units\": \"mm\",\n"
		"  \"walls\": [\n"
		"    {\n"
		"      \"id\": \"00000000-0000-0000-0000-000000000001\",\n"
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
	REQUIRE(loaded.value().walls.size() == original.walls.size());
	for (std::size_t i = 0; i < original.walls.size(); ++i)
	{
		CHECK(loaded.value().walls[i] == original.walls[i]);
	}
	CHECK(writeProjectJson(loaded.value()) == writeProjectJson(original));
}

TEST_CASE("an empty project is valid")
{
	const Result<Project> loaded = readProjectJson(writeProjectJson(Project{}));
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls.empty());
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
	CHECK(readError(R"({"format":"101st-floor-project","schemaVersion":3,"units":"mm","walls":[]})") == ErrorCode::InvalidArgument);
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
	REQUIRE(saveProjectFile(file, Project{}).ok());
	CHECK(loadProjectFile(file).value().walls.empty());

	CHECK(loadProjectFile(folder / u8"missing.json").error().code == ErrorCode::NotFound);
	std::filesystem::remove_all(folder.parent_path(), ignored);
}

TEST_CASE("schema 2 writes openings and reads them back exactly; schema 1 files still load")
{
	Project project;
	Wall wall{WallId{Uuid{0, 1}}, {0, 0}, {4000, 0}, 120, 2400};
	wall.openings.push_back(Opening{OpeningId{Uuid{0, 7}}, OpeningKind::Door, 300, 900, 2100, 0});
	wall.openings.push_back(Opening{OpeningId{Uuid{0, 8}}, OpeningKind::Window, 2000, 1200, 1200, 900});
	project.walls.push_back(wall);
	const std::string json = writeProjectJson(project);
	CHECK(json.find("\"kind\": \"door\"") != std::string::npos);
	CHECK(json.find("\"sillMm\": 900") != std::string::npos);
	const Result<Project> loaded = readProjectJson(json);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().walls == project.walls);

	// A wall without openings writes no "openings" key; version 1 input (no openings anywhere) is accepted.
	CHECK(writeProjectJson(sampleProject()).find("openings") == std::string::npos);
	CHECK(readProjectJson(minimalJson(R"([{"id":"00000000-0000-0000-0000-000000000001","start":[0,0],"end":[3550,0],"thicknessMm":120,"heightMm":2400}])")).ok());
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
