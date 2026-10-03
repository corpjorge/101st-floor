#pragma once

#include "F101/Cad/Geometry2D.h"
#include "F101/Geometry/MeshData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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

	// Appends a planar convex polygon (corners in perimeter order, either direction) as a triangle fan.
	inline void addConvexPolygon(MeshData& mesh, const char* slot, std::vector<Corner> corners, const Vec3& normal)
	{
		if (corners.size() < 3)
		{
			return;
		}
		if (dot(cross(subtract(corners[1].position, corners[0].position), subtract(corners[2].position, corners[0].position)), normal) < 0.0)
		{
			std::reverse(corners.begin() + 1, corners.end());
		}
		const auto base = static_cast<std::uint32_t>(mesh.positions.size());
		for (const Corner& corner : corners)
		{
			mesh.positions.push_back(corner.position);
			mesh.normals.push_back(normal);
			mesh.uvs.push_back(corner.uv);
		}
		MeshSection& section = sectionFor(mesh, slot);
		for (std::uint32_t i = 1; i + 1 < corners.size(); ++i)
		{
			section.indices.insert(section.indices.end(), {base, base + i, base + i + 1});
		}
	}

	inline Vec3 normalized(const Vec3& v)
	{
		const double length = std::sqrt(dot(v, v));
		return length > 0.0 ? Vec3{v.x / length, v.y / length, v.z / length} : Vec3{0.0, 0.0, 1.0};
	}

	// Two unit vectors square to a unit direction and to each other, (u, v, direction) right-handed: u horizontal unless the
	// direction is vertical.
	inline std::pair<Vec3, Vec3> frameAround(const Vec3& direction)
	{
		const Vec3 up{0.0, 0.0, 1.0};
		Vec3 u = cross(direction, up);
		u = dot(u, u) < 1e-12 ? Vec3{1.0, 0.0, 0.0} : normalized(u);
		return {u, cross(direction, u)};
	}

	// A smooth tube of `radius` from `a` to `b` with `sides` facets: smooth side normals, outward; flat caps where asked.
	// UVs in metres: v along the tube from `vStart`, u around it.
	inline void addTube(MeshData& mesh, const char* slot, const Vec3& a, const Vec3& b, double radius, int sides, double vStart,
		bool capStart, bool capEnd)
	{
		const Vec3 axis = subtract(b, a);
		const double length = std::sqrt(dot(axis, axis));
		if (length <= 0.0 || radius <= 0.0 || sides < 3)
		{
			return;
		}
		const Vec3 d{axis.x / length, axis.y / length, axis.z / length};
		const auto [u, v] = frameAround(d);
		constexpr double Tau = 6.28318530717958647692;
		const double circumference = Tau * radius;
		const auto base = static_cast<std::uint32_t>(mesh.positions.size());
		for (int i = 0; i <= sides; ++i)
		{
			const double angle = Tau * static_cast<double>(i) / static_cast<double>(sides);
			const Vec3 n{u.x * std::cos(angle) + v.x * std::sin(angle), u.y * std::cos(angle) + v.y * std::sin(angle),
				u.z * std::cos(angle) + v.z * std::sin(angle)};
			const double around = circumference * static_cast<double>(i) / static_cast<double>(sides) * MetersPerMillimeter;
			for (const auto& [end, along] : {std::pair{a, vStart}, {b, vStart + length}})
			{
				mesh.positions.push_back(Vec3{end.x + n.x * radius, end.y + n.y * radius, end.z + n.z * radius});
				mesh.normals.push_back(n);
				mesh.uvs.push_back(Vec2{around, along * MetersPerMillimeter});
			}
		}
		MeshSection& section = sectionFor(mesh, slot);
		for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(sides); ++i)
		{
			const std::uint32_t a0 = base + i * 2;
			const std::uint32_t b0 = a0 + 1;
			const std::uint32_t a1 = a0 + 2;
			const std::uint32_t b1 = a0 + 3;
			section.indices.insert(section.indices.end(), {a0, a1, b1, a0, b1, b0});
		}
		for (const auto& [end, wanted, sign] : {std::tuple{a, capStart, -1.0}, {b, capEnd, 1.0}})
		{
			if (!wanted)
			{
				continue;
			}
			std::vector<Corner> corners;
			for (int i = 0; i < sides; ++i)
			{
				const double angle = Tau * static_cast<double>(i) / static_cast<double>(sides);
				const double cu = std::cos(angle) * radius;
				const double cv = std::sin(angle) * radius;
				corners.push_back(Corner{Vec3{end.x + u.x * cu + v.x * cv, end.y + u.y * cu + v.y * cv, end.z + u.z * cu + v.z * cv},
					Vec2{cu * MetersPerMillimeter, cv * MetersPerMillimeter}});
			}
			addConvexPolygon(mesh, slot, std::move(corners), Vec3{d.x * sign, d.y * sign, d.z * sign});
		}
	}

	// A smooth ball of `radius` at `centre` (`rings` bands, `sides` around), outward normals.
	inline void addBall(MeshData& mesh, const char* slot, const Vec3& centre, double radius, int rings, int sides)
	{
		constexpr double Pi = 3.14159265358979323846;
		const auto base = static_cast<std::uint32_t>(mesh.positions.size());
		for (int r = 0; r <= rings; ++r)
		{
			const double polar = Pi * static_cast<double>(r) / static_cast<double>(rings);
			for (int s = 0; s <= sides; ++s)
			{
				const double azimuth = 2.0 * Pi * static_cast<double>(s) / static_cast<double>(sides);
				const Vec3 n{std::sin(polar) * std::cos(azimuth), std::sin(polar) * std::sin(azimuth), std::cos(polar)};
				mesh.positions.push_back(Vec3{centre.x + n.x * radius, centre.y + n.y * radius, centre.z + n.z * radius});
				mesh.normals.push_back(n);
				mesh.uvs.push_back(Vec2{2.0 * Pi * radius * static_cast<double>(s) / static_cast<double>(sides) * MetersPerMillimeter,
					Pi * radius * static_cast<double>(r) / static_cast<double>(rings) * MetersPerMillimeter});
			}
		}
		MeshSection& section = sectionFor(mesh, slot);
		const auto row = static_cast<std::uint32_t>(sides + 1);
		for (std::uint32_t r = 0; r < static_cast<std::uint32_t>(rings); ++r)
		{
			for (std::uint32_t s = 0; s < static_cast<std::uint32_t>(sides); ++s)
			{
				const std::uint32_t p = base + r * row + s; // towards the bottom pole as r grows
				const std::uint32_t q = p + row;
				if (r > 0)
				{
					section.indices.insert(section.indices.end(), {p, q, p + 1});
				}
				if (r + 1 < static_cast<std::uint32_t>(rings))
				{
					section.indices.insert(section.indices.end(), {p + 1, q, q + 1});
				}
			}
		}
	}

	// Box aligned with a wall: `u` along `axis` (unit, plan), `s` across it towards `left` (unit, plan), `z` up, from
	// `origin` (plan, z = 0). Six flat faces; UVs in metres ((s, z) on the ends, (u, z) on the sides, (u, s) top and
	// bottom).
	inline void addWallBox(MeshData& mesh, const char* slot, cad::PlanPoint origin, cad::PlanPoint axis, cad::PlanPoint left,
		double u0, double u1, double s0, double s1, double z0, double z1)
	{
		auto at = [&](double u, double s, double z)
		{
			const cad::PlanPoint p = add(origin, add(scale(axis, u), scale(left, s)));
			return Vec3{p.x_mm, p.y_mm, z};
		};
		auto corner = [&](double u, double s, double z, double tu, double tv)
		{
			return Corner{at(u, s, z), Vec2{tu * MetersPerMillimeter, tv * MetersPerMillimeter}};
		};
		const Vec3 alongNormal{axis.x_mm, axis.y_mm, 0.0};
		const Vec3 acrossNormal{left.x_mm, left.y_mm, 0.0};
		// Ends (normal -axis and +axis).
		for (const auto& [u, sign] : {std::pair{u0, -1.0}, {u1, 1.0}})
		{
			addQuad(mesh, slot, corner(u, s0, z0, s0, z0), corner(u, s1, z0, s1, z0), corner(u, s1, z1, s1, z1),
				corner(u, s0, z1, s0, z1), Vec3{alongNormal.x * sign, alongNormal.y * sign, 0.0});
		}
		// Sides (normal -left and +left).
		for (const auto& [s, sign] : {std::pair{s0, -1.0}, {s1, 1.0}})
		{
			addQuad(mesh, slot, corner(u0, s, z0, u0, z0), corner(u1, s, z0, u1, z0), corner(u1, s, z1, u1, z1),
				corner(u0, s, z1, u0, z1), Vec3{acrossNormal.x * sign, acrossNormal.y * sign, 0.0});
		}
		// Bottom and top.
		for (const auto& [z, sign] : {std::pair{z0, -1.0}, {z1, 1.0}})
		{
			addQuad(mesh, slot, corner(u0, s0, z, u0, s0), corner(u1, s0, z, u1, s0), corner(u1, s1, z, u1, s1),
				corner(u0, s1, z, u0, s1), Vec3{0.0, 0.0, sign});
		}
	}
}
