#include "F101/Geometry/Rooms.h"

#include "F101/Cad/Assert.h"
#include "F101/Cad/Wall.h"
#include "F101/Geometry/PolygonGeometry.h"
#include "MeshBuilding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <tuple>

namespace f101::geometry
{
	using namespace detail;

	namespace
	{
		constexpr double MergeToleranceMm = 0.5;
		constexpr double ParallelEpsilon = 1e-9;
		constexpr double BodyToleranceMm = 1e-6;

		struct Edge
		{
			std::size_t from = 0;
			std::size_t to = 0;
			std::size_t wall = 0;
			std::size_t segment = 0; // index along the wall, from its start
		};

		// Directed side of an edge; the face it bounds is on its left.
		struct HalfEdge
		{
			std::size_t from = 0;
			std::size_t to = 0;
			std::size_t edge = 0;
			bool forward = true; // runs from the wall's start towards its end
			double angle = 0.0;
		};

		class Graph
		{
		public:
			std::size_t vertex(const cad::PlanPoint& point)
			{
				for (std::size_t i = 0; i < vertices.size(); ++i)
				{
					if (length2(sub(vertices[i], point)) <= MergeToleranceMm)
					{
						return i;
					}
				}
				vertices.push_back(point);
				return vertices.size() - 1;
			}

			std::vector<cad::PlanPoint> vertices;
		};

		// Where a wall end touches another wall's body away from its ends (T), the end joins that wall's axis there.
		struct Host
		{
			std::size_t wall = 0;
			double along = 0.0;
			double across = std::numeric_limits<double>::infinity();
		};

		std::optional<Host> findHost(const cad::Project& project, std::size_t self, const cad::Point2D& point)
		{
			std::optional<Host> best;
			const cad::PlanPoint p = toPlan(point);
			for (std::size_t index = 0; index < project.walls.size(); ++index)
			{
				const cad::Wall& wall = project.walls[index];
				if (index == self || wall.start == point || wall.end == point)
				{
					continue;
				}
				const double length = wall.lengthMm();
				const cad::PlanPoint axis = scale(sub(toPlan(wall.end), toPlan(wall.start)), 1.0 / length);
				const cad::PlanPoint relative = sub(p, toPlan(wall.start));
				const double along = dot2(relative, axis);
				const double across = std::fabs(cross2(axis, relative));
				if (along > 0.0 && along < length && across <= static_cast<double>(wall.thicknessMm) / 2.0 + BodyToleranceMm &&
					(!best || across < best->across))
				{
					best = Host{index, along, across};
				}
			}
			return best;
		}

		cad::PlanPoint unit(const cad::PlanPoint& v)
		{
			return scale(v, 1.0 / length2(v));
		}

		std::optional<cad::PlanPoint> intersectLines(cad::PlanPoint p, cad::PlanPoint d, cad::PlanPoint q, cad::PlanPoint e)
		{
			const double denominator = cross2(d, e);
			if (std::fabs(denominator) < ParallelEpsilon)
			{
				return std::nullopt;
			}
			return add(p, scale(d, cross2(sub(q, p), e) / denominator));
		}

		cad::PlanPoint labelPointOf(const std::vector<cad::PlanPoint>& outline, double area)
		{
			cad::PlanPoint centroid;
			for (std::size_t i = 0; i < outline.size(); ++i)
			{
				const cad::PlanPoint& a = outline[i];
				const cad::PlanPoint& b = outline[(i + 1) % outline.size()];
				const double w = cross2(a, b);
				centroid = add(centroid, scale(add(a, b), w));
			}
			centroid = scale(centroid, 1.0 / (6.0 * area));
			if (containsPoint(outline, centroid))
			{
				return centroid;
			}
			// Concave room whose centroid falls outside: the centre of its largest triangle is inside.
			const std::vector<std::uint32_t> triangles = triangulatePolygon(outline);
			cad::PlanPoint best = outline.front();
			double bestArea = -1.0;
			for (std::size_t t = 0; t < triangles.size(); t += 3)
			{
				const cad::PlanPoint& a = outline[triangles[t]];
				const cad::PlanPoint& b = outline[triangles[t + 1]];
				const cad::PlanPoint& c = outline[triangles[t + 2]];
				const double triangleArea = cross2(sub(b, a), sub(c, a)) / 2.0;
				if (triangleArea > bestArea)
				{
					bestArea = triangleArea;
					best = scale(add(add(a, b), c), 1.0 / 3.0);
				}
			}
			return best;
		}
	}

