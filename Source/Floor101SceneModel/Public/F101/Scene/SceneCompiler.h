#pragma once

#include "F101/Cad/Project.h"
#include "F101/Scene/Scene.h"

#include <cstdint>
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
		std::vector<cad::Point2D> markers; // e.g. draggable endpoint handles, on the active level
		std::int64_t markerSizeMm = 240;
		std::int64_t markerHeightMm = 2500;
	};

	// Node carrying the body geometry of a wall.
	FLOOR101SCENEMODEL_API SceneNodeId wallBodyNodeId(const cad::WallId& id);

	// Compiles the whole project into a scene, deterministically (same project, same ids, geometry and hashes):
	// one node per wall in model order with its joins resolved ("<wall>/body"), then joint fills
	// ("<wall>/joint-start|end"), then room floors ("<room key>/floor"), then the slab over each room ("<room key>/slab",
	// the top band of its level), then the carpentry of each opening ("<wall>/opening/<opening>", ADR-0013). Joins and
	// rooms are solved per level, and each level is lifted to its elevation. Scene::rooms lists the rooms with their levels.
	FLOOR101SCENEMODEL_API Scene compileScene(const cad::Project& project);

	// compileScene of the project as previewed by the overlay, plus view styles: previewed walls are Preview (new ones
	// get the ids "preview/wall/<index>"), the selected wall is Selected, and markers are "marker/<index>" (Handle).
	FLOOR101SCENEMODEL_API Scene compileScene(const cad::Project& project, const ViewOverlay& overlay);

	// FNV-1a 64 over everything a consumer renders (positions, normals, uvs, sections).
	FLOOR101SCENEMODEL_API std::uint64_t hashMesh(const geometry::MeshData& mesh);
	// hashMesh plus the finish of each section: the node's contentHash.
	FLOOR101SCENEMODEL_API std::uint64_t hashNode(const geometry::MeshData& mesh, const std::vector<std::string>& finishes);
}
