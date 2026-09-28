#include "doctest.h"

#include "F101/Cad/Document.h"

#include <vector>

using namespace f101;
using namespace f101::cad;

namespace
{
	struct TwoWalls
	{
		Document document{makeSequentialUuidSource()};
		WallId first = document.addWall({0, 0}, {3550, 0}, 120, 2400).value();
		WallId second = document.addWall({3550, 0}, {3550, 4000}, 120, 2400).value();
		std::vector<ChangeSet> changes;

		TwoWalls()
		{
			document.subscribe([this](const ChangeSet& set) { changes.push_back(set); });
		}

		const Wall& wall(const WallId& id) const
		{
			const Wall* found = document.project().findWall(id);
			REQUIRE(found != nullptr);
			return *found;
		}
	};
}

TEST_CASE_FIXTURE(TwoWalls, "create is undoable and redoable")
{
	CHECK(document.canUndo());
	CHECK(document.undoLabel() == "Create wall");
	REQUIRE(document.undo().ok());
	CHECK(document.project().walls.size() == 1);
	CHECK(document.project().findWall(second) == nullptr);
	CHECK(changes.back().removedWalls == std::vector<WallId>{second});
	CHECK(document.redoLabel() == "Create wall");

	REQUIRE(document.redo().ok());
	CHECK(document.project().walls.size() == 2);
	CHECK(wall(second).end == Point2D{3550, 4000});
	CHECK(changes.back().addedWalls == std::vector<WallId>{second});
}

TEST_CASE_FIXTURE(TwoWalls, "undoing a delete restores the same wall, id and position")
{
	REQUIRE(document.execute(DeleteWallCommand{first}).ok());
	CHECK(document.project().walls.size() == 1);
	REQUIRE(document.undo().ok());
	REQUIRE(document.project().walls.size() == 2);
	CHECK(document.project().walls[0].id == first); // original order kept
	CHECK(wall(first).end == Point2D{3550, 0});
	CHECK(changes.back().addedWalls == std::vector<WallId>{first});
}

TEST_CASE_FIXTURE(TwoWalls, "move endpoint, thickness and height, each undone independently")
{
	REQUIRE(document.execute(MoveWallEndpointCommand{first, WallEndpoint::Start, {-500, 0}}).ok());
	REQUIRE(document.execute(ChangeWallThicknessCommand{first, 200}).ok());
	REQUIRE(document.execute(ChangeWallHeightCommand{first, 2700}).ok());
	CHECK(wall(first).start == Point2D{-500, 0});
	CHECK(wall(first).thicknessMm == 200);
	CHECK(wall(first).heightMm == 2700);

	CHECK(document.undoLabel() == "Change wall height");
	REQUIRE(document.undo().ok());
	CHECK(wall(first).heightMm == 2400);
	CHECK(wall(first).thicknessMm == 200);
	REQUIRE(document.undo().ok());
	CHECK(wall(first).thicknessMm == 120);
	REQUIRE(document.undo().ok());
	CHECK(wall(first).start == Point2D{0, 0});
	CHECK(changes.back().modifiedWalls == std::vector<WallId>{first});
}

TEST_CASE_FIXTURE(TwoWalls, "change length keeps start and direction")
{
	REQUIRE(document.execute(ChangeWallLengthCommand{first, 4200}).ok());
	CHECK(wall(first).start == Point2D{0, 0});
	CHECK(wall(first).end == Point2D{4200, 0});

	REQUIRE(document.execute(ChangeWallLengthCommand{second, 2000}).ok());
	CHECK(wall(second).start == Point2D{3550, 0});
	CHECK(wall(second).end == Point2D{3550, 2000});
}

