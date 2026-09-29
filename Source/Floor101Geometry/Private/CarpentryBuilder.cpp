#include "F101/Geometry/CarpentryBuilder.h"

#include "MeshBuilding.h"

#include <algorithm>

namespace f101::geometry
{
	using namespace detail;

	MeshData buildOpeningMesh(const cad::Wall& wall, const WallFootprint& footprint, const cad::Opening& opening)
	{
		MeshData mesh;
		const cad::PlanPoint start = toPlan(wall.start);
		const cad::PlanPoint axisVector = sub(toPlan(wall.end), start);
		const cad::PlanPoint axis = scale(axisVector, 1.0 / length2(axisVector));
		const cad::PlanPoint left = leftOf(axis);
		auto along = [&](const cad::PlanPoint& p) { return dot2(sub(p, start), axis); };

		// The hole, trimmed like the wall's (WallGeometryBuilder).
		const double faceStart = std::max(along(footprint.startLeft), along(footprint.startRight));
		const double faceEnd = std::min(along(footprint.endLeft), along(footprint.endRight));
		const double u0 = std::max(static_cast<double>(opening.offsetMm), faceStart);
		const double u1 = std::min(static_cast<double>(opening.offsetMm + opening.widthMm), faceEnd);
		const double z0 = static_cast<double>(opening.sillMm);
		const double z1 = static_cast<double>(opening.sillMm + opening.heightMm);
		if (u1 - u0 <= 0.0 || z1 - z0 <= 0.0)
		{
			return mesh;
		}

		// Small openings get thinner profiles so something is left inside.
		const double frame = std::min({FrameWidthMm, (u1 - u0) / 4.0, (z1 - z0) / 4.0});
		const double depth = std::min(FrameDepthMm, static_cast<double>(wall.thicknessMm)) / 2.0;
		auto box = [&](const char* slot, double bu0, double bu1, double bs0, double bs1, double bz0, double bz1)
		{
			if (bu1 > bu0 && bs1 > bs0 && bz1 > bz0)
			{
				addWallBox(mesh, slot, start, axis, left, bu0, bu1, bs0, bs1, bz0, bz1);
			}
		};

		const bool door = opening.kind == cad::OpeningKind::Door;
		box(carpentry_slots::Frame, u0, u0 + frame, -depth, depth, z0, z1);            // jambs
		box(carpentry_slots::Frame, u1 - frame, u1, -depth, depth, z0, z1);
		box(carpentry_slots::Frame, u0 + frame, u1 - frame, -depth, depth, z1 - frame, z1); // head
		if (door)
		{
			const double leaf = std::min(LeafThicknessMm, 2.0 * depth) / 2.0;
			const double leafU0 = u0 + frame + LeafClearanceMm;
			const double leafU1 = u1 - frame - LeafClearanceMm;
			const double leafZ1 = z1 - frame - LeafClearanceMm;
			box(carpentry_slots::Leaf, leafU0, leafU1, -leaf, leaf, z0 + DoorFloorGapMm, leafZ1);
			// A lever on each face, near the side opposite the hinges, at the usual height when the leaf allows it.
			const double handleZ = std::min(1000.0, (z0 + leafZ1) / 2.0);
			if (leafU1 - leafU0 >= 300.0)
			{
				const double handleU1 = leafU1 - 60.0;
				for (const double sign : {1.0, -1.0})
				{
					const double inner = sign * leaf;
					const double outer = sign * (leaf + 30.0);
					box(carpentry_slots::Hardware, handleU1 - 120.0, handleU1, std::min(inner, outer), std::max(inner, outer),
						handleZ, handleZ + 25.0);
				}
			}
		}
		else
		{
			box(carpentry_slots::Frame, u0 + frame, u1 - frame, -depth, depth, z0, z0 + frame); // bottom rail
			if (u1 - u0 > MullionFromWidthMm)
			{
				const double middle = (u0 + u1) / 2.0;
				box(carpentry_slots::Frame, middle - frame / 2.0, middle + frame / 2.0, -depth, depth, z0 + frame, z1 - frame);
			}
			const double glass = GlassThicknessMm / 2.0;
			box(carpentry_slots::Glass, u0 + frame, u1 - frame, -glass, glass, z0 + frame, z1 - frame);
		}
		return mesh;
	}
}
