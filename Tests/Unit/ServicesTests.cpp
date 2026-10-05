#include "doctest.h"

#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Geometry/PolygonGeometry.h"
#include "F101/Geometry/ServicesBuilder.h"
#include "F101/Scene/SceneCompiler.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::cad;

// Columns and building services (ADR-0021): the model and its commands, project.json schema 10, the solids, the scene
// with its services layer, the plan, picking and the Column and Services modes.
namespace
{
	Ray3 down(double x, double y)
	{
		return Ray3{x, y, 10000.0, 0.0, 0.0, -1.0};
	}

	struct Building
	{
		Document document{makeSequentialUuidSource()};
		EditController editor{document, EditSettings{}};
		LevelId ground = document.project().levels.front().id;
	};

	ServiceRun runOf(ServiceKind kind, std::vector<ServicePoint> points, LevelId level = {})
	{
		ServiceRun run;
		run.level = level;
		run.kind = kind;
		run.points = std::move(points);
		return run;
	}

	const scene::SceneNode* findNode(const scene::Scene& scene, const std::string& id)
	{
		const auto found = std::find_if(scene.nodes.begin(), scene.nodes.end(), [&id](const scene::SceneNode& node) { return node.id.value == id; });
		return found == scene.nodes.end() ? nullptr : &*found;
	}

	std::size_t count(const drawing::PlanDrawing& plan, drawing::Ink ink)
	{
		return static_cast<std::size_t>(std::count_if(plan.strokes.begin(), plan.strokes.end(),
			[ink](const drawing::Stroke& stroke) { return stroke.ink == ink; }));
	}

	std::size_t fills(const drawing::PlanDrawing& plan, drawing::Ink ink)
	{
		return static_cast<std::size_t>(std::count_if(plan.fills.begin(), plan.fills.end(),
			[ink](const drawing::Fill& fill) { return fill.ink == ink; }));
	}

	// Distance from a point to the segment a-b.
	double toSegment(const geometry::Vec3& p, const ServicePoint& a, const ServicePoint& b)
	{
		const double ax = static_cast<double>(a.x_mm);
		const double ay = static_cast<double>(a.y_mm);
		const double az = static_cast<double>(a.z_mm);
		const double sx = static_cast<double>(b.x_mm) - ax;
		const double sy = static_cast<double>(b.y_mm) - ay;
		const double sz = static_cast<double>(b.z_mm) - az;
		const double t = std::clamp(((p.x - ax) * sx + (p.y - ay) * sy + (p.z - az) * sz) / (sx * sx + sy * sy + sz * sz), 0.0, 1.0);
		return std::sqrt(std::pow(p.x - ax - sx * t, 2) + std::pow(p.y - ay - sy * t, 2) + std::pow(p.z - az - sz * t, 2));
	}
}

TEST_CASE("columns are rectangles or round, checked by size, shape, rotation and a finish for walls")
{
	const Column square = makeColumn(ColumnShape::Rectangle, {1000, 2000});
	CHECK(square.widthMm == DefaultColumnSizeMm);
	CHECK(square.depthMm == DefaultColumnSizeMm);
	CHECK(square.position == Point2D{1000, 2000});
	CHECK(validateColumn(square).ok());
	CHECK(resolveColumnFinish(square) == "concrete");
	CHECK(validateFinish(DefaultColumnFinish, Surface::Wall).ok());

	const Column round = makeColumn(ColumnShape::Round, {0, 0}, 400, 250, 4500);
	CHECK(round.depthMm == 400); // one diameter
	CHECK(round.rotationCentidegrees == 0);
	CHECK(validateColumn(round).ok());

	Column thin = square;
	thin.widthMm = MinColumnSizeMm - 1;
	CHECK(validateColumn(thin).error().code == ErrorCode::OutOfRange);
	thin.widthMm = MaxColumnSizeMm + 1;
	CHECK(validateColumn(thin).error().code == ErrorCode::OutOfRange);
	Column oval = round;
	oval.depthMm = 300;
	CHECK(validateColumn(oval).error().code == ErrorCode::InvalidArgument);
	Column turnedRound = round;
	turnedRound.rotationCentidegrees = 9000;
	CHECK(validateColumn(turnedRound).error().code == ErrorCode::InvalidArgument);
	Column turned = square;
	turned.rotationCentidegrees = FullTurnCentidegrees;
	CHECK(validateColumn(turned).error().code == ErrorCode::OutOfRange);
	Column piped = square;
	piped.finish = "service-water"; // only for runs
	CHECK_FALSE(validateColumn(piped).ok());
	Column painted = square;
	painted.finish = "paint-sage";
	CHECK(validateColumn(painted).ok());
	CHECK(resolveColumnFinish(painted) == "paint-sage");

	CHECK(columnShapeId(ColumnShape::Round) == "round");
	CHECK(parseColumnShape("rectangle") == ColumnShape::Rectangle);
	CHECK_FALSE(parseColumnShape("oval").has_value());
}

TEST_CASE("a column's footprint turns about its centre, exact at quarter turns; round ones contain their circle")
{
	const Column column = makeColumn(ColumnShape::Rectangle, {1000, 2000}, 600, 300, 9000);
	const std::array<PlanPoint, 4> corners = columnFootprint(column);
	CHECK(corners[0].x_mm == 1150.0);
	CHECK(corners[0].y_mm == 1700.0);
	CHECK(corners[1].x_mm == 1150.0);
	CHECK(corners[1].y_mm == 2300.0);
	CHECK(corners[2].x_mm == 850.0);
	CHECK(corners[3].y_mm == 1700.0);
	CHECK(geometry::signedArea(std::vector<PlanPoint>(corners.begin(), corners.end())) == doctest::Approx(600.0 * 300.0));
	CHECK(columnContains(column, {1100.0, 2250.0}));
	CHECK_FALSE(columnContains(column, {1200.0, 2000.0}));
	CHECK(columnContains(column, {1200.0, 2000.0}, 60.0));

	const Column round = makeColumn(ColumnShape::Round, {0, 0}, 400);
	CHECK(columnContains(round, {0.0, 190.0}));
	CHECK_FALSE(columnContains(round, {150.0, 150.0}));
	const std::vector<PlanPoint> outline = geometry::columnOutline(round);
	REQUIRE(outline.size() == static_cast<std::size_t>(geometry::RoundColumnSides));
	CHECK(geometry::signedArea(outline) > 0.0);
	for (const PlanPoint& point : outline)
	{
		CHECK(std::hypot(point.x_mm, point.y_mm) == doctest::Approx(200.0));
	}
	CHECK(geometry::columnOutline(column).size() == 4);
}

