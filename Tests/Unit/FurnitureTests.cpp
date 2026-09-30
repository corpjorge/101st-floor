#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Geometry/FurnitureBuilder.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Scene/SceneCompiler.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::cad;

// Furniture (ADR-0016): the catalogue, the model and its commands, project.json schema 7, the shapes, the scene, the
// plan and the Furniture mode.
namespace
{
	Ray3 down(double x, double y)
	{
		return Ray3{x, y, 10000.0, 0.0, 0.0, -1.0};
	}

	struct Furnishing
	{
		Document document{makeSequentialUuidSource()};
		EditController editor{document, EditSettings{}};
		LevelId ground = document.project().levels.front().id;
	};

	std::size_t countFurnitureChanges(const std::vector<ChangeSet>& seen, std::vector<FurnitureId> ChangeSet::*list)
	{
		std::size_t count = 0;
		for (const ChangeSet& changes : seen)
		{
			count += (changes.*list).size();
		}
		return count;
	}
}

TEST_CASE("the catalogue has twelve types with defaults inside their ranges and a furniture finish")
{
	const std::vector<FurnitureType>& catalogue = furnitureCatalogue();
	CHECK(catalogue.size() == 12);
	std::set<std::string_view> ids;
	for (const FurnitureType& type : catalogue)
	{
		CAPTURE(type.id);
		CHECK(ids.insert(type.id).second);
		CHECK(findFurnitureType(type.id) == &type);
		CHECK(type.minWidthMm <= type.widthMm);
		CHECK(type.widthMm <= type.maxWidthMm);
		CHECK(type.minDepthMm <= type.depthMm);
		CHECK(type.depthMm <= type.maxDepthMm);
		CHECK(type.minHeightMm <= type.heightMm);
		CHECK(type.heightMm <= type.maxHeightMm);
		CHECK(validateFinish(type.bodyFinish, Surface::Furniture).ok());
		const Furniture piece = makeFurniture(type.id, {100, 200}, 9000);
		CHECK(piece.widthMm == type.widthMm);
		CHECK(piece.depthMm == type.depthMm);
		CHECK(piece.heightMm == type.heightMm);
		CHECK(piece.position == Point2D{100, 200});
		CHECK(piece.rotationCentidegrees == 9000);
		CHECK(validateFurniture(piece).ok());
		CHECK(resolveFurnitureFinish(piece) == type.bodyFinish);
	}
	for (const char* id : {"bed-double", "bed-single", "sofa", "table", "chair", "wardrobe", "kitchen-counter", "fridge", "toilet",
			 "washbasin", "bathtub", "shower-tray"})
	{
		CHECK(findFurnitureType(id) != nullptr);
	}
	CHECK(findFurnitureType("spaceship") == nullptr);
	CHECK_FALSE(findFurnitureType("table")->againstWall);
	CHECK_FALSE(findFurnitureType("chair")->againstWall);
	CHECK(findFurnitureType("sofa")->againstWall);
}

TEST_CASE("a piece is validated against its type: sizes, rotation, position and a finish for furniture")
{
	Furniture sofa = makeFurniture("sofa", {0, 0});
	CHECK(validateFurniture(sofa).ok());

	Furniture unknown = sofa;
	unknown.type = "spaceship";
	CHECK(validateFurniture(unknown).error().code == ErrorCode::NotFound);

	Furniture wide = sofa;
	wide.widthMm = 3201;
	CHECK(validateFurniture(wide).error().code == ErrorCode::OutOfRange);
	CHECK(validateFurniture(wide).error().message == "Its width goes from 1400 to 3200 mm");
	Furniture shallow = sofa;
	shallow.depthMm = 799;
	CHECK_FALSE(validateFurniture(shallow).ok());
	Furniture tall = sofa;
	tall.heightMm = 1001;
	CHECK_FALSE(validateFurniture(tall).ok());

	Furniture turned = sofa;
	turned.rotationCentidegrees = 36000;
	CHECK(validateFurniture(turned).error().code == ErrorCode::OutOfRange);
	turned.rotationCentidegrees = -1;
	CHECK_FALSE(validateFurniture(turned).ok());

	Furniture far = sofa;
	far.position.x_mm = MaxAbsMillimeters + 1;
	CHECK_FALSE(validateFurniture(far).ok());

	Furniture finished = sofa;
	finished.finish = "fabric-sand";
	CHECK(validateFurniture(finished).ok());
	CHECK(resolveFurnitureFinish(finished) == "fabric-sand");
	finished.finish = "brick-red"; // walls only
	CHECK(validateFurniture(finished).error().code == ErrorCode::InvalidArgument);
	finished.finish = "no-such-finish";
	CHECK(validateFurniture(finished).error().code == ErrorCode::NotFound);
}

