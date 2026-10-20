#include "F101/Geometry/CarpentryBuilder.h"

#include "F101/Geometry/Rooms.h"
#include "F101/Cad/OpeningCatalogue.h"
#include "MeshBuilding.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace f101::geometry
{
	using namespace detail;

	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		cad::PlanPoint rotate(cad::PlanPoint v, double radians)
		{
			const double c = std::cos(radians);
			const double s = std::sin(radians);
			return {v.x_mm * c - v.y_mm * s, v.x_mm * s + v.y_mm * c};
		}

		// Where boxes are built: `u` along `d`, `s` along `n` (unit, plan), from `origin`; z unchanged.
		struct Local
		{
			cad::PlanPoint origin;
			cad::PlanPoint d;
			cad::PlanPoint n;
		};

		// An opening's hole in its wall's frame (u along the axis from the wall start, s across towards the left face).
		struct Hole
		{
			cad::PlanPoint start;
			cad::PlanPoint axis; // unit
			cad::PlanPoint left; // unit
			double u0 = 0.0;
			double u1 = 0.0;
			double z0 = 0.0;
			double z1 = 0.0;
			double frame = 0.0; // frame profile width
			double depth = 0.0; // half the frame depth
			double sigma = 1.0; // +1: it opens towards the left face, -1: the right one
			double open = 0.0;  // 0 closed .. 1 fully open
			const cad::OpeningFamily* family = nullptr;
		};

		// The hole trimmed like the wall's (WallGeometryBuilder); nullopt if nothing is left of it.
		std::optional<Hole> holeOf(const cad::Wall& wall, const WallFootprint& footprint, const cad::Opening& opening)
		{
			Hole hole;
			hole.family = cad::findOpeningFamily(opening.family);
			hole.start = toPlan(wall.start);
			const cad::PlanPoint axisVector = sub(toPlan(wall.end), hole.start);
			hole.axis = scale(axisVector, 1.0 / length2(axisVector));
			hole.left = leftOf(hole.axis);
			auto along = [&hole](const cad::PlanPoint& p) { return dot2(sub(p, hole.start), hole.axis); };
			const double faceStart = std::max(along(footprint.startLeft), along(footprint.startRight));
			const double faceEnd = std::min(along(footprint.endLeft), along(footprint.endRight));
			hole.u0 = std::max(static_cast<double>(opening.offsetMm), faceStart);
			hole.u1 = std::min(static_cast<double>(opening.offsetMm + opening.widthMm), faceEnd);
			hole.z0 = static_cast<double>(opening.sillMm);
			hole.z1 = static_cast<double>(opening.sillMm + opening.heightMm);
			if (hole.u1 - hole.u0 <= 0.0 || hole.z1 - hole.z0 <= 0.0)
			{
				return std::nullopt;
			}
			// Small openings get thinner profiles so something is left inside.
			hole.frame = std::min({FrameWidthMm, (hole.u1 - hole.u0) / 4.0, (hole.z1 - hole.z0) / 4.0});
			hole.depth = std::min(FrameDepthMm, static_cast<double>(wall.thicknessMm)) / 2.0;
			hole.sigma = opening.opensTo == cad::WallSide::Left ? 1.0 : -1.0;
			hole.open = opening.operation == cad::OpeningOperation::Fixed ? 0.0 : std::clamp(opening.openPercent, 0, 100) / 100.0;
			return hole;
		}

		void box(MeshData& mesh, const char* slot, const Local& at, double u0, double u1, double s0, double s1, double z0, double z1)
		{
			if (u1 > u0 && s1 > s0 && z1 > z0)
			{
				addWallBox(mesh, slot, at.origin, at.d, at.n, u0, u1, s0, s1, z0, z1);
			}
		}

		Local wallFrame(const Hole& hole)
		{
			return Local{hole.start, hole.axis, hole.left};
		}

		// The frame of a leaf or sash 2 * half thick hinged at `hingeU`, reaching towards +axis (`fromStart`) or -axis,
		// turned towards the opensTo face by the opening's angle. In it, u runs from the hinge along the leaf and s from the
		// face it swings towards (0) to the other one (2 * half); the hinge is on the first face.
		Local hingedFrame(const Hole& hole, double hingeU, bool fromStart, double half)
		{
			const double angle = hole.open * OpenDegrees * Pi / 180.0 * hole.sigma * (fromStart ? 1.0 : -1.0);
			const cad::PlanPoint d = fromStart ? hole.axis : scale(hole.axis, -1.0);
			const cad::PlanPoint n = scale(hole.left, -hole.sigma);
			const cad::PlanPoint pivot = add(add(hole.start, scale(hole.axis, hingeU)), scale(hole.left, hole.sigma * half));
			return Local{pivot, rotate(d, angle), rotate(n, angle)};
		}

		// A divided infill in leaf coordinates. Each pane occupies its own cell; profiles retain their physical size.
		void dividedLeaf(MeshData& mesh, const Local& at, double width, double s0, double s1, double z0, double z1,
			const cad::OpeningFamily& family)
		{
			const double profile = std::min({60.0, width / (family.columns * 4.0), (z1 - z0) / (family.rows * 4.0)});
			const double height = z1 - z0;
			const bool glazed = family.design == cad::OpeningDesign::Glazed;
			if (!glazed)
			{
				box(mesh, carpentry_slots::Leaf, at, 0.0, width, s0, s1, z0, z1);
			}
			else
			{
				box(mesh, carpentry_slots::Leaf, at, 0.0, profile, s0, s1, z0, z1);
				box(mesh, carpentry_slots::Leaf, at, width - profile, width, s0, s1, z0, z1);
				box(mesh, carpentry_slots::Leaf, at, profile, width - profile, s0, s1, z0, z0 + profile);
				box(mesh, carpentry_slots::Leaf, at, profile, width - profile, s0, s1, z1 - profile, z1);
				for (int c = 1; c < family.columns; ++c)
				{
					const double x = width * c / family.columns;
					box(mesh, carpentry_slots::Leaf, at, x - profile / 2.0, x + profile / 2.0, s0, s1, z0 + profile, z1 - profile);
				}
				for (int r = 1; r < family.rows; ++r)
				{
					const double z = z0 + height * r / family.rows;
					box(mesh, carpentry_slots::Leaf, at, profile, width - profile, s0, s1, z - profile / 2.0, z + profile / 2.0);
				}
			}
			for (int c = 0; c < family.columns; ++c)
			{
				for (int r = 0; r < family.rows; ++r)
				{
					const double x0 = width * c / family.columns + (c == 0 ? profile : profile / 2.0);
					const double x1 = width * (c + 1) / family.columns - (c + 1 == family.columns ? profile : profile / 2.0);
					const double a = z0 + height * r / family.rows + (r == 0 ? profile : profile / 2.0);
					const double b = z0 + height * (r + 1) / family.rows - (r + 1 == family.rows ? profile : profile / 2.0);
					if (glazed)
					{
						const double middle = (s0 + s1) / 2.0;
						const double glass = std::min(GlassThicknessMm, s1 - s0) / 2.0;
						box(mesh, carpentry_slots::Glass, at, x0, x1, middle - glass, middle + glass, a, b);
					}
					else
					{
						box(mesh, carpentry_slots::Leaf, at, x0, x1, s0 - 4.0, s0, a, b);
						box(mesh, carpentry_slots::Leaf, at, x0, x1, s1, s1 + 4.0, a, b);
					}
				}
			}
		}

		void doorLeaf(MeshData& mesh, const Hole& hole, const Local& at, double width, double s0, double s1, double z0, double z1)
		{
			if (hole.family && hole.family->design != cad::OpeningDesign::Solid)
			{
				dividedLeaf(mesh, at, width, s0, s1, z0, z1, *hole.family);
			}
			else { box(mesh, carpentry_slots::Leaf, at, 0.0, width, s0, s1, z0, z1); }
		}

		// A door leaf over [u0, u1] hinged at one end, with a lever on each face near its free edge at the usual height.
		void hingedLeaf(MeshData& mesh, const Hole& hole, double u0, double u1, bool hingeAtStart, double half, double z0, double z1)
		{
			const double width = u1 - u0;
			const Local at = hingedFrame(hole, hingeAtStart ? u0 : u1, hingeAtStart, half);
			doorLeaf(mesh, hole, at, width, 0.0, 2.0 * half, z0, z1);
			if (width >= 300.0)
			{
				const double handleZ = std::min(1000.0, (z0 + z1) / 2.0);
				box(mesh, carpentry_slots::Hardware, at, width - 180.0, width - 60.0, -30.0, 0.0, handleZ, handleZ + 25.0);
				box(mesh, carpentry_slots::Hardware, at, width - 180.0, width - 60.0, 2.0 * half, 2.0 * half + 30.0, handleZ, handleZ + 25.0);
			}
		}

		double sashProfile(double width, double z0, double z1)
		{
			return std::min({SashProfileMm, width / 4.0, (z1 - z0) / 4.0});
		}

		// A window sash: profiles round a pane, `width` along u from 0 and across s0..s1.
		void sash(MeshData& mesh, const Local& at, double width, double s0, double s1, double z0, double z1,
			const cad::OpeningFamily* family)
		{
			if (family)
			{
				dividedLeaf(mesh, at, width, s0, s1, z0, z1, *family);
				return;
			}
			const double profile = sashProfile(width, z0, z1);
			box(mesh, carpentry_slots::Leaf, at, 0.0, profile, s0, s1, z0, z1);
			box(mesh, carpentry_slots::Leaf, at, width - profile, width, s0, s1, z0, z1);
			box(mesh, carpentry_slots::Leaf, at, profile, width - profile, s0, s1, z0, z0 + profile);
			box(mesh, carpentry_slots::Leaf, at, profile, width - profile, s0, s1, z1 - profile, z1);
			const double middle = (s0 + s1) / 2.0;
			const double glass = std::min(GlassThicknessMm, s1 - s0) / 2.0;
			box(mesh, carpentry_slots::Glass, at, profile, width - profile, middle - glass, middle + glass, z0 + profile, z1 - profile);
		}

		// A window handle centred on `u`, across sFrom..sTo, at mid height.
		void windowHandle(MeshData& mesh, const Local& at, double u, double sFrom, double sTo, double z0, double z1)
		{
			const double middle = (z0 + z1) / 2.0;
			box(mesh, carpentry_slots::Hardware, at, u - 12.0, u + 12.0, sFrom, sTo, middle - 60.0, middle + 60.0);
		}

		void buildDoor(MeshData& mesh, const Hole& hole, const cad::Opening& opening)
		{
			const double half = std::min(LeafThicknessMm / 2.0, hole.depth);
			const double u0 = hole.u0 + hole.frame + LeafClearanceMm;
			const double u1 = hole.u1 - hole.frame - LeafClearanceMm;
			const double z0 = hole.z0 + DoorFloorGapMm;
			const double z1 = hole.z1 - hole.frame - LeafClearanceMm;
			if (u1 <= u0 || z1 <= z0)
			{
				return;
			}
			const bool towardStart = opening.hinge == cad::OpeningEnd::Start;
			switch (opening.operation)
			{
			case cad::OpeningOperation::DoubleHinged:
			{
				const double middle = (u0 + u1) / 2.0;
				hingedLeaf(mesh, hole, u0, middle - LeafClearanceMm / 2.0, true, half, z0, z1);
				hingedLeaf(mesh, hole, middle + LeafClearanceMm / 2.0, u1, false, half, z0, z1);
				break;
			}
			case cad::OpeningOperation::Sliding:
			{
				// Into its pocket, where the wall hides it; a flush pull on each face near the edge left in the hole.
				const Local at = wallFrame(hole);
				const double shift = hole.open * (u1 - u0) * (towardStart ? -1.0 : 1.0);
				const Local leafAt{add(hole.start, scale(hole.axis, u0 + shift)), hole.axis, hole.left};
				doorLeaf(mesh, hole, leafAt, u1 - u0, -half, half, z0, z1);
				const double pullU = towardStart ? u1 + shift - 60.0 : u0 + shift + 60.0;
				const double pullZ = std::min(1000.0, (z0 + z1) / 2.0);
				box(mesh, carpentry_slots::Hardware, at, pullU - 10.0, pullU + 10.0, half, half + 5.0, pullZ - 75.0, pullZ + 75.0);
				box(mesh, carpentry_slots::Hardware, at, pullU - 10.0, pullU + 10.0, -half - 5.0, -half, pullZ - 75.0, pullZ + 75.0);
				break;
			}
			default:
				hingedLeaf(mesh, hole, u0, u1, towardStart, half, z0, z1);
				break;
			}
		}

		void buildWindow(MeshData& mesh, const Hole& hole, const cad::Opening& opening)
		{
			const Local at = wallFrame(hole);
			box(mesh, carpentry_slots::Frame, at, hole.u0 + hole.frame, hole.u1 - hole.frame, -hole.depth, hole.depth, hole.z0,
				hole.z0 + hole.frame); // bottom rail
			const double iu0 = hole.u0 + hole.frame;
			const double iu1 = hole.u1 - hole.frame;
			const double iz0 = hole.z0 + hole.frame;
			const double iz1 = hole.z1 - hole.frame;
			if (iu1 <= iu0 || iz1 <= iz0)
			{
				return;
			}
			const bool towardStart = opening.hinge == cad::OpeningEnd::Start;
			switch (opening.operation)
			{
			case cad::OpeningOperation::Fixed:
			{
				if (hole.family)
				{
					const Local fixedAt{add(hole.start, scale(hole.axis, iu0)), hole.axis, hole.left};
					sash(mesh, fixedAt, iu1 - iu0, -hole.depth / 2.0, hole.depth / 2.0, iz0, iz1, hole.family);
					break;
				}
				if (hole.u1 - hole.u0 > MullionFromWidthMm)
				{
					const double middle = (hole.u0 + hole.u1) / 2.0;
					box(mesh, carpentry_slots::Frame, at, middle - hole.frame / 2.0, middle + hole.frame / 2.0, -hole.depth, hole.depth, iz0, iz1);
				}
				const double glass = GlassThicknessMm / 2.0;
				box(mesh, carpentry_slots::Glass, at, iu0, iu1, -glass, glass, iz0, iz1);
				break;
			}
			case cad::OpeningOperation::Sliding:
			{
				// Two sashes overlapping in the middle on two tracks: the fixed one at the end it opens towards, the moving
				// one on the opensTo side, travelling over it.
				const double z0 = iz0 + LeafClearanceMm;
				const double z1 = iz1 - LeafClearanceMm;
				const double width = iu1 - iu0;
				const double overlap = std::min(SlidingOverlapMm, width / 4.0);
				const double each = (width + overlap) / 2.0;
				const double thickness = std::max(hole.depth - 4.0, 2.0);
				const double movingS0 = hole.sigma > 0.0 ? 2.0 : -2.0 - thickness;
				const double fixedS0 = hole.sigma > 0.0 ? -2.0 - thickness : 2.0;
				const double fixedU0 = towardStart ? iu0 : iu1 - each;
				const double movingU0 = (towardStart ? iu1 - each : iu0) + hole.open * (each - overlap) * (towardStart ? -1.0 : 1.0);
				sash(mesh, Local{add(hole.start, scale(hole.axis, fixedU0)), hole.axis, hole.left}, each, fixedS0, fixedS0 + thickness, z0, z1, hole.family);
				const Local moving{add(hole.start, scale(hole.axis, movingU0)), hole.axis, hole.left};
				sash(mesh, moving, each, movingS0, movingS0 + thickness, z0, z1, hole.family);
				// Its pull, at the edge away from the fixed sash, on its face.
				const double profile = sashProfile(each, z0, z1);
				const double pullS0 = hole.sigma > 0.0 ? movingS0 + thickness : movingS0 - 20.0;
				windowHandle(mesh, moving, towardStart ? each - profile / 2.0 : profile / 2.0, pullS0, pullS0 + 20.0, z0, z1);
				break;
			}
			default:
			{
				// Hinged sashes turn like door leaves; the (last) one carries the handle on the face it opens towards.
				const double half = std::min(SashDepthMm / 2.0, hole.depth);
				const double u0 = iu0 + LeafClearanceMm;
				const double u1 = iu1 - LeafClearanceMm;
				const double z0 = iz0 + LeafClearanceMm;
				const double z1 = iz1 - LeafClearanceMm;
				auto hingedSash = [&](double a, double b, bool fromStart, bool withHandle)
				{
					const double width = b - a;
					const Local sashAt = hingedFrame(hole, fromStart ? a : b, fromStart, half);
					sash(mesh, sashAt, width, 0.0, 2.0 * half, z0, z1, hole.family);
					if (withHandle && width > 0.0)
					{
						windowHandle(mesh, sashAt, width - sashProfile(width, z0, z1) / 2.0, -25.0, 0.0, z0, z1);
					}
				};
				if (opening.operation == cad::OpeningOperation::DoubleHinged)
				{
					const double middle = (u0 + u1) / 2.0;
					hingedSash(u0, middle - LeafClearanceMm / 2.0, true, false);
					hingedSash(middle + LeafClearanceMm / 2.0, u1, false, true);
				}
				else
				{
					hingedSash(u0, u1, towardStart, true);
				}
				break;
			}
			}
		}
	}

	MeshData buildOpeningMesh(const cad::Wall& wall, const WallFootprint& footprint, const cad::Opening& opening)
	{
		MeshData mesh;
		const std::optional<Hole> hole = holeOf(wall, footprint, opening);
		if (!hole)
		{
			return mesh;
		}
		const Local at = wallFrame(*hole);
		box(mesh, carpentry_slots::Frame, at, hole->u0, hole->u0 + hole->frame, -hole->depth, hole->depth, hole->z0, hole->z1); // jambs
		box(mesh, carpentry_slots::Frame, at, hole->u1 - hole->frame, hole->u1, -hole->depth, hole->depth, hole->z0, hole->z1);
		box(mesh, carpentry_slots::Frame, at, hole->u0 + hole->frame, hole->u1 - hole->frame, -hole->depth, hole->depth,
			hole->z1 - hole->frame, hole->z1); // head
		if (opening.kind == cad::OpeningKind::Door)
		{
			buildDoor(mesh, *hole, opening);
		}
		else
		{
			buildWindow(mesh, *hole, opening);
		}
		return mesh;
	}

	MeshData buildOpeningSymbolMesh(const cad::Wall& wall, const WallFootprint& footprint, const cad::Opening& opening)
	{
		MeshData mesh;
		const bool swings = opening.operation == cad::OpeningOperation::Hinged || opening.operation == cad::OpeningOperation::DoubleHinged;
		const std::optional<Hole> hole = opening.kind == cad::OpeningKind::Door && swings ? holeOf(wall, footprint, opening) : std::nullopt;
		if (!hole)
		{
			return mesh;
		}
		const double u0 = hole->u0 + hole->frame + LeafClearanceMm;
		const double u1 = hole->u1 - hole->frame - LeafClearanceMm;
		if (u1 <= u0)
		{
			return mesh;
		}
		const double z = FloorThicknessMm + SwingLiftMm;
		const double line = SwingLineMm / 2.0;
		const cad::PlanPoint out = scale(hole->left, hole->sigma); // towards the face it opens to
		auto corner = [z](const cad::PlanPoint& p)
		{
			return Corner{Vec3{p.x_mm, p.y_mm, z}, Vec2{p.x_mm * MetersPerMillimeter, p.y_mm * MetersPerMillimeter}};
		};
		const Vec3 up{0.0, 0.0, 1.0};
		// The arc the free edge sweeps (quarter circle) and the leaf open at 90 degrees, from its hinge on the wall face.
		auto leafSymbol = [&](double hingeU, bool fromStart, double radius)
		{
			const cad::PlanPoint pivot = add(add(hole->start, scale(hole->axis, hingeU)),
				scale(out, static_cast<double>(wall.thicknessMm) / 2.0));
			const cad::PlanPoint along = fromStart ? hole->axis : scale(hole->axis, -1.0);
			constexpr int Segments = 16;
			for (int i = 0; i < Segments; ++i)
			{
				const double a = Pi / 2.0 * i / Segments;
				const double b = Pi / 2.0 * (i + 1) / Segments;
				const cad::PlanPoint da = add(scale(along, std::cos(a)), scale(out, std::sin(a)));
				const cad::PlanPoint db = add(scale(along, std::cos(b)), scale(out, std::sin(b)));
				addQuad(mesh, carpentry_slots::Swing, corner(add(pivot, scale(da, radius - line))), corner(add(pivot, scale(da, radius + line))),
					corner(add(pivot, scale(db, radius + line))), corner(add(pivot, scale(db, radius - line))), up);
			}
			addQuad(mesh, carpentry_slots::Swing, corner(add(pivot, scale(along, -line))), corner(add(pivot, scale(along, line))),
				corner(add(add(pivot, scale(out, radius)), scale(along, line))), corner(add(add(pivot, scale(out, radius)), scale(along, -line))), up);
		};
		if (opening.operation == cad::OpeningOperation::DoubleHinged)
		{
			const double middle = (u0 + u1) / 2.0;
			leafSymbol(u0, true, middle - u0);
			leafSymbol(u1, false, u1 - middle);
		}
		else
		{
			const bool fromStart = opening.hinge == cad::OpeningEnd::Start;
			leafSymbol(fromStart ? u0 : u1, fromStart, u1 - u0);
		}
		return mesh;
	}
}
