#pragma once

#include "F101/Cad/Export.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifndef FLOOR101GEOMETRY_API
	#define FLOOR101GEOMETRY_API
#endif

namespace f101::geometry
{
	struct Vec3
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
	};

	struct Vec2
	{
		double u = 0.0;
		double v = 0.0;
	};

	struct Bounds3
	{
		Vec3 min;
		Vec3 max;
	};

	// Triangles that share one material slot. Every three indices form a triangle whose vertices are counter-clockwise
	// when seen from the side its normal points to (right-handed scene space, ADR-0003).
	struct MeshSection
	{
		std::string slot;
		std::vector<std::uint32_t> indices;
	};

	// Engine-neutral triangle mesh in scene space: millimetres, right-handed, X east, Y north, Z up.
	// Derived data: regenerated from the model, never edited or used as a source of truth.
	struct MeshData
	{
		std::vector<Vec3> positions;
		std::vector<Vec3> normals; // per vertex, unit length
		std::vector<Vec2> uvs;     // metres (1.0 = 1000 mm), so materials tile at physical scale
		std::vector<MeshSection> sections;

		std::size_t vertexCount() const { return positions.size(); }

		std::size_t triangleCount() const
		{
			std::size_t count = 0;
			for (const MeshSection& section : sections)
			{
				count += section.indices.size() / 3;
			}
			return count;
		}
	};

	// Axis-aligned bounds of all vertices. Precondition: the mesh has at least one vertex.
	FLOOR101GEOMETRY_API Bounds3 computeBounds(const MeshData& mesh);
}
