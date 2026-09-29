#include "doctest.h"

#include "F101/Cad/Document.h"
#include "F101/Cad/Units.h"

#include <vector>

using namespace f101;
using namespace f101::cad;

namespace
{
	// MVP 0 reference wall: (0,0) -> (3550,0), 120 mm thick, 2400 mm high.
	struct ReferenceWall
	{
		Document document{makeSequentialUuidSource()};
		WallId id = document.addWall({0, 0}, {3550, 0}, 120, 2400).value();

		const Wall& wall() const
		{
			const Wall* found = document.project().findWall(id);
			REQUIRE(found != nullptr);
			return *found;
		}
	};
}

TEST_CASE_FIXTURE(ReferenceWall, "reference wall: 3550 x 120 x 2400 mm with a persistent id")
{
	CHECK(wall().lengthMm() == 3550.0);
	CHECK(wall().thicknessMm == 120);
	CHECK(wall().heightMm == 2400);
	CHECK(wall().start == Point2D{0, 0});
	CHECK(wall().end == Point2D{3550, 0});
	CHECK(toString(id) == "00000000-0000-0000-0000-000000000002"); // 1: the ground level of the new document
	CHECK(wall().level == document.project().levels[0].id);
	CHECK(document.project().walls.size() == 1);
}

TEST_CASE_FIXTURE(ReferenceWall, "changing the length to 4200 mm keeps the same wall")
{
	REQUIRE(document.setWallEnd(id, {4200, 0}).ok());
	CHECK(wall().lengthMm() == 4200.0);
	CHECK(wall().id == id);
	CHECK(wall().thicknessMm == 120);
	CHECK(wall().heightMm == 2400);
	CHECK(document.project().walls.size() == 1);
}

TEST_CASE_FIXTURE(ReferenceWall, "a rejected mutation leaves the project untouched")
{
	const Status degenerate = document.setWallEnd(id, {0, 0});
	CHECK_FALSE(degenerate.ok());
	CHECK(degenerate.error().code == ErrorCode::InvalidArgument);
	CHECK(wall().end == Point2D{3550, 0});

	const Status outOfRange = document.setWallEnd(id, {MaxAbsMillimeters + 1, 0});
	CHECK(outOfRange.error().code == ErrorCode::OutOfRange);
	CHECK(wall().end == Point2D{3550, 0});
}

TEST_CASE("invalid walls are rejected")
{
	Document document{makeSequentialUuidSource()};
	CHECK(document.addWall({0, 0}, {0, 0}, 120, 2400).error().code == ErrorCode::InvalidArgument);
	CHECK(document.addWall({0, 0}, {3550, 0}, 0, 2400).error().code == ErrorCode::OutOfRange);
	CHECK(document.addWall({0, 0}, {3550, 0}, 120, -1).error().code == ErrorCode::OutOfRange);
	CHECK(document.project().walls.empty());
}

TEST_CASE("unknown wall ids are reported, not asserted")
{
	Document document{makeSequentialUuidSource()};
	const WallId unknown{Uuid{7, 7}};
	CHECK(document.setWallEnd(unknown, {10, 0}).error().code == ErrorCode::NotFound);
	CHECK(document.removeWall(unknown).error().code == ErrorCode::NotFound);
}

TEST_CASE("listeners receive exactly the entities each successful mutation touched")
{
	Document document{makeSequentialUuidSource()};
	std::vector<ChangeSet> received;
	const std::size_t handle = document.subscribe([&received](const ChangeSet& changes) { received.push_back(changes); });

	const WallId id = document.addWall({0, 0}, {3550, 0}, 120, 2400).value();
	REQUIRE(received.size() == 1);
	CHECK(received[0].addedWalls == std::vector<WallId>{id});

	REQUIRE(document.setWallEnd(id, {4200, 0}).ok());
	REQUIRE(received.size() == 2);
	CHECK(received[1].modifiedWalls == std::vector<WallId>{id});

	CHECK_FALSE(document.setWallEnd(id, {0, 0}).ok());
	CHECK(received.size() == 2); // failed mutations are silent

	REQUIRE(document.removeWall(id).ok());
	REQUIRE(received.size() == 3);
	CHECK(received[2].removedWalls == std::vector<WallId>{id});
	CHECK(document.project().walls.empty());

	document.unsubscribe(handle);
	CHECK(document.addWall({0, 0}, {1000, 0}, 120, 2400).ok());
	CHECK(received.size() == 3);
}
