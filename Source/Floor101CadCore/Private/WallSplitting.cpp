#include "WallSplitting.h"

#include "F101/Cad/Units.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace f101::cad
{
	namespace
	{
		constexpr double EpsilonMm = 1e-6;

		PlanPoint vector(Point2D from, Point2D to)
		{
			return {static_cast<double>(to.x_mm - from.x_mm), static_cast<double>(to.y_mm - from.y_mm)};
		}

		double cross(PlanPoint a, PlanPoint b) { return a.x_mm * b.y_mm - a.y_mm * b.x_mm; }
		double dot(PlanPoint a, PlanPoint b) { return a.x_mm * b.x_mm + a.y_mm * b.y_mm; }

		Result<Point2D> quantize(PlanPoint point)
		{
			const auto x = quantizeMillimeters(point.x_mm);
			const auto y = quantizeMillimeters(point.y_mm);
			if (!x) return x.error();
			if (!y) return y.error();
			return Point2D{x.value(), y.value()};
		}

		bool involvesEdit(const Wall& a, const Wall& b, const std::vector<WallId>& edited)
		{
			return a.level == b.level && (std::find(edited.begin(), edited.end(), a.id) != edited.end() ||
				std::find(edited.begin(), edited.end(), b.id) != edited.end());
		}

		struct Cut
		{
			double alongMm;
			Point2D point;
		};

		void addCut(std::vector<Cut>& cuts, const Wall& wall, double fraction, Point2D point)
		{
			if (fraction > 0.0 && fraction < 1.0 && point != wall.start && point != wall.end)
			{
				cuts.push_back({dot(vector(wall.start, point), vector(wall.start, wall.end)) / wall.lengthMm(), point});
			}
		}
	}

	Status splitJoinedWalls(Project& project, const std::vector<WallId>& edited, const UuidSource& newUuid)
	{
		// A T may end on the host's face rather than its axis. Join its endpoint to the axis along its own direction,
		// so all pieces have the same millimetre joint and the geometry solver can treat it as one junction.
		const std::vector<Wall> reference = project.walls;
		std::vector<std::vector<Cut>> cuts(project.walls.size());
		for (std::size_t i = 0; i < reference.size(); ++i)
		{
			const Wall& branch = reference[i];
			const PlanPoint direction = vector(branch.start, branch.end);
			for (const bool atStart : {true, false})
			{
				const Point2D end = atStart ? branch.start : branch.end;
				double closest = std::numeric_limits<double>::infinity();
				Point2D joint = end;
				std::size_t hostIndex = reference.size();
				double hostAlong = 0.0;
				for (std::size_t j = 0; j < reference.size(); ++j)
				{
					const Wall& host = reference[j];
					if (i == j || !involvesEdit(branch, host, edited) || end == host.start || end == host.end) continue;
					const PlanPoint axis = vector(host.start, host.end);
					const double denominator = cross(direction, axis);
					if (std::fabs(denominator) <= 1e-9 * branch.lengthMm() * host.lengthMm()) continue;
					const PlanPoint relative = vector(host.start, end);
					const double along = dot(relative, axis) / dot(axis, axis);
					const double across = std::fabs(cross(axis, relative)) / host.lengthMm();
					if (along <= 0.0 || along >= 1.0 || across > static_cast<double>(host.thicknessMm) / 2.0 + EpsilonMm ||
						across >= closest) continue;
					const PlanPoint between = vector(branch.start, host.start);
					const double hostFraction = cross(between, direction) / denominator;
					if (hostFraction <= 0.0 || hostFraction >= 1.0) continue;
					const auto point = quantize({static_cast<double>(host.start.x_mm) + hostFraction * axis.x_mm,
						static_cast<double>(host.start.y_mm) + hostFraction * axis.y_mm});
					if (!point) return point.error();
					joint = point.value();
					hostIndex = j;
					hostAlong = hostFraction;
					closest = across;
				}
				(atStart ? project.walls[i].start : project.walls[i].end) = joint;
				if (hostIndex != reference.size()) addCut(cuts[hostIndex], reference[hostIndex], hostAlong, joint);
			}
			Wall& joined = project.walls[i];
			if (joined.axis() != branch.axis() && joined.lengthMm() > 0.0)
			{
				// Moving an endpoint onto a face's axis must not move the doors/windows in world space.
				const PlanPoint newAxis = vector(joined.start, joined.end);
				const PlanPoint shiftedStart = vector(joined.start, branch.start);
				for (Opening& opening : joined.openings)
				{
					const double offset = static_cast<double>(opening.offsetMm) / branch.lengthMm();
					const PlanPoint relative{shiftedStart.x_mm + direction.x_mm * offset, shiftedStart.y_mm + direction.y_mm * offset};
					const auto moved = quantizeMillimeters(dot(relative, newAxis) / joined.lengthMm());
					if (!moved) return moved.error();
					opening.offsetMm = moved.value();
				}
			}
			if (Status valid = validateWall(joined); !valid) return valid;
		}

		for (std::size_t i = 0; i < project.walls.size(); ++i)
		{
			const Wall& a = project.walls[i];
			const PlanPoint da = vector(a.start, a.end);
			for (std::size_t j = i + 1; j < project.walls.size(); ++j)
			{
				const Wall& b = project.walls[j];
				if (!involvesEdit(a, b, edited)) continue;
				const PlanPoint db = vector(b.start, b.end);
				const double denominator = cross(da, db);
				if (std::fabs(denominator) <= 1e-9 * a.lengthMm() * b.lengthMm()) continue;
				const PlanPoint between = vector(a.start, b.start);
				const double t = cross(between, db) / denominator;
				const double u = cross(between, da) / denominator;
				if (t < -1e-9 || t > 1.0 + 1e-9 || u < -1e-9 || u > 1.0 + 1e-9) continue;
				const auto point = quantize({static_cast<double>(a.start.x_mm) + t * da.x_mm,
					static_cast<double>(a.start.y_mm) + t * da.y_mm});
				if (!point) return point.error();
				addCut(cuts[i], a, t, point.value());
				addCut(cuts[j], b, u, point.value());
			}
		}

		std::vector<Wall> added;
		for (std::size_t i = 0; i < project.walls.size(); ++i)
		{
			auto& wallCuts = cuts[i];
			if (wallCuts.empty()) continue;
			const Wall source = project.walls[i];
			std::sort(wallCuts.begin(), wallCuts.end(), [](const Cut& a, const Cut& b) { return a.alongMm < b.alongMm; });
			wallCuts.erase(std::unique(wallCuts.begin(), wallCuts.end(), [](const Cut& a, const Cut& b)
				{ return a.point == b.point; }), wallCuts.end());
			for (const Cut& cut : wallCuts)
			{
				for (const Opening& opening : source.openings)
				{
					const AxisSpan span = occupiedSpan(opening);
					if (cut.alongMm > static_cast<double>(span.fromMm) + EpsilonMm &&
						cut.alongMm < static_cast<double>(span.toMm) - EpsilonMm)
					{
						return Error{ErrorCode::OutOfRange, "No se puede dividir la pared a traves de una puerta, ventana o bolsillo de puerta corrediza"};
					}
				}
			}
			wallCuts.push_back({source.lengthMm(), source.end});
			Point2D start = source.start;
			double fromMm = 0.0;
			for (std::size_t segment = 0; segment < wallCuts.size(); ++segment)
			{
				const Cut& end = wallCuts[segment];
				Wall part = source;
				part.start = start;
				part.end = end.point;
				part.openings.clear();
				if (segment != 0) part.id = WallId{newUuid()};
				const auto origin = quantizeMillimeters(fromMm);
				if (!origin) return origin.error();
				for (const Opening& opening : source.openings)
				{
					if (static_cast<double>(opening.offsetMm) + EpsilonMm >= fromMm &&
						static_cast<double>(opening.offsetMm) < end.alongMm - EpsilonMm)
					{
						Opening moved = opening;
						moved.offsetMm -= origin.value();
						part.openings.push_back(std::move(moved));
					}
				}
				if (Status valid = validateWall(part); !valid) return valid;
				if (segment == 0) project.walls[i] = std::move(part);
				else added.push_back(std::move(part));
				start = end.point;
				fromMm = end.alongMm;
			}
		}
		project.walls.insert(project.walls.end(), added.begin(), added.end());
		return Status::success();
	}
}