TEST_CASE("rotations are whole hundredths of a degree in [0, 36000); quarter turns are exact")
{
	CHECK(toCentidegrees(0.0) == 0);
	CHECK(toCentidegrees(90.0) == 9000);
	CHECK(toCentidegrees(-90.0) == 27000);
	CHECK(toCentidegrees(360.0) == 0);
	CHECK(toCentidegrees(22.5) == 2250);
	CHECK(toCentidegrees(725.004) == 500);

	Furniture table = makeFurniture("table", {1000, 2000}, 9000); // 1600 x 900
	const auto corners = furnitureFootprint(table);
	// Front left, front right, back right, back left: its width now runs north, its back faces west.
	CHECK(corners[0].x_mm == 1450.0);
	CHECK(corners[0].y_mm == 1200.0);
	CHECK(corners[1].x_mm == 1450.0);
	CHECK(corners[1].y_mm == 2800.0);
	CHECK(corners[2].x_mm == 550.0);
	CHECK(corners[2].y_mm == 2800.0);
	CHECK(corners[3].x_mm == 550.0);
	CHECK(corners[3].y_mm == 1200.0);
	const PlanPoint local = planToFurniture(table, {1000.0, 2700.0});
	CHECK(local.x_mm == doctest::Approx(700.0));
	CHECK(local.y_mm == doctest::Approx(0.0));
	CHECK(furnitureContains(table, {1400.0, 2700.0}));
	CHECK_FALSE(furnitureContains(table, {1500.0, 2700.0}));
	CHECK(furnitureContains(table, {1500.0, 2700.0}, 60.0));

	Furniture turned = makeFurniture("sofa", {0, 0}, 4500);
	const PlanPoint corner = furnitureToPlan(turned, 1000.0, -450.0);
	CHECK(corner.x_mm == doctest::Approx((1000.0 + 450.0) / std::sqrt(2.0)));
	CHECK(corner.y_mm == doctest::Approx((1000.0 - 450.0) / std::sqrt(2.0)));
}

TEST_CASE_FIXTURE(Furnishing, "furniture is added, changed and deleted as undoable commands with change notifications")
{
	std::vector<ChangeSet> seen;
	document.subscribe([&seen](const ChangeSet& changes) { seen.push_back(changes); });

	Furniture sofa = makeFurniture("sofa", {3000, 550}, 18000);
	sofa.id = FurnitureId{Uuid{7, 7}}; // ignored: a new id is assigned
	const Result<FurnitureId> added = document.addFurniture(sofa);
	REQUIRE(added.ok());
	CHECK(added.value() != sofa.id);
	CHECK(document.undoLabel() == "Add furniture");
	REQUIRE(document.project().furniture.size() == 1);
	const Furniture& stored = document.project().furniture[0];
	CHECK(stored.id == added.value());
	CHECK(stored.level == ground); // nil level: the first one
	CHECK(stored.position == Point2D{3000, 550});
	CHECK(countFurnitureChanges(seen, &ChangeSet::addedFurniture) == 1);

	Furniture moved = stored;
	moved.position = Point2D{2000, 550};
	moved.finish = "fabric-sand";
	REQUIRE(document.execute(ChangeFurnitureCommand{moved}).ok());
	CHECK(document.undoLabel() == "Change furniture");
	CHECK(document.project().furniture[0] == moved);
	CHECK(countFurnitureChanges(seen, &ChangeSet::modifiedFurniture) == 1);

	// Nothing to change: no history entry.
	const Result<Transaction> same = document.execute(ChangeFurnitureCommand{moved});
	REQUIRE(same.ok());
	CHECK(same.value().empty());
	CHECK(document.undoLabel() == "Change furniture");

	REQUIRE(document.execute(DeleteFurnitureCommand{moved.id}).ok());
	CHECK(document.undoLabel() == "Delete furniture");
	CHECK(document.project().furniture.empty());
	CHECK(countFurnitureChanges(seen, &ChangeSet::removedFurniture) == 1);

	REQUIRE(document.undo().ok());
	REQUIRE(document.project().furniture.size() == 1);
	CHECK(document.project().furniture[0] == moved);
	REQUIRE(document.undo().ok());
	CHECK(document.project().furniture[0].position == Point2D{3000, 550});
	REQUIRE(document.undo().ok());
	CHECK(document.project().furniture.empty());
	REQUIRE(document.redo().ok());
	CHECK(document.project().furniture.size() == 1);
}

TEST_CASE_FIXTURE(Furnishing, "furniture commands are rejected when invalid, leaving the project untouched")
{
	Furniture piece = makeFurniture("table", {0, 0});
	piece.level = LevelId{Uuid{9, 9}};
	CHECK(document.addFurniture(piece).error().code == ErrorCode::NotFound);

	Furniture huge = makeFurniture("table", {0, 0});
	huge.widthMm = 5000;
	CHECK(document.addFurniture(huge).error().code == ErrorCode::OutOfRange);

	Furniture missing = makeFurniture("table", {0, 0});
	missing.id = FurnitureId{Uuid{1, 2}};
	CHECK(document.execute(ChangeFurnitureCommand{missing}).error().code == ErrorCode::NotFound);
	CHECK(document.execute(DeleteFurnitureCommand{missing.id}).error().code == ErrorCode::NotFound);

	const FurnitureId table = document.addFurniture(makeFurniture("table", {0, 0})).value();
	Furniture changed = *document.project().findFurniture(table);
	changed.rotationCentidegrees = 40000;
	CHECK(document.execute(ChangeFurnitureCommand{changed}).error().code == ErrorCode::OutOfRange);
	CHECK(document.project().findFurniture(table)->rotationCentidegrees == 0);
	CHECK(document.undoLabel() == "Add furniture");
}

