#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "F101/Geometry/MeshData.h"

// The only place where scene space becomes Unreal space (ADR-0003).
// Scene: millimetres, right-handed, X east, Y north, Z up. Unreal: centimetres, left-handed, X forward, Y right, Z up.
namespace Floor101::Conversion
{
	inline constexpr double CentimetersPerMillimeter = 0.1;

	// Mirrors Y (handedness) and converts mm -> cm.
	inline FVector3d ToUnrealPosition(const f101::geometry::Vec3& P)
	{
		return FVector3d(P.x * CentimetersPerMillimeter, -P.y * CentimetersPerMillimeter, P.z * CentimetersPerMillimeter);
	}

	inline FVector3f ToUnrealNormal(const f101::geometry::Vec3& N)
	{
		return FVector3f(static_cast<float>(N.x), static_cast<float>(-N.y), static_cast<float>(N.z));
	}

	inline double ToUnrealLength(double Millimeters)
	{
		return Millimeters * CentimetersPerMillimeter;
	}

	// Builds a dynamic mesh with per-vertex normals, UVs and one material id (and polygroup) per section.
	// Triangle order is kept on purpose: the Y mirror flips the sign of the cross product and Unreal's left-handed
	// convention (VectorUtil::Normal uses edge2 x edge1) flips it back.
	UE::Geometry::FDynamicMesh3 ToDynamicMesh(const f101::geometry::MeshData& Mesh);
}
