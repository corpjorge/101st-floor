#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Result.h"

#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace f101::cad
{
	// 128-bit universally unique identifier, persistent identity of model entities.
	struct Uuid
	{
		std::uint64_t high = 0;
		std::uint64_t low = 0;

		friend constexpr auto operator<=>(const Uuid&, const Uuid&) = default;
		constexpr bool isNil() const { return high == 0 && low == 0; }
	};

	// Canonical lowercase text form: 8-4-4-4-12 hex digits.
	FLOOR101CADCORE_API std::string toString(const Uuid& uuid);
	FLOOR101CADCORE_API Result<Uuid> parseUuid(std::string_view text);

	// Where new identities come from. Injectable so tests are deterministic.
	using UuidSource = std::function<Uuid()>;

	// Random RFC 9562 version 4 UUIDs (production).
	FLOOR101CADCORE_API UuidSource makeRandomUuidSource();
	// 00000000-0000-0000-0000-000000000001, ...2, ... (tests only).
	FLOOR101CADCORE_API UuidSource makeSequentialUuidSource(std::uint64_t first = 1);

	// Typed identity: a WallId can never be passed where another entity's id is expected.
	template <typename Tag>
	struct TypedId
	{
		Uuid uuid;

		friend constexpr auto operator<=>(const TypedId&, const TypedId&) = default;
		constexpr bool isNil() const { return uuid.isNil(); }
	};

	template <typename Tag>
	std::string toString(const TypedId<Tag>& id)
	{
		return toString(id.uuid);
	}

	struct WallTag;
	using WallId = TypedId<WallTag>;

	struct OpeningTag;
	using OpeningId = TypedId<OpeningTag>;
}