TEST_CASE_FIXTURE(Furnishing, "deleting a level removes its furniture in the same step; undo brings it back")
{
	const LevelId upper = document.addLevel().value();
	Furniture bed = makeFurniture("bed-double", {1000, 1000});
	bed.level = upper;
	const FurnitureId upstairs = document.addFurniture(bed).value();
	const FurnitureId downstairs = document.addFurniture(makeFurniture("table", {0, 0})).value();

	REQUIRE(document.execute(DeleteLevelCommand{upper}).ok());
	REQUIRE(document.project().furniture.size() == 1);
	CHECK(document.project().furniture[0].id == downstairs);

	REQUIRE(document.undo().ok());
	REQUIRE(document.project().furniture.size() == 2);
	CHECK(document.project().furniture[0].id == upstairs); // back in its place
	CHECK(document.project().findLevel(upper) != nullptr);
}

TEST_CASE_FIXTURE(Furnishing, "a whole project is checked: unique furniture ids on existing levels")
{
	Project project = document.project();
	Furniture piece = makeFurniture("chair", {0, 0});
	piece.id = FurnitureId{Uuid{0, 50}};
	piece.level = ground;
	project.furniture = {piece};
	CHECK(validateProject(project).ok());

	project.furniture = {piece, piece};
	CHECK_FALSE(validateProject(project).ok());

	Furniture nowhere = piece;
	nowhere.level = LevelId{Uuid{0, 99}};
	project.furniture = {nowhere};
	CHECK_FALSE(validateProject(project).ok());

	Furniture nil = piece;
	nil.id = FurnitureId{};
	project.furniture = {nil};
	CHECK_FALSE(validateProject(project).ok());

	std::vector<ChangeSet> seen;
	document.subscribe([&seen](const ChangeSet& changes) { seen.push_back(changes); });
	project.furniture = {piece};
	REQUIRE(document.replaceProject(project).ok());
	CHECK(document.project().furniture.size() == 1);
	REQUIRE(seen.size() == 1);
	CHECK(seen[0].addedFurniture == std::vector<FurnitureId>{piece.id});
}

TEST_CASE("project.json schema 7 saves furniture only when there is some, and reads it back")
{
	Document document{makeSequentialUuidSource(100)};
	Furniture sofa = makeFurniture("sofa", {3000, 550}, 18000);
	sofa.finish = "fabric-sand";
	REQUIRE(document.addFurniture(sofa).ok());
	REQUIRE(document.addFurniture(makeFurniture("table", {-1200, 2400}, 4500)).ok());

	const std::string json = writeProjectJson(document.project());
	CHECK(json.find("\"schemaVersion\": 7") != std::string::npos);
	CHECK(json.find(
		"  \"furniture\": [\n"
		"    {\n"
		"      \"id\": \"00000000-0000-0000-0000-000000000065\",\n"
		"      \"type\": \"sofa\",\n"
		"      \"level\": \"00000000-0000-0000-0000-000000000064\",\n"
		"      \"position\": [\n"
		"        3000,\n"
		"        550\n"
		"      ],\n"
		"      \"rotationCentidegrees\": 18000,\n"
		"      \"widthMm\": 2000,\n"
		"      \"depthMm\": 900,\n"
		"      \"heightMm\": 850,\n"
		"      \"finish\": \"fabric-sand\"\n"
		"    },\n") != std::string::npos);

	const Result<Project> loaded = readProjectJson(json);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().furniture == document.project().furniture);
	CHECK(writeProjectJson(loaded.value()) == json);

	Document empty{makeSequentialUuidSource()};
	CHECK(writeProjectJson(empty.project()).find("furniture") == std::string::npos);
}

TEST_CASE("project.json furniture is refused when malformed or invalid")
{
	const std::string head = R"({"format":"101st-floor-project","schemaVersion":7,"units":"mm",)"
		R"("levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[],)";
	auto withPiece = [&head](const std::string& fields)
	{
		return head + R"("furniture":[{"id":"00000000-0000-0000-0000-000000000065","level":"00000000-0000-0000-0000-000000000064",)" +
			fields + "}]}";
	};
	const std::string good = R"("type":"table","position":[0,0],"rotationCentidegrees":9000,"widthMm":1600,"depthMm":900,"heightMm":750)";
	REQUIRE(readProjectJson(withPiece(good)).ok());
	CHECK(readProjectJson(withPiece(good)).value().furniture[0].rotationCentidegrees == 9000);

	CHECK(readProjectJson(withPiece(R"("type":"table","position":[0,0],"rotationCentidegrees":36000,"widthMm":1600,"depthMm":900,"heightMm":750)"))
			  .error().code == ErrorCode::ParseError);
	CHECK(readProjectJson(withPiece(R"("type":"table","position":[0,0],"widthMm":1600.5,"depthMm":900,"heightMm":750)"))
			  .error().code == ErrorCode::ParseError);
	CHECK(readProjectJson(withPiece(R"("type":"table","position":[0],"widthMm":1600,"depthMm":900,"heightMm":750)"))
			  .error().code == ErrorCode::ParseError);
	CHECK(readProjectJson(withPiece(R"("position":[0,0],"widthMm":1600,"depthMm":900,"heightMm":750)")).error().code ==
		ErrorCode::ParseError);
	const Result<Project> spaceship =
		readProjectJson(withPiece(R"("type":"spaceship","position":[0,0],"widthMm":1600,"depthMm":900,"heightMm":750)"));
	REQUIRE_FALSE(spaceship.ok());
	CHECK(spaceship.error().message == "Invalid project file: Unknown furniture type 'spaceship'");
	CHECK_FALSE(readProjectJson(withPiece(R"("type":"table","position":[0,0],"widthMm":100,"depthMm":900,"heightMm":750)")).ok());
	CHECK(readProjectJson(head + R"("furniture":{}})").error().code == ErrorCode::ParseError);
	CHECK_FALSE(readProjectJson(head + R"("furniture":[{"id":"00000000-0000-0000-0000-000000000065","level":)"
		R"("00000000-0000-0000-0000-000000000099","type":"table","position":[0,0],"widthMm":1600,"depthMm":900,"heightMm":750}]})").ok());
}