TEST_CASE("four kinds of run, each with its colour and diameter; runs are checked point by point")
{
	REQUIRE(serviceKinds().size() == 4);
	const std::vector<std::pair<ServiceKind, Rgb8>> colours{{ServiceKind::Electrical, {46, 156, 72}}, {ServiceKind::Water, {238, 238, 234}},
		{ServiceKind::Sewage, {232, 186, 34}}, {ServiceKind::Gas, {212, 172, 64}}};
	for (const auto& [kind, colour] : colours)
	{
		const ServiceKindInfo& info = serviceKindInfo(kind);
		CAPTURE(info.id);
		CHECK(info.kind == kind);
		CHECK(parseServiceKind(info.id) == kind);
		CHECK(validateFinish(info.finish, Surface::Services).ok());
		CHECK_FALSE(validateFinish(info.finish, Surface::Wall).ok());
		REQUIRE(findFinish(info.finish) != nullptr);
		CHECK(findFinish(info.finish)->color == colour);
	}
	CHECK(serviceKindInfo(ServiceKind::Electrical).id == "electrical");
	CHECK(serviceKindInfo(ServiceKind::Sewage).diameterMm > serviceKindInfo(ServiceKind::Water).diameterMm);
	CHECK(serviceKindInfo(ServiceKind::Gas).diameterMm < 30);
	CHECK_FALSE(parseServiceKind("steam").has_value());

	const ServiceRun run = runOf(ServiceKind::Water, {{0, 0, 300}, {2000, 0, 300}, {2000, 0, 2400}});
	CHECK(validateServiceRun(run).ok());
	CHECK(serviceRunLengthMm(run) == doctest::Approx(4100.0));
	CHECK(validateServiceRun(runOf(ServiceKind::Water, {{0, 0, 300}})).error().code == ErrorCode::InvalidArgument);
	CHECK(validateServiceRun(runOf(ServiceKind::Water, {{0, 0, 300}, {0, 0, 300}, {10, 0, 300}})).error().code ==
		ErrorCode::InvalidArgument);
	CHECK(validateServiceRun(runOf(ServiceKind::Water, {{0, 0, 300}, {0, 0, MaxServiceHeightMm + 1}})).error().code ==
		ErrorCode::OutOfRange);
	CHECK(validateServiceRun(runOf(ServiceKind::Water, {{0, 0, MinServiceHeightMm}, {0, 0, 0}})).ok());
	CHECK(validateServiceRun(runOf(ServiceKind::Water, std::vector<ServicePoint>(MaxServicePoints + 1, ServicePoint{}))).error().code ==
		ErrorCode::InvalidArgument);
}

TEST_CASE_FIXTURE(Building, "columns and runs are added, changed and deleted as undoable commands with change notifications")
{
	std::vector<ChangeSet> seen;
	document.subscribe([&seen](const ChangeSet& changes) { seen.push_back(changes); });

	Column column = makeColumn(ColumnShape::Rectangle, {1000, 1000});
	column.id = ColumnId{Uuid{7, 7}}; // ignored: a new id is assigned
	const Result<ColumnId> added = document.addColumn(column);
	REQUIRE(added.ok());
	CHECK(added.value() != column.id);
	CHECK(document.undoLabel() == "Add column");
	REQUIRE(document.project().columns.size() == 1);
	CHECK(document.project().columns[0].level == ground); // nil level: the first one
	REQUIRE(seen.size() == 1);
	CHECK(seen[0].addedColumns == std::vector<ColumnId>{added.value()});

	Column moved = document.project().columns[0];
	moved.position = Point2D{2000, 1000};
	moved.finish = "brick-red";
	REQUIRE(document.execute(ChangeColumnCommand{moved}).ok());
	CHECK(document.undoLabel() == "Change column");
	CHECK(document.project().columns[0] == moved);
	CHECK(seen.back().modifiedColumns == std::vector<ColumnId>{moved.id});
	CHECK(document.execute(ChangeColumnCommand{moved}).value().empty()); // nothing to change
	CHECK(document.undoLabel() == "Change column");

	const Result<ServiceRunId> run = document.addServiceRun(runOf(ServiceKind::Gas, {{0, 0, 2400}, {3000, 0, 2400}}));
	REQUIRE(run.ok());
	CHECK(document.undoLabel() == "Add service run");
	REQUIRE(document.project().services.size() == 1);
	CHECK(document.project().services[0].level == ground);
	CHECK(seen.back().addedServiceRuns == std::vector<ServiceRunId>{run.value()});

	ServiceRun changed = document.project().services[0];
	changed.kind = ServiceKind::Water;
	changed.points.push_back(ServicePoint{3000, 2000, 2400});
	REQUIRE(document.execute(ChangeServiceRunCommand{changed}).ok());
	CHECK(document.undoLabel() == "Change service run");
	CHECK(document.project().services[0] == changed);
	CHECK(seen.back().modifiedServiceRuns == std::vector<ServiceRunId>{run.value()});

	REQUIRE(document.execute(DeleteServiceRunCommand{run.value()}).ok());
	CHECK(document.undoLabel() == "Delete service run");
	CHECK(seen.back().removedServiceRuns == std::vector<ServiceRunId>{run.value()});
	REQUIRE(document.execute(DeleteColumnCommand{moved.id}).ok());
	CHECK(document.undoLabel() == "Delete column");
	CHECK(seen.back().removedColumns == std::vector<ColumnId>{moved.id});
	CHECK(document.project().columns.empty());
	CHECK(document.project().services.empty());

	REQUIRE(document.undo().ok());
	CHECK(document.project().columns[0] == moved);
	REQUIRE(document.undo().ok());
	CHECK(document.project().services[0] == changed);
	REQUIRE(document.undo().ok());
	CHECK(document.project().services[0].kind == ServiceKind::Gas);
	REQUIRE(document.redo().ok());
	CHECK(document.project().services[0].kind == ServiceKind::Water);
}

TEST_CASE_FIXTURE(Building, "column and run commands are rejected when invalid, leaving the project untouched")
{
	Column nowhere = makeColumn(ColumnShape::Rectangle, {0, 0});
	nowhere.level = LevelId{Uuid{9, 9}};
	CHECK(document.addColumn(nowhere).error().code == ErrorCode::NotFound);
	CHECK(document.addColumn(makeColumn(ColumnShape::Rectangle, {0, 0}, 50)).error().code == ErrorCode::OutOfRange);
	Column glass = makeColumn(ColumnShape::Rectangle, {0, 0});
	glass.finish = "asset:00000000-0000-4000-8000-000000000001"; // no such asset
	CHECK_FALSE(document.addColumn(glass).ok());
	Column missing = makeColumn(ColumnShape::Rectangle, {0, 0});
	missing.id = ColumnId{Uuid{1, 2}};
	CHECK(document.execute(ChangeColumnCommand{missing}).error().code == ErrorCode::NotFound);
	CHECK(document.execute(DeleteColumnCommand{missing.id}).error().code == ErrorCode::NotFound);

	CHECK_FALSE(document.addServiceRun(runOf(ServiceKind::Water, {{0, 0, 0}})).ok());
	ServiceRun stray = runOf(ServiceKind::Water, {{0, 0, 0}, {10, 0, 0}});
	stray.level = LevelId{Uuid{9, 9}};
	CHECK(document.addServiceRun(stray).error().code == ErrorCode::NotFound);
	stray.id = ServiceRunId{Uuid{1, 3}};
	stray.level = ground;
	CHECK(document.execute(ChangeServiceRunCommand{stray}).error().code == ErrorCode::NotFound);
	CHECK(document.execute(DeleteServiceRunCommand{stray.id}).error().code == ErrorCode::NotFound);
	CHECK(document.project().columns.empty());
	CHECK(document.project().services.empty());
	CHECK_FALSE(document.canUndo());
}

TEST_CASE_FIXTURE(Building, "deleting a level removes its columns and runs in the same step; undo brings them back")
{
	const LevelId upper = document.addLevel().value();
	Column column = makeColumn(ColumnShape::Round, {0, 0});
	column.level = upper;
	REQUIRE(document.addColumn(column).ok());
	const ColumnId below = document.addColumn(makeColumn(ColumnShape::Rectangle, {0, 0})).value();
	REQUIRE(document.addServiceRun(runOf(ServiceKind::Sewage, {{0, 0, -100}, {0, 3000, -100}}, upper)).ok());

	REQUIRE(document.execute(DeleteLevelCommand{upper}).ok());
	REQUIRE(document.project().columns.size() == 1);
	CHECK(document.project().columns[0].id == below);
	CHECK(document.project().services.empty());
	REQUIRE(document.undo().ok());
	CHECK(document.project().columns.size() == 2);
	CHECK(document.project().services.size() == 1);
}

