#include "F101/Cad/Id.h"

#include <memory>
#include <random>

namespace f101::cad
{
	namespace
	{
		constexpr char HexDigits[] = "0123456789abcdef";

		void appendHex(std::string& out, std::uint64_t value, int firstNibble, int nibbleCount)
		{
			for (int i = 0; i < nibbleCount; ++i)
			{
				const int shift = (15 - (firstNibble + i)) * 4;
				out.push_back(HexDigits[(value >> shift) & 0xF]);
			}
		}

		int hexValue(char c)
		{
			if (c >= '0' && c <= '9')
			{
				return c - '0';
			}
			if (c >= 'a' && c <= 'f')
			{
				return c - 'a' + 10;
			}
			if (c >= 'A' && c <= 'F')
			{
				return c - 'A' + 10;
			}
			return -1;
		}
	}

	std::string toString(const Uuid& uuid)
	{
		std::string out;
		out.reserve(36);
		appendHex(out, uuid.high, 0, 8);
		out.push_back('-');
		appendHex(out, uuid.high, 8, 4);
		out.push_back('-');
		appendHex(out, uuid.high, 12, 4);
		out.push_back('-');
		appendHex(out, uuid.low, 0, 4);
		out.push_back('-');
		appendHex(out, uuid.low, 4, 12);
		return out;
	}

	Result<Uuid> parseUuid(std::string_view text)
	{
		if (text.size() != 36 || text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-')
		{
			return Error{ErrorCode::ParseError, "UUID must have the form xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"};
		}
		Uuid uuid;
		int nibble = 0;
		for (const char c : text)
		{
			if (c == '-')
			{
				continue;
			}
			const int value = hexValue(c);
			if (value < 0)
			{
				return Error{ErrorCode::ParseError, "UUID contains a non-hexadecimal character"};
			}
			std::uint64_t& half = nibble < 16 ? uuid.high : uuid.low;
			half = (half << 4) | static_cast<std::uint64_t>(value);
			++nibble;
		}
		return uuid;
	}

	UuidSource makeRandomUuidSource()
	{
		// Shared so copies of the source keep drawing from one stream.
		std::random_device seedSource;
		const std::uint64_t seed = (static_cast<std::uint64_t>(seedSource()) << 32) ^ seedSource();
		auto engine = std::make_shared<std::mt19937_64>(seed);
		return [engine]()
		{
			Uuid uuid{(*engine)(), (*engine)()};
			uuid.high = (uuid.high & 0xFFFF'FFFF'FFFF'0FFFull) | 0x0000'0000'0000'4000ull; // version 4
			uuid.low = (uuid.low & 0x3FFF'FFFF'FFFF'FFFFull) | 0x8000'0000'0000'0000ull;   // RFC 9562 variant
			return uuid;
		};
	}

	UuidSource makeSequentialUuidSource(std::uint64_t first)
	{
		auto next = std::make_shared<std::uint64_t>(first);
		return [next]() { return Uuid{0, (*next)++}; };
	}
}
