#include "doctest.h"

#include "F101/Cad/Id.h"

#include <set>
#include <string>

using namespace f101;
using namespace f101::cad;

TEST_CASE("UUID text form round-trips")
{
	const Uuid uuid{0x0123'4567'89ab'cdefull, 0xfedc'ba98'7654'3210ull};
	CHECK(toString(uuid) == "01234567-89ab-cdef-fedc-ba9876543210");
	CHECK(parseUuid("01234567-89ab-cdef-fedc-ba9876543210").value() == uuid);
	CHECK(parseUuid("01234567-89AB-CDEF-FEDC-BA9876543210").value() == uuid);
}

TEST_CASE("malformed UUID text is rejected")
{
	CHECK(parseUuid("").error().code == ErrorCode::ParseError);
	CHECK(parseUuid("01234567-89ab-cdef-fedc-ba987654321").error().code == ErrorCode::ParseError);
	CHECK(parseUuid("0123456789ab-cdef-fedc-ba98-76543210").error().code == ErrorCode::ParseError);
	CHECK(parseUuid("0123456g-89ab-cdef-fedc-ba9876543210").error().code == ErrorCode::ParseError);
}

TEST_CASE("random UUIDs are version 4, RFC 9562 variant, and distinct")
{
	const UuidSource next = makeRandomUuidSource();
	std::set<Uuid> seen;
	for (int i = 0; i < 1000; ++i)
	{
		const Uuid uuid = next();
		const std::string text = toString(uuid);
		CHECK(text[14] == '4');
		CHECK(std::string("89ab").find(text[19]) != std::string::npos);
		seen.insert(uuid);
	}
	CHECK(seen.size() == 1000);
}

TEST_CASE("the sequential source is deterministic (tests only)")
{
	const UuidSource a = makeSequentialUuidSource();
	const UuidSource b = makeSequentialUuidSource();
	CHECK(toString(a()) == "00000000-0000-0000-0000-000000000001");
	CHECK(toString(a()) == "00000000-0000-0000-0000-000000000002");
	CHECK(b() == Uuid{0, 1});
}

TEST_CASE("typed ids compare by value and know when they are nil")
{
	const WallId nil;
	CHECK(nil.isNil());
	const WallId a{Uuid{0, 1}};
	const WallId b{Uuid{0, 1}};
	CHECK(a == b);
	CHECK_FALSE(a.isNil());
	CHECK(toString(a) == "00000000-0000-0000-0000-000000000001");
}
