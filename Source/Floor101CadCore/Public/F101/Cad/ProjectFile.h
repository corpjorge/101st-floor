#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Project.h"
#include "F101/Cad/Result.h"

#include <filesystem>
#include <string>
#include <string_view>

// project.json: the persistent, versionable project format (ADR-0007). Integer millimetres, UUID text ids.
namespace f101::cad
{
	inline constexpr std::string_view ProjectFormatName = "101st-floor-project";
	// 1: walls. 2: walls with openings (doors, windows). 3: levels, every wall on one (ADR-0011). 4: finishes of wall
	// faces and rooms (ADR-0012). 5: finishes of door and window carpentry (ADR-0013).
	inline constexpr int ProjectSchemaVersion = 5;

	// Files before schema 3 have no levels: their walls are read onto one level with this id
	// (f1010000-0000-4000-8000-000000000001), as tall as the tallest wall so the derived slabs stay inside the walls.
	inline constexpr Uuid LegacyLevelUuid{0xf101'0000'0000'4000ull, 0x8000'0000'0000'0001ull};

	// Deterministic, human-readable JSON (fixed key order, 2-space indentation, trailing newline).
	FLOOR101CADCORE_API std::string writeProjectJson(const Project& project);

	// Parses and validates a project. Rejects malformed JSON, other formats, newer schema versions, non-integer
	// millimetres, invalid ids, duplicate ids and invalid walls. Unknown fields are ignored.
	FLOOR101CADCORE_API Result<Project> readProjectJson(std::string_view json);

	// Writes through a temporary file and a rename, so a failed save never truncates an existing project.
	FLOOR101CADCORE_API Status saveProjectFile(const std::filesystem::path& path, const Project& project);
	FLOOR101CADCORE_API Result<Project> loadProjectFile(const std::filesystem::path& path);
}
