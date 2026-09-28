#pragma once

#include "F101/Cad/Project.h"
#include "F101/Scene/Scene.h"

#include <optional>
#include <vector>

namespace f101::scene
{
	// View state layered on top of the project's scene (selection, tool previews, handles). Never saved; exporters
	// compile the project without it.
	struct ViewOverlay
	{
		cad::WallId selectedWall;             // nil: nothing selected
		cad::WallId hiddenWall;               // nil: none (e.g. the original of a wall whose endpoint is being dragged)
		std::optional<cad::Wall> preview;     // e.g. the wall being drawn; its id is not used
		std::vector<cad::Point2D> markers;    // e.g. draggable endpoint handles
		std::int64_t markerSizeMm = 240;
		std::int64_t markerHeightMm = 2500;
	};

	// Node carrying the body geometry of a wall.
	FLOOR101SCENEMODEL_API SceneNodeId wallBodyNodeId(const cad::WallId& id);

	// Compiles the whole project into a scene: one node per wall, in model order. Deterministic: the same project
	// always produces the same scene (ids, geometry and hashes).
	FLOOR101SCENEMODEL_API Scene compileScene(const cad::Project& project);

	// compileScene(project) plus the overlay: Selected/Hidden styles on wall nodes, a "preview/wall" node (Preview) and
	// "marker/<index>" nodes (Handle). A preview that is not a valid wall is left out.
	FLOOR101SCENEMODEL_API Scene compileScene(const cad::Project& project, const ViewOverlay& overlay);

	// FNV-1a 64 over everything a consumer renders (positions, normals, uvs, sections).
	FLOOR101SCENEMODEL_API std::uint64_t hashMesh(const geometry::MeshData& mesh);
}
