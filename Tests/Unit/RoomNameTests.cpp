#include <doctest/doctest.h>

#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Scene/SceneCompiler.h"

#include <algorithm>

using namespace f101;
using namespace f101::cad;

namespace
{
	Project enclosedRoom(const Document& document)
	{
		Project project = document.project();
		project.walls = {
			Wall{WallId{Uuid{0, 100}}, {0, 0}, {10000, 0}, 120, 2800},
			Wall{WallId{Uuid{0, 101}}, {10000, 0}, {10000, 4000}, 120, 2800},
			Wall{WallId{Uuid{0, 102}}, {10000, 4000}, {0, 4000}, 120, 2800},
			Wall{WallId{Uuid{0, 103}}, {0, 4000}, {0, 0}, 120, 2800}};
		for (Wall& wall : project.walls) wall.level = project.levels.front().id;
		return project;
	}
}

TEST_CASE("room names are reversible independent edits; blank restores the automatic name")
{
	Document document{makeSequentialUuidSource()};
	EditController editor{document, EditSettings{}};
	CHECK_FALSE(editor.setSelectedRoomName("Cocina").ok());
	REQUIRE(editor.selectRoom("room-a").ok());
	REQUIRE(editor.setSelectedRoomFloor("tile-grey").ok());
	std::vector<std::string> notified;
	document.subscribe([&](const ChangeSet& changes) { notified = changes.changedRoomNames; });
	REQUIRE(editor.setSelectedRoomName("  Cocina  ").ok());
	CHECK(document.project().findRoomName("room-a")->name == "Cocina");
	CHECK(notified == std::vector<std::string>{"room-a"});
	CHECK(document.execute(SetRoomNameCommand{"room-a", "Cocina"}).value().empty());
	REQUIRE(editor.setSelectedRoomName("Sala de estar").ok());
	REQUIRE(editor.undo().ok());
	CHECK(document.project().findRoomName("room-a")->name == "Cocina");
	REQUIRE(editor.redo().ok());
	CHECK(document.project().findRoomName("room-a")->name == "Sala de estar");
	REQUIRE(editor.setSelectedRoomName("   ").ok());
	CHECK(document.project().roomNames.empty());
	CHECK(document.project().findRoomFinish("room-a")->floor == "tile-grey");
	REQUIRE(editor.undo().ok());
	CHECK(document.project().findRoomName("room-a")->name == "Sala de estar");
}

TEST_CASE("invalid room names leave the project and history unchanged")
{
	Document document{makeSequentialUuidSource()};
	const auto before = writeProjectJson(document.project());
	CHECK_FALSE(document.execute(SetRoomNameCommand{"", "Cocina"}).ok());
	CHECK_FALSE(document.execute(SetRoomNameCommand{"room-a", std::string(MaxRoomNameBytes + 1, 'x')}).ok());
	CHECK_FALSE(document.execute(SetRoomNameCommand{"room-a", "Sala\nCocina"}).ok());
	CHECK_FALSE(document.execute(SetRoomNameCommand{"room-a", std::string("A\0B", 3)}).ok());
	CHECK(writeProjectJson(document.project()) == before);
	CHECK_FALSE(document.canUndo());
}

TEST_CASE("custom UTF-8 room names round trip and old projects keep automatic names")
{
	Document document{makeSequentialUuidSource()};
	REQUIRE(document.execute(SetRoomNameCommand{"room-a", "Baño & cocina \"principal\""}).ok());
	REQUIRE(document.execute(SetRoomNameCommand{"room-b", "Dormitorio {n}"}).ok());
	const auto encoded = writeProjectJson(document.project());
	const auto decoded = readProjectJson(encoded);
	REQUIRE(decoded.ok());
	CHECK(decoded.value().roomNames == document.project().roomNames);
	CHECK(writeProjectJson(decoded.value()) == encoded);
	const auto legacy = readProjectJson(R"({"format":"101st-floor-project","schemaVersion":10,"units":"mm","levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[]})");
	REQUIRE(legacy.ok());
	CHECK(legacy.value().roomNames.empty());
}

TEST_CASE("malformed and duplicate room names cannot be loaded")
{
	const std::string head = R"({"format":"101st-floor-project","schemaVersion":11,"units":"mm","levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[],"roomNames":)";
	for (const std::string names : {R"({})", R"([{"room":"a","name":5}])", R"([{"room":"a"}])",
		R"([{"room":"a","name":""}])", R"([{"room":"a","name":"   "}])", R"([{"room":"","name":"Cocina"}])",
		R"([{"room":"a","name":"Cocina"},{"room":"a","name":"Sala"}])", R"([{"room":"a","name":"A\nB"}])"})
	{
		CHECK_FALSE(readProjectJson(head + names + "}").ok());
	}
}

TEST_CASE("plan and scene show custom names without changing geometry or automatic labels")
{
	Document document{makeSequentialUuidSource()};
	Project project = enclosedRoom(document);
	const auto room = geometry::detectRooms(project).front();
	project.roomNames.push_back({room.key, "Dormitorio {n}"});
	drawing::DrawingTexts texts;
	texts.roomName = "Habitación {n}";
	const auto plan = drawing::buildPlanDrawing(project, project.levels.front().id, {}, texts);
	CHECK(std::any_of(plan.labels.begin(), plan.labels.end(), [](const auto& label) { return label.text == "Dormitorio {n}"; }));
	const auto scene = scene::compileScene(project);
	CHECK(std::any_of(scene.nodes.begin(), scene.nodes.end(), [](const auto& node) { return node.name.starts_with("Dormitorio {n}"); }));
	CHECK(geometry::detectRooms(project).front().areaMm2 == room.areaMm2);
	project.roomNames.clear();
	const auto automatic = drawing::buildPlanDrawing(project, project.levels.front().id, {}, texts);
	CHECK(std::any_of(automatic.labels.begin(), automatic.labels.end(), [](const auto& label) { return label.text == "Habitación 1"; }));
}

TEST_CASE("automatic wall splitting inherits names; rooms can then be renamed independently")
{
	Document document{makeSequentialUuidSource()};
	document.setRoomNameTransfer(geometry::makeRoomNameTransfer());
	Project project = enclosedRoom(document);
	project.roomNames.push_back({geometry::detectRooms(project).front().key, "Sala"});
	REQUIRE(document.replaceProject(project).ok());
	REQUIRE(document.addWall({4000, 0}, {4000, 4000}, 120, 2800).ok());
	const auto rooms = geometry::detectRooms(document.project());
	REQUIRE(rooms.size() == 2);
	for (const auto& room : rooms)
	{
		REQUIRE(document.project().findRoomName(room.key) != nullptr);
		CHECK(document.project().findRoomName(room.key)->name == "Sala");
	}
	const auto inherited = document.project().roomNames;
	REQUIRE(document.undo().ok());
	CHECK(document.project().roomNames == project.roomNames);
	REQUIRE(document.redo().ok());
	CHECK(document.project().roomNames == inherited);
	REQUIRE(document.execute(SetRoomNameCommand{rooms.front().key, "Cocina"}).ok());
	CHECK(document.project().findRoomName(rooms.back().key)->name == "Sala");
}