TEST_CASE_FIXTURE(Building, "a whole project is checked: unique column and run ids on existing levels")
{
	Project project = document.project();
	Column column = makeColumn(ColumnShape::Rectangle, {0, 0});
	column.id = ColumnId{Uuid{0, 50}};
	column.level = ground;
	ServiceRun run = runOf(ServiceKind::Electrical, {{0, 0, 300}, {100, 0, 300}}, ground);
	run.id = ServiceRunId{Uuid{0, 51}};
	project.columns = {column};
	project.services = {run};
	CHECK(validateProject(project).ok());

	project.columns = {column, column};
	CHECK_FALSE(validateProject(project).ok());
	project.columns = {column};
	project.services = {run, run};
	CHECK_FALSE(validateProject(project).ok());
	ServiceRun stray = run;
	stray.level = LevelId{Uuid{0, 99}};
	project.services = {stray};
	CHECK_FALSE(validateProject(project).ok());
	Column nil = column;
	nil.id = ColumnId{};
	project.services = {run};
	project.columns = {nil};
	CHECK_FALSE(validateProject(project).ok());

	std::vector<ChangeSet> seen;
	document.subscribe([&seen](const ChangeSet& changes) { seen.push_back(changes); });
	project.columns = {column};
	REQUIRE(document.replaceProject(project).ok());
	REQUIRE(seen.size() == 1);
	CHECK(seen[0].addedColumns == std::vector<ColumnId>{column.id});
	CHECK(seen[0].addedServiceRuns == std::vector<ServiceRunId>{run.id});
}

TEST_CASE("project.json schema 10 saves columns and runs only when there are some, and reads them back")
{
	Document document{makeSequentialUuidSource(100)};
	Column column = makeColumn(ColumnShape::Rectangle, {1000, 2000}, 400, 300, 9000);
	column.finish = "brick-red";
	REQUIRE(document.addColumn(column).ok());
	REQUIRE(document.addColumn(makeColumn(ColumnShape::Round, {-500, 0}, 350)).ok());
	REQUIRE(document.addServiceRun(runOf(ServiceKind::Gas, {{0, 0, -100}, {0, 0, 2400}, {3000, 0, 2400}})).ok());

	const std::string json = writeProjectJson(document.project());
	CHECK(json.find("\"schemaVersion\": 11") != std::string::npos);
	CHECK(json.find(
		"  \"columns\": [\n"
		"    {\n"
		"      \"id\": \"00000000-0000-0000-0000-000000000065\",\n"
		"      \"level\": \"00000000-0000-0000-0000-000000000064\",\n"
		"      \"position\": [\n"
		"        1000,\n"
		"        2000\n"
		"      ],\n"
		"      \"shape\": \"rectangle\",\n"
		"      \"widthMm\": 400,\n"
		"      \"depthMm\": 300,\n"
		"      \"rotationCentidegrees\": 9000,\n"
		"      \"finish\": \"brick-red\"\n"
		"    },\n") != std::string::npos);
	CHECK(json.find("\"shape\": \"round\"") != std::string::npos);
	CHECK(json.find(
		"  \"services\": [\n"
		"    {\n"
		"      \"id\": \"00000000-0000-0000-0000-000000000067\",\n"
		"      \"level\": \"00000000-0000-0000-0000-000000000064\",\n"
		"      \"kind\": \"gas\",\n"
		"      \"points\": [\n"
		"        [\n"
		"          0,\n"
		"          0,\n"
		"          -100\n"
		"        ],\n") != std::string::npos);

	const Result<Project> loaded = readProjectJson(json);
	REQUIRE(loaded.ok());
	CHECK(loaded.value().columns == document.project().columns);
	CHECK(loaded.value().services == document.project().services);
	CHECK(writeProjectJson(loaded.value()) == json);

	Document empty{makeSequentialUuidSource()};
	const std::string bare = writeProjectJson(empty.project());
	CHECK(bare.find("columns") == std::string::npos);
	CHECK(bare.find("services") == std::string::npos);
}

TEST_CASE("project.json columns and runs are refused when malformed or invalid")
{
	const std::string head = R"({"format":"101st-floor-project","schemaVersion":10,"units":"mm",)"
		R"("levels":[{"id":"00000000-0000-0000-0000-000000000064","heightMm":2800,"slabThicknessMm":250}],"walls":[],)";
	const std::string ids = R"("id":"00000000-0000-0000-0000-000000000065","level":"00000000-0000-0000-0000-000000000064",)";
	auto withColumn = [&](const std::string& fields) { return head + R"("columns":[{)" + ids + fields + "}]}"; };
	auto withRun = [&](const std::string& fields) { return head + R"("services":[{)" + ids + fields + "}]}"; };

	const Result<Project> column = readProjectJson(withColumn(R"("position":[10,20],"shape":"round","widthMm":350,"depthMm":350)"));
	REQUIRE(column.ok());
	CHECK(column.value().columns[0].shape == ColumnShape::Round);
	CHECK(column.value().columns[0].rotationCentidegrees == 0); // optional
	CHECK(column.value().columns[0].finish.empty());
	CHECK(readProjectJson(withColumn(R"("position":[0,0],"shape":"oval","widthMm":350,"depthMm":350)")).error().code ==
		ErrorCode::ParseError);
	CHECK(readProjectJson(withColumn(R"("position":[0,0],"shape":"rectangle","widthMm":350.5,"depthMm":350)")).error().code ==
		ErrorCode::ParseError);
	CHECK(readProjectJson(withColumn(R"("position":[0,0],"shape":"rectangle","widthMm":350,"depthMm":350,"rotationCentidegrees":36000)"))
			  .error().code == ErrorCode::ParseError);
	CHECK_FALSE(readProjectJson(withColumn(R"("position":[0,0],"shape":"round","widthMm":350,"depthMm":300)")).ok());
	CHECK_FALSE(readProjectJson(withColumn(R"("position":[0,0],"shape":"rectangle","widthMm":50,"depthMm":300)")).ok());

	const Result<Project> run = readProjectJson(withRun(R"("kind":"sewage","points":[[0,0,-100],[0,4000,-100]])"));
	REQUIRE(run.ok());
	CHECK(run.value().services[0].kind == ServiceKind::Sewage);
	CHECK(run.value().services[0].points[1] == ServicePoint{0, 4000, -100});
	CHECK(readProjectJson(withRun(R"("kind":"steam","points":[[0,0,0],[1,0,0]])")).error().code == ErrorCode::ParseError);
	CHECK(readProjectJson(withRun(R"("kind":"gas","points":[[0,0],[1,0]])")).error().code == ErrorCode::ParseError);
	CHECK(readProjectJson(withRun(R"("kind":"gas","points":[[0,0,0.5],[1,0,0]])")).error().code == ErrorCode::ParseError);
	CHECK(readProjectJson(withRun(R"("kind":"gas","points":{})")).error().code == ErrorCode::ParseError);
	CHECK_FALSE(readProjectJson(withRun(R"("kind":"gas","points":[[0,0,0]])")).ok());
	CHECK(readProjectJson(head + R"("columns":{}})").error().code == ErrorCode::ParseError);
	CHECK_FALSE(readProjectJson(head + R"("services":[{"id":"00000000-0000-0000-0000-000000000065","level":)"
		R"("00000000-0000-0000-0000-000000000099","kind":"gas","points":[[0,0,0],[1,0,0]]}]})").ok());
}