TEST_CASE("every type builds closed boxes inside its footprint, and details inside its outline, at any allowed size")
{
	for (const FurnitureType& type : furnitureCatalogue())
	{
		struct Size
		{
			std::int64_t width;
			std::int64_t depth;
			std::int64_t height;
		};
		for (const Size size : {Size{type.widthMm, type.depthMm, type.heightMm}, Size{type.minWidthMm, type.minDepthMm, type.minHeightMm},
				 Size{type.maxWidthMm, type.maxDepthMm, type.maxHeightMm}})
		{
			CAPTURE(type.id);
			CAPTURE(size.width);
			Furniture piece = makeFurniture(type.id, {0, 0});
			piece.widthMm = size.width;
			piece.depthMm = size.depth;
			piece.heightMm = size.height;
			REQUIRE(validateFurniture(piece).ok());
			const geometry::FurnitureShape shape = geometry::furnitureShape(piece);
			CHECK(shape.boxes.size() >= 2);
			const double w = static_cast<double>(size.width) / 2.0;
			const double d = static_cast<double>(size.depth) / 2.0;
			const double h = static_cast<double>(size.height);
			for (const geometry::FurnitureBox& box : shape.boxes)
			{
				CHECK(box.x0 < box.x1);
				CHECK(box.y0 < box.y1);
				CHECK(box.z0 < box.z1);
				CHECK(box.x0 >= -w);
				CHECK(box.x1 <= w);
				CHECK(box.y0 >= -d - 30.0); // handles stand out of the front
				CHECK(box.y1 <= d);
				CHECK(box.z0 >= 0.0);
				CHECK(box.z1 <= h + 260.0); // taps rise above
			}
			for (const geometry::FurnitureOutline& detail : shape.planDetails)
			{
				CHECK(detail.size() >= 4);
				for (const PlanPoint& point : detail)
				{
					CHECK(std::fabs(point.x_mm) <= w + 1e-9);
					CHECK(std::fabs(point.y_mm) <= d + 1e-9);
				}
			}
			const geometry::MeshData mesh = geometry::buildFurnitureMesh(piece);
			CHECK(mesh.triangleCount() == shape.boxes.size() * 12);
			CHECK(mesh.normals.size() == mesh.positions.size());
			CHECK(mesh.uvs.size() == mesh.positions.size());
			const geometry::Bounds3 bounds = geometry::computeBounds(mesh);
			CHECK(bounds.min.z >= geometry::FloorThicknessMm - 1e-9); // on the floor, or hung on the wall (a washbasin)
		}
	}
	Furniture unknown = makeFurniture("table", {0, 0});
	unknown.type = "spaceship";
	CHECK(geometry::furnitureShape(unknown).boxes.empty());
	CHECK(geometry::buildFurnitureMesh(unknown).sections.empty());
}

TEST_CASE("a piece's mesh is turned and placed; each section gets its finish")
{
	const Furniture table = makeFurniture("table", {1000, 2000}, 9000);
	const geometry::Bounds3 bounds = geometry::computeBounds(geometry::buildFurnitureMesh(table));
	CHECK(bounds.min.x == doctest::Approx(550.0));
	CHECK(bounds.max.x == doctest::Approx(1450.0));
	CHECK(bounds.min.y == doctest::Approx(1200.0));
	CHECK(bounds.max.y == doctest::Approx(2800.0));
	CHECK(bounds.max.z == doctest::Approx(geometry::FloorThicknessMm + 750.0));

	Furniture kitchen = makeFurniture("kitchen-counter", {0, 0});
	const geometry::MeshData mesh = geometry::buildFurnitureMesh(kitchen);
	std::set<std::string> slots;
	for (const geometry::MeshSection& section : mesh.sections)
	{
		slots.insert(section.slot);
		const std::string_view finish = geometry::furnitureSlotFinish(kitchen, section.slot);
		CHECK(validateFinish(finish, Surface::Furniture).ok());
	}
	CHECK(slots == std::set<std::string>{"body", "metal", "stone", "dark"});
	CHECK(geometry::furnitureSlotFinish(kitchen, geometry::furniture_slots::Body) == "lacquer-white");
	CHECK(geometry::furnitureSlotFinish(kitchen, geometry::furniture_slots::Stone) == "stone-dark");
	kitchen.finish = "wood-walnut";
	CHECK(geometry::furnitureSlotFinish(kitchen, geometry::furniture_slots::Body) == "wood-walnut");
	CHECK(geometry::furnitureSlotFinish(kitchen, geometry::furniture_slots::Metal) == "steel-brushed");
	const Furniture bed = makeFurniture("bed-double", {0, 0});
	CHECK(geometry::furnitureSlotFinish(bed, geometry::furniture_slots::Linen) == "linen-white");
	CHECK(geometry::furnitureShape(bed).planDetails.size() == 3); // two pillows and the duvet
	CHECK(geometry::furnitureShape(makeFurniture("bed-single", {0, 0})).planDetails.size() == 2);
}