	std::vector<Room> detectRooms(const cad::Project& project)
	{
		const std::vector<cad::Wall>& walls = project.walls;
		const std::size_t count = walls.size();

		// 1. Where each wall's axis is split (distance from its start), and where T ends join their host's axis.
		std::vector<std::vector<double>> splits(count);
		std::vector<std::optional<cad::PlanPoint>> startJoin(count);
		std::vector<std::optional<cad::PlanPoint>> endJoin(count);
		for (std::size_t index = 0; index < count; ++index)
		{
			const cad::Wall& wall = walls[index];
			F101_ASSERT(cad::validateWallGeometry(wall).ok());
			splits[index] = {0.0, wall.lengthMm()};
			for (const bool atStart : {true, false})
			{
				if (const std::optional<Host> host = findHost(project, index, atStart ? wall.start : wall.end))
				{
					const cad::Wall& hostWall = walls[host->wall];
					splits[host->wall].push_back(host->along);
					const cad::PlanPoint onAxis = add(toPlan(hostWall.start),
						scale(unit(sub(toPlan(hostWall.end), toPlan(hostWall.start))), host->along));
					(atStart ? startJoin : endJoin)[index] = onAxis;
				}
			}
		}
		for (std::size_t i = 0; i < count; ++i)
		{
			for (std::size_t j = i + 1; j < count; ++j)
			{
				const cad::PlanPoint si = toPlan(walls[i].start);
				const cad::PlanPoint di = sub(toPlan(walls[i].end), si);
				const cad::PlanPoint sj = toPlan(walls[j].start);
				const cad::PlanPoint dj = sub(toPlan(walls[j].end), sj);
				const double denominator = cross2(di, dj);
				if (std::fabs(denominator) < ParallelEpsilon)
				{
					continue;
				}
				const double t = cross2(sub(sj, si), dj) / denominator;
				const double s = cross2(sub(sj, si), di) / denominator;
				const double li = walls[i].lengthMm();
				const double lj = walls[j].lengthMm();
				if (t * li > MergeToleranceMm && (1.0 - t) * li > MergeToleranceMm && s * lj > MergeToleranceMm && (1.0 - s) * lj > MergeToleranceMm)
				{
					splits[i].push_back(t * li); // crossing axes
					splits[j].push_back(s * lj);
				}
			}
		}

		// 2. Planar graph of axis segments.
		Graph graph;
		std::vector<Edge> edges;
		for (std::size_t index = 0; index < count; ++index)
		{
			std::vector<double>& along = splits[index];
			std::sort(along.begin(), along.end());
			along.erase(std::unique(along.begin(), along.end(), [](double a, double b) { return b - a <= MergeToleranceMm; }), along.end());
			const cad::Wall& wall = walls[index];
			const cad::PlanPoint start = toPlan(wall.start);
			const cad::PlanPoint axis = unit(sub(toPlan(wall.end), start));
			std::vector<std::size_t> points;
			for (std::size_t k = 0; k < along.size(); ++k)
			{
				cad::PlanPoint p = add(start, scale(axis, along[k]));
				if (k == 0 && startJoin[index])
				{
					p = *startJoin[index];
				}
				else if (k + 1 == along.size())
				{
					p = endJoin[index] ? *endJoin[index] : toPlan(wall.end);
				}
				points.push_back(graph.vertex(p));
			}
			for (std::size_t k = 0; k + 1 < points.size(); ++k)
			{
				if (points[k] != points[k + 1])
				{
					edges.push_back(Edge{points[k], points[k + 1], index, k});
				}
			}
		}

		std::vector<HalfEdge> halves;
		std::vector<std::vector<std::size_t>> outgoing(graph.vertices.size());
		for (std::size_t e = 0; e < edges.size(); ++e)
		{
			for (const bool forward : {true, false})
			{
				const std::size_t from = forward ? edges[e].from : edges[e].to;
				const std::size_t to = forward ? edges[e].to : edges[e].from;
				const cad::PlanPoint d = sub(graph.vertices[to], graph.vertices[from]);
				outgoing[from].push_back(halves.size());
				halves.push_back(HalfEdge{from, to, e, forward, std::atan2(d.y_mm, d.x_mm)});
			}
		}
		for (std::vector<std::size_t>& list : outgoing)
		{
			std::sort(list.begin(), list.end(), [&](std::size_t a, std::size_t b) { return halves[a].angle < halves[b].angle; });
		}
		// The face on the left continues with the outgoing side just clockwise of the way back.
		auto next = [&](std::size_t h) {
			const std::vector<std::size_t>& list = outgoing[halves[h].to];
			const std::size_t twin = h ^ 1u;
			const std::size_t k = static_cast<std::size_t>(std::find(list.begin(), list.end(), twin) - list.begin());
			return list[(k + list.size() - 1) % list.size()];
		};

		// 3. Bounded faces (counter-clockwise) become rooms.
		std::vector<Room> rooms;
		std::vector<bool> visited(halves.size(), false);
		for (std::size_t first = 0; first < halves.size(); ++first)
		{
			if (visited[first])
			{
				continue;
			}
			std::vector<std::size_t> cycle;
			for (std::size_t h = first; !visited[h]; h = next(h))
			{
				visited[h] = true;
				cycle.push_back(h);
			}
			std::vector<cad::PlanPoint> axisOutline;
			for (const std::size_t h : cycle)
			{
				axisOutline.push_back(graph.vertices[halves[h].from]);
			}
			if (signedArea(axisOutline) <= 0.0)
			{
				continue; // the outside of a group of walls
			}

			// Floor: each side moved inwards (left) by its wall's half-thickness.
			std::vector<cad::PlanPoint> outline;
			auto push = [&](const cad::PlanPoint& p) {
				if (outline.empty() || length2(sub(p, outline.back())) > 1e-6)
				{
					outline.push_back(p);
				}
			};
			for (std::size_t i = 0; i < cycle.size(); ++i)
			{
				const HalfEdge& a = halves[cycle[i]];
				const HalfEdge& b = halves[cycle[(i + 1) % cycle.size()]];
				const cad::PlanPoint corner = graph.vertices[a.to];
				const cad::PlanPoint da = unit(sub(graph.vertices[a.to], graph.vertices[a.from]));
				const cad::PlanPoint db = unit(sub(graph.vertices[b.to], graph.vertices[b.from]));
				const double ha = static_cast<double>(walls[edges[a.edge].wall].thicknessMm) / 2.0;
				const double hb = static_cast<double>(walls[edges[b.edge].wall].thicknessMm) / 2.0;
				const cad::PlanPoint sideA = add(corner, scale(leftOf(da), ha));
				const cad::PlanPoint sideB = add(corner, scale(leftOf(db), hb));
				if (const std::optional<cad::PlanPoint> meet = intersectLines(sideA, da, sideB, db))
				{
					push(*meet);
				}
				else
				{
					push(sideA); // straight on (possibly a step in thickness) or around the free end of a wall
					push(sideB);
				}
			}
			while (outline.size() > 1 && length2(sub(outline.front(), outline.back())) <= 1e-6)
			{
				outline.pop_back();
			}
			const double area = signedArea(outline);
			if (outline.size() < 3 || area <= 0.0 || triangulatePolygon(outline).empty())
			{
				continue; // too small for a floor once the walls are taken out
			}

			Room room;
			std::tuple<cad::WallId, bool, std::size_t> identity{walls[edges[halves[cycle[0]].edge].wall].id, true, 0};
			bool haveIdentity = false;
			for (const std::size_t h : cycle)
			{
				const Edge& edge = edges[halves[h].edge];
				room.walls.push_back(walls[edge.wall].id);
				// "left" first: the room is on the wall's left when the side runs along the wall.
				const std::tuple<cad::WallId, bool, std::size_t> candidate{walls[edge.wall].id, !halves[h].forward, edge.segment};
				if (!haveIdentity || candidate < identity)
				{
					identity = candidate;
					haveIdentity = true;
				}
			}
			std::sort(room.walls.begin(), room.walls.end());
			room.walls.erase(std::unique(room.walls.begin(), room.walls.end()), room.walls.end());
			room.key = cad::toString(std::get<0>(identity)) + (std::get<1>(identity) ? "/room-right-" : "/room-left-") +
				std::to_string(std::get<2>(identity));
			room.areaMm2 = area;
			room.labelPoint = labelPointOf(outline, area);
			room.outline = std::move(outline);
			rooms.push_back(std::move(room));
		}
		std::sort(rooms.begin(), rooms.end(), [](const Room& a, const Room& b) { return a.key < b.key; });
		return rooms;
	}

	MeshData buildFloorMesh(const Room& room)
	{
		return buildPrismMesh(room.outline, 0.0, FloorThicknessMm, PrismSlots{floor_slots::Top, floor_slots::Side, floor_slots::Bottom});
	}

	cad::RoomLocator makeRoomLocator()
	{
		return [](const cad::Project& levelWalls, cad::PlanPoint point) -> std::optional<std::string>
		{
			for (const Room& room : detectRooms(levelWalls))
			{
				if (containsPoint(room.outline, point))
				{
					return room.key;
				}
			}
			return std::nullopt;
		};
	}

	MeshData buildSlabMesh(const Room& room, double bottomZ, double topZ)
	{
		return buildPrismMesh(room.outline, bottomZ, topZ, PrismSlots{slab_slots::Top, slab_slots::Side, slab_slots::Ceiling});
	}
}
