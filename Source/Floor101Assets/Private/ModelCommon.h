#pragma once

#include "F101/Assets/ModelReader.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string_view>
#include <system_error>
#include <vector>

// Helpers shared by the model readers. Not part of the module's API.
namespace f101::assets::detail
{
	inline std::uint8_t linearToSrgb(double linear)
	{
		const double c = std::clamp(linear, 0.0, 1.0);
		const double srgb = c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
		return static_cast<std::uint8_t>(std::lround(std::clamp(srgb, 0.0, 1.0) * 255.0));
	}

	inline cad::Rgb8 linearToSrgb(double r, double g, double b)
	{
		return cad::Rgb8{linearToSrgb(r), linearToSrgb(g), linearToSrgb(b)};
	}

	inline geometry::Vec3 normalized(const geometry::Vec3& v)
	{
		const double length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return length > 1e-20 ? geometry::Vec3{v.x / length, v.y / length, v.z / length} : geometry::Vec3{0.0, 0.0, 1.0};
	}

	// Unit normal of a counter-clockwise triangle.
	inline geometry::Vec3 faceNormal(const geometry::Vec3& a, const geometry::Vec3& b, const geometry::Vec3& c)
	{
		const geometry::Vec3 u{b.x - a.x, b.y - a.y, b.z - a.z};
		const geometry::Vec3 v{c.x - a.x, c.y - a.y, c.z - a.z};
		return normalized(geometry::Vec3{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x});
	}

	inline std::vector<std::string_view> splitWhitespace(std::string_view text)
	{
		std::vector<std::string_view> parts;
		std::size_t at = 0;
		while (at < text.size())
		{
			while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r'))
			{
				++at;
			}
			const std::size_t start = at;
			while (at < text.size() && text[at] != ' ' && text[at] != '\t' && text[at] != '\r')
			{
				++at;
			}
			if (at > start)
			{
				parts.push_back(text.substr(start, at - start));
			}
		}
		return parts;
	}

	inline bool toDouble(std::string_view text, double& out)
	{
		if (!text.empty() && text.front() == '+')
		{
			text.remove_prefix(1);
		}
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
		return error == std::errc() && end == text.data() + text.size() && std::isfinite(out);
	}

	inline bool toInteger(std::string_view text, long long& out)
	{
		if (!text.empty() && text.front() == '+')
		{
			text.remove_prefix(1);
		}
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
		return error == std::errc() && end == text.data() + text.size();
	}
}
