#include "F101/Scene/SceneCompiler.h"

#include "F101/Geometry/CarpentryBuilder.h"
#include "F101/Geometry/FurnitureBuilder.h"
#include "F101/Geometry/PolygonGeometry.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Geometry/WallGeometryBuilder.h"
#include "F101/Geometry/WallJoins.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <utility>
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

	SceneNodeId furnitureNodeId(const cad::FurnitureId& id)
	{
		return SceneNodeId{"furniture/" + cad::toString(id)};
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

	std::uint64_t hashNode(const geometry::MeshData& mesh, const std::vector<std::string>& finishes)
	{
		std::uint64_t hash = hashMesh(mesh);
		for (const std::string& finish : finishes)
		{
			hashBytes(hash, finish.data(), finish.size());
			hashBytes(hash, "|", 1);
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

		// The finish of each section of a mesh: `special` for the named slot, `rest` for the others.
		std::vector<std::string> sectionFinishes(const geometry::MeshData& mesh,
			std::initializer_list<std::pair<std::string_view, std::string_view>> special, std::string_view rest)
		{
			std::vector<std::string> finishes;
			finishes.reserve(mesh.sections.size());
			for (const geometry::MeshSection& section : mesh.sections)
			{
				std::string_view finish = rest;
				for (const auto& [slot, value] : special)
				{
					if (section.slot == slot)
					{
						finish = value;
					}
				}
				finishes.emplace_back(finish);
			}
			return finishes;
		}

		void translateZ(geometry::MeshData& mesh, double dz)
		{
			for (geometry::Vec3& p : mesh.positions)
			{
				p.z += dz;
			}
		}

		// A level of the project with its derived elevation. A project without levels (e.g. geometry built in a test)
		// is one group at elevation 0 holding every wall, with no slabs.
		struct LevelGroup
		{
			cad::LevelId level;
			std::int64_t elevationMm = 0;
			const cad::Level* info = nullptr;
		};

		std::vector<LevelGroup> levelGroups(const cad::Project& project)
		{
			std::vector<LevelGroup> groups;
			if (project.levels.empty())
			{
				groups.push_back(LevelGroup{});
				return groups;
			}
			std::int64_t elevation = 0;
			for (const cad::Level& level : project.levels)
			{
				groups.push_back(LevelGroup{level.id, elevation, &level});
				elevation += level.heightMm;
			}
			return groups;
		}

		// Node ids and view styles of the walls and furniture of a project (same order as the project).
		struct Presentation
		{
			std::vector<SceneNodeId> wallIds;
			std::vector<NodeStyle> wallStyles;
			std::vector<SceneNodeId> furnitureIds;
			std::vector<NodeStyle> furnitureStyles;
		};

		// Walls, joint fills, room floors, slabs, carpentry and furniture of a project (with the node ids and styles of
		// `shown`), level by level: joins and rooms never mix levels, and each level's geometry is lifted to its elevation.
		// `activeLevel` (with `showAll` false) hides the levels above it and its own slabs and dims the levels below it.
		// Each section gets its finish (ADR-0012); the floor of `selectedRoom` is Selected. `symbols`: door swings on the
		// floors of the levels that are not dimmed (ADR-0014), last.
		Scene compileProject(const cad::Project& project, const Presentation& shown, const cad::LevelId& activeLevel, bool showAll,
			const std::string& selectedRoom, bool symbols)
		{
			const std::vector<LevelGroup> groups = levelGroups(project);
			std::optional<std::size_t> active;
			if (!activeLevel.isNil() && !showAll)
			{
				active = project.levelIndex(activeLevel);
			}

			std::vector<std::optional<SceneNode>> wallNodes(project.walls.size());
			std::vector<SceneNode> fills;
			std::vector<SceneNode> floors;
			std::vector<SceneNode> slabs;
			std::vector<SceneNode> openings;
			std::vector<std::optional<SceneNode>> furnitureNodes(project.furniture.size());
			std::vector<SceneNode> swings;
			Scene scene;
			for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
			{
				const LevelGroup& group = groups[groupIndex];
				if (active && groupIndex > *active)
				{
					break;
				}
				const bool context = active && groupIndex < *active;
				const bool withSlabs = group.info != nullptr && !(active && groupIndex == *active);
				const double dz = static_cast<double>(group.elevationMm);

				cad::Project levelProject;
				std::vector<std::size_t> indices;
				for (std::size_t index = 0; index < project.walls.size(); ++index)
				{
					if (project.levels.empty() || project.levelOf(project.walls[index]) == group.level)
					{
						levelProject.walls.push_back(project.walls[index]);
						indices.push_back(index);
					}
				}
				const geometry::WallJoins joins = geometry::solveWallJoins(levelProject);
				const std::vector<geometry::Room> rooms = geometry::detectRooms(levelProject);

				for (std::size_t k = 0; k < indices.size(); ++k)
				{
					const std::size_t index = indices[k];
					const cad::Wall& wall = project.walls[index];
					SceneNode node;
					node.id = shown.wallIds[index];
					node.sourceKind = SourceKind::Wall;
					node.sourceId = cad::toString(wall.id);
					node.level = group.level;
					node.name = "Wall " + node.sourceId.substr(0, 8);
					node.mesh = geometry::buildWallMesh(wall, joins.footprints[k]);
					translateZ(node.mesh, dz);
					node.finishes = sectionFinishes(node.mesh,
						{{geometry::wall_slots::Left, cad::resolveFinish(wall.finishLeft, cad::Surface::Wall)},
							{geometry::wall_slots::Right, cad::resolveFinish(wall.finishRight, cad::Surface::Wall)}},
						cad::DefaultWallFinish);
					node.contentHash = hashNode(node.mesh, node.finishes);
					node.style = context && shown.wallStyles[index] == NodeStyle::Normal ? NodeStyle::Context : shown.wallStyles[index];
					for (const cad::Opening& opening : wall.openings)
					{
						SceneNode carpentry;
						carpentry.id = SceneNodeId{node.sourceId + "/opening/" + cad::toString(opening.id)};
						carpentry.sourceKind = SourceKind::Opening;
						carpentry.sourceId = cad::toString(opening.id);
						carpentry.level = group.level;
						carpentry.name = std::string(opening.kind == cad::OpeningKind::Door ? "Door " : "Window ") + carpentry.sourceId.substr(0, 8);
						carpentry.mesh = geometry::buildOpeningMesh(wall, joins.footprints[k], opening);
						if (carpentry.mesh.sections.empty())
						{
							continue;
						}
						translateZ(carpentry.mesh, dz);
						const std::string_view frame = cad::resolveFinish(opening.finish, cad::Surface::Carpentry);
						carpentry.finishes = sectionFinishes(carpentry.mesh,
							{{geometry::carpentry_slots::Hardware, cad::HardwareFinish}, {geometry::carpentry_slots::Glass, cad::DefaultGlazingFinish}},
							frame);
						carpentry.contentHash = hashNode(carpentry.mesh, carpentry.finishes);
						carpentry.style = node.style; // with its wall: selected, previewed, dimmed
						if (symbols && !context)
						{
							SceneNode swing;
							swing.mesh = geometry::buildOpeningSymbolMesh(wall, joins.footprints[k], opening);
							if (!swing.mesh.sections.empty())
							{
								swing.id = SceneNodeId{carpentry.id.value + "/swing"};
								swing.sourceKind = SourceKind::Opening;
								swing.sourceId = carpentry.sourceId;
								swing.level = group.level;
								swing.name = "Swing " + carpentry.sourceId.substr(0, 8);
								translateZ(swing.mesh, dz);
								swing.finishes = sectionFinishes(swing.mesh, {}, cad::DefaultWallFinish);
								swing.contentHash = hashNode(swing.mesh, swing.finishes);
								swing.style = NodeStyle::Symbol;
								swings.push_back(std::move(swing));
							}
						}
						openings.push_back(std::move(carpentry));
					}
					wallNodes[index] = std::move(node);
				}
				for (const geometry::JointFill& fill : joins.fills)
				{
					SceneNode node;
					node.sourceKind = SourceKind::Joint;
					node.sourceId = cad::toString(fill.wall);
					node.level = group.level;
					node.id = SceneNodeId{node.sourceId + (fill.atStart ? "/joint-start" : "/joint-end")};
					node.name = "Joint " + node.sourceId.substr(0, 8);
					node.mesh = geometry::buildPrismMesh(fill.outline, dz, dz + static_cast<double>(fill.heightMm));
					node.finishes = sectionFinishes(node.mesh, {}, cad::DefaultWallFinish);
					node.contentHash = hashNode(node.mesh, node.finishes);
					node.style = context ? NodeStyle::Context : NodeStyle::Normal;
					fills.push_back(std::move(node));
				}
				for (const geometry::Room& room : rooms)
				{
					const cad::RoomFinish* finish = project.findRoomFinish(room.key);
					SceneNode floor;
					floor.id = SceneNodeId{room.key + "/floor"};
					floor.sourceKind = SourceKind::Room;
					floor.sourceId = room.key;
					floor.level = group.level;
					floor.name = "Room " + areaText(room.areaMm2);
					floor.mesh = geometry::buildFloorMesh(room);
					translateZ(floor.mesh, dz);
					const std::string_view floorFinish = finish ? std::string_view(finish->floor) : std::string_view();
					const std::string_view ceilingFinish = finish ? std::string_view(finish->ceiling) : std::string_view();
					floor.finishes = sectionFinishes(floor.mesh, {}, cad::resolveFinish(floorFinish, cad::Surface::Floor));
					floor.contentHash = hashNode(floor.mesh, floor.finishes);
					floor.style = context ? NodeStyle::Context : (room.key == selectedRoom ? NodeStyle::Selected : NodeStyle::Normal);
					floors.push_back(std::move(floor));
					if (withSlabs)
					{
						const double top = dz + static_cast<double>(group.info->heightMm);
						SceneNode slab;
						slab.id = SceneNodeId{room.key + "/slab"};
						slab.sourceKind = SourceKind::Slab;
						slab.sourceId = room.key;
						slab.level = group.level;
						slab.name = "Slab " + areaText(room.areaMm2);
						slab.mesh = geometry::buildSlabMesh(room, top - static_cast<double>(group.info->slabThicknessMm), top);
						slab.finishes = sectionFinishes(slab.mesh,
							{{geometry::slab_slots::Ceiling, cad::resolveFinish(ceilingFinish, cad::Surface::Ceiling)}},
							cad::DefaultWallFinish);
						slab.contentHash = hashNode(slab.mesh, slab.finishes);
						slab.style = context ? NodeStyle::Context : NodeStyle::Normal;
						slabs.push_back(std::move(slab));
					}
					scene.rooms.push_back(SceneRoom{room, group.level, group.elevationMm});
				}
				for (std::size_t index = 0; index < project.furniture.size(); ++index)
				{
					const cad::Furniture& piece = project.furniture[index];
					if (!project.levels.empty() && piece.level != group.level)
					{
						continue;
					}
					SceneNode node;
					node.id = shown.furnitureIds[index];
					node.sourceKind = SourceKind::Furniture;
					node.sourceId = cad::toString(piece.id);
					node.level = group.level;
					node.name = "Furniture " + piece.type + " " + node.sourceId.substr(0, 8);
					node.mesh = geometry::buildFurnitureMesh(piece);
					if (node.mesh.sections.empty())
					{
						continue;
					}
					translateZ(node.mesh, dz);
					for (const geometry::MeshSection& section : node.mesh.sections)
					{
						node.finishes.emplace_back(geometry::furnitureSlotFinish(piece, section.slot));
					}
					node.contentHash = hashNode(node.mesh, node.finishes);
					node.style = context && shown.furnitureStyles[index] == NodeStyle::Normal ? NodeStyle::Context
						: shown.furnitureStyles[index];
					furnitureNodes[index] = std::move(node);
				}
			}

			scene.nodes.reserve(project.walls.size() + fills.size() + floors.size() + slabs.size() + openings.size() +
				project.furniture.size() + swings.size());
			for (std::optional<SceneNode>& node : wallNodes)
			{
				if (node)
				{
					scene.nodes.push_back(std::move(*node));
				}
			}
			for (std::vector<SceneNode>* group : {&fills, &floors, &slabs, &openings})
			{
				for (SceneNode& node : *group)
				{
					scene.nodes.push_back(std::move(node));
				}
			}
			for (std::optional<SceneNode>& node : furnitureNodes)
			{
				if (node)
				{
					scene.nodes.push_back(std::move(*node));
				}
			}
			for (SceneNode& node : swings)
			{
				scene.nodes.push_back(std::move(node));
			}
			return scene;
		}
	}

	Scene compileScene(const cad::Project& project)
	{
		Presentation shown;
		for (const cad::Wall& wall : project.walls)
		{
			shown.wallIds.push_back(wallBodyNodeId(wall.id));
		}
		for (const cad::Furniture& piece : project.furniture)
		{
			shown.furnitureIds.push_back(furnitureNodeId(piece.id));
		}
		shown.wallStyles.assign(project.walls.size(), NodeStyle::Normal);
		shown.furnitureStyles.assign(project.furniture.size(), NodeStyle::Normal);
		return compileProject(project, shown, cad::LevelId(), true, std::string(), false);
	}

	Scene compileScene(const cad::Project& project, const ViewOverlay& overlay)
	{
		cad::Project previewed = project;
		Presentation shown;
		std::vector<SceneNodeId>& ids = shown.wallIds;
		std::vector<NodeStyle>& styles = shown.wallStyles;
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
		for (const cad::Furniture& piece : project.furniture)
		{
			shown.furnitureIds.push_back(furnitureNodeId(piece.id));
			shown.furnitureStyles.push_back(
				!overlay.selectedFurniture.isNil() && piece.id == overlay.selectedFurniture ? NodeStyle::Selected : NodeStyle::Normal);
		}
		std::size_t addedFurniture = 0;
		for (const cad::Furniture& preview : overlay.previewFurniture)
		{
			if (!cad::validateFurniture(preview).ok())
			{
				continue;
			}
			const auto existing = std::find_if(previewed.furniture.begin(), previewed.furniture.end(),
				[&preview](const cad::Furniture& piece) { return !preview.id.isNil() && piece.id == preview.id; });
			if (existing != previewed.furniture.end())
			{
				*existing = preview;
				shown.furnitureStyles[static_cast<std::size_t>(existing - previewed.furniture.begin())] = NodeStyle::Preview;
				continue;
			}
			previewed.furniture.push_back(preview);
			shown.furnitureIds.push_back(SceneNodeId{"preview/furniture/" + std::to_string(addedFurniture++)});
			shown.furnitureStyles.push_back(NodeStyle::Preview);
		}

		Scene scene = compileProject(previewed, shown, overlay.activeLevel, overlay.showAllLevels, overlay.selectedRoom, true);
		// Handles stand on the level being edited.
		const double markerElevation = static_cast<double>(cad::levelElevationMm(project, overlay.activeLevel));
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
			translateZ(node.mesh, markerElevation);
			node.level = overlay.activeLevel;
			node.finishes = sectionFinishes(node.mesh, {}, cad::DefaultWallFinish);
			node.contentHash = hashNode(node.mesh, node.finishes);
			node.style = NodeStyle::Handle;
			scene.nodes.push_back(std::move(node));
		}
		return scene;
	}
}
