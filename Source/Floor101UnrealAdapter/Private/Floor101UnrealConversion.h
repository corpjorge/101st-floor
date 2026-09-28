#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "F101/Geometry/MeshData.h"
#include "Floor101SceneSpace.h"

namespace Floor101::Conversion
{
	// Builds a dynamic mesh with per-vertex normals, UVs and one material id (and polygroup) per section.
	// Triangle order is kept on purpose: the Y mirror flips the sign of the cross product and Unreal's left-handed
	// convention (VectorUtil::Normal uses edge2 x edge1) flips it back.
	UE::Geometry::FDynamicMesh3 ToDynamicMesh(const f101::geometry::MeshData& Mesh);
}
