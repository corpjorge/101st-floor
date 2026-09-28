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
	inline constexpr int ProjectSchemaVersion = 1;

	// Deterministic, human-readable JSON (fixed key order, 2-space indentation, trailing newline).
	FLOOR101CADCORE_API std::string writeProjectJson(const Project& project);

	// Parses and validates a project. Rejects malformed JSON, other formats, newer schema versions, non-integer
	// millimetres, invalid ids, duplicate ids and invalid walls. Unknown fields are ignored.
	FLOOR101CADCORE_API Result<Project> readProjectJson(std::string_view json);

	// Writes through a temporary file and a rename, so a failed save never truncates an existing project.
	FLOOR101CADCORE_API Status saveProjectFile(const std::filesystem::path& path, const Project& project);
	FLOOR101CADCORE_API Result<Project> loadProjectFile(const std::filesystem::path& path);
}
