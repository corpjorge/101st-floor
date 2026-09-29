#include "F101/Scene/SceneCompiler.h"

#include "F101/Geometry/PolygonGeometry.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Geometry/WallGeometryBuilder.h"
#include "F101/Geometry/WallJoins.h"

#include <algorithm>
#include <cmath>
#include <string>

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

	namespace
	{
		// "11.17 m2" (floor area rounded to 0.01 m2).
		std::string areaText(double areaMm2)
		{
			const long long hundredths = std::llround(areaMm2 / 10'000.0);
			const std::string cents = std::to_string(hundredths % 100);
			return std::to_string(hundredths / 100) + "." + (cents.size() < 2 ? "0" + cents : cents) + " m2";
		}

		// Walls (with their node ids and styles), joint fills and room floors of a project.
		Scene compileProject(const cad::Project& project, const std::vector<SceneNodeId>& wallIds, const std::vector<NodeStyle>& wallStyles)
		{
			Scene scene;
			const geometry::WallJoins joins = geometry::solveWallJoins(project);
			scene.rooms = geometry::detectRooms(project);
			scene.nodes.reserve(project.walls.size() + joins.fills.size() + scene.rooms.size());

			for (std::size_t index = 0; index < project.walls.size(); ++index)
			{
				const cad::Wall& wall = project.walls[index];
				SceneNode node;
				node.id = wallIds[index];
				node.sourceKind = SourceKind::Wall;
				node.sourceId = cad::toString(wall.id);
				node.name = "Wall " + node.sourceId.substr(0, 8);
				node.mesh = geometry::buildWallMesh(wall, joins.footprints[index]);
				node.contentHash = hashMesh(node.mesh);
				node.style = wallStyles[index];
				scene.nodes.push_back(std::move(node));
			}
			for (const geometry::JointFill& fill : joins.fills)
			{
				SceneNode node;
				node.sourceKind = SourceKind::Joint;
				node.sourceId = cad::toString(fill.wall);
				node.id = SceneNodeId{node.sourceId + (fill.atStart ? "/joint-start" : "/joint-end")};
				node.name = "Joint " + node.sourceId.substr(0, 8);
				node.mesh = geometry::buildPrismMesh(fill.outline, 0.0, static_cast<double>(fill.heightMm));
				node.contentHash = hashMesh(node.mesh);
				scene.nodes.push_back(std::move(node));
			}
			for (const geometry::Room& room : scene.rooms)
			{
				SceneNode node;
				node.id = SceneNodeId{room.key + "/floor"};
				node.sourceKind = SourceKind::Room;
				node.sourceId = room.key;
				node.name = "Room " + areaText(room.areaMm2);
				node.mesh = geometry::buildFloorMesh(room);
				node.contentHash = hashMesh(node.mesh);
				scene.nodes.push_back(std::move(node));
			}
			return scene;
		}
	}

	Scene compileScene(const cad::Project& project)
	{
		std::vector<SceneNodeId> ids;
		for (const cad::Wall& wall : project.walls)
		{
			ids.push_back(wallBodyNodeId(wall.id));
		}
		return compileProject(project, ids, std::vector<NodeStyle>(project.walls.size(), NodeStyle::Normal));
	}

	Scene compileScene(const cad::Project& project, const ViewOverlay& overlay)
	{
		cad::Project previewed = project;
		std::vector<SceneNodeId> ids;
		std::vector<NodeStyle> styles;
		for (const cad::Wall& wall : project.walls)
		{
			ids.push_back(wallBodyNodeId(wall.id));
			styles.push_back(!overlay.selectedWall.isNil() && wall.id == overlay.selectedWall ? NodeStyle::Selected : NodeStyle::Normal);
		}
		std::uint64_t added = 0;
		for (const cad::Wall& preview : overlay.previewWalls)
		{
			if (!cad::validateWallGeometry(preview).ok())
			{
				continue;
			}
			const auto existing = std::find_if(previewed.walls.begin(), previewed.walls.end(),
				[&preview](const cad::Wall& wall) { return !preview.id.isNil() && wall.id == preview.id; });
			if (existing != previewed.walls.end())
			{
				*existing = preview;
				styles[static_cast<std::size_t>(existing - previewed.walls.begin())] = NodeStyle::Preview;
				continue;
			}
			// A new wall gets a stand-in id (never a real UUID v4) so joins and rooms can refer to it.
			cad::Wall wall = preview;
			wall.id = cad::WallId{cad::Uuid{~0ull, ++added}};
			previewed.walls.push_back(wall);
			ids.push_back(SceneNodeId{"preview/wall/" + std::to_string(added - 1)});
			styles.push_back(NodeStyle::Preview);
		}

		Scene scene = compileProject(previewed, ids, styles);
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
}
