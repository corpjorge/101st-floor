#include "F101/Cad/Units.h"

#include <cmath>
#include <string>

namespace f101::cad
{
	namespace
	{
		// 15 significant digits: mantissa (< 10^15) * 1000 mm/m stays far below the uint64 limit, and so does the
		// rounding step (numerator + 10^15 / 2).
		constexpr int MaxSignificantDigits = 15;

		Error parseError(std::string message)
		{
			return Error{ErrorCode::ParseError, std::move(message)};
		}

		bool isBlank(char c)
		{
			return c == ' ' || c == '\t';
		}

		char toLowerAscii(char c)
		{
			return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
		}

		std::string_view trim(std::string_view text)
		{
			while (!text.empty() && isBlank(text.front()))
			{
				text.remove_prefix(1);
			}
			while (!text.empty() && isBlank(text.back()))
			{
				text.remove_suffix(1);
			}
			return text;
		}
	}

	Result<std::int64_t> parseLength(std::string_view text, LengthUnit defaultUnit)
	{
		const std::string_view input = trim(text);
		if (input.empty())
		{
			return parseError("Empty length");
		}

		std::size_t pos = 0;
		bool negative = false;
		if (input[pos] == '+' || input[pos] == '-')
		{
			negative = input[pos] == '-';
			++pos;
		}

		// The number is read as mantissa / 10^fractionDigits, exactly.
		std::uint64_t mantissa = 0;
		int significantDigits = 0;
		int fractionDigits = 0;
		bool sawDigit = false;
		bool sawSeparator = false;
		for (; pos < input.size(); ++pos)
		{
			const char c = input[pos];
			if (c >= '0' && c <= '9')
			{
				sawDigit = true;
				if (sawSeparator)
				{
					++fractionDigits;
				}
				else if (mantissa == 0 && c == '0')
				{
					continue; // leading zeros of the integer part carry no information
				}
				if (++significantDigits > MaxSignificantDigits)
				{
					return parseError("Too many digits");
				}
				mantissa = mantissa * 10 + static_cast<std::uint64_t>(c - '0');
			}
			else if (c == '.' || c == ',')
			{
				if (sawSeparator)
				{
					return parseError("More than one decimal separator");
				}
				sawSeparator = true;
			}
			else
			{
				break; // start of the unit suffix (or invalid text, rejected below)
			}
		}
		if (!sawDigit)
		{
			return parseError("No digits in length");
		}

		while (pos < input.size() && isBlank(input[pos]))
		{
			++pos;
		}
		std::string suffix;
		for (; pos < input.size(); ++pos)
		{
			suffix.push_back(toLowerAscii(input[pos]));
		}

		LengthUnit unit = defaultUnit;
		if (suffix == "mm")
		{
			unit = LengthUnit::Millimeter;
		}
		else if (suffix == "cm")
		{
			unit = LengthUnit::Centimeter;
		}
		else if (suffix == "m")
		{
			unit = LengthUnit::Meter;
		}
		else if (!suffix.empty())
		{
			return parseError("Unknown unit '" + suffix + "' (use mm, cm or m)");
		}

		std::uint64_t denominator = 1;
		for (int i = 0; i < fractionDigits; ++i)
		{
			denominator *= 10;
		}
		const std::uint64_t numerator = mantissa * static_cast<std::uint64_t>(millimetersPerUnit(unit));
		// Rounding the magnitude half up == rounding the signed value half away from zero.
		const std::uint64_t magnitude = (numerator + denominator / 2) / denominator;
		if (magnitude > static_cast<std::uint64_t>(MaxAbsMillimeters))
		{
			return Error{ErrorCode::OutOfRange, "Length exceeds the model range (1,000 km)"};
		}

		const std::int64_t millimeters = static_cast<std::int64_t>(magnitude);
		return negative ? -millimeters : millimeters;
	}

	std::string formatLength(double millimeters, LengthUnit unit)
	{
		const std::int64_t rounded = std::isfinite(millimeters) ? std::llround(millimeters) : 0;
		const std::int64_t perUnit = millimetersPerUnit(unit);
		const std::uint64_t magnitude = static_cast<std::uint64_t>(rounded < 0 ? -rounded : rounded);

		std::string text = rounded < 0 ? "-" : "";
		text += std::to_string(magnitude / static_cast<std::uint64_t>(perUnit));
		std::uint64_t remainder = magnitude % static_cast<std::uint64_t>(perUnit);
		if (remainder != 0)
		{
			// Fixed number of decimals for the unit (cm: 1, m: 3), then trailing zeros removed.
			std::string decimals = std::to_string(remainder);
			const std::size_t width = perUnit == 10 ? 1 : 3;
			decimals.insert(0, width - decimals.size(), '0');
			while (!decimals.empty() && decimals.back() == '0')
			{
				decimals.pop_back();
			}
			text += "." + decimals;
		}
		switch (unit)
		{
		case LengthUnit::Millimeter: return text + " mm";
		case LengthUnit::Centimeter: return text + " cm";
		case LengthUnit::Meter: return text + " m";
		}
		return text;
	}

	Result<std::int64_t> quantizeMillimeters(double valueMm)
	{
		if (!std::isfinite(valueMm))
		{
			return Error{ErrorCode::InvalidArgument, "Length is not a finite number"};
		}
		// Guard before rounding so std::llround never overflows.
		if (std::fabs(valueMm) > static_cast<double>(MaxAbsMillimeters) + 1.0)
		{
			return Error{ErrorCode::OutOfRange, "Length exceeds the model range (1,000 km)"};
		}
		const std::int64_t rounded = std::llround(valueMm); // half away from zero
		if (rounded > MaxAbsMillimeters || rounded < -MaxAbsMillimeters)
		{
			return Error{ErrorCode::OutOfRange, "Length exceeds the model range (1,000 km)"};
		}
		return rounded;
	}
}