TEST_CASE_FIXTURE(Furnishing, "the scene has a node per piece, lifted to its level, with view styles from the overlay")
{
	const LevelId upper = document.addLevel(3000).value();
	const FurnitureId sofa = document.addFurniture(makeFurniture("sofa", {3000, 550}, 18000)).value();
	Furniture bed = makeFurniture("bed-double", {1000, 3000});
	bed.level = upper;
	const FurnitureId upstairs = document.addFurniture(bed).value();

	const scene::Scene all = scene::compileScene(document.project());
	auto find = [](const scene::Scene& scene, const std::string& id) -> const scene::SceneNode*
	{
		const auto found = std::find_if(scene.nodes.begin(), scene.nodes.end(), [&id](const scene::SceneNode& node) { return node.id.value == id; });
		return found == scene.nodes.end() ? nullptr : &*found;
	};
	const scene::SceneNode* sofaNode = find(all, "furniture/" + toString(sofa));
	REQUIRE(sofaNode != nullptr);
	CHECK(sofaNode->id == scene::furnitureNodeId(sofa));
	CHECK(sofaNode->sourceKind == scene::SourceKind::Furniture);
	CHECK(sofaNode->sourceId == toString(sofa));
	CHECK(sofaNode->level == ground);
	CHECK(sofaNode->style == scene::NodeStyle::Normal);
	CHECK(sofaNode->finishes.size() == sofaNode->mesh.sections.size());
	CHECK(std::find(sofaNode->finishes.begin(), sofaNode->finishes.end(), "fabric-grey") != sofaNode->finishes.end());
	const scene::SceneNode* bedNode = find(all, "furniture/" + toString(upstairs));
	REQUIRE(bedNode != nullptr);
	CHECK(geometry::computeBounds(bedNode->mesh).min.z == doctest::Approx(2800.0 + geometry::FloorThicknessMm));

	scene::ViewOverlay overlay;
	overlay.activeLevel = ground;
	overlay.selectedFurniture = sofa;
	Furniture placing = makeFurniture("chair", {500, 500});
	placing.level = ground;
	overlay.previewFurniture = {placing};
	const scene::Scene groundView = scene::compileScene(document.project(), overlay);
	CHECK(find(groundView, "furniture/" + toString(sofa))->style == scene::NodeStyle::Selected);
	CHECK(find(groundView, "furniture/" + toString(upstairs)) == nullptr); // above the active level
	REQUIRE(find(groundView, "preview/furniture/0") != nullptr);
	CHECK(find(groundView, "preview/furniture/0")->style == scene::NodeStyle::Preview);

	Furniture dragged = *document.project().findFurniture(sofa);
	dragged.position = Point2D{2000, 550};
	overlay.previewFurniture = {dragged};
	const scene::Scene dragging = scene::compileScene(document.project(), overlay);
	const scene::SceneNode* draggedNode = find(dragging, "furniture/" + toString(sofa));
	REQUIRE(draggedNode != nullptr);
	CHECK(draggedNode->style == scene::NodeStyle::Preview);
	CHECK(geometry::computeBounds(draggedNode->mesh).min.x == doctest::Approx(1000.0));

	overlay = scene::ViewOverlay{};
	overlay.activeLevel = upper;
	const scene::Scene upperView = scene::compileScene(document.project(), overlay);
	CHECK(find(upperView, "furniture/" + toString(sofa))->style == scene::NodeStyle::Context);
	CHECK(find(upperView, "furniture/" + toString(upstairs))->style == scene::NodeStyle::Normal);
}

TEST_CASE_FIXTURE(Furnishing, "furniture is picked by its box on the active level only")
{
	const LevelId upper = document.addLevel().value();
	const FurnitureId sofa = document.addFurniture(makeFurniture("sofa", {0, 0}, 4500)).value();
	CHECK(pickFurniture(document.project(), down(600.0, 0.0), ground)->furniture == sofa);
	CHECK(pickFurniture(document.project(), down(600.0, 0.0), ground)->distance == doctest::Approx(10000.0 - 850.0));
	CHECK_FALSE(pickFurniture(document.project(), down(0.0, 800.0), ground).has_value()); // off its turned footprint
	CHECK_FALSE(pickFurniture(document.project(), down(600.0, 0.0), upper).has_value());
	CHECK_FALSE(pickFurniture(document.project(), Ray3{0.0, 0.0, 10000.0, 0.0, 0.0, 1.0}, ground).has_value()); // looking up
}

