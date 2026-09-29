#include "doctest.h"

#include "F101/Cad/WallTool.h"

#include <cmath>
#include <string>

using namespace f101;
using namespace f101::cad;

namespace
{
	struct Drawing
	{
		Document document{makeSequentialUuidSource()};
		WallTool tool{document, WallToolSettings{}};

		const Wall& lastWall() const
		{
			REQUIRE_FALSE(document.project().walls.empty());
			return document.project().walls.back();
		}
	};
}

TEST_CASE_FIXTURE(Drawing, "master prompt §19: click, move, type a length, Enter -> exactly 3550 mm")
{
	for (const std::string typed : {"355", "355 cm", "3.55 m", "3,55 m", "3550 mm"})
	{
		CAPTURE(typed);
		Document fresh{makeSequentialUuidSource()};
		WallToolSettings settings;
		settings.heightMm = 2400; // the reference wall; by default walls take their level's height (2800)
		WallTool wallTool{fresh, settings};
		CHECK(WallTool(fresh, WallToolSettings{}).wallHeightMm() == 2800);

		REQUIRE(wallTool.click({0.0, 0.0}).ok());               // 2. initial click
		CHECK(wallTool.state() == WallTool::State::Drafting);
		wallTool.pointerMoved({2987.0, 23.0});                  // 3. move the mouse
		REQUIRE(wallTool.preview().has_value());                // 4. preview
		wallTool.appendText(typed);                             // 6. type the length
		CHECK(wallTool.typedTextError().empty());
		CHECK(wallTool.preview()->end == Point2D{3550, 0});     // 5. preview shows the typed length
		const Result<WallId> created = wallTool.confirm();      // 7. Enter
		REQUIRE(created.ok());

		const Wall& wall = *fresh.project().findWall(created.value());
		CHECK(wall.start == Point2D{0, 0});                     // 8. exactly 3550 mm
		CHECK(wall.end == Point2D{3550, 0});
		CHECK(wall.lengthMm() == 3550.0);
		CHECK(wall.thicknessMm == 120);
		CHECK(wall.heightMm == 2400);
	}
}

TEST_CASE_FIXTURE(Drawing, "without typing, the snapped cursor defines the wall")
{
	REQUIRE(tool.click({12.0, -18.0}).ok()); // snaps to the grid origin
	const Result<std::optional<WallId>> created = tool.click({3531.0, 44.0});
	REQUIRE(created.ok());
	REQUIRE(created.value().has_value());
	CHECK(lastWall().start == Point2D{0, 0});
	CHECK(lastWall().end == Point2D{3550, 0});
}

TEST_CASE_FIXTURE(Drawing, "walls chain until cancel, and each one is undoable")
{
	REQUIRE(tool.click({0.0, 0.0}).ok());
	tool.pointerMoved({3000.0, 0.0});
	tool.appendText("355");
	REQUIRE(tool.confirm().ok());
	CHECK(tool.state() == WallTool::State::Drafting);
	CHECK(tool.typedText().empty());

	tool.pointerMoved({3560.0, 1500.0}); // straight up from the previous end
	tool.appendText("4 m");
	REQUIRE(tool.confirm().ok());
	CHECK(lastWall().start == Point2D{3550, 0});
	CHECK(lastWall().end == Point2D{3550, 4000});

	tool.cancel();
	CHECK(tool.state() == WallTool::State::Idle);
	CHECK_FALSE(tool.preview().has_value());
	CHECK(document.project().walls.size() == 2);

	REQUIRE(document.undo().ok());
	CHECK(document.project().walls.size() == 1);
}

TEST_CASE_FIXTURE(Drawing, "an invalid typed length is reported and blocks Enter")
{
	REQUIRE(tool.click({0.0, 0.0}).ok());
	tool.pointerMoved({1000.0, 0.0});
	tool.appendText("3.5.5");
	CHECK_FALSE(tool.typedTextError().empty());
	CHECK(tool.confirm().error().code == ErrorCode::ParseError);
	CHECK(document.project().walls.empty());

	tool.backspace();
	tool.backspace();
	CHECK(tool.typedText() == "3.5");
	CHECK(tool.typedTextError().empty());
	REQUIRE(tool.confirm().ok());
	CHECK(lastWall().end == Point2D{35, 0}); // 3.5 cm
}

TEST_CASE_FIXTURE(Drawing, "unrelated characters are ignored and typing needs a started wall")
{
	tool.appendText("355");
	CHECK(tool.typedText().empty()); // Idle: nothing to type into
	REQUIRE(tool.click({0.0, 0.0}).ok());
	tool.appendText("3x5!5");
	CHECK(tool.typedText() == "355");
}

TEST_CASE_FIXTURE(Drawing, "Enter with no length is refused")
{
	CHECK(tool.confirm().error().code == ErrorCode::InvalidArgument); // idle
	REQUIRE(tool.click({0.0, 0.0}).ok());
	CHECK(tool.confirm().error().code == ErrorCode::InvalidArgument); // cursor still on the start
}

TEST_CASE_FIXTURE(Drawing, "idle hover snaps to existing endpoints so walls connect")
{
	REQUIRE(tool.click({0.0, 0.0}).ok());
	tool.appendText("355");
	REQUIRE(tool.confirm().ok());
	tool.cancel();
	tool.pointerMoved({3490.0, 70.0});
	CHECK(tool.hoverPoint() == Point2D{3550, 0});
	CHECK(tool.snapKind() == SnapKind::Endpoint);
}

TEST_CASE_FIXTURE(Drawing, "a typed length on a snapped diagonal reads back as typed")
{
	// Live test regression: 75 degrees, "300" gave 2999.4 mm because the direction came from the rounded cursor.
	REQUIRE(tool.click({0.0, 0.0}).ok());
	tool.pointerMoved({310.0, 1170.0});
	tool.appendText("300");
	REQUIRE(tool.confirm().ok());
	CHECK(std::fabs(lastWall().lengthMm() - 3000.0) < 0.5);
	CHECK(lastWall().end == Point2D{776, 2898});
}
