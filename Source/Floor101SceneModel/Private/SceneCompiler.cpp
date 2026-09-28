#include "F101/Scene/SceneCompiler.h"

#include "F101/Geometry/WallGeometryBuilder.h"

namespace f101::scene
{
	namespace
	{
		constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
		constexpr std::uint64_t FnvPrime = 1099511628211ull;

		void hashBytes(std::uint64_t& hash, const void* data, std::size_t size)
		{
			const auto* bytes = static_cast<const unsigned char*>(data);
			for (std::size_t i = 0; i < size; ++i)
			{
				hash ^= bytes[i];
				hash *= FnvPrime;
			}
		}

		void hashDouble(std::uint64_t& hash, double value)
		{
			hashBytes(hash, &value, sizeof(value));
		}
	}

	SceneNodeId wallBodyNodeId(const cad::WallId& id)
	{
		return SceneNodeId{cad::toString(id) + "/body"};
	}

	std::uint64_t hashMesh(const geometry::MeshData& mesh)
	{
		std::uint64_t hash = FnvOffsetBasis;
		for (const geometry::Vec3& p : mesh.positions)
		{
			hashDouble(hash, p.x);
			hashDouble(hash, p.y);
			hashDouble(hash, p.z);
		}
		for (const geometry::Vec3& n : mesh.normals)
		{
			hashDouble(hash, n.x);
			hashDouble(hash, n.y);
			hashDouble(hash, n.z);
		}
		for (const geometry::Vec2& uv : mesh.uvs)
		{
			hashDouble(hash, uv.u);
			hashDouble(hash, uv.v);
		}
		for (const geometry::MeshSection& section : mesh.sections)
		{
			hashBytes(hash, section.slot.data(), section.slot.size());
			hashBytes(hash, section.indices.data(), section.indices.size() * sizeof(std::uint32_t));
		}
		return hash;
	}

	Scene compileScene(const cad::Project& project, const ViewOverlay& overlay)
	{
		Scene scene = compileScene(project);
		for (SceneNode& node : scene.nodes)
		{
			if (!overlay.hiddenWall.isNil() && node.id == wallBodyNodeId(overlay.hiddenWall))
			{
				node.style = NodeStyle::Hidden;
			}
			else if (!overlay.selectedWall.isNil() && node.id == wallBodyNodeId(overlay.selectedWall))
			{
				node.style = NodeStyle::Selected;
			}
		}

		if (overlay.preview && cad::validateWallGeometry(*overlay.preview).ok())
		{
			SceneNode node;
			node.id = SceneNodeId{"preview/wall"};
			node.name = "Wall preview";
			node.sourceKind = SourceKind::Wall;
			node.mesh = geometry::buildWallMesh(*overlay.preview);
			node.contentHash = hashMesh(node.mesh);
			node.style = NodeStyle::Preview;
			scene.nodes.push_back(std::move(node));
		}

		for (std::size_t index = 0; index < overlay.markers.size(); ++index)
		{
			// A marker is a square post: a short "wall" as long as it is thick, centred on the point.
			const cad::Point2D& point = overlay.markers[index];
			const std::int64_t half = overlay.markerSizeMm / 2;
			const cad::Wall post{cad::WallId{}, {point.x_mm - half, point.y_mm}, {point.x_mm + half, point.y_mm},
				overlay.markerSizeMm, overlay.markerHeightMm};
			if (!cad::validateWallGeometry(post).ok())
			{
				continue;
			}
			SceneNode node;
			node.id = SceneNodeId{"marker/" + std::to_string(index)};
			node.name = "Marker " + std::to_string(index);
			node.sourceKind = SourceKind::Marker;
			node.mesh = geometry::buildWallMesh(post);
			node.contentHash = hashMesh(node.mesh);
			node.style = NodeStyle::Handle;
			scene.nodes.push_back(std::move(node));
		}
		return scene;
	}

	Scene compileScene(const cad::Project& project)
	{
		Scene scene;
		scene.nodes.reserve(project.walls.size());
		for (const cad::Wall& wall : project.walls)
		{
			SceneNode node;
			node.id = wallBodyNodeId(wall.id);
			node.sourceKind = SourceKind::Wall;
			node.sourceId = cad::toString(wall.id);
			node.name = "Wall " + node.sourceId.substr(0, 8);
			node.mesh = geometry::buildWallMesh(wall);
			node.contentHash = hashMesh(node.mesh);
			scene.nodes.push_back(std::move(node));
		}
		return scene;
	}
}
