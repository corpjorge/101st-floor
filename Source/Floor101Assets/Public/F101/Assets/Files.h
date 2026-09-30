#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#ifndef FLOOR101ASSETS_API
	#define FLOOR101ASSETS_API
#endif

// File helpers of the asset importers (ADR-0017). Paths cross this boundary as UTF-8 text: on Windows a std::string path
// would be read in the ANSI code page and lose characters such as "ñ".
namespace f101::assets
{
	FLOOR101ASSETS_API std::filesystem::path pathFromUtf8(std::string_view text);
	FLOOR101ASSETS_API std::string utf8Of(const std::filesystem::path& path);

	// The whole file; an error if it cannot be read or is larger than `maxBytes`.
	FLOOR101ASSETS_API Result<std::vector<std::uint8_t>> readFileBytes(const std::filesystem::path& path,
		std::uintmax_t maxBytes = 512ull << 20);

	// A path a model file names (a texture, a buffer, a material library): backslashes read as '/', "%20" and other
	// percent escapes decoded when `percentEncoded` (glTF URIs), "./" dropped.
	FLOOR101ASSETS_API std::string normalizeReference(std::string_view reference, bool percentEncoded);

	// `reference` (normalized) as a path inside `folder` if it stays inside it; else the file of that name directly in
	// `folder` (a reference to an absolute path or outside its folder, found next to the model); else, with
	// `allowOutside` (importing), the absolute path or the path outside the folder itself. Empty if none exists.
	FLOOR101ASSETS_API std::filesystem::path resolveReference(const std::filesystem::path& folder, std::string_view reference,
		bool allowOutside = false);

	// `reference` as a relative path that stays inside its folder ("textures/wood.png"); empty if it is absolute or
	// leaves the folder.
	FLOOR101ASSETS_API std::string insideReference(std::string_view reference);
}
