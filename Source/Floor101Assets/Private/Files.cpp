#include "F101/Assets/Files.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace f101::assets
{
	std::filesystem::path pathFromUtf8(std::string_view text)
	{
		return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
	}

	std::string utf8Of(const std::filesystem::path& path)
	{
		const std::u8string text = path.u8string();
		return std::string(text.begin(), text.end());
	}

	Result<std::vector<std::uint8_t>> readFileBytes(const std::filesystem::path& path, std::uintmax_t maxBytes)
	{
		std::error_code error;
		const std::uintmax_t size = std::filesystem::file_size(path, error);
		if (error)
		{
			return Error{ErrorCode::NotFound, "Cannot open " + utf8Of(path)};
		}
		if (size > maxBytes)
		{
			return Error{ErrorCode::OutOfRange, utf8Of(path) + " is too large (" + std::to_string(size >> 20) + " MB)"};
		}
		std::ifstream in(path, std::ios::binary);
		if (!in)
		{
			return Error{ErrorCode::NotFound, "Cannot open " + utf8Of(path)};
		}
		std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
		in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		if (!in)
		{
			return Error{ErrorCode::InvalidArgument, "Failed while reading " + utf8Of(path)};
		}
		return bytes;
	}

	std::string normalizeReference(std::string_view reference, bool percentEncoded)
	{
		std::string result;
		for (std::size_t i = 0; i < reference.size(); ++i)
		{
			const char c = reference[i];
			auto hex = [](char digit) -> int
			{
				if (digit >= '0' && digit <= '9') { return digit - '0'; }
				if (digit >= 'a' && digit <= 'f') { return digit - 'a' + 10; }
				if (digit >= 'A' && digit <= 'F') { return digit - 'A' + 10; }
				return -1;
			};
			if (percentEncoded && c == '%' && i + 2 < reference.size() && hex(reference[i + 1]) >= 0 && hex(reference[i + 2]) >= 0)
			{
				result += static_cast<char>(hex(reference[i + 1]) * 16 + hex(reference[i + 2]));
				i += 2;
				continue;
			}
			result += c == '\\' ? '/' : c;
		}
		while (result.rfind("./", 0) == 0)
		{
			result.erase(0, 2);
		}
		return result;
	}

	std::string insideReference(std::string_view reference)
	{
		if (reference.empty() || reference.front() == '/' || reference.find(':') != std::string_view::npos)
		{
			return {};
		}
		std::string result;
		std::size_t start = 0;
		while (start <= reference.size())
		{
			const std::size_t end = reference.find('/', start);
			const std::string_view part = reference.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
			if (part == "..")
			{
				return {};
			}
			if (!part.empty() && part != ".")
			{
				result += (result.empty() ? "" : "/") + std::string(part);
			}
			if (end == std::string_view::npos)
			{
				break;
			}
			start = end + 1;
		}
		return result;
	}

	std::filesystem::path resolveReference(const std::filesystem::path& folder, std::string_view reference, bool allowOutside)
	{
		std::error_code error;
		if (const std::string inside = insideReference(reference); !inside.empty())
		{
			const std::filesystem::path candidate = folder / pathFromUtf8(inside);
			if (std::filesystem::is_regular_file(candidate, error))
			{
				return candidate;
			}
		}
		const std::size_t slash = reference.find_last_of('/');
		const std::string_view name = slash == std::string_view::npos ? reference : reference.substr(slash + 1);
		if (!name.empty() && name != "." && name != "..")
		{
			const std::filesystem::path candidate = folder / pathFromUtf8(name);
			if (std::filesystem::is_regular_file(candidate, error))
			{
				return candidate;
			}
		}
		if (allowOutside && !reference.empty())
		{
			const std::filesystem::path given = pathFromUtf8(reference);
			const std::filesystem::path candidate = given.is_absolute() ? given : folder / given;
			if (std::filesystem::is_regular_file(candidate, error))
			{
				return candidate.lexically_normal();
			}
		}
		return {};
	}
}
