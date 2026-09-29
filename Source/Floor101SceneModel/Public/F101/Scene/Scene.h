#pragma once

#include "F101/Geometry/MeshData.h"
#include "F101/Geometry/Rooms.h"

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

#ifndef FLOOR101SCENEMODEL_API
	#define FLOOR101SCENEMODEL_API
#endif

// Neutral scene compiled from the model and consumed by Unreal, exporters and 2D views (ADR-0003).
// Read-only for consumers; never saved as project data and never edited.
namespace f101::scene
{
	// Stable identity of a node, derived from its source entity and role (e.g. "<wall uuid>/body"),
	// so recompiling an unchanged entity always yields the same id.
	struct SceneNodeId
	{
		std::string value;

		friend auto operator<=>(const SceneNodeId&, const SceneNodeId&) = default;
	};

	enum class SourceKind
	{
		Wall,
		Joint,  // fill at the centre of a junction of three or more walls
		Room,   // floor of a room
		Marker,
	};

	// How a consumer presents a node. View state (selection, previews, handles): never saved, ignored by exporters.
	enum class NodeStyle
	{
		Normal,
		Selected,
		Preview,
		Handle,
	};

	struct SceneNode
	{
		SceneNodeId id;
		std::string name;            // human readable, e.g. "Wall 3f2a9c1e"
		SourceKind sourceKind = SourceKind::Wall;
		std::string sourceId;        // UUID text of the source wall (for floors: the room key)
		geometry::MeshData mesh;     // scene space (mm, right-handed, Z up); node transform is identity for now
		std::uint64_t contentHash = 0; // hash of the mesh; changes whenever the geometry changes
		NodeStyle style = NodeStyle::Normal;
	};

	struct Scene
	{
		std::vector<SceneNode> nodes;
		// The rooms whose floors are among the nodes ("<room key>/floor"): areas and label points for views.
		std::vector<geometry::Room> rooms;

		const SceneNode* findNode(const SceneNodeId& id) const
		{
			for (const SceneNode& node : nodes)
			{
				if (node.id == id)
				{
					return &node;
				}
			}
			return nullptr;
		}
	};
}
