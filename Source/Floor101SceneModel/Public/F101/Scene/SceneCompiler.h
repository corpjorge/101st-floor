#pragma once

#include "F101/Cad/Project.h"
#include "F101/Scene/Scene.h"

namespace f101::scene
{
	// Node carrying the body geometry of a wall.
	FLOOR101SCENEMODEL_API SceneNodeId wallBodyNodeId(const cad::WallId& id);

	// Compiles the whole project into a scene: one node per wall, in model order. Deterministic: the same project
	// always produces the same scene (ids, geometry and hashes).
	FLOOR101SCENEMODEL_API Scene compileScene(const cad::Project& project);

	// FNV-1a 64 over everything a consumer renders (positions, normals, uvs, sections).
	FLOOR101SCENEMODEL_API std::uint64_t hashMesh(const geometry::MeshData& mesh);
}
