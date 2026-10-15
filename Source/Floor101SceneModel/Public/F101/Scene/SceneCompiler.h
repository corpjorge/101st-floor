#pragma once

#include "F101/Cad/Project.h"
#include "F101/Geometry/ModelMesh.h"
#include "F101/Scene/Scene.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace f101::scene
{
	// View state layered on top of the project's scene (selection, tool previews, handles). Never saved; exporters
	// compile the project without it.
	struct ViewOverlay
	{
		// The level being edited (ADR-0011). Unless showAllLevels, the levels above it are left out, it shows no slabs (so
		// its rooms can be seen from above) and the levels below it are Context. Nil: every level, slabs included.
		cad::LevelId activeLevel;
		bool showAllLevels = false;
		cad::WallId selectedWall; // nil: nothing selected
		std::string selectedRoom; // room key whose floor is shown Selected; empty: none
		// Walls as they would be once the current gesture is applied: a wall with the id of an existing wall replaces
		// it (e.g. a dragged endpoint), a wall with a nil id is added (the wall being drawn). The scene is compiled
		// from that previewed project, so joins and rooms follow the preview. Invalid walls are left out.
		std::vector<cad::Wall> previewWalls;
		cad::FurnitureId selectedFurniture; // nil: none
		// Furniture as it would be once the current gesture is applied, like previewWalls: a piece with the id of an
		// existing one replaces it (e.g. while it is dragged), a piece with a nil id is added (the piece being placed).
		// Invalid pieces are left out.
		std::vector<cad::Furniture> previewFurniture;
		std::vector<cad::Point2D> markers; // e.g. draggable endpoint handles, on the active level
		std::int64_t markerSizeMm = 240;
		std::int64_t markerHeightMm = 2500;
		// Door swings on the floor (editing aids, ADR-0014); off for a clean view such as a photo (ADR-0018).
		bool symbols = true;
		// The building services layer (ADR-0021): on, the runs are shown and the walls, floors, slabs and carpentry turn
		// see-through (Ghost) so the runs inside them can be seen; off, the runs are left out (previews still show).
		bool services = false;
		cad::ServiceKindSet serviceKinds; // the networks the layer shows (runs being previewed show whatever their kind)
		cad::ColumnId selectedColumn;       // nil: none
		std::vector<cad::Column> previewColumns; // like previewFurniture: an existing id replaces it, a nil id is added
		cad::ServiceRunId selectedServiceRun;    // nil: none
		std::vector<cad::ServiceRun> previewServiceRuns; // the run being drawn (nil id), or one being changed
	};

	// The meshes of imported models (ADR-0017), read from their files by the application (f101::assets). Null when a
	// model cannot be read: its pieces show as plain boxes of their size. Without a provider every model piece is a box.
	using ModelProvider = std::function<const geometry::ModelMesh*(const cad::Asset& asset)>;

	// Node carrying the body geometry of a wall.
	FLOOR101SCENEMODEL_API SceneNodeId wallBodyNodeId(const cad::WallId& id);
	// Node of a piece of furniture.
	FLOOR101SCENEMODEL_API SceneNodeId furnitureNodeId(const cad::FurnitureId& id);
	// Nodes of a column ("column/<uuid>") and of a run of the building services ("service/<uuid>"), ADR-0021.
	FLOOR101SCENEMODEL_API SceneNodeId columnNodeId(const cad::ColumnId& id);
	FLOOR101SCENEMODEL_API SceneNodeId serviceRunNodeId(const cad::ServiceRunId& id);

	// A column is drawn over the walls and slabs it overlaps (ADR-0021): its mesh stands this far proud of its outline on
	// every side and of its level's height, so none of its faces lies in the plane of a wall's or a slab's (as tall as the
	// level, or flush with a wall face), where the renderer would alternate between the two (z-fighting). The model and
	// the plan keep its exact size.
	inline constexpr std::int64_t ColumnProudMm = 1;

	// Compiles the whole project into a scene, deterministically (same project, same ids, geometry and hashes):
	// one node per wall in model order with its joins resolved ("<wall>/body"), then joint fills
	// ("<wall>/joint-start|end"), then room floors ("<room key>/floor"), then the slab over each room ("<room key>/slab",
	// the top band of its level), then the carpentry of each opening ("<wall>/opening/<opening>", ADR-0013), then each
	// piece of furniture in model order ("furniture/<piece>", ADR-0016; an imported model with one section per material,
	// finished "asset:<uuid>#<material>" unless the piece has a finish, ADR-0017), then each column ("column/<id>", as tall
	// as its level, ColumnProudMm proud) and each run of the building services ("service/<id>", in its kind's colour), ADR-0021. Joins and rooms
	// are solved per level, and each level is lifted to its elevation. Scene::rooms lists the rooms with their levels.
	FLOOR101SCENEMODEL_API Scene compileScene(const cad::Project& project, const ModelProvider& models = {});

	// compileScene of the project as previewed by the overlay, plus view styles: previewed walls and furniture are Preview
	// (new ones get the ids "preview/wall/<index>" and "preview/furniture/<index>"), the selected wall and piece are
	// Selected, and markers are "marker/<index>" (Handle).
	// Unless overlay.symbols is off, it also draws the swing of each hinged or double door on the floor of the levels shown
	// undimmed ("<wall>/opening/<opening>/swing", Symbol; ADR-0014), after the carpentry: an editing aid exporters never see.
	FLOOR101SCENEMODEL_API Scene compileScene(const cad::Project& project, const ViewOverlay& overlay,
		const ModelProvider& models = {});

	// FNV-1a 64 over everything a consumer renders (positions, normals, uvs, sections).
	FLOOR101SCENEMODEL_API std::uint64_t hashMesh(const geometry::MeshData& mesh);
	// hashMesh plus the finish of each section: the node's contentHash.
	FLOOR101SCENEMODEL_API std::uint64_t hashNode(const geometry::MeshData& mesh, const std::vector<std::string>& finishes);
}
