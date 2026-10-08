#include "doctest.h"

#include "F101/Cad/Document.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Cad/Units.h"
#include <limits>

#include <string>
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
	CHECK(wall(first).end == Point2D{3550, 0}); // the existing joint becomes an independent span
	CHECK(document.project().walls.back().start == Point2D{3550, 0});
	CHECK(document.project().walls.back().end == Point2D{4200, 0});

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
	CHECK(document.replaceProject(loaded).error().code == ErrorCode::InvalidArgument); // no level
	CHECK(changes.empty());
	loaded.levels.push_back(Level{LevelId{Uuid{0, 78}}});
	CHECK(document.replaceProject(loaded).error().code == ErrorCode::InvalidArgument); // the wall is on no level of it
	loaded.walls[0].level = loaded.levels[0].id;
	REQUIRE(document.replaceProject(loaded).ok());
	CHECK(document.project().walls.size() == 1);
	CHECK_FALSE(document.canUndo());
	CHECK(changes.back().removedWalls.size() == 2);
	CHECK(changes.back().addedWalls == std::vector<WallId>{WallId{Uuid{0, 77}}});
	CHECK(changes.back().removedLevels.size() == 1);
	CHECK(changes.back().addedLevels == std::vector<LevelId>{LevelId{Uuid{0, 78}}});

	Project duplicate;
	duplicate.levels = loaded.levels;
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

TEST_CASE("moving a corner moves every wall that ends there, as one undoable step")
{
	Document document{makeSequentialUuidSource()};
	const WallId south = document.addWall({0, 0}, {4000, 0}, 120, 2400).value();
	const WallId east = document.addWall({4000, 0}, {4000, 3000}, 120, 2400).value();
	const WallId other = document.addWall({0, 5000}, {3000, 5000}, 120, 2400).value();

	const Result<Transaction> moved = document.execute(MoveJointCommand{{4000, 0}, {4500, -200}});
	REQUIRE(moved.ok());
	CHECK(moved.value().label == "Move corner");
	CHECK(moved.value().walls.size() == 2);
	CHECK(document.project().findWall(south)->end == Point2D{4500, -200});
	CHECK(document.project().findWall(east)->start == Point2D{4500, -200});
	CHECK(document.project().findWall(other)->start == Point2D{0, 5000});

	REQUIRE(document.undo().ok());
	CHECK(document.project().findWall(south)->end == Point2D{4000, 0});
	CHECK(document.project().findWall(east)->start == Point2D{4000, 0});
}

TEST_CASE("a corner move that would collapse a wall, or finds no wall end, changes nothing")
{
	Document document{makeSequentialUuidSource()};
	const WallId south = document.addWall({0, 0}, {4000, 0}, 120, 2400).value();
	REQUIRE(document.addWall({4000, 0}, {4000, 3000}, 120, 2400).ok());
	CHECK_FALSE(document.execute(MoveJointCommand{{4000, 0}, {0, 0}}).ok()); // the south wall would have no length
	CHECK(document.project().findWall(south)->end == Point2D{4000, 0});
	const Result<Transaction> missing = document.execute(MoveJointCommand{{123, 456}, {0, 0}});
	REQUIRE_FALSE(missing.ok());
	CHECK(missing.error().code == ErrorCode::NotFound);
	CHECK(document.execute(MoveJointCommand{{4000, 0}, {4000, 0}}).value().walls.empty()); // no-op
}

TEST_CASE("openings are added, changed and deleted as undoable wall changes")
{
	Document document{makeSequentialUuidSource()};
	const WallId wall = document.addWall({0, 0}, {4000, 0}, 120, 2400).value();
	const Result<OpeningId> window = document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Window, 2000, 1200, 1200, 900});
	REQUIRE(window.ok());
	CHECK(document.undoLabel() == "Add window");
	const Result<OpeningId> door = document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Door, 300, 900, 2100, 0});
	REQUIRE(door.ok());
	CHECK(window.value() != door.value());
	const Wall& host = *document.project().findWall(wall);
	REQUIRE(host.openings.size() == 2);
	CHECK(host.openings[0].id == door.value()); // kept sorted by offset

	Opening wider = *host.findOpening(window.value());
	wider.widthMm = 1500;
	REQUIRE(document.execute(ChangeOpeningCommand{wall, wider}).ok());
	CHECK(document.project().findWall(wall)->findOpening(window.value())->widthMm == 1500);

	wider.widthMm = 3000; // would overlap the door and leave the wall
	CHECK_FALSE(document.execute(ChangeOpeningCommand{wall, wider}).ok());
	CHECK(document.project().findWall(wall)->findOpening(window.value())->widthMm == 1500);

	REQUIRE(document.execute(DeleteOpeningCommand{wall, door.value()}).ok());
	CHECK(document.undoLabel() == "Delete opening");
	CHECK(document.project().findWall(wall)->openings.size() == 1);
	REQUIRE(document.undo().ok());
	CHECK(document.project().findWall(wall)->findOpening(door.value()) != nullptr);
	CHECK(document.execute(DeleteOpeningCommand{wall, OpeningId{Uuid{9, 9}}}).error().code == ErrorCode::NotFound);
}