TEST_CASE_FIXTURE(Furnishing, "Furniture mode: a free piece follows the pointer on the grid; Space turns the next ones; a click adds it")
{
	editor.setMode(EditMode::Furniture);
	REQUIRE(editor.setFurnitureType("table").ok());
	CHECK(editor.setFurnitureType("spaceship").error().code == ErrorCode::NotFound);
	editor.pointerMoved(down(2013.0, 1987.0));
	REQUIRE(editor.furniturePlacement().has_value());
	CHECK(editor.furniturePlacement()->position == Point2D{2000, 2000});
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 0);
	CHECK(editor.furniturePlacement()->level == ground);
	CHECK(editor.furniturePlacement()->id.isNil());
	REQUIRE(editor.view().previewFurniture.size() == 1);
	CHECK(editor.view().previewFurniture[0] == *editor.furniturePlacement());

	REQUIRE(editor.flip().ok());
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 9000);
	REQUIRE(editor.pointerPressed(down(2013.0, 1987.0)).ok());
	REQUIRE(editor.pointerReleased(down(2013.0, 1987.0)).ok());
	REQUIRE(document.project().furniture.size() == 1);
	CHECK(document.undoLabel() == "Add furniture");
	CHECK(document.project().furniture[0].type == "table");
	CHECK(document.project().furniture[0].position == Point2D{2000, 2000});
	CHECK(document.project().furniture[0].rotationCentidegrees == 9000);
	CHECK(editor.furniturePlacement().has_value()); // ready for the next one

	editor.cancel(); // Esc leaves the mode
	CHECK(editor.mode() == EditMode::Select);
	CHECK_FALSE(editor.furniturePlacement().has_value());
	CHECK(editor.view().previewFurniture.empty());
}

TEST_CASE_FIXTURE(Furnishing, "Furniture mode: a piece that goes against walls turns its back to the nearest face and touches it")
{
	document.addWall({0, 0}, {6000, 0}, 200, 2600).value();
	editor.setMode(EditMode::Furniture); // a sofa by default
	editor.pointerMoved(down(3020.0, 700.0));
	REQUIRE(editor.furniturePlacement().has_value());
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 18000); // facing north, away from the wall
	CHECK(editor.furniturePlacement()->position == Point2D{3000, 550});
	const auto corners = furnitureFootprint(*editor.furniturePlacement());
	CHECK(corners[2].y_mm == doctest::Approx(100.0)); // its back on the wall's face
	CHECK(corners[3].y_mm == doctest::Approx(100.0));

	editor.pointerMoved(down(3020.0, -700.0)); // the other face
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 0);
	CHECK(editor.furniturePlacement()->position == Point2D{3000, -550});

	editor.pointerMoved(down(3020.0, 1500.0)); // out of reach: free, as turned by Space
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 0);
	CHECK(editor.furniturePlacement()->position == Point2D{3000, 1500});
	REQUIRE(editor.flip().ok());
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 9000);
	editor.pointerMoved(down(3020.0, 700.0)); // back near the wall: the wall decides
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 18000);
}

TEST_CASE_FIXTURE(Furnishing, "Furniture mode: sliding along a wall into a corner stops at the other wall's face")
{
	document.addWall({0, 0}, {6000, 0}, 200, 2600).value();
	document.addWall({0, 0}, {0, 4000}, 200, 2600).value();
	editor.setMode(EditMode::Furniture);
	editor.pointerMoved(down(900.0, 560.0));
	REQUIRE(editor.furniturePlacement().has_value());
	CHECK(editor.furniturePlacement()->rotationCentidegrees == 18000);
	CHECK(editor.furniturePlacement()->position == Point2D{1100, 550}); // its side on the west wall's face (x = 100)

	// Along a diagonal wall the rotation follows the wall to a hundredth of a degree.
	Document diagonal{makeSequentialUuidSource()};
	EditController tilted{diagonal, EditSettings{}};
	diagonal.addWall({0, 0}, {3000, 4000}, 200, 2600).value();
	tilted.setMode(EditMode::Furniture);
	REQUIRE(tilted.setFurnitureType("wardrobe").ok());
	tilted.pointerMoved(down(1000.0, 2000.0)); // left of the wall (north-west of it)
	REQUIRE(tilted.furniturePlacement().has_value());
	// Out of the wall towards the left: (-0.8, 0.6); facing that way means a rotation of atan2(-0.8, -0.6).
	CHECK(tilted.furniturePlacement()->rotationCentidegrees == toCentidegrees(std::atan2(-0.8, -0.6) * 180.0 / 3.14159265358979323846));
	const auto corners = furnitureFootprint(*tilted.furniturePlacement());
	for (const PlanPoint& back : {corners[2], corners[3]})
	{
		const double across = (back.x_mm * -0.8 + back.y_mm * 0.6); // distance from the axis on the left
		CHECK(across == doctest::Approx(100.0).epsilon(0.005));
	}
}

