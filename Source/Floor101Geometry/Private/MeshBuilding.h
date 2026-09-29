#pragma once

#include "F101/Cad/Geometry2D.h"
#include "F101/Geometry/MeshData.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

// Helpers shared by the builders of this module. Not part of its API.
namespace f101::geometry::detail
{
	inline constexpr double MetersPerMillimeter = 0.001;

	inline Vec3 subtract(const Vec3& a, const Vec3& b)
	{
		return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
	}

	inline Vec3 cross(const Vec3& a, const Vec3& b)
	{
		return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	}

	inline double dot(const Vec3& a, const Vec3& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	// Plan-view vector helpers on PlanPoint (mm).
	inline cad::PlanPoint add(cad::PlanPoint a, cad::PlanPoint b) { return {a.x_mm + b.x_mm, a.y_mm + b.y_mm}; }
	inline cad::PlanPoint sub(cad::PlanPoint a, cad::PlanPoint b) { return {a.x_mm - b.x_mm, a.y_mm - b.y_mm}; }
	inline cad::PlanPoint scale(cad::PlanPoint a, double s) { return {a.x_mm * s, a.y_mm * s}; }
	inline double dot2(cad::PlanPoint a, cad::PlanPoint b) { return a.x_mm * b.x_mm + a.y_mm * b.y_mm; }
	inline double cross2(cad::PlanPoint a, cad::PlanPoint b) { return a.x_mm * b.y_mm - a.y_mm * b.x_mm; }
	inline double length2(cad::PlanPoint a) { return std::hypot(a.x_mm, a.y_mm); }
	inline cad::PlanPoint leftOf(cad::PlanPoint direction) { return {-direction.y_mm, direction.x_mm}; }
	inline cad::PlanPoint toPlan(cad::Point2D p) { return {static_cast<double>(p.x_mm), static_cast<double>(p.y_mm)}; }

	struct Corner
	{
		Vec3 position;
		Vec2 uv; // metres
	};

	inline MeshSection& sectionFor(MeshData& mesh, const char* slot)
	{
		for (MeshSection& section : mesh.sections)
		{
			if (section.slot == slot)
			{
				return section;
			}
		}
		mesh.sections.push_back(MeshSection{slot, {}});
		return mesh.sections.back();
	}

	// Appends a planar quad from its corners in perimeter order (either direction) and its outward normal, to the
	// section of that slot. The winding is derived from the normal, so every face is counter-clockwise seen from outside.
	inline void addQuad(MeshData& mesh, const char* slot, Corner a, Corner b, Corner c, Corner d, const Vec3& normal)
	{
		if (dot(cross(subtract(b.position, a.position), subtract(c.position, a.position)), normal) < 0.0)
		{
			std::swap(b, d);
		}
		const auto base = static_cast<std::uint32_t>(mesh.positions.size());
		for (const Corner& corner : {a, b, c, d})
		{
			mesh.positions.push_back(corner.position);
			mesh.normals.push_back(normal);
			mesh.uvs.push_back(corner.uv);
		}
		MeshSection& section = sectionFor(mesh, slot);
		section.indices.insert(section.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
	}
}