TEST_CASE("a column is a box or a smooth cylinder as tall as its level, in one section")
{
	const Column square = makeColumn(ColumnShape::Rectangle, {1000, 2000}, 400, 300);
	const geometry::MeshData box = geometry::buildColumnMesh(square, 2800.0);
	REQUIRE(box.sections.size() == 1);
	CHECK(box.sections[0].slot == geometry::services_slots::Column);
	CHECK(box.triangleCount() == 12);
	const geometry::Bounds3 boxBounds = geometry::computeBounds(box);
	CHECK(boxBounds.min.x == doctest::Approx(800.0));
	CHECK(boxBounds.max.y == doctest::Approx(2150.0));
	CHECK(boxBounds.min.z == doctest::Approx(0.0));
	CHECK(boxBounds.max.z == doctest::Approx(2800.0));

	const Column round = makeColumn(ColumnShape::Round, {0, 0}, 400);
	const geometry::MeshData cylinder = geometry::buildColumnMesh(round, 3000.0);
	REQUIRE(cylinder.sections.size() == 1);
	const geometry::Bounds3 cylinderBounds = geometry::computeBounds(cylinder);
	CHECK(cylinderBounds.max.x == doctest::Approx(200.0));
	CHECK(cylinderBounds.min.y == doctest::Approx(-200.0));
	CHECK(cylinderBounds.max.z == doctest::Approx(3000.0));
	for (std::size_t index = 0; index < cylinder.positions.size(); ++index)
	{
		const geometry::Vec3& p = cylinder.positions[index];
		CHECK(std::hypot(p.x, p.y) <= 200.0 + 1e-6);
		CHECK(std::fabs(std::hypot(cylinder.normals[index].x, std::hypot(cylinder.normals[index].y, cylinder.normals[index].z)) - 1.0) < 1e-9);
	}
	CHECK(geometry::buildColumnMesh(square, 0.0).sections.empty());
}

TEST_CASE("a run is a tube of its kind's diameter along its points, joined at its bends, with flat ends")
{
	const ServiceRun run = runOf(ServiceKind::Water, {{0, 0, 300}, {2000, 0, 300}, {2000, 0, 2400}, {2000, 1500, 2400}});
	const geometry::MeshData mesh = geometry::buildServiceRunMesh(run);
	REQUIRE(mesh.sections.size() == 1);
	CHECK(mesh.sections[0].slot == geometry::services_slots::Pipe);
	CHECK(mesh.triangleCount() > 0);
	const double radius = static_cast<double>(serviceKindInfo(ServiceKind::Water).diameterMm) / 2.0;
	for (const geometry::Vec3& p : mesh.positions)
	{
		double nearest = 1e18;
		for (std::size_t index = 1; index < run.points.size(); ++index)
		{
			nearest = std::min(nearest, toSegment(p, run.points[index - 1], run.points[index]));
		}
		CHECK(nearest <= radius + 1e-6);
	}
	const geometry::Bounds3 bounds = geometry::computeBounds(mesh);
	CHECK(bounds.min.x == doctest::Approx(0.0)); // its flat start
	CHECK(bounds.max.y == doctest::Approx(1500.0)); // its flat end
	CHECK(bounds.max.z == doctest::Approx(2400.0 + radius).epsilon(0.001)); // the facets reach at least its radius times cos 15 degrees
	CHECK(bounds.min.z == doctest::Approx(300.0 - radius).epsilon(0.002));

	const geometry::Bounds3 sewer = geometry::computeBounds(geometry::buildServiceRunMesh(runOf(ServiceKind::Sewage, {{0, 0, 0}, {1000, 0, 0}})));
	CHECK(sewer.max.z - sewer.min.z == doctest::Approx(110.0).epsilon(0.05));
	CHECK(geometry::buildServiceRunMesh(runOf(ServiceKind::Gas, {{0, 0, 0}})).sections.empty());
}

TEST_CASE_FIXTURE(Building, "the scene shows columns always, and runs with the services layer, the architecture ghosted")
{
	const WallId wall = document.addWall({0, 0}, {4000, 0}, 120, 2800).value();
	const LevelId upper = document.addLevel(3000).value();
	const ColumnId column = document.addColumn(makeColumn(ColumnShape::Rectangle, {2000, 0}, 300, 300)).value();
	const ServiceRunId run = document.addServiceRun(runOf(ServiceKind::Electrical, {{0, 0, 300}, {4000, 0, 300}})).value();
	Column above = makeColumn(ColumnShape::Round, {0, 0});
	above.level = upper;
	const ColumnId upstairs = document.addColumn(above).value();

	// The whole project (exports): every column and run.
	const scene::Scene all = scene::compileScene(document.project());
	const scene::SceneNode* columnNode = findNode(all, "column/" + toString(column));
	REQUIRE(columnNode != nullptr);
	CHECK(columnNode->id == scene::columnNodeId(column));
	CHECK(columnNode->sourceKind == scene::SourceKind::Column);
	CHECK(columnNode->sourceId == toString(column));
	CHECK(columnNode->finishes == std::vector<std::string>{"concrete"});
	CHECK(geometry::computeBounds(columnNode->mesh).max.z == doctest::Approx(2800.0));
	const scene::SceneNode* runNode = findNode(all, "service/" + toString(run));
	REQUIRE(runNode != nullptr);
	CHECK(runNode->id == scene::serviceRunNodeId(run));
	CHECK(runNode->sourceKind == scene::SourceKind::Service);
	CHECK(runNode->finishes == std::vector<std::string>{"service-electrical"});
	CHECK(runNode->style == scene::NodeStyle::Normal);
	CHECK(findNode(all, scene::wallBodyNodeId(wall).value)->style == scene::NodeStyle::Normal);
	const geometry::Bounds3 upstairsBounds = geometry::computeBounds(findNode(all, "column/" + toString(upstairs))->mesh);
	CHECK(upstairsBounds.min.z == doctest::Approx(2800.0)); // on the level above, as tall as it
	CHECK(upstairsBounds.max.z == doctest::Approx(5800.0));

	// Editing without the layer: columns, no runs.
	scene::ViewOverlay overlay;
	overlay.activeLevel = ground;
	overlay.selectedColumn = column;
	const scene::Scene plain = scene::compileScene(document.project(), overlay);
	CHECK(findNode(plain, "column/" + toString(column))->style == scene::NodeStyle::Selected);
	CHECK(findNode(plain, "service/" + toString(run)) == nullptr);
	CHECK(findNode(plain, scene::wallBodyNodeId(wall).value)->style == scene::NodeStyle::Normal);

	// With the layer: the runs, and the walls see-through; columns stay solid.
	overlay.services = true;
	overlay.selectedColumn = ColumnId();
	overlay.selectedServiceRun = run;
	const scene::Scene layered = scene::compileScene(document.project(), overlay);
	CHECK(findNode(layered, "service/" + toString(run))->style == scene::NodeStyle::Selected);
	CHECK(findNode(layered, scene::wallBodyNodeId(wall).value)->style == scene::NodeStyle::Ghost);
	CHECK(findNode(layered, "column/" + toString(column))->style == scene::NodeStyle::Normal);

	// Previews show even without the layer.
	overlay.services = false;
	Column placing = makeColumn(ColumnShape::Round, {1000, 1000});
	placing.level = ground;
	overlay.previewColumns = {placing};
	overlay.previewServiceRuns = {runOf(ServiceKind::Gas, {{0, 1000, 2400}, {3000, 1000, 2400}}, ground),
		runOf(ServiceKind::Gas, {{0, 1000, 2400}}, ground)}; // the second is invalid: left out
	const scene::Scene previewing = scene::compileScene(document.project(), overlay);
	REQUIRE(findNode(previewing, "preview/column/0") != nullptr);
	CHECK(findNode(previewing, "preview/column/0")->style == scene::NodeStyle::Preview);
	REQUIRE(findNode(previewing, "preview/service/0") != nullptr);
	CHECK(findNode(previewing, "preview/service/0")->style == scene::NodeStyle::Preview);
	CHECK(findNode(previewing, "preview/service/1") == nullptr);
	CHECK(findNode(previewing, "service/" + toString(run)) == nullptr);

	ServiceRun dragged = *document.project().findServiceRun(run);
	dragged.points[1].x_mm = 3000;
	overlay.previewServiceRuns = {dragged};
	const scene::Scene dragging = scene::compileScene(document.project(), overlay);
	const scene::SceneNode* draggedNode = findNode(dragging, "service/" + toString(run));
	REQUIRE(draggedNode != nullptr);
	CHECK(draggedNode->style == scene::NodeStyle::Preview);
	CHECK(geometry::computeBounds(draggedNode->mesh).max.x == doctest::Approx(3000.0));

	scene::ViewOverlay upperView;
	upperView.activeLevel = upper;
	const scene::Scene fromAbove = scene::compileScene(document.project(), upperView);
	CHECK(findNode(fromAbove, "column/" + toString(column))->style == scene::NodeStyle::Context);
	CHECK(findNode(fromAbove, "column/" + toString(upstairs))->style == scene::NodeStyle::Normal);
}

