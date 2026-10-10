#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Result.h"

#include <cstdint>
#include <string>
#include <string_view>

// Units policy: docs/adr/ADR-0001-units-mm.md
namespace f101::cad
{
	enum class LengthUnit
	{
		Millimeter,
		Centimeter,
		Meter,
	};

	// Largest coordinate or dimension magnitude accepted in the model: 1,000 km. Exact in double.
	inline constexpr std::int64_t MaxAbsMillimeters = 1'000'000'000;

	constexpr std::int64_t millimetersPerUnit(LengthUnit unit)
	{
		switch (unit)
		{
		case LengthUnit::Millimeter: return 1;
		case LengthUnit::Centimeter: return 10;
		case LengthUnit::Meter: return 1000;
		}
		return 1;
	}

	// Parses user input such as "355", "355 cm", "3.55 m", "3,55 m" or "3550 mm" into integer millimetres.
	// Exact decimal arithmetic (never through double); rounds half away from zero to 1 mm.
	// `defaultUnit` applies when the text has no unit suffix (project input unit, centimetres by default).
	FLOOR101CADCORE_API Result<std::int64_t> parseLength(std::string_view text, LengthUnit defaultUnit = LengthUnit::Centimeter);

	// Quantizes a computed value in millimetres to the model resolution (nearest mm, half away from zero).
	// This is the single place where computed geometry re-enters the model.
	FLOOR101CADCORE_API Result<std::int64_t> quantizeMillimeters(double valueMm);

	// Display text in the given unit at millimetre resolution, without trailing zeros:
	// 3550 -> "3550 mm", "355 cm", "3.55 m"; 3555 -> "355.5 cm"; 1234.4 -> "1234 mm".
	FLOOR101CADCORE_API std::string formatLength(double millimeters, LengthUnit unit);
	// Two decimals of a whole number of hundredths, with the given decimal separator: "3.55", "3,55", "-0.07".
	FLOOR101CADCORE_API std::string formatHundredths(std::int64_t hundredths, char decimalSeparator);
}
