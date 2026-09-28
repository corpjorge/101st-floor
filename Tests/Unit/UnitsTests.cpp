#include "doctest.h"

#include "F101/Cad/Units.h"

#include <cmath>
#include <limits>
#include <string>

using namespace f101;
using namespace f101::cad;

namespace
{
	constexpr std::int64_t ParseFailed = std::numeric_limits<std::int64_t>::min();

	std::int64_t parsed(std::string_view text, LengthUnit defaultUnit = LengthUnit::Centimeter)
	{
		const Result<std::int64_t> result = parseLength(text, defaultUnit);
		CHECK_MESSAGE(result.ok(), "failed to parse: " << std::string(text));
		return result.ok() ? result.value() : ParseFailed;
	}

	ErrorCode parseErrorCode(std::string_view text)
	{
		const Result<std::int64_t> result = parseLength(text);
		REQUIRE_MESSAGE(!result.ok(), "unexpectedly parsed: " << std::string(text));
		return result.error().code;
	}
}

TEST_CASE("the master prompt's inputs for the reference wall all mean 3550 mm")
{
	CHECK(parsed("355") == 3550); // no unit: project input unit, centimetres by default
	CHECK(parsed("355 cm") == 3550);
	CHECK(parsed("3.55 m") == 3550);
	CHECK(parsed("3550 mm") == 3550);
}

TEST_CASE("accepted spellings: comma decimal, no space, any case, surrounding blanks")
{
	CHECK(parsed("3,55 m") == 3550);
	CHECK(parsed("3.55m") == 3550);
	CHECK(parsed("355CM") == 3550);
	CHECK(parsed("  3550 Mm\t") == 3550);
	CHECK(parsed(".5 m") == 500);
	CHECK(parsed("5.") == 50);
	CHECK(parsed("+12 cm") == 120);
	CHECK(parsed("-12 cm") == -120);
}

TEST_CASE("MVP 0 wall dimensions")
{
	CHECK(parsed("12 cm") == 120);
	CHECK(parsed("2.4 m") == 2400);
	CHECK(parsed("4.2 m") == 4200);
}

TEST_CASE("the default unit is configurable")
{
	CHECK(parsed("355", LengthUnit::Millimeter) == 355);
	CHECK(parsed("355", LengthUnit::Meter) == 355000);
	CHECK(parsed("355 mm", LengthUnit::Meter) == 355); // an explicit suffix wins
}

TEST_CASE("decimal parsing is exact where double arithmetic is not")
{
	// In double, 1.005 * 1000 == 1004.9999999999999: truncating would give 1004 mm, a 1 mm error.
	CHECK(1.005 * 1000.0 < 1005.0);
	CHECK(static_cast<std::int64_t>(1.005 * 1000.0) == 1004);
	CHECK(parsed("1.005 m") == 1005);
	CHECK(parsed("4.35 m") == 4350);
	CHECK(parsed("0.1 m") == 100);
}

TEST_CASE("rounds to 1 mm, half away from zero")
{
	CHECK(parsed("3.5555 m") == 3556);
	CHECK(parsed("3.5554 m") == 3555);
	CHECK(parsed("0.5 mm") == 1);
	CHECK(parsed("-0.5 mm") == -1);
	CHECK(parsed("-2.5 mm") == -3);
	CHECK(parsed("0.4 mm") == 0);
}

TEST_CASE("rejects malformed input")
{
	CHECK(parseErrorCode("") == ErrorCode::ParseError);
	CHECK(parseErrorCode("   ") == ErrorCode::ParseError);
	CHECK(parseErrorCode("abc") == ErrorCode::ParseError);
	CHECK(parseErrorCode(".") == ErrorCode::ParseError);
	CHECK(parseErrorCode("-") == ErrorCode::ParseError);
	CHECK(parseErrorCode("3.5.5 m") == ErrorCode::ParseError);
	CHECK(parseErrorCode("3,5.5") == ErrorCode::ParseError);
	CHECK(parseErrorCode("355 km") == ErrorCode::ParseError);
	CHECK(parseErrorCode("355 c m") == ErrorCode::ParseError);
	CHECK(parseErrorCode("1e3") == ErrorCode::ParseError);
	CHECK(parseErrorCode("12 mm mm") == ErrorCode::ParseError);
	CHECK(parseErrorCode("1234567890123456") == ErrorCode::ParseError); // more than 15 significant digits
}

TEST_CASE("rejects lengths outside the model range")
{
	CHECK(parsed("1000000 m") == MaxAbsMillimeters);
	CHECK(parseErrorCode("1000001 m") == ErrorCode::OutOfRange);
}

TEST_CASE("quantizeMillimeters: the single re-entry point for computed values")
{
	CHECK(quantizeMillimeters(3549.5).value() == 3550);
	CHECK(quantizeMillimeters(-3549.5).value() == -3550);
	CHECK(quantizeMillimeters(3549.49).value() == 3549);
	CHECK(quantizeMillimeters(std::nan("")).error().code == ErrorCode::InvalidArgument);
	CHECK(quantizeMillimeters(std::numeric_limits<double>::infinity()).error().code == ErrorCode::InvalidArgument);
	CHECK(quantizeMillimeters(2e9).error().code == ErrorCode::OutOfRange);
}