TEST_CASE_FIXTURE(Building, "columns are picked by their solid, runs within their radius plus a tolerance, on the active level")
{
	const LevelId upper = document.addLevel().value();
	const ColumnId square = document.addColumn(makeColumn(ColumnShape::Rectangle, {1000, 1000}, 400, 300)).value();
	const ColumnId round = document.addColumn(makeColumn(ColumnShape::Round, {3000, 0}, 400)).value();
	const Project& project = document.project();
	CHECK(pickColumn(project, down(1150.0, 1000.0), ground)->column == square);
	CHECK(pickColumn(project, down(1150.0, 1000.0), ground)->distance == doctest::Approx(10000.0 - 2800.0));
	CHECK_FALSE(pickColumn(project, down(1250.0, 1000.0), ground).has_value());
	CHECK(pickColumn(project, Ray3{0.0, 1000.0, 1000.0, 1.0, 0.0, 0.0}, ground)->distance == doctest::Approx(800.0));
	CHECK_FALSE(pickColumn(project, Ray3{0.0, 1000.0, 3000.0, 1.0, 0.0, 0.0}, ground).has_value()); // above it
	CHECK(pickColumn(project, down(3000.0, 190.0), ground)->column == round);
	CHECK_FALSE(pickColumn(project, down(3150.0, 150.0), ground).has_value()); // in its square, off its circle
	CHECK(pickColumn(project, Ray3{0.0, 0.0, 1000.0, 1.0, 0.0, 0.0}, ground)->distance == doctest::Approx(2800.0));
	CHECK_FALSE(pickColumn(project, down(1000.0, 1000.0), upper).has_value());

	const ServiceRunId ceiling = document.addServiceRun(runOf(ServiceKind::Water, {{0, 2000, 2400}, {4000, 2000, 2400}})).value();
	const ServiceRunId floor = document.addServiceRun(runOf(ServiceKind::Sewage, {{0, 2000, -100}, {4000, 2000, -100}})).value();
	const std::optional<ServiceRunHit> hit = pickServiceRun(document.project(), down(2000.0, 2050.0), ground, 80.0);
	REQUIRE(hit.has_value());
	CHECK(hit->run == ceiling); // nearer along the ray
	CHECK(hit->distance == doctest::Approx(10000.0 - 2400.0));
	CHECK(pickServiceRun(document.project(), down(2000.0, 2120.0), ground, 80.0)->run == floor); // the thick one reaches further
	CHECK_FALSE(pickServiceRun(document.project(), down(2000.0, 2200.0), ground, 80.0).has_value());
	CHECK(pickServiceRun(document.project(), Ray3{1000.0, 0.0, 2400.0, 0.0, 1.0, 0.0}, ground, 0.0)->distance == doctest::Approx(2000.0));
	CHECK_FALSE(pickServiceRun(document.project(), Ray3{1000.0, 3000.0, 2400.0, 0.0, 1.0, 0.0}, ground, 80.0).has_value()); // behind
	CHECK_FALSE(pickServiceRun(document.project(), down(2000.0, 2000.0), upper, 80.0).has_value());
}

TEST_CASE_FIXTURE(Building, "plans cut columns as solid fills and draw runs with the layer, a casing under their colour")
{
	const LevelId upper = document.addLevel().value();
	const ColumnId square = document.addColumn(makeColumn(ColumnShape::Rectangle, {1000, 1000})).value();
	REQUIRE(document.addColumn(makeColumn(ColumnShape::Round, {3000, 0}, 400)).ok());
	Column above = makeColumn(ColumnShape::Rectangle, {0, 0});
	above.level = upper;
	REQUIRE(document.addColumn(above).ok());
	const ServiceRunId water =
		document.addServiceRun(runOf(ServiceKind::Water, {{0, 2000, 300}, {2000, 2000, 300}, {2000, 2000, 2400}, {2000, 4000, 2400}})).value();
	REQUIRE(document.addServiceRun(runOf(ServiceKind::Sewage, {{0, 0, -100}, {0, 3000, -100}})).ok());

	const drawing::PlanDrawing plan = drawing::buildPlanDrawing(document.project(), ground);
	CHECK(fills(plan, drawing::Ink::Cut) == 2); // the two columns of the level
	const auto roundFill = std::find_if(plan.fills.begin(), plan.fills.end(),
		[](const drawing::Fill& fill) { return fill.outline.size() == static_cast<std::size_t>(geometry::RoundColumnSides); });
	REQUIRE(roundFill != plan.fills.end());
	CHECK(roundFill->triangles.size() == static_cast<std::size_t>(3 * (geometry::RoundColumnSides - 2)));
	CHECK(count(plan, drawing::Ink::ServiceCasing) == 0); // the layer is off

	drawing::PlanOverlay overlay;
	overlay.services = true;
	const drawing::PlanDrawing layered = drawing::buildPlanDrawing(document.project(), ground, overlay);
	// Two paths and the water's ring where it rises: each a casing and a core.
	CHECK(count(layered, drawing::Ink::ServiceCasing) == 3);
	CHECK(count(layered, drawing::Ink::ServiceWater) == 2);
	CHECK(count(layered, drawing::Ink::ServiceSewage) == 1);
	for (const drawing::Stroke& stroke : layered.strokes)
	{
		if (stroke.ink == drawing::Ink::ServiceWater && !stroke.closed)
		{
			CHECK(stroke.points.size() == 3); // the rise collapses in plan
			CHECK(stroke.widthMm == drawing::MinServiceCoreMm); // thinner than that is drawn that wide
		}
		if (stroke.ink == drawing::Ink::ServiceSewage)
		{
			CHECK(stroke.widthMm == 110.0);
		}
		if (stroke.ink == drawing::Ink::ServiceCasing && stroke.points.size() == 2)
		{
			CHECK(stroke.widthMm == 110.0 + 2.0 * drawing::ServiceCasingMm);
		}
	}
	CHECK(std::any_of(layered.strokes.begin(), layered.strokes.end(),
		[](const drawing::Stroke& stroke) { return stroke.ink == drawing::Ink::ServiceWater && stroke.closed; }));

	// Selected and previewed, tinted; previews show without the layer.
	overlay.selectedColumn = square;
	overlay.selectedServiceRun = water;
	Column placing = makeColumn(ColumnShape::Rectangle, {5000, 5000});
	placing.level = ground;
	overlay.previewColumns = {placing};
	const drawing::PlanDrawing editing = drawing::buildPlanDrawing(document.project(), ground, overlay);
	CHECK(fills(editing, drawing::Ink::Selected) == 1);
	CHECK(fills(editing, drawing::Ink::Preview) == 1);
	CHECK(count(editing, drawing::Ink::Selected) == 2); // the water's casing: its path and its ring
	overlay = drawing::PlanOverlay{};
	overlay.previewServiceRuns = {runOf(ServiceKind::Gas, {{0, 0, 2400}, {1000, 0, 2400}}, ground)};
	const drawing::PlanDrawing drawingRun = drawing::buildPlanDrawing(document.project(), ground, overlay);
	CHECK(count(drawingRun, drawing::Ink::Preview) == 1);
	CHECK(count(drawingRun, drawing::Ink::ServiceGas) == 1);
	CHECK(count(drawingRun, drawing::Ink::ServiceWater) == 0);

	CHECK(fills(drawing::buildPlanDrawing(document.project(), upper), drawing::Ink::Cut) == 1);
	CHECK(drawing::serviceInk(ServiceKind::Gas) == drawing::Ink::ServiceGas);
	const drawing::Rgb white = drawing::inkColor(drawing::Ink::ServiceWater);
	CHECK(white.r == 238);
	const drawing::Rgb casing = drawing::inkColor(drawing::Ink::ServiceCasing);
	CHECK(casing.r < 100);
}