TEST_CASE_FIXTURE(Furnishing, "Select mode: a click selects a piece, dragging moves it in one undoable change")
{
	const FurnitureId table = document.addFurniture(makeFurniture("table", {2000, 2000})).value();
	REQUIRE(editor.pointerPressed(down(2000.0, 2000.0)).ok());
	CHECK(editor.selectedFurniture()->id == table);
	CHECK(editor.view().selectedFurniture == table);
	CHECK(editor.selection().isNil());
	CHECK(editor.isDraggingFurniture());
	CHECK(editor.view().previewFurniture.empty()); // pressing alone moves nothing

	editor.pointerMoved(down(2512.0, 2263.0));
	REQUIRE(editor.view().previewFurniture.size() == 1);
	CHECK(editor.view().previewFurniture[0].id == table);
	CHECK(editor.view().previewFurniture[0].position == Point2D{2500, 2250});
	CHECK(document.project().furniture[0].position == Point2D{2000, 2000});

	REQUIRE(editor.pointerReleased(down(2512.0, 2263.0)).ok());
	CHECK_FALSE(editor.isDragging());
	CHECK(document.project().furniture[0].position == Point2D{2500, 2250});
	CHECK(document.undoLabel() == "Change furniture");
	CHECK(editor.selectedFurniture()->id == table);

	REQUIRE(editor.undo().ok());
	CHECK(document.project().furniture[0].position == Point2D{2000, 2000});

	// A click without moving changes nothing; Esc during a drag stops it, then clears the selection.
	REQUIRE(editor.pointerPressed(down(2100.0, 2100.0)).ok());
	REQUIRE(editor.pointerReleased(down(2100.0, 2100.0)).ok());
	CHECK(document.undoLabel() == "Add furniture");
	REQUIRE(editor.pointerPressed(down(2100.0, 2100.0)).ok());
	editor.pointerMoved(down(3100.0, 2100.0));
	editor.cancel();
	CHECK_FALSE(editor.isDragging());
	CHECK(editor.selectedFurniture().has_value());
	REQUIRE(editor.pointerReleased(down(3100.0, 2100.0)).ok());
	CHECK(document.project().furniture[0].position == Point2D{2000, 2000});
	editor.cancel();
	CHECK_FALSE(editor.selectedFurniture().has_value());

	// Empty floor clears it.
	REQUIRE(editor.select(WallId{}).ok());
	REQUIRE(editor.selectFurniture(table).ok());
	REQUIRE(editor.pointerPressed(down(8000.0, 8000.0)).ok());
	CHECK_FALSE(editor.selectedFurniture().has_value());
}

TEST_CASE_FIXTURE(Furnishing, "dragging a piece along its wall keeps it on the pointer's side even when its grab offset crosses the wall")
{
	document.addWall({0, 0}, {6000, 0}, 200, 2600).value();
	const FurnitureId wardrobe = document.addFurniture(makeFurniture("wardrobe", {3000, 400}, 18000)).value(); // back on y = 100
	// Grabbed near its front (250 mm north of its centre), dragged east with the pointer close to the wall's face: the
	// centre it asks for is south of the axis, but the pointer is north of it.
	REQUIRE(editor.pointerPressed(down(3000.0, 650.0)).ok());
	REQUIRE(editor.selectedFurniture()->id == wardrobe);
	editor.pointerMoved(down(4000.0, 150.0));
	REQUIRE(editor.view().previewFurniture.size() == 1);
	CHECK(editor.view().previewFurniture[0].rotationCentidegrees == 18000);
	CHECK(editor.view().previewFurniture[0].position == Point2D{4000, 400});
	REQUIRE(editor.pointerReleased(down(4000.0, 150.0)).ok());
	CHECK(document.project().findFurniture(wardrobe)->position == Point2D{4000, 400});

	// Pointing past the wall moves it to the other face.
	REQUIRE(editor.pointerPressed(down(4000.0, 400.0)).ok());
	editor.pointerMoved(down(4000.0, -500.0));
	REQUIRE(editor.pointerReleased(down(4000.0, -500.0)).ok());
	CHECK(document.project().findFurniture(wardrobe)->position == Point2D{4000, -400});
	CHECK(document.project().findFurniture(wardrobe)->rotationCentidegrees == 0);
}

TEST_CASE_FIXTURE(Furnishing, "a piece in front of a wall is picked before it; the wall behind is picked elsewhere")
{
	const WallId wall = document.addWall({0, 0}, {6000, 0}, 200, 2600).value();
	const FurnitureId sofa = document.addFurniture(makeFurniture("sofa", {3000, 550}, 18000)).value();
	// Looking north-down from the south-west: the ray meets the sofa before the wall behind it.
	REQUIRE(editor.pointerPressed(Ray3{3000.0, -4000.0, 3000.0, 0.0, 1.0, -0.5}).ok());
	CHECK_FALSE(editor.selectedFurniture().has_value()); // the wall is in front: it is picked
	CHECK(editor.selection() == wall);
	REQUIRE(editor.pointerReleased(down(0.0, 0.0)).ok());
	REQUIRE(editor.pointerPressed(Ray3{3000.0, 4000.0, 3000.0, 0.0, -1.0, -0.6}).ok()); // from the north: the sofa first
	CHECK(editor.selectedFurniture()->id == sofa);
	CHECK(editor.selection().isNil());
}

