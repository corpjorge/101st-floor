#include "F101/Geometry/MeshData.h"

#include "F101/Cad/Assert.h"

#include <algorithm>

namespace f101::geometry
{
	Bounds3 computeBounds(const MeshData& mesh)
	{
		F101_ASSERT(!mesh.positions.empty());
		Bounds3 bounds{mesh.positions.front(), mesh.positions.front()};
		for (const Vec3& p : mesh.positions)
		{
			bounds.min = Vec3{std::min(bounds.min.x, p.x), std::min(bounds.min.y, p.y), std::min(bounds.min.z, p.z)};
			bounds.max = Vec3{std::max(bounds.max.x, p.x), std::max(bounds.max.y, p.y), std::max(bounds.max.z, p.z)};
		}
		return bounds;
	}
}