TEST_CASE_FIXTURE(Building, "Column mode: the column follows the pointer onto corners and the grid; Space turns it; a click adds it")
{
	REQUIRE(document.addWall({0, 0}, {4000, 0}, 120, 2800).ok());
	REQUIRE(document.addWall({4000, 0}, {4000, 3000}, 120, 2800).ok());
	editor.setMode(EditMode::Column);
	editor.pointerMoved(down(1013.0, 987.0));
	REQUIRE(editor.columnPlacement().has_value());
	CHECK(editor.columnPlacement()->position == Point2D{1000, 1000});
	CHECK(editor.columnPlacement()->level == ground);
	CHECK(editor.columnPlacement()->id.isNil());
	REQUIRE(editor.view().previewColumns.size() == 1);
	CHECK(editor.view().previewColumns[0] == *editor.columnPlacement());

	editor.pointerMoved(down(3920.0, 90.0));
	CHECK(editor.columnPlacement()->position == Point2D{4000, 0}); // the corner

	REQUIRE(editor.setNewColumnWidth("40").ok());
	CHECK(editor.settings().columnWidthMm == 400);
	CHECK(editor.setNewColumnDepth("5").error().code == ErrorCode::OutOfRange);
	CHECK(editor.setNewColumnWidth("ancho").error().code != ErrorCode::OutOfRange);
	REQUIRE(editor.flip().ok());
	CHECK(editor.columnPlacement()->rotationCentidegrees == 9000);
	CHECK(editor.columnPlacement()->widthMm == 400);
	REQUIRE(editor.pointerPressed(down(3920.0, 90.0)).ok());
	REQUIRE(editor.pointerReleased(down(3920.0, 90.0)).ok());
	REQUIRE(document.project().columns.size() == 1);
	CHECK(document.undoLabel() == "Add column");
	CHECK(document.project().columns[0].position == Point2D{4000, 0});
	CHECK(document.project().columns[0].rotationCentidegrees == 9000);

	REQUIRE(editor.setNewColumnShape(ColumnShape::Round).ok());
	CHECK(editor.columnPlacement()->shape == ColumnShape::Round);
	CHECK(editor.columnPlacement()->depthMm == 400);
	CHECK(editor.columnPlacement()->rotationCentidegrees == 0);
	REQUIRE(editor.pointerPressed(down(1013.0, 987.0)).ok());
	REQUIRE(document.project().columns.size() == 2);
	CHECK(validateColumn(document.project().columns[1]).ok());

	editor.cancel(); // Esc leaves the mode
	CHECK(editor.mode() == EditMode::Select);
	CHECK_FALSE(editor.columnPlacement().has_value());
	CHECK(editor.view().previewColumns.empty());
}