TEST_CASE("change length on a diagonal quantizes the end to millimetres")
{
	Document document{makeSequentialUuidSource()};
	const WallId id = document.addWall({0, 0}, {3000, 4000}, 120, 2400).value();
	REQUIRE(document.execute(ChangeWallLengthCommand{id, 1000}).ok());
	CHECK(document.project().findWall(id)->end == Point2D{600, 800}); // exact on a 3-4-5 triangle
	REQUIRE(document.execute(ChangeWallLengthCommand{id, 1234}).ok());
	const double length = document.project().findWall(id)->lengthMm();
	CHECK(length == doctest::Approx(1234.0).epsilon(0.001)); // within 1 mm
}

TEST_CASE_FIXTURE(TwoWalls, "rejected commands change nothing, not even the history")
{
	const std::size_t notifications = changes.size();
	CHECK(document.execute(ChangeWallThicknessCommand{first, 0}).error().code == ErrorCode::OutOfRange);
	CHECK(document.execute(ChangeWallLengthCommand{first, -1}).error().code == ErrorCode::OutOfRange);
	CHECK(document.execute(MoveWallEndpointCommand{first, WallEndpoint::End, {0, 0}}).error().code == ErrorCode::InvalidArgument);
	CHECK(document.execute(DeleteWallCommand{WallId{Uuid{9, 9}}}).error().code == ErrorCode::NotFound);
	CHECK(document.execute(CreateWallCommand{{0, 0}, {0, 0}, 120, 2400}).error().code == ErrorCode::InvalidArgument);
	CHECK(changes.size() == notifications);
	CHECK(document.undoLabel() == "Create wall");
	CHECK(wall(first).thicknessMm == 120);
}

TEST_CASE_FIXTURE(TwoWalls, "a command that changes nothing is not recorded")
{
	const Result<Transaction> same = document.execute(ChangeWallHeightCommand{first, 2400});
	REQUIRE(same.ok());
	CHECK(same.value().walls.empty());
	CHECK(document.undoLabel() == "Create wall");
}

TEST_CASE_FIXTURE(TwoWalls, "a new command clears the redo history")
{
	REQUIRE(document.undo().ok());
	CHECK(document.canRedo());
	REQUIRE(document.execute(ChangeWallHeightCommand{first, 3000}).ok());
	CHECK_FALSE(document.canRedo());
	CHECK(document.redo().error().code == ErrorCode::NotFound);
}

TEST_CASE("undo and redo with an empty history report it")
{
	Document document{makeSequentialUuidSource()};
	CHECK_FALSE(document.canUndo());
	CHECK(document.undo().error().code == ErrorCode::NotFound);
	CHECK(document.undoLabel().empty());
}

TEST_CASE_FIXTURE(TwoWalls, "replaceProject validates, notifies and clears the history")
{
	Project loaded;
	loaded.walls.push_back(Wall{WallId{Uuid{0, 77}}, {0, 0}, {1000, 0}, 100, 2500});
	REQUIRE(document.replaceProject(loaded).ok());
	CHECK(document.project().walls.size() == 1);
	CHECK_FALSE(document.canUndo());
	CHECK(changes.back().removedWalls.size() == 2);
	CHECK(changes.back().addedWalls == std::vector<WallId>{WallId{Uuid{0, 77}}});

	Project duplicate;
	duplicate.walls.push_back(loaded.walls[0]);
	duplicate.walls.push_back(loaded.walls[0]);
	CHECK(document.replaceProject(duplicate).error().code == ErrorCode::InvalidArgument);
	CHECK(document.project().walls.size() == 1);
}

TEST_CASE("command labels")
{
	CHECK(commandLabel(CreateWallCommand{}) == "Create wall");
	CHECK(commandLabel(DeleteWallCommand{}) == "Delete wall");
	CHECK(commandLabel(MoveWallEndpointCommand{}) == "Move wall endpoint");
	CHECK(commandLabel(ChangeWallLengthCommand{}) == "Change wall length");
	CHECK(commandLabel(ChangeWallThicknessCommand{}) == "Change wall thickness");
	CHECK(commandLabel(ChangeWallHeightCommand{}) == "Change wall height");
}
