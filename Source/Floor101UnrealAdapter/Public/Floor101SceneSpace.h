#pragma once

#include "CoreMinimal.h"
#include "F101/Geometry/MeshData.h"

// The only place where scene space and Unreal space meet (ADR-0003).
// Scene: millimetres, right-handed, X east, Y north, Z up. Unreal: centimetres, left-handed, X forward, Y right, Z up.
namespace Floor101::Conversion
{
	inline constexpr double CentimetersPerMillimeter = 0.1;
	inline constexpr double MillimetersPerCentimeter = 10.0;

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

	// Inverse of ToUnrealPosition: cm -> mm, Y mirrored back.
	inline f101::geometry::Vec3 ToScenePosition(const FVector3d& P)
	{
		return {P.X * MillimetersPerCentimeter, -P.Y * MillimetersPerCentimeter, P.Z * MillimetersPerCentimeter};
	}

	// Directions only mirror Y; their length is kept.
	inline f101::geometry::Vec3 ToSceneDirection(const FVector3d& D)
	{
		return {D.X, -D.Y, D.Z};
	}
}