TEST_CASE("a wall change that would leave an opening outside the wall is refused")
{
	Document document{makeSequentialUuidSource()};
	const WallId wall = document.addWall({0, 0}, {4000, 0}, 120, 2400).value();
	REQUIRE(document.addOpening(wall, Opening{OpeningId{}, OpeningKind::Door, 2500, 900, 2100, 0}).ok());
	const Result<Transaction> shorter = document.execute(ChangeWallLengthCommand{wall, 3000});
	REQUIRE_FALSE(shorter.ok());
	CHECK(shorter.error().message.find("does not fit") != std::string::npos);
	CHECK_FALSE(document.execute(ChangeWallHeightCommand{wall, 2100}).ok()); // no lintel left
	CHECK(document.project().findWall(wall)->end == Point2D{4000, 0});
}

TEST_CASE_FIXTURE(TwoWalls, "whole wall translation only moves joints on its level and preserves finishes")
{
	const LevelId upper = document.addLevel(3000, 300).value();
	const WallId stacked = document.addWall({0, 0}, {3550, 0}, 120, 2400, upper).value();
	REQUIRE(document.execute(SetWallFinishCommand{first, WallSide::Left, "plaster-white"}).ok());
	const std::string before = writeProjectJson(document.project());
	REQUIRE(document.execute(TranslateWallCommand{first, {311, -417}}).ok());
	CHECK(wall(first).start == Point2D{311, -417});
	CHECK(wall(first).end == Point2D{3861, -417});
	CHECK(wall(second).start == wall(first).end);
	CHECK(wall(second).end == Point2D{3550, 4000});
	CHECK(wall(stacked).start == Point2D{0, 0});
	CHECK(wall(stacked).end == Point2D{3550, 0});
	CHECK(wall(first).finishLeft == "plaster-white");
	CHECK(changes.back().modifiedWalls.size() == 2);
	const std::string after = writeProjectJson(document.project());
	REQUIRE(document.undo().ok());
	CHECK(writeProjectJson(document.project()) == before);
	REQUIRE(document.redo().ok());
	CHECK(writeProjectJson(document.project()) == after);
}

TEST_CASE_FIXTURE(TwoWalls, "whole wall translation rejects collapsed neighbours and out of range deltas atomically")
{
	const std::string before = writeProjectJson(document.project());
	CHECK_FALSE(document.execute(TranslateWallCommand{first, {0, 4000}}).ok());
	CHECK_FALSE(document.execute(TranslateWallCommand{first, {std::numeric_limits<std::int64_t>::max(), 0}}).ok());
	CHECK_FALSE(document.execute(TranslateWallCommand{first, {MaxAbsMillimeters, 0}}).ok());
	CHECK_FALSE(document.execute(TranslateWallCommand{WallId{}, {100, 0}}).ok());
	CHECK(writeProjectJson(document.project()) == before);
	CHECK(changes.empty());
	CHECK(document.undoLabel() == "Create wall");
	REQUIRE(document.execute(TranslateWallCommand{first, {0, 0}}).ok());
	CHECK(changes.empty());
	CHECK(document.undoLabel() == "Create wall");
}

TEST_CASE_FIXTURE(TwoWalls, "whole wall translation rejects an attached wall shortened through its opening")
{
	REQUIRE(document.addOpening(second, Opening{OpeningId{}, OpeningKind::Door, 2500, 900, 2100, 0}).ok());
	const std::string before = writeProjectJson(document.project());
	CHECK_FALSE(document.execute(TranslateWallCommand{first, {0, 1000}}).ok());
	CHECK(writeProjectJson(document.project()) == before);
}

TEST_CASE("whole wall translation retains automatic splitting and undoes the entire new junction")
{
	Document document{makeSequentialUuidSource()};
	const WallId moving = document.addWall({0, 0}, {0, 4000}, 120, 2800).value();
	REQUIRE(document.addWall({1000, 2000}, {4000, 2000}, 120, 2800).ok());
	const std::string before = writeProjectJson(document.project());
	REQUIRE(document.execute(TranslateWallCommand{moving, {2000, 0}}).ok());
	CHECK(document.project().walls.size() == 4);
	CHECK(document.project().findWall(moving)->start == Point2D{2000, 0});
	CHECK(document.project().findWall(moving)->end == Point2D{2000, 2000});
	const std::string after = writeProjectJson(document.project());
	REQUIRE(document.undo().ok());
	CHECK(writeProjectJson(document.project()) == before);
	REQUIRE(document.redo().ok());
	CHECK(writeProjectJson(document.project()) == after);
}