TEST_CASE_FIXTURE(Furnishing, "the selected piece is edited, turned with Space and deleted")
{
	const FurnitureId sofa = document.addFurniture(makeFurniture("sofa", {3000, 550}, 18000)).value();
	CHECK(editor.setFurnitureWidth("180").error().code == ErrorCode::NotFound); // nothing selected
	REQUIRE(editor.selectFurniture(sofa).ok());
	CHECK(editor.selectFurniture(FurnitureId{Uuid{5, 5}}).error().code == ErrorCode::NotFound);

	REQUIRE(editor.setFurnitureWidth("240").ok()); // centimetres by default
	CHECK(document.project().furniture[0].widthMm == 2400);
	CHECK(editor.setFurnitureWidth("10 m").error().code == ErrorCode::OutOfRange);
	CHECK(editor.setFurnitureWidth("wide").error().code == ErrorCode::ParseError);
	REQUIRE(editor.setFurnitureDepth("95 cm").ok());
	CHECK(document.project().furniture[0].depthMm == 950);
	REQUIRE(editor.setFurnitureHeight("0.8 m").ok());
	CHECK(document.project().furniture[0].heightMm == 800);
	REQUIRE(editor.setFurnitureRotation("45").ok());
	CHECK(document.project().furniture[0].rotationCentidegrees == 4500);
	REQUIRE(editor.setFurnitureRotation("-90\xC2\xB0").ok());
	CHECK(document.project().furniture[0].rotationCentidegrees == 27000);
	REQUIRE(editor.setFurnitureRotation("22,5").ok());
	CHECK(document.project().furniture[0].rotationCentidegrees == 2250);
	CHECK(editor.setFurnitureRotation("north").error().code == ErrorCode::InvalidArgument);
	REQUIRE(editor.setFurnitureFinish("fabric-sand").ok());
	CHECK(document.project().furniture[0].finish == "fabric-sand");
	CHECK_FALSE(editor.setFurnitureFinish("brick-red").ok());
	REQUIRE(editor.setFurnitureFinish("").ok());
	CHECK(document.project().furniture[0].finish.empty());

	REQUIRE(editor.flip().ok()); // Space turns it
	CHECK(document.project().furniture[0].rotationCentidegrees == 11250);
	REQUIRE(editor.rotateFurniture().ok());
	CHECK(document.project().furniture[0].rotationCentidegrees == 20250);
	CHECK(document.project().furniture[0].position == Point2D{3000, 550}); // about its centre

	REQUIRE(editor.deleteSelection().ok());
	CHECK(document.project().furniture.empty());
	CHECK(document.undoLabel() == "Delete furniture");
	CHECK_FALSE(editor.selectedFurniture().has_value());
	CHECK(editor.flip().error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(Furnishing, "furniture is edited on the active level; changing level drops its selection")
{
	const LevelId upper = document.addLevel().value();
	Furniture bed = makeFurniture("bed-double", {2000, 2000});
	bed.level = upper;
	const FurnitureId upstairs = document.addFurniture(bed).value();

	REQUIRE(editor.pointerPressed(down(2000.0, 2000.0)).ok()); // on the ground level: nothing there
	CHECK_FALSE(editor.selectedFurniture().has_value());
	REQUIRE(editor.pointerReleased(down(2000.0, 2000.0)).ok());

	REQUIRE(editor.selectFurniture(upstairs).ok()); // selecting it goes to its level
	CHECK(editor.activeLevel() == upper);
	REQUIRE(editor.levelDown().ok());
	CHECK_FALSE(editor.selectedFurniture().has_value());

	REQUIRE(editor.setActiveLevel(upper).ok());
	editor.setMode(EditMode::Furniture);
	editor.pointerMoved(down(1000.0, 1000.0)); // taken down to the upper level's floor
	REQUIRE(editor.furniturePlacement().has_value());
	CHECK(editor.furniturePlacement()->level == upper);
	CHECK(editor.furniturePlacement()->position == Point2D{1000, 1000});
}

TEST_CASE_FIXTURE(Furnishing, "plans draw each piece's outline and details on its level, tinted by the overlay")
{
	const LevelId upper = document.addLevel().value();
	const FurnitureId bed = document.addFurniture(makeFurniture("bed-double", {2000, 2000})).value();
	Furniture table = makeFurniture("table", {0, 0});
	table.level = upper;
	REQUIRE(document.addFurniture(table).ok());

	auto strokes = [](const drawing::PlanDrawing& plan, drawing::Ink ink)
	{
		return static_cast<std::size_t>(std::count_if(plan.strokes.begin(), plan.strokes.end(),
			[ink](const drawing::Stroke& stroke) { return stroke.ink == ink && stroke.closed; }));
	};
	const drawing::PlanDrawing plan = drawing::buildPlanDrawing(document.project(), ground);
	CHECK(strokes(plan, drawing::Ink::Furniture) == 4); // the bed's outline, two pillows and the duvet; not the table
	CHECK(plan.min.x_mm == doctest::Approx(1250.0 - 4.0)); // half the line width
	CHECK(plan.max.y_mm == doctest::Approx(3000.0 + 4.0));

	drawing::PlanOverlay overlay;
	overlay.selectedFurniture = bed;
	Furniture chair = makeFurniture("chair", {5000, 0});
	chair.level = ground;
	overlay.previewFurniture = {chair};
	const drawing::PlanDrawing editing = drawing::buildPlanDrawing(document.project(), ground, overlay);
	CHECK(strokes(editing, drawing::Ink::Selected) == 4);
	CHECK(strokes(editing, drawing::Ink::Preview) == 2); // the chair and its back
	CHECK(strokes(editing, drawing::Ink::Furniture) == 0);

	CHECK(strokes(drawing::buildPlanDrawing(document.project(), upper), drawing::Ink::Furniture) == 1);
	const drawing::Rgb grey = drawing::inkColor(drawing::Ink::Furniture);
	CHECK(grey.r == 96);
}