TEST_CASE_FIXTURE(Building, "Select mode: a column is selected before the wall it stands on, dragged, edited, turned and deleted")
{
	const WallId wall = document.addWall({0, 0}, {4000, 0}, 120, 2800).value();
	const ColumnId column = document.addColumn(makeColumn(ColumnShape::Rectangle, {2000, 0})).value();
	REQUIRE(editor.pointerPressed(down(2000.0, 0.0)).ok()); // as near as the wall's top: the column
	CHECK(editor.selectedColumn()->id == column);
	CHECK(editor.view().selectedColumn == column);
	CHECK(editor.selection().isNil());
	CHECK(editor.isDragging());
	CHECK(editor.view().previewColumns.empty());

	editor.pointerMoved(down(2000.0, 1213.0));
	REQUIRE(editor.view().previewColumns.size() == 1);
	CHECK(editor.view().previewColumns[0].position == Point2D{2000, 1200});
	REQUIRE(editor.pointerReleased(down(2000.0, 1213.0)).ok());
	CHECK(document.project().columns[0].position == Point2D{2000, 1200});
	CHECK(document.undoLabel() == "Change column");

	REQUIRE(editor.pointerPressed(down(2000.0, 30.0)).ok()); // only the wall there now
	CHECK(editor.selection() == wall);
	CHECK_FALSE(editor.selectedColumn().has_value());

	REQUIRE(editor.selectColumn(column).ok());
	REQUIRE(editor.setColumnWidth("50").ok());
	CHECK(document.project().columns[0].widthMm == 500);
	REQUIRE(editor.setColumnRotation("45").ok());
	CHECK(document.project().columns[0].rotationCentidegrees == 4500);
	REQUIRE(editor.flip().ok());
	CHECK(document.project().columns[0].rotationCentidegrees == 13500);
	REQUIRE(editor.setColumnFinish("brick-red").ok());
	CHECK(document.project().columns[0].finish == "brick-red");
	CHECK_FALSE(editor.setColumnFinish("service-gas").ok());
	REQUIRE(editor.setColumnShape(ColumnShape::Round).ok());
	CHECK(document.project().columns[0].depthMm == 500);
	CHECK(document.project().columns[0].rotationCentidegrees == 0);
	CHECK_FALSE(editor.setColumnRotation("45").ok());
	REQUIRE(editor.setColumnDepth("60").ok());
	CHECK(document.project().columns[0].widthMm == 600); // one diameter
	CHECK(editor.selectColumn(ColumnId{Uuid{5, 5}}).error().code == ErrorCode::NotFound);

	REQUIRE(editor.deleteSelection().ok());
	CHECK(document.project().columns.empty());
	CHECK(document.undoLabel() == "Delete column");
	CHECK_FALSE(editor.selectedColumn().has_value());
	CHECK(editor.setColumnWidth("50").error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(Building, "Services mode: points snap from the last one, a new height rises on the spot, a double click finishes")
{
	editor.setMode(EditMode::Services);
	CHECK(editor.showsServices());
	CHECK(editor.view().showServices);
	REQUIRE(editor.setServiceKind(ServiceKind::Water).ok());
	CHECK(editor.confirm().ok()); // nothing being drawn

	editor.pointerMoved(down(1013.0, 987.0));
	REQUIRE(editor.runPointer().has_value());
	CHECK(*editor.runPointer() == ServicePoint{1000, 1000, 300});
	CHECK(editor.view().handles == std::vector<Point2D>{{1000, 1000}});
	CHECK(editor.view().previewServiceRuns.empty()); // one point is no run yet
	REQUIRE(editor.pointerPressed(down(1013.0, 987.0)).ok());
	CHECK(editor.runPoints() == std::vector<ServicePoint>{{1000, 1000, 300}});

	editor.pointerMoved(down(3020.0, 1010.0));
	CHECK(*editor.runPointer() == ServicePoint{3000, 1000, 300}); // along the snapped angle, on the grid
	REQUIRE(editor.view().previewServiceRuns.size() == 1);
	CHECK(editor.view().previewServiceRuns[0].id.isNil());
	CHECK(editor.view().previewServiceRuns[0].kind == ServiceKind::Water);
	CHECK(editor.view().previewServiceRuns[0].points.size() == 2);
	REQUIRE(editor.pointerPressed(down(3020.0, 1010.0)).ok());

	REQUIRE(editor.setServiceHeight("240").ok()); // centimetres, like every typed length
	CHECK(editor.settings().serviceHeightMm == 2400);
	CHECK(editor.setServiceHeightMm(MaxServiceHeightMm + 1).error().code == ErrorCode::OutOfRange);
	editor.pointerMoved(down(3010.0, 3020.0));
	CHECK(*editor.runPointer() == ServicePoint{3000, 3000, 2400});
	CHECK(editor.view().previewServiceRuns[0].points ==
		std::vector<ServicePoint>{{1000, 1000, 300}, {3000, 1000, 300}, {3000, 1000, 2400}, {3000, 3000, 2400}});
	REQUIRE(editor.pointerPressed(down(3010.0, 3020.0)).ok());
	CHECK(editor.runPoints().size() == 4);
	editor.backspace();
	CHECK(editor.runPoints().size() == 3); // the rise stays
	REQUIRE(editor.pointerPressed(down(3010.0, 3020.0)).ok());
	CHECK(editor.runPoints().size() == 4);
	CHECK(document.project().services.empty());

	REQUIRE(editor.pointerPressed(down(3010.0, 3020.0)).ok()); // again on the last point: a double click
	REQUIRE(document.project().services.size() == 1);
	CHECK(document.undoLabel() == "Add service run");
	const ServiceRun& run = document.project().services[0];
	CHECK(run.kind == ServiceKind::Water);
	CHECK(run.level == ground);
	CHECK(run.points == std::vector<ServicePoint>{{1000, 1000, 300}, {3000, 1000, 300}, {3000, 1000, 2400}, {3000, 3000, 2400}});
	CHECK(editor.runPoints().empty());
	CHECK(editor.mode() == EditMode::Services); // ready for the next one

	// Enter finishes too; one point is not enough. Esc drops the run, then leaves the mode with the layer still shown.
	REQUIRE(editor.pointerPressed(down(0.0, 0.0)).ok());
	CHECK(editor.confirm().error().code == ErrorCode::InvalidArgument);
	REQUIRE(editor.setServiceHeightMm(-100).ok());
	editor.pointerMoved(down(0.0, 0.0));
	REQUIRE(editor.pointerPressed(down(0.0, 0.0)).ok()); // the same spot, lower: a drop
	CHECK(editor.runPoints() == std::vector<ServicePoint>{{0, 0, 2400}, {0, 0, -100}});
	REQUIRE(editor.confirm().ok());
	CHECK(document.project().services.size() == 2);
	REQUIRE(editor.pointerPressed(down(500.0, 0.0)).ok());
	editor.cancel();
	CHECK(editor.runPoints().empty());
	CHECK(editor.mode() == EditMode::Services);
	editor.cancel();
	CHECK(editor.mode() == EditMode::Select);
	CHECK(editor.showsServices());
	CHECK(document.project().services.size() == 2);

	// Undo while drawing drops the run being drawn.
	editor.setMode(EditMode::Services);
	REQUIRE(editor.pointerPressed(down(500.0, 0.0)).ok());
	REQUIRE(editor.undo().ok());
	CHECK(editor.runPoints().empty());
	CHECK(document.project().services.size() == 1);
}

TEST_CASE_FIXTURE(Building, "a double click finishes the run even where the snapped point is off the pointer")
{
	// The pointer is 120 mm off the 0-degree line: the click snaps onto the line, so the same pointer measured from the
	// new point would be a short stretch more, not the last point again.
	editor.setMode(EditMode::Services);
	REQUIRE(editor.pointerPressed(down(0.0, 0.0)).ok());
	REQUIRE(editor.pointerPressed(down(2000.0, 120.0)).ok());
	REQUIRE(editor.runPoints().size() == 2);
	CHECK(editor.runPoints().back() == ServicePoint{2000, 0, 300});
	editor.pointerMoved(down(2000.0, 120.0));
	REQUIRE(editor.runPointer().has_value());
	CHECK_FALSE(*editor.runPointer() == editor.runPoints().back()); // a second plain click would add a point

	REQUIRE(editor.pointerDoubleClicked(down(2000.0, 120.0)).ok());
	REQUIRE(document.project().services.size() == 1);
	CHECK(document.project().services[0].points == std::vector<ServicePoint>{{0, 0, 300}, {2000, 0, 300}});
	CHECK(editor.runPoints().empty());

	// With one point a double click cannot make a run; outside the Services mode it is a press.
	REQUIRE(editor.pointerPressed(down(0.0, 1000.0)).ok());
	CHECK(editor.pointerDoubleClicked(down(0.0, 1000.0)).error().code == ErrorCode::InvalidArgument);
	editor.cancel();
	editor.cancel();
	const ColumnId column = document.addColumn(makeColumn(ColumnShape::Rectangle, {3000, 3000})).value();
	REQUIRE(editor.pointerDoubleClicked(down(3000.0, 3000.0)).ok());
	CHECK(editor.selectedColumn()->id == column);
}

TEST_CASE_FIXTURE(Building, "with the layer shown a run is selected through the walls, dragged whole or by a bend, edited and deleted")
{
	const WallId wall = document.addWall({0, 0}, {4000, 0}, 120, 2800).value();
	const ServiceRunId inWall = document.addServiceRun(runOf(ServiceKind::Electrical, {{0, 0, 300}, {4000, 0, 300}})).value();
	const ServiceRunId free =
		document.addServiceRun(runOf(ServiceKind::Water, {{1000, 1000, 300}, {3000, 1000, 300}, {3000, 1000, 2400}, {3000, 3000, 2400}})).value();

	// Hidden: the wall.
	REQUIRE(editor.pointerPressed(down(2000.0, 0.0)).ok());
	CHECK(editor.selection() == wall);
	CHECK(editor.selectServiceRun(ServiceRunId{Uuid{5, 5}}).error().code == ErrorCode::NotFound);

	editor.setShowServices(true);
	REQUIRE(editor.pointerReleased(down(2000.0, 0.0)).ok());
	REQUIRE(editor.pointerPressed(down(2000.0, 0.0)).ok());
	REQUIRE(editor.selectedServiceRun().has_value());
	CHECK(editor.selectedServiceRun()->id == inWall);
	CHECK(editor.view().selectedServiceRun == inWall);
	CHECK(editor.selection().isNil());
	REQUIRE(editor.pointerReleased(down(2000.0, 0.0)).ok());
	CHECK(document.undoLabel() == "Add service run"); // a click moves nothing

	// The whole run moves by grid steps.
	REQUIRE(editor.pointerPressed(down(2000.0, 1000.0)).ok());
	CHECK(editor.selectedServiceRun()->id == free);
	CHECK(editor.view().handles == std::vector<Point2D>{{1000, 1000}, {3000, 1000}, {3000, 3000}}); // one per bend
	editor.pointerMoved(down(2260.0, 1490.0));
	REQUIRE(editor.view().previewServiceRuns.size() == 1);
	CHECK(editor.view().previewServiceRuns[0].points[0] == ServicePoint{1250, 1500, 300});
	REQUIRE(editor.pointerReleased(down(2260.0, 1490.0)).ok());
	CHECK(document.undoLabel() == "Change service run");
	CHECK(document.project().findServiceRun(free)->points[3] == ServicePoint{3250, 3500, 2400});
	REQUIRE(editor.undo().ok());

	// A bend moves with the rise there.
	REQUIRE(editor.pointerPressed(down(3000.0, 1000.0)).ok()); // its handle
	editor.pointerMoved(down(3513.0, 1008.0));
	REQUIRE(editor.pointerReleased(down(3513.0, 1008.0)).ok());
	CHECK(document.project().findServiceRun(free)->points ==
		std::vector<ServicePoint>{{1000, 1000, 300}, {3500, 1000, 300}, {3500, 1000, 2400}, {3000, 3000, 2400}});

	REQUIRE(editor.setServiceRunKind(ServiceKind::Gas).ok());
	CHECK(document.project().findServiceRun(free)->kind == ServiceKind::Gas);
	CHECK(document.undoLabel() == "Change service run");
	REQUIRE(editor.deleteSelection().ok());
	CHECK(document.undoLabel() == "Delete service run");
	CHECK(document.project().findServiceRun(free) == nullptr);
	CHECK(editor.setServiceRunKind(ServiceKind::Gas).error().code == ErrorCode::NotFound);

	// Hiding the layer drops the selection; selecting a run shows it again.
	REQUIRE(editor.selectServiceRun(inWall).ok());
	editor.setShowServices(false);
	CHECK_FALSE(editor.selectedServiceRun().has_value());
	CHECK_FALSE(editor.view().showServices);
	REQUIRE(editor.selectServiceRun(inWall).ok());
	CHECK(editor.showsServices());
	editor.setMode(EditMode::Services);
	editor.setShowServices(false);
	CHECK(editor.mode() == EditMode::Select);
}

TEST_CASE_FIXTURE(Building, "columns and runs are edited on the active level; changing level drops the run being drawn")
{
	const LevelId upper = document.addLevel().value();
	Column above = makeColumn(ColumnShape::Rectangle, {0, 0});
	above.level = upper;
	const ColumnId upstairs = document.addColumn(above).value();
	REQUIRE(editor.pointerPressed(down(0.0, 0.0)).ok());
	CHECK_FALSE(editor.selectedColumn().has_value()); // on another level

	REQUIRE(editor.selectColumn(upstairs).ok());
	CHECK(editor.activeLevel() == upper);
	editor.setMode(EditMode::Services);
	editor.pointerMoved(down(0.0, 0.0));
	REQUIRE(editor.pointerPressed(down(0.0, 0.0)).ok());
	REQUIRE(editor.pointerPressed(down(1000.0, 0.0)).ok());
	REQUIRE(editor.confirm().ok());
	REQUIRE(document.project().services.size() == 1);
	CHECK(document.project().services[0].level == upper);

	REQUIRE(editor.pointerPressed(down(0.0, 2000.0)).ok());
	REQUIRE(editor.levelDown().ok());
	CHECK(editor.runPoints().empty());
	CHECK_FALSE(editor.runPointer().has_value());
}

TEST_CASE("a set of networks holds any of the four kinds; every kind by default")
{
	ServiceKindSet kinds;
	for (const ServiceKindInfo& info : serviceKinds())
	{
		CHECK(kinds.contains(info.kind));
	}
	kinds.set(ServiceKind::Water, false);
	CHECK_FALSE(kinds.contains(ServiceKind::Water));
	CHECK(kinds.contains(ServiceKind::Gas));
	CHECK_FALSE(kinds == ServiceKindSet{});
	kinds.set(ServiceKind::Water, true);
	CHECK(kinds == ServiceKindSet{});
	for (const ServiceKindInfo& info : serviceKinds())
	{
		kinds.set(info.kind, false);
	}
	CHECK(kinds.bits == 0);
}

TEST_CASE_FIXTURE(Building, "each network can be shown or hidden: the scene, the plan and picking follow; previews always show")
{
	const ServiceRunId wiring = document.addServiceRun(runOf(ServiceKind::Electrical, {{0, 0, 300}, {4000, 0, 300}})).value();
	const ServiceRunId water = document.addServiceRun(runOf(ServiceKind::Water, {{0, 0, 2400}, {4000, 0, 2400}})).value();
	ServiceKindSet noWiring;
	noWiring.set(ServiceKind::Electrical, false);

	scene::ViewOverlay overlay;
	overlay.activeLevel = ground;
	overlay.services = true;
	overlay.serviceKinds = noWiring;
	const scene::Scene filtered = scene::compileScene(document.project(), overlay);
	CHECK(findNode(filtered, "service/" + toString(wiring)) == nullptr);
	CHECK(findNode(filtered, "service/" + toString(water)) != nullptr);
	ServiceRun moved = *document.project().findServiceRun(wiring);
	moved.points[1].x_mm = 3000;
	overlay.previewServiceRuns = {moved};
	CHECK(findNode(scene::compileScene(document.project(), overlay), "service/" + toString(wiring)) != nullptr);
	CHECK(findNode(scene::compileScene(document.project()), "service/" + toString(wiring)) != nullptr); // exports: all

	drawing::PlanOverlay plan;
	plan.services = true;
	plan.serviceKinds = noWiring;
	const drawing::PlanDrawing withoutWiring = drawing::buildPlanDrawing(document.project(), ground, plan);
	CHECK(count(withoutWiring, drawing::Ink::ServiceElectrical) == 0);
	CHECK(count(withoutWiring, drawing::Ink::ServiceWater) == 1);
	plan.serviceKinds = ServiceKindSet{};
	CHECK(count(drawing::buildPlanDrawing(document.project(), ground, plan), drawing::Ink::ServiceElectrical) == 1);

	// The wiring is nearer the ray's end: with it hidden, the water above is picked.
	CHECK(pickServiceRun(document.project(), down(2000.0, 0.0), ground, 80.0)->run == water);
	CHECK(pickServiceRun(document.project(), Ray3{2000.0, 0.0, -1000.0, 0.0, 0.0, 1.0}, ground, 80.0)->run == wiring);
	CHECK(pickServiceRun(document.project(), Ray3{2000.0, 0.0, -1000.0, 0.0, 0.0, 1.0}, ground, 80.0, noWiring)->run == water);
}

TEST_CASE_FIXTURE(Building, "the editor shows networks one by one: hiding one drops its selected run; drawing or selecting shows it")
{
	const ServiceRunId wiring = document.addServiceRun(runOf(ServiceKind::Electrical, {{0, 0, 300}, {4000, 0, 300}})).value();
	editor.setShowServices(true);
	CHECK(editor.shownServiceKinds() == ServiceKindSet{});
	REQUIRE(editor.pointerPressed(down(2000.0, 0.0)).ok());
	REQUIRE(editor.pointerReleased(down(2000.0, 0.0)).ok());
	REQUIRE(editor.selectedServiceRun().has_value());

	editor.setServiceKindShown(ServiceKind::Electrical, false);
	CHECK_FALSE(editor.shownServiceKinds().contains(ServiceKind::Electrical));
	CHECK_FALSE(editor.view().serviceKinds.contains(ServiceKind::Electrical));
	CHECK_FALSE(editor.selectedServiceRun().has_value());
	REQUIRE(editor.pointerPressed(down(2000.0, 0.0)).ok()); // not picked while hidden
	CHECK_FALSE(editor.selectedServiceRun().has_value());
	REQUIRE(editor.pointerReleased(down(2000.0, 0.0)).ok());

	REQUIRE(editor.selectServiceRun(wiring).ok());
	CHECK(editor.shownServiceKinds().contains(ServiceKind::Electrical));
	editor.setServiceKindShown(ServiceKind::Gas, false);
	REQUIRE(editor.setServiceRunKind(ServiceKind::Gas).ok());
	CHECK(editor.shownServiceKinds().contains(ServiceKind::Gas)); // the run stays in sight
	CHECK(editor.selectedServiceRun().has_value());

	editor.setServiceKindShown(ServiceKind::Water, false);
	editor.setMode(EditMode::Services);
	REQUIRE(editor.setServiceKind(ServiceKind::Water).ok());
	CHECK(editor.shownServiceKinds().contains(ServiceKind::Water));
	editor.setServiceKindShown(ServiceKind::Sewage, false);
	editor.cancel();
	REQUIRE(editor.setServiceKind(ServiceKind::Sewage).ok());
	editor.setServiceKindShown(ServiceKind::Sewage, false);
	editor.setMode(EditMode::Services);
	CHECK(editor.shownServiceKinds().contains(ServiceKind::Sewage)); // entering the mode shows what it draws
}
