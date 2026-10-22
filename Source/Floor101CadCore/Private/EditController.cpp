#include "F101/Cad/EditController.h"
#include "F101/Cad/OpeningCatalogue.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>
#include <utility>

namespace f101::cad
{
	namespace
	{
		Status toStatus(const Result<Transaction>& applied)
		{
			return applied ? Status::success() : Status(applied.error());
		}

		Error nothingSelected()
		{
			return Error{ErrorCode::NotFound, "Nothing is selected"};
		}

		Point2D endpointOf(const Wall& wall, WallEndpoint endpoint)
		{
			return endpoint == WallEndpoint::Start ? wall.start : wall.end;
		}

		WallEndpoint opposite(WallEndpoint endpoint)
		{
			return endpoint == WallEndpoint::Start ? WallEndpoint::End : WallEndpoint::Start;
		}

		struct AxisPosition
		{
			double along = 0.0;  // from the wall start, mm
			double across = 0.0; // distance from the axis line, mm
		};

		AxisPosition locate(const Wall& wall, PlanPoint p)
		{
			const double length = wall.lengthMm();
			const double ax = static_cast<double>(wall.end.x_mm - wall.start.x_mm) / length;
			const double ay = static_cast<double>(wall.end.y_mm - wall.start.y_mm) / length;
			const double rx = p.x_mm - static_cast<double>(wall.start.x_mm);
			const double ry = p.y_mm - static_cast<double>(wall.start.y_mm);
			return AxisPosition{rx * ax + ry * ay, std::fabs(rx * ay - ry * ax)};
		}

		// Point of the wall axis at a distance from its start, in whole millimetres (handles).
		Point2D pointOnAxis(const Wall& wall, std::int64_t distanceMm)
		{
			const double t = static_cast<double>(distanceMm) / wall.lengthMm();
			return Point2D{std::llround(static_cast<double>(wall.start.x_mm) + static_cast<double>(wall.end.x_mm - wall.start.x_mm) * t),
				std::llround(static_cast<double>(wall.start.y_mm) + static_cast<double>(wall.end.y_mm - wall.start.y_mm) * t)};
		}

		std::int64_t snapDown(std::int64_t value, std::int64_t grid)
		{
			return value >= 0 ? value / grid * grid : -((-value + grid - 1) / grid * grid);
		}

		std::int64_t snapUp(std::int64_t value, std::int64_t grid)
		{
			return -snapDown(-value, grid);
		}

		// The gaps of [0, length] left by `blocked` (spans sorted or not, possibly overlapping).
		std::vector<AxisSpan> freeGaps(std::int64_t length, std::vector<AxisSpan> blocked)
		{
			std::sort(blocked.begin(), blocked.end(), [](const AxisSpan& a, const AxisSpan& b) { return a.fromMm < b.fromMm; });
			std::vector<AxisSpan> gaps;
			std::int64_t cursor = 0;
			for (const AxisSpan& span : blocked)
			{
				if (span.fromMm > cursor)
				{
					gaps.push_back(AxisSpan{cursor, std::min(span.fromMm, length)});
				}
				cursor = std::max(cursor, span.toMm);
			}
			if (cursor < length)
			{
				gaps.push_back(AxisSpan{cursor, length});
			}
			return gaps;
		}

		// The side of a wall's axis a plan point is on.
		WallSide sideOf(const Wall& wall, PlanPoint p)
		{
			const double ax = static_cast<double>(wall.end.x_mm - wall.start.x_mm);
			const double ay = static_cast<double>(wall.end.y_mm - wall.start.y_mm);
			const double px = p.x_mm - static_cast<double>(wall.start.x_mm);
			const double py = p.y_mm - static_cast<double>(wall.start.y_mm);
			return ax * py - ay * px >= 0.0 ? WallSide::Left : WallSide::Right;
		}

		bool fitsGap(const AxisSpan& span, const std::vector<AxisSpan>& gaps)
		{
			return std::any_of(gaps.begin(), gaps.end(),
				[&span](const AxisSpan& gap) { return gap.fromMm <= span.fromMm && span.toMm <= gap.toMm; });
		}

		// Offset for `opening` as close to `wanted` as the gaps allow, with everything it takes (its hole and a sliding
		// door's pocket, ADR-0014) inside one gap, on the grid when the gap has room for it; nullopt when no gap is wide
		// enough (ADR-0013). The opening's own offset is ignored.
		std::optional<std::int64_t> fitOpening(double wanted, const Opening& opening, const std::vector<AxisSpan>& gaps, std::int64_t grid)
		{
			const AxisSpan taken = occupiedSpan(opening);
			const std::int64_t before = opening.offsetMm - taken.fromMm; // what it takes before its offset
			const std::int64_t after = taken.toMm - opening.offsetMm;   // from its offset on
			std::optional<std::int64_t> best;
			for (const AxisSpan& gap : gaps)
			{
				const std::int64_t low = gap.fromMm + before;
				const std::int64_t high = gap.toMm - after;
				if (high < low)
				{
					continue;
				}
				const std::int64_t snapped = std::llround(wanted / static_cast<double>(grid)) * grid;
				const std::int64_t gridLow = snapUp(low, grid);
				const std::int64_t gridHigh = snapDown(high, grid);
				const std::int64_t candidate = gridLow <= gridHigh ? std::clamp(snapped, gridLow, gridHigh)
					: std::clamp(static_cast<std::int64_t>(std::llround(wanted)), low, high);
				if (!best || std::fabs(static_cast<double>(candidate) - wanted) < std::fabs(static_cast<double>(*best) - wanted))
				{
					best = candidate;
				}
			}
			return best;
		}

		const Opening* openingAt(const Wall& wall, double along)
		{
			for (const Opening& opening : wall.openings)
			{
				if (along >= static_cast<double>(opening.offsetMm) && along <= static_cast<double>(opening.offsetMm + opening.widthMm))
				{
					return &opening;
				}
			}
			return nullptr;
		}

		using Quad = std::array<PlanPoint, 4>;

		// A wall's solid in plan: the box centred on its axis.
		Quad wallBody(const Wall& wall)
		{
			const double length = wall.lengthMm();
			const PlanPoint axis{static_cast<double>(wall.end.x_mm - wall.start.x_mm) / length,
				static_cast<double>(wall.end.y_mm - wall.start.y_mm) / length};
			const double half = static_cast<double>(wall.thicknessMm) / 2.0;
			const PlanPoint side{-axis.y_mm * half, axis.x_mm * half};
			const double sx = static_cast<double>(wall.start.x_mm);
			const double sy = static_cast<double>(wall.start.y_mm);
			const double ex = static_cast<double>(wall.end.x_mm);
			const double ey = static_cast<double>(wall.end.y_mm);
			return {PlanPoint{sx - side.x_mm, sy - side.y_mm}, PlanPoint{ex - side.x_mm, ey - side.y_mm},
				PlanPoint{ex + side.x_mm, ey + side.y_mm}, PlanPoint{sx + side.x_mm, sy + side.y_mm}};
		}

		// The open interval of t for which `moving`, shifted by t * `direction` (unit), overlaps `fixed` (two rectangles,
		// separating axis test); nullopt if they never overlap.
		std::optional<std::pair<double, double>> slideOverlap(const Quad& moving, const Quad& fixed, PlanPoint direction)
		{
			double low = -std::numeric_limits<double>::infinity();
			double high = std::numeric_limits<double>::infinity();
			for (const Quad* quad : {&moving, &fixed})
			{
				for (std::size_t edge = 0; edge < 2; ++edge)
				{
					const PlanPoint along{(*quad)[edge + 1].x_mm - (*quad)[edge].x_mm, (*quad)[edge + 1].y_mm - (*quad)[edge].y_mm};
					const double length = std::hypot(along.x_mm, along.y_mm);
					if (length <= 0.0)
					{
						continue;
					}
					const PlanPoint normal{-along.y_mm / length, along.x_mm / length};
					auto extent = [&normal](const Quad& shape)
					{
						double from = std::numeric_limits<double>::infinity();
						double to = -std::numeric_limits<double>::infinity();
						for (const PlanPoint& p : shape)
						{
							const double value = p.x_mm * normal.x_mm + p.y_mm * normal.y_mm;
							from = std::min(from, value);
							to = std::max(to, value);
						}
						return std::pair{from, to};
					};
					const auto [movingFrom, movingTo] = extent(moving);
					const auto [fixedFrom, fixedTo] = extent(fixed);
					const double speed = direction.x_mm * normal.x_mm + direction.y_mm * normal.y_mm;
					if (std::fabs(speed) < 1e-9)
					{
						if (movingTo <= fixedFrom || fixedTo <= movingFrom)
						{
							return std::nullopt; // separated on this axis whatever t is
						}
						continue;
					}
					double enter = (fixedFrom - movingTo) / speed;
					double leave = (fixedTo - movingFrom) / speed;
					if (enter > leave)
					{
						std::swap(enter, leave);
					}
					low = std::max(low, enter);
					high = std::min(high, leave);
				}
			}
			if (low >= high)
			{
				return std::nullopt;
			}
			return std::pair{low, high};
		}

		// Parses a whole or decimal number of degrees ("90", "22.5", "22,5", "90°").
		std::optional<double> parseDegrees(std::string_view text)
		{
			std::string number;
			for (std::size_t i = 0; i < text.size(); ++i)
			{
				const char c = text[i];
				if (c == ' ')
				{
					continue;
				}
				if (static_cast<unsigned char>(c) == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xB0)
				{
					++i; // UTF-8 degree sign
					continue;
				}
				number += c == ',' ? '.' : c;
			}
			double value = 0.0;
			const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);
			if (number.empty() || error != std::errc() || end != number.data() + number.size() || !std::isfinite(value) ||
				std::fabs(value) > 1e6)
			{
				return std::nullopt;
			}
			return value;
		}

		// Where the ray meets the horizontal plane at height `z`, in front of its origin and within 100 km.
		std::optional<PlanPoint> atHeight(const Ray3& ray, double z)
		{
			if (std::fabs(ray.directionZ) < 1e-12)
			{
				return std::nullopt;
			}
			const double t = (z - ray.originZ) / ray.directionZ;
			const PlanPoint at{ray.originX + ray.directionX * t, ray.originY + ray.directionY * t};
			if (t < 0.0 || std::fabs(at.x_mm) > 1e8 || std::fabs(at.y_mm) > 1e8)
			{
				return std::nullopt;
			}
			return at;
		}

		bool sameSpot(const ServicePoint& a, const ServicePoint& b)
		{
			return a.x_mm == b.x_mm && a.y_mm == b.y_mm;
		}

		// The points a run takes from `last` to `next`: when it changes height elsewhere, it first rises or drops on the spot.
		std::vector<ServicePoint> stepTo(const ServicePoint& last, const ServicePoint& next)
		{
			std::vector<ServicePoint> steps;
			if (next.z_mm != last.z_mm && !sameSpot(next, last))
			{
				steps.push_back(ServicePoint{last.x_mm, last.y_mm, next.z_mm});
			}
			steps.push_back(next);
			return steps;
		}
	}

	EditController::EditController(Document& targetDocument, EditSettings settings)
		: document(targetDocument)
		, editSettings(std::move(settings))
		, tool(targetDocument, editSettings.wallTool)
		, dimensions(targetDocument)
	{
	}

	void EditController::setMode(EditMode mode)
	{
		if (mode == currentMode)
		{
			return;
		}
		dropDrags();
		tool.cancel();
		runDraft.clear();
		dimensions.clearDraft();
		placement.reset();
		furnitureAt.reset();
		columnAt.reset();
		if (mode != EditMode::Select)
		{
			clearSelected();
		}
		if (mode == EditMode::Services)
		{
			servicesShown = true; // the runs being drawn among the others, the architecture seen through
			serviceKindsShown.set(editSettings.serviceKind, true);
		}
		currentMode = mode;
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
			updateFurniturePlacement();
			updateColumnPlacement();
		}
		updateRunPointer();
		paintAt = mode == EditMode::Paint && pointerRay ? resolvePaintTarget(levelProject(), *pointerRay) : std::nullopt;
	}

	void EditController::dropDrags()
	{
		dimensions.dropDrag();
		roomTextDrag.reset();
		drag.reset();
		wallBodyDrag.reset();
		openingDrag.reset();
		furnitureDrag.reset();
		columnDrag.reset();
		runDrag.reset();
	}

	void EditController::clearSelected()
	{
		selected = WallId();
		selectedOpeningId = OpeningId();
		selectedRoomKey.clear();
		dimensions.clearSelection();
		selectedFurnitureId = FurnitureId();
		selectedColumnId = ColumnId();
		selectedRunId = ServiceRunId();
	}

	LevelId EditController::activeLevel() const
	{
		return tool.targetLevel(); // the chosen level while it exists, else the first
	}

	Project EditController::levelProject() const
	{
		return wallsOnLevel(document.project(), activeLevel());
	}

	Ray3 EditController::toLevel(const Ray3& ray) const
	{
		Ray3 local = ray;
		local.originZ -= static_cast<double>(levelElevationMm(document.project(), activeLevel()));
		return local;
	}

	std::optional<PlanPoint> EditController::planPointOf(const Ray3& ray, const std::vector<WallId>& ignoreWalls) const
	{
		// Pointing at a wall (its top or a face) means that spot of the plan, not the floor behind it.
		if (const std::optional<WallHit> hit = pickWall(levelProject(), ray, ignoreWalls))
		{
			return PlanPoint{ray.originX + ray.directionX * hit->distance, ray.originY + ray.directionY * hit->distance};
		}
		return intersectFloor(ray);
	}

	void EditController::pointerMoved(const Ray3& ray)
	{
		movePointer(toLevel(ray));
	}

	void EditController::movePointer(const Ray3& ray)
	{
		// While dragging a corner its own walls are not targets: they are what moves.
		pointerRay = ray;
		planPoint = wallBodyDrag ? intersectFloor(ray) : planPointOf(ray, drag ? drag->joined : std::vector<WallId>{});
		paintAt = currentMode == EditMode::Paint ? resolvePaintTarget(levelProject(), ray) : std::nullopt;
		// Runs follow the pointer on their own planes, wherever the floor is.
		updateRunPointer();
		if (runDrag)
		{
			updateRunDrag();
			return;
		}
		if (!planPoint)
		{
			return; // off the floor: keep the last preview
		}
		if (roomTextDrag)
		{
			updateRoomTextDrag();
			return;
		}
		if (dimensions.dragging())
		{
			dimensions.moveDrag(*planPoint);
			return;
		}
		if (currentMode == EditMode::Wall)
		{
			tool.pointerMoved(*planPoint);
		}
		else if (isOpeningMode())
		{
			updatePlacement();
		}
		else if (currentMode == EditMode::Furniture)
		{
			updateFurniturePlacement();
		}
		else if (currentMode == EditMode::Column)
		{
			updateColumnPlacement();
		}
		else if (columnDrag)
		{
			updateColumnDrag();
		}
		else if (drag)
		{
			updateDrag();
		}
		else if (wallBodyDrag)
		{
			updateWallBodyDrag();
		}
		else if (openingDrag)
		{
			updateOpeningDrag();
		}
		else if (furnitureDrag)
		{
			updateFurnitureDrag();
		}
	}

	Furniture EditController::placeFurniture(Furniture piece, PlanPoint wanted, PlanPoint pointer,
		std::int64_t freeRotation) const
	{
		const std::int64_t grid = gridMm();
		const auto onGrid = [grid](double value) { return std::llround(value / static_cast<double>(grid)) * grid; };
		piece.level = activeLevel();
		piece.position = Point2D{onGrid(wanted.x_mm), onGrid(wanted.y_mm)};
		piece.rotationCentidegrees = freeRotation;
		const std::optional<FurnitureType> type = furnitureSpec(document.project(), piece.type);
		if (!type || !type->againstWall)
		{
			return piece;
		}
		// The wall face nearest to where the piece's back would be, within reach.
		const Project project = levelProject();
		const double halfDepth = static_cast<double>(piece.depthMm) / 2.0;
		const Wall* host = nullptr;
		double hostAlong = 0.0;
		double nearest = editSettings.furnitureWallReachMm;
		for (const Wall& wall : project.walls)
		{
			const AxisPosition at = locate(wall, wanted);
			const double gap = at.across - static_cast<double>(wall.thicknessMm) / 2.0 - halfDepth;
			if (at.along >= 0.0 && at.along <= wall.lengthMm() && gap <= nearest)
			{
				host = &wall;
				hostAlong = at.along;
				nearest = gap;
			}
		}
		if (!host)
		{
			return piece;
		}
		// Its back against the face on the pointer's side, facing away from the wall, its centre along the wall on the grid.
		const double length = host->lengthMm();
		const PlanPoint axis{static_cast<double>(host->end.x_mm - host->start.x_mm) / length,
			static_cast<double>(host->end.y_mm - host->start.y_mm) / length};
		const double side = sideOf(*host, pointer) == WallSide::Left ? 1.0 : -1.0;
		const PlanPoint out{-axis.y_mm * side, axis.x_mm * side};
		constexpr double DegreesPerRadian = 180.0 / 3.14159265358979323846;
		piece.rotationCentidegrees = toCentidegrees(std::atan2(out.x_mm, -out.y_mm) * DegreesPerRadian);
		const double along = static_cast<double>(onGrid(hostAlong));
		const double off = static_cast<double>(host->thicknessMm) / 2.0 + halfDepth;
		PlanPoint centre{static_cast<double>(host->start.x_mm) + axis.x_mm * along + out.x_mm * off,
			static_cast<double>(host->start.y_mm) + axis.y_mm * along + out.y_mm * off};
		const auto at = [&piece](PlanPoint point)
		{
			Furniture moved = piece;
			moved.position = Point2D{std::llround(point.x_mm), std::llround(point.y_mm)};
			return moved;
		};
		// Slides along the wall out of the walls it would run into (into a corner it stops at the other wall's face). If it
		// cannot get clear (a niche narrower than it), it stays on the grid.
		const auto blocking = [&](const Furniture& candidate) -> std::optional<std::pair<double, double>>
		{
			for (const Wall& wall : project.walls)
			{
				if (wall.id == host->id)
				{
					continue;
				}
				const auto overlap = slideOverlap(furnitureFootprint(candidate), wallBody(wall), axis);
				if (overlap && overlap->first < -0.5 && overlap->second > 0.5)
				{
					return overlap;
				}
			}
			return std::nullopt;
		};
		PlanPoint cleared = centre;
		for (int pass = 0; pass < 4; ++pass)
		{
			const auto overlap = blocking(at(cleared));
			if (!overlap)
			{
				centre = cleared;
				break;
			}
			const double shift = std::fabs(overlap->first) < std::fabs(overlap->second) ? overlap->first : overlap->second;
			cleared = PlanPoint{cleared.x_mm + axis.x_mm * shift, cleared.y_mm + axis.y_mm * shift};
		}
		return at(centre);
	}

	void EditController::updateFurniturePlacement()
	{
		furnitureAt.reset();
		const std::optional<FurnitureType> spec =
			currentMode == EditMode::Furniture ? furnitureSpec(document.project(), editSettings.furnitureType) : std::nullopt;
		if (!planPoint || !spec)
		{
			return;
		}
		furnitureAt = placeFurniture(makeFurniture(*spec, Point2D{}), *planPoint, *planPoint,
			editSettings.furnitureRotationCentidegrees);
	}

	void EditController::updateFurnitureDrag()
	{
		const Furniture* original = document.project().findFurniture(furnitureDrag->furniture.id);
		if (!original)
		{
			furnitureDrag.reset(); // gone under the gesture
			return;
		}
		const PlanPoint wanted{planPoint->x_mm + furnitureDrag->grabOffset.x_mm, planPoint->y_mm + furnitureDrag->grabOffset.y_mm};
		furnitureDrag->furniture = placeFurniture(*original, wanted, *planPoint, original->rotationCentidegrees);
	}

	Column EditController::placeColumn(Column column, PlanPoint wanted) const
	{
		// Like a wall's end: onto a corner, else onto a wall's axis, else the grid; the level below guides too (stacking).
		column.level = activeLevel();
		column.position = snapPoint(snapReference(document.project(), activeLevel()), wanted, editSettings.wallTool.snap).point;
		return column;
	}

	void EditController::updateColumnPlacement()
	{
		columnAt.reset();
		if (currentMode != EditMode::Column || !planPoint)
		{
			return;
		}
		columnAt = placeColumn(makeColumn(editSettings.columnShape, Point2D{}, editSettings.columnWidthMm, editSettings.columnDepthMm,
			editSettings.columnRotationCentidegrees), *planPoint);
	}

	void EditController::updateColumnDrag()
	{
		const Column* original = document.project().findColumn(columnDrag->column.id);
		if (!original)
		{
			columnDrag.reset(); // gone under the gesture
			return;
		}
		columnDrag->column = placeColumn(*original,
			PlanPoint{planPoint->x_mm + columnDrag->grabOffset.x_mm, planPoint->y_mm + columnDrag->grabOffset.y_mm});
	}

	void EditController::updateRunPointer()
	{
		runCursor.reset();
		if (currentMode != EditMode::Services || !pointerRay)
		{
			return;
		}
		// Under the pointer at the height being drawn (else where it points on the plan), snapped like a wall's ends.
		std::optional<PlanPoint> at = atHeight(*pointerRay, static_cast<double>(editSettings.serviceHeightMm));
		if (!at)
		{
			at = planPoint;
		}
		if (!at)
		{
			return;
		}
		const Project reference = levelProject();
		const Point2D snapped = runDraft.empty() ? snapPoint(reference, *at, editSettings.wallTool.snap).point
			: snapFromAnchor(reference, Point2D{runDraft.back().x_mm, runDraft.back().y_mm}, *at, editSettings.wallTool.snap).point;
		runCursor = ServicePoint{snapped.x_mm, snapped.y_mm, editSettings.serviceHeightMm};
	}

	void EditController::updateRunDrag()
	{
		const ServiceRun* original = document.project().findServiceRun(runDrag->run.id);
		if (!original)
		{
			runDrag.reset(); // gone under the gesture
			return;
		}
		const std::optional<PlanPoint> at = pointerRay ? atHeight(*pointerRay, runDrag->planeZ) : std::nullopt;
		if (!at)
		{
			return;
		}
		const PlanPoint shift{at->x_mm - runDrag->grab.x_mm, at->y_mm - runDrag->grab.y_mm};
		ServiceRun moved = *original;
		if (!runDrag->bend)
		{
			// The whole run, by whole grid steps.
			const std::int64_t grid = gridMm();
			const std::int64_t dx = std::llround(shift.x_mm / static_cast<double>(grid)) * grid;
			const std::int64_t dy = std::llround(shift.y_mm / static_cast<double>(grid)) * grid;
			for (ServicePoint& point : moved.points)
			{
				point.x_mm += dx;
				point.y_mm += dy;
			}
		}
		else
		{
			// The bend, snapped like a wall's end; every point there (a rise or drop on the spot) moves with it.
			const ServicePoint bend = original->points[*runDrag->bend];
			const Point2D snapped = snapPoint(levelProject(),
				PlanPoint{static_cast<double>(bend.x_mm) + shift.x_mm, static_cast<double>(bend.y_mm) + shift.y_mm},
				editSettings.wallTool.snap).point;
			for (std::size_t index = *runDrag->bend; index < moved.points.size() && sameSpot(original->points[index], bend); ++index)
			{
				moved.points[index].x_mm = snapped.x_mm;
				moved.points[index].y_mm = snapped.y_mm;
			}
		}
		if (validateServiceRun(moved).ok()) // not onto the next point: keep the last place
		{
			runDrag->run = moved;
		}
	}

	void EditController::updateDrag()
	{
		const Project project = levelProject();
		const Wall* wall = project.findWall(drag->wall);
		if (!wall)
		{
			drag.reset(); // the wall disappeared under the gesture
			return;
		}
		// Angle and length snap relative to the fixed end; other walls' endpoints attract.
		const Point2D fixed = endpointOf(*wall, opposite(drag->endpoint));
		const PlanPoint cursor{planPoint->x_mm + drag->grabOffset.x_mm, planPoint->y_mm + drag->grabOffset.y_mm};
		const Project reference = snapReference(document.project(), activeLevel()); // the level below guides too (stacking)
		drag->position = snapFromAnchor(reference, fixed, cursor, editSettings.wallTool.snap, drag->joined).point;
	}

	void EditController::updateWallBodyDrag()
	{
		const auto dx = quantizeMillimeters(planPoint->x_mm - wallBodyDrag->grab.x_mm);
		const auto dy = quantizeMillimeters(planPoint->y_mm - wallBodyDrag->grab.y_mm);
		if (!dx || !dy) return;
		const Vector2D delta{dx.value(), dy.value()};
		// Keep the last valid preview if shortening an attached wall would collapse it or cut an opening.
		for (const WallId& id : wallBodyDrag->joined)
		{
			const Wall* before = document.project().findWall(id);
			if (!before)
			{
				wallBodyDrag.reset();
				return;
			}
			Wall after = *before;
			for (Point2D* point : {&after.start, &after.end})
			{
				if (*point == wallBodyDrag->wall.start || *point == wallBodyDrag->wall.end)
				{
					point->x_mm += delta.dx_mm;
					point->y_mm += delta.dy_mm;
				}
			}
			if (!validateWall(after)) return;
		}
		wallBodyDrag->delta = delta;
	}

	void EditController::updatePlacement()
	{
		placement.reset();
		if (!planPoint || !isOpeningMode())
		{
			return;
		}
		const Opening& defaults = currentMode == EditMode::Door ? editSettings.door : editSettings.window;
		// Host: the wall whose axis is nearest the pointer, within its body plus the tolerance.
		const Wall* host = nullptr;
		double hostAlong = 0.0;
		double best = std::numeric_limits<double>::infinity();
		const Project project = levelProject();
		for (const Wall& wall : project.walls)
		{
			const AxisPosition position = locate(wall, *planPoint);
			const double reach = static_cast<double>(wall.thicknessMm) / 2.0 + editSettings.openingToleranceMm;
			if (position.along >= 0.0 && position.along <= wall.lengthMm() && position.across <= reach && position.across < best)
			{
				host = &wall;
				hostAlong = position.along;
				best = position.across;
			}
		}
		if (!host)
		{
			return;
		}
		// Centred on the pointer, its edge on the snap grid, in the free stretch of the wall nearest to it: clear of the
		// joined walls and of the other openings (ADR-0013). It opens towards the side of the wall the pointer is on
		// (ADR-0014).
		Opening opening = defaults;
		opening.id = OpeningId();
		opening.opensTo = sideOf(*host, *planPoint);
		const auto length = static_cast<std::int64_t>(std::floor(host->lengthMm()));
		const std::vector<AxisSpan> gaps = freeGaps(length, blockedSpans(project, *host, OpeningId()));
		const std::optional<std::int64_t> offset =
			fitOpening(hostAlong - static_cast<double>(defaults.widthMm) / 2.0, opening, gaps, gridMm());
		if (!offset)
		{
			return;
		}
		opening.offsetMm = *offset;
		placement = OpeningRef{host->id, opening};
	}

	std::int64_t EditController::gridMm() const
	{
		return editSettings.wallTool.snap.gridMm > 0 ? editSettings.wallTool.snap.gridMm : 1;
	}

	std::vector<AxisSpan> EditController::blockedSpans(const Project& levelWalls, const Wall& wall, const OpeningId& except) const
	{
		std::vector<AxisSpan> blocked = document.openingObstructions(levelWalls, wall);
		for (const Opening& opening : wall.openings)
		{
			if (opening.id != except)
			{
				blocked.push_back(occupiedSpan(opening));
			}
		}
		return blocked;
	}

	void EditController::updateOpeningDrag()
	{
		const Project project = levelProject();
		const Wall* wall = project.findWall(openingDrag->wall);
		const Opening* original = wall ? wall->findOpening(openingDrag->opening.id) : nullptr;
		if (!original)
		{
			openingDrag.reset(); // gone under the gesture
			return;
		}
		const double along = locate(*wall, *planPoint).along - openingDrag->grab;
		const auto length = static_cast<std::int64_t>(std::floor(wall->lengthMm()));
		const std::vector<AxisSpan> gaps = freeGaps(length, blockedSpans(project, *wall, original->id));
		Opening moved = *original;
		if (openingDrag->part == OpeningDrag::Part::Body)
		{
			const std::optional<std::int64_t> offset = fitOpening(along, *original, gaps, gridMm());
			if (offset)
			{
				moved.offsetMm = *offset;
			}
		}
		else
		{
			// An edge moves within the free stretch that holds the opening, keeping a minimum width.
			const std::int64_t start = original->offsetMm;
			const std::int64_t end = original->offsetMm + original->widthMm;
			AxisSpan room{start, end};
			for (const AxisSpan& gap : gaps)
			{
				if (gap.fromMm <= start && end <= gap.toMm)
				{
					room = gap;
				}
			}
			const std::int64_t snapped = std::llround(along / static_cast<double>(gridMm())) * gridMm();
			const std::int64_t minimum = std::min(editSettings.minOpeningWidthMm, original->widthMm);
			if (openingDrag->part == OpeningDrag::Part::StartEdge)
			{
				moved.offsetMm = std::clamp(snapped, std::min(room.fromMm, end - minimum), end - minimum);
				moved.widthMm = end - moved.offsetMm;
			}
			else
			{
				moved.widthMm = std::clamp(snapped, start + minimum, std::max(room.toMm, start + minimum)) - start;
			}
			if (!fitsGap(occupiedSpan(moved), gaps))
			{
				return; // a sliding door's pocket (as wide as the door) would run out of room: keep the last width
			}
		}
		openingDrag->opening = moved;
	}

	Status EditController::pointerPressed(const Ray3& sceneRay)
	{
		const Ray3 ray = toLevel(sceneRay);
		movePointer(ray);
		const Project project = levelProject();
		if (currentMode == EditMode::Dimension)
		{
			if (!dimensions.visible() || !planPoint) return Error{ErrorCode::InvalidArgument, "Use the 2D plan to place dimensions"};
			const Result<std::optional<std::string>> placed = dimensions.place(*planPoint, activeLevel());
			if (!placed) return placed.error();
			if (placed.value())
			{
				setMode(EditMode::Select); // keeps the new dimension selected
			}
			return Status::success();
		}
		if (dimensions.visible() && currentMode == EditMode::Select && planPoint)
		{
			const std::optional<DimensionEditor::Grab> grab =
				dimensions.hasProvider() ? dimensions.hit(*planPoint, activeLevel()) : std::nullopt;
			// The selected dimension's grips first; then room texts, so a name can be taken off a figure (ADR-0035).
			const bool grip = grab && grab->grip != DimensionEditor::Grab::Line;
			if (const std::optional<RoomText> text = grip ? std::nullopt : roomTextAt(*planPoint))
			{
				dropDrags();
				clearSelected();
				selectedRoomKey = text->room;
				const RoomLabel* stored = document.project().findRoomLabel(text->room);
				roomTextDrag = RoomTextDrag{stored ? *stored : RoomLabel{text->room}, text->area, text->centre, *planPoint, text->anchor,
					text->heightMm * 0.2};
				return Status::success();
			}
			if (grab)
			{
				dropDrags();
				if (grab->grip == DimensionEditor::Grab::Line)
				{
					clearSelected(); // a grip belongs to the dimension already selected
				}
				dimensions.startDrag(*grab, *planPoint);
				return Status::success();
			}
		}
		if (currentMode == EditMode::Wall)
		{
			if (!planPoint)
			{
				return Error{ErrorCode::InvalidArgument, "Point at the floor to draw"};
			}
			const Result<std::optional<WallId>> clicked = tool.click(*planPoint);
			return clicked ? Status::success() : Status(clicked.error());
		}
		if (isOpeningMode())
		{
			if (!placement)
			{
				return Error{ErrorCode::InvalidArgument, "Point at a wall long enough for it"};
			}
			const Status added = toStatus(document.execute(AddOpeningCommand{placement->wall, placement->opening}));
			updatePlacement();
			return added;
		}
		if (currentMode == EditMode::Paint)
		{
			return paint(project, ray);
		}
		if (currentMode == EditMode::Furniture)
		{
			if (!furnitureAt)
			{
				return Error{ErrorCode::InvalidArgument, "Point at the floor to place it"};
			}
			const Status added = toStatus(document.execute(AddFurnitureCommand{*furnitureAt}));
			updateFurniturePlacement();
			return added;
		}
		if (currentMode == EditMode::Column)
		{
			if (!columnAt)
			{
				return Error{ErrorCode::InvalidArgument, "Point at the floor to place it"};
			}
			const Status added = toStatus(document.execute(AddColumnCommand{*columnAt}));
			updateColumnPlacement();
			return added;
		}
		if (currentMode == EditMode::Services)
		{
			return addRunPoint();
		}

		// The handles at the bends of the selected run move them (ADR-0021).
		if (const std::optional<ServiceRun> run = selectedServiceRun())
		{
			std::optional<double> nearest;
			for (std::size_t index = 0; index < run->points.size(); ++index)
			{
				const ServicePoint& point = run->points[index];
				if (index > 0 && sameSpot(run->points[index - 1], point))
				{
					continue; // one handle per bend
				}
				const std::optional<double> hit = intersectPost(ray, Point2D{point.x_mm, point.y_mm},
					static_cast<double>(editSettings.handleSizeMm), static_cast<double>(editSettings.handleHeightMm));
				if (hit && (!nearest || *hit < *nearest))
				{
					nearest = hit;
					runDrag = RunDrag{*run, index, static_cast<double>(point.z_mm), {}};
				}
			}
			if (runDrag)
			{
				const ServicePoint& bend = run->points[*runDrag->bend];
				runDrag->grab = atHeight(ray, runDrag->planeZ)
					.value_or(PlanPoint{static_cast<double>(bend.x_mm), static_cast<double>(bend.y_mm)});
				return Status::success();
			}
		}

		// The selected door or window: its edge handles resize it, the hole itself moves it (ADR-0013).
		if (const std::optional<OpeningRef> current = selectedOpening(); current && planPoint)
		{
			if (const Wall* host = project.findWall(current->wall))
			{
				const double along = locate(*host, *planPoint).along;
				const std::int64_t startEdge = current->opening.offsetMm;
				const std::int64_t endEdge = current->opening.offsetMm + current->opening.widthMm;
				const std::optional<WallHit> hit = pickWall(project, ray);
				const bool onHole = hit && hit->wall == host->id && openingAt(*host, along) == host->findOpening(current->opening.id);
				const auto grabBody = [&] {
					openingDrag = OpeningDrag{host->id, current->opening, OpeningDrag::Part::Body, along - static_cast<double>(startEdge)};
					return Status::success();
				};
				// Well inside the hole it moves: an oblique ray through it can go on into the far edge's post.
				const double handleHalf = static_cast<double>(editSettings.handleSizeMm) / 2.0;
				if (onHole && along > static_cast<double>(startEdge) + handleHalf && along < static_cast<double>(endEdge) - handleHalf)
				{
					return grabBody();
				}
				for (const auto& [part, edge] : {std::pair{OpeningDrag::Part::StartEdge, startEdge}, {OpeningDrag::Part::EndEdge, endEdge}})
				{
					if (intersectPost(ray, pointOnAxis(*host, edge), static_cast<double>(editSettings.handleSizeMm),
							static_cast<double>(editSettings.handleHeightMm)))
					{
						openingDrag = OpeningDrag{host->id, current->opening, part, along - static_cast<double>(edge)};
						return Status::success();
					}
				}
				if (onHole)
				{
					return grabBody();
				}
			}
		}

		// A handle of the selected wall starts a drag. The endpoint only moves once the pointer moves.
		const WallId current = selection();
		if (const Wall* wall = current.isNil() ? nullptr : project.findWall(current))
		{
			std::optional<double> nearest;
			for (const WallEndpoint endpoint : {WallEndpoint::Start, WallEndpoint::End})
			{
				const std::optional<double> hit = intersectPost(ray, endpointOf(*wall, endpoint),
					static_cast<double>(editSettings.handleSizeMm), static_cast<double>(editSettings.handleHeightMm));
				if (hit && (!nearest || *hit < *nearest))
				{
					nearest = hit;
					drag = EndpointDrag{current, endpoint, endpointOf(*wall, endpoint), endpointOf(*wall, endpoint), {}, {}};
				}
			}
			if (drag)
			{
				for (const Wall& other : project.walls)
				{
					if (other.start == drag->origin || other.end == drag->origin)
					{
						drag->joined.push_back(other.id);
					}
				}
				// Handles are grabbed anywhere up their height: keep that offset, measured as the drag will measure.
				if (const std::optional<PlanPoint> grabbed = planPointOf(ray, drag->joined))
				{
					drag->grabOffset = PlanPoint{static_cast<double>(drag->origin.x_mm) - grabbed->x_mm,
						static_cast<double>(drag->origin.y_mm) - grabbed->y_mm};
				}
				return Status::success();
			}
		}

		// What is nearest under the pointer: selected, and a piece, a column or a run grabbed (it moves once the pointer moves).
		clearSelected();
		const Project& model = document.project();
		const std::optional<WallHit> wallHit = pickWall(project, ray);
		const std::optional<FurnitureHit> pieceHit = pickFurniture(model, ray, activeLevel());
		const std::optional<ColumnHit> columnHit = pickColumn(model, ray, activeLevel());
		const std::optional<ServiceRunHit> runHit =
			servicesShown ? pickServiceRun(model, ray, activeLevel(), editSettings.servicePickToleranceMm, serviceKindsShown) : std::nullopt;
		constexpr double Far = std::numeric_limits<double>::infinity();
		const double wallDistance = wallHit ? wallHit->distance : Far;
		const double pieceDistance = pieceHit ? pieceHit->distance : Far;
		const double columnDistance = columnHit ? columnHit->distance : Far;
		// A run is seen through the walls (the layer shows them ghosted): only furniture and columns hide it.
		if (runHit && runHit->distance < pieceDistance && runHit->distance < columnDistance)
		{
			const ServiceRun& run = *model.findServiceRun(runHit->run);
			selectedRunId = run.id;
			const double t = runHit->distance;
			runDrag = RunDrag{run, std::nullopt, ray.originZ + ray.directionZ * t,
				PlanPoint{ray.originX + ray.directionX * t, ray.originY + ray.directionY * t}};
			return Status::success();
		}
		if (columnHit && columnDistance <= wallDistance && columnDistance < pieceDistance)
		{
			const Column& column = *model.findColumn(columnHit->column);
			selectedColumnId = column.id;
			columnDrag = ColumnDrag{column, {}};
			if (planPoint)
			{
				columnDrag->grabOffset = PlanPoint{static_cast<double>(column.position.x_mm) - planPoint->x_mm,
					static_cast<double>(column.position.y_mm) - planPoint->y_mm};
			}
			return Status::success();
		}
		if (pieceHit && pieceDistance < wallDistance)
		{
			const Furniture& piece = *document.project().findFurniture(pieceHit->furniture);
			selectedFurnitureId = piece.id;
			furnitureDrag = FurnitureDrag{piece, {}};
			if (planPoint)
			{
				furnitureDrag->grabOffset = PlanPoint{static_cast<double>(piece.position.x_mm) - planPoint->x_mm,
					static_cast<double>(piece.position.y_mm) - planPoint->y_mm};
			}
			return Status::success();
		}
		// A wall, or the opening in it where the pointer is along the wall; else the room whose floor is under the pointer.
		if (const std::optional<WallHit>& hit = wallHit)
		{
			selected = hit->wall;
			const Wall& wall = *project.findWall(hit->wall);
			const PlanPoint where{ray.originX + ray.directionX * hit->distance, ray.originY + ray.directionY * hit->distance};
			if (const Opening* opening = openingAt(wall, locate(wall, where).along))
			{
				selectedOpeningId = opening->id;
			}
			else
			{
				wallBodyDrag = WallBodyDrag{wall, {}, {}, {}};
				for (const Wall& other : project.walls)
				{
					if (other.start == wall.start || other.end == wall.start || other.start == wall.end || other.end == wall.end)
						wallBodyDrag->joined.push_back(other.id);
				}
				const auto grabbed = intersectFloor(ray);
				if (grabbed) wallBodyDrag->grab = *grabbed;
				else wallBodyDrag.reset();
			}
		}
		else if (planPoint && roomLocator)
		{
			selectedRoomKey = roomLocator(project, *planPoint).value_or(std::string());
		}
		return Status::success();
	}

	Status EditController::pointerDoubleClicked(const Ray3& sceneRay)
	{
		if (currentMode == EditMode::Services && !runDraft.empty())
		{
			movePointer(toLevel(sceneRay));
			return finishRun();
		}
		return pointerPressed(sceneRay);
	}

	Status EditController::pointerReleased(const Ray3&)
	{
		if (roomTextDrag)
		{
			const RoomTextDrag finished = *roomTextDrag;
			roomTextDrag.reset();
			return finished.moved ? toStatus(document.execute(SetRoomLabelCommand{finished.label})) : Status::success();
		}
		if (dimensions.dragging())
		{
			return dimensions.release();
		}
		// Each drag: moved as the last pointerMoved() left it; one undoable change, or none if it did not move.
		if (wallBodyDrag)
		{
			const WallBodyDrag finished = *wallBodyDrag;
			wallBodyDrag.reset();
			return toStatus(document.execute(TranslateWallCommand{finished.wall.id, finished.delta}));
		}
		if (columnDrag)
		{
			const Column moved = columnDrag->column;
			columnDrag.reset();
			const Column* original = document.project().findColumn(moved.id);
			if (!original || *original == moved)
			{
				return Status::success();
			}
			return toStatus(document.execute(ChangeColumnCommand{moved}));
		}
		if (runDrag)
		{
			const ServiceRun moved = runDrag->run;
			runDrag.reset();
			const ServiceRun* original = document.project().findServiceRun(moved.id);
			if (!original || *original == moved)
			{
				return Status::success();
			}
			return toStatus(document.execute(ChangeServiceRunCommand{moved}));
		}
		if (furnitureDrag)
		{
			// Moved as the last pointerMoved() left it; one undoable change, or none if it did not move.
			const Furniture moved = furnitureDrag->furniture;
			furnitureDrag.reset();
			const Furniture* original = document.project().findFurniture(moved.id);
			if (!original || *original == moved)
			{
				return Status::success();
			}
			return toStatus(document.execute(ChangeFurnitureCommand{moved}));
		}
		if (openingDrag)
		{
			// Moved or resized as the last pointerMoved() left it; one undoable change, or none if it did not move.
			const OpeningDrag finished = *openingDrag;
			openingDrag.reset();
			const Wall* wall = document.project().findWall(finished.wall);
			const Opening* original = wall ? wall->findOpening(finished.opening.id) : nullptr;
			if (!original || *original == finished.opening)
			{
				return Status::success();
			}
			return toStatus(document.execute(ChangeOpeningCommand{finished.wall, finished.opening}));
		}
		if (!drag)
		{
			return Status::success();
		}
		// The position comes from the last pointerMoved(); releasing never moves the endpoint by itself.
		const EndpointDrag finished = *drag;
		drag.reset();
		if (finished.joined.size() > 1)
		{
			return toStatus(document.execute(MoveJointCommand{finished.origin, finished.position, activeLevel()}));
		}
		return toStatus(document.execute(MoveWallEndpointCommand{finished.wall, finished.endpoint, finished.position}));
	}

	Status EditController::confirm()
	{
		if (currentMode == EditMode::Services)
		{
			return runDraft.empty() ? Status::success() : finishRun();
		}
		if (currentMode != EditMode::Wall)
		{
			return Status::success();
		}
		const Result<WallId> created = tool.confirm();
		return created ? Status::success() : Status(created.error());
	}

	Status EditController::addRunPoint()
	{
		if (!runCursor)
		{
			return Error{ErrorCode::InvalidArgument, "Point at the plan to place the run's points"};
		}
		if (!runDraft.empty())
		{
			if (*runCursor == runDraft.back())
			{
				return finishRun(); // a second click on the last point (a double click)
			}
			const std::vector<ServicePoint> steps = stepTo(runDraft.back(), *runCursor);
			if (runDraft.size() + steps.size() > MaxServicePoints)
			{
				return Error{ErrorCode::OutOfRange, "A run has at most " + std::to_string(MaxServicePoints) +
					" points: finish this one and start another"};
			}
			runDraft.insert(runDraft.end(), steps.begin(), steps.end());
		}
		else
		{
			runDraft.push_back(*runCursor);
		}
		updateRunPointer(); // snapping now measures from the new last point
		return Status::success();
	}

	Status EditController::finishRun()
	{
		if (runDraft.size() < 2)
		{
			return Error{ErrorCode::InvalidArgument, "Place a second point to make a run"};
		}
		ServiceRun run;
		run.level = activeLevel();
		run.kind = editSettings.serviceKind;
		run.points = runDraft;
		const Status added = toStatus(document.execute(AddServiceRunCommand{run}));
		if (added)
		{
			runDraft.clear();
			updateRunPointer();
		}
		return added;
	}

	std::optional<ServiceRun> EditController::runPreview() const
	{
		if (runDraft.empty())
		{
			return std::nullopt;
		}
		ServiceRun run;
		run.level = activeLevel();
		run.kind = editSettings.serviceKind;
		run.points = runDraft;
		if (runCursor && !(*runCursor == runDraft.back()))
		{
			const std::vector<ServicePoint> steps = stepTo(runDraft.back(), *runCursor);
			run.points.insert(run.points.end(), steps.begin(), steps.end());
		}
		return run.points.size() >= 2 ? std::optional<ServiceRun>(run) : std::nullopt;
	}

	void EditController::cancel()
	{
		if (isDragging())
		{
			dropDrags();
		}
		else if (currentMode == EditMode::Dimension && dimensions.hasDraft())
		{
			dimensions.clearDraft();
		}
		else if (currentMode == EditMode::Wall && tool.state() == WallTool::State::Drafting)
		{
			tool.cancel();
		}
		else if (currentMode == EditMode::Services && !runDraft.empty())
		{
			runDraft.clear();
			updateRunPointer();
		}
		else if (currentMode != EditMode::Select)
		{
			setMode(EditMode::Select);
		}
		else
		{
			clearSelected();
		}
	}

	void EditController::appendText(std::string_view text)
	{
		if (currentMode == EditMode::Wall)
		{
			tool.appendText(text);
		}
	}

	void EditController::backspace()
	{
		if (currentMode == EditMode::Wall)
		{
			tool.backspace();
		}
		else if (currentMode == EditMode::Services && !runDraft.empty())
		{
			runDraft.pop_back();
			updateRunPointer();
		}
	}

	Status EditController::deleteSelection()
	{
		dropDrags();
		if (currentMode == EditMode::Select)
		{
			if (const std::optional<Status> deleted = dimensions.deleteSelected(activeLevel()))
			{
				return *deleted;
			}
		}
		if (const std::optional<Column> column = selectedColumn())
		{
			const Status deleted = toStatus(document.execute(DeleteColumnCommand{column->id}));
			if (deleted)
			{
				selectedColumnId = ColumnId();
			}
			return deleted;
		}
		if (const std::optional<ServiceRun> run = selectedServiceRun())
		{
			const Status deleted = toStatus(document.execute(DeleteServiceRunCommand{run->id}));
			if (deleted)
			{
				selectedRunId = ServiceRunId();
			}
			return deleted;
		}
		if (const std::optional<Furniture> piece = selectedFurniture())
		{
			const Status deleted = toStatus(document.execute(DeleteFurnitureCommand{piece->id}));
			if (deleted)
			{
				selectedFurnitureId = FurnitureId();
			}
			return deleted;
		}
		if (const std::optional<OpeningRef> opening = selectedOpening())
		{
			const Status deleted = toStatus(document.execute(DeleteOpeningCommand{opening->wall, opening->opening.id}));
			if (deleted)
			{
				selectedOpeningId = OpeningId();
				selected = WallId();
			}
			return deleted;
		}
		const WallId current = selection();
		if (current.isNil())
		{
			return nothingSelected();
		}
		const Status deleted = toStatus(document.execute(DeleteWallCommand{current}));
		if (deleted)
		{
			selected = WallId();
		}
		return deleted;
	}

	Status EditController::undo()
	{
		dropDrags();
		tool.cancel();
		runDraft.clear();
		dimensions.clearDraft();
		const Status undone = document.undo();
		refreshPlacements();
		return undone;
	}

	Status EditController::redo()
	{
		dropDrags();
		tool.cancel();
		runDraft.clear();
		dimensions.clearDraft();
		const Status redone = document.redo();
		refreshPlacements();
		return redone;
	}

	std::optional<Dimension> EditController::selectedDimension() const
	{
		return currentMode == EditMode::Select ? dimensions.selected(activeLevel()) : std::nullopt;
	}

	std::optional<Dimension> EditController::dimensionPreview() const
	{
		return dimensions.preview(planPoint, activeLevel(), currentMode == EditMode::Dimension);
	}

	Status EditController::selectDimension(std::string_view id)
	{
		setMode(EditMode::Select);
		clearSelection();
		return dimensions.select(id, activeLevel());
	}

	Status EditController::setDimensionOffset(std::string_view text)
	{
		if (currentMode != EditMode::Select) return nothingSelected();
		return dimensions.setOffset(text, editSettings.wallTool.inputUnit, activeLevel());
	}

	Status EditController::setDimensionText(std::string_view text)
	{
		if (currentMode != EditMode::Select) return nothingSelected();
		return dimensions.setText(text, activeLevel());
	}

	Status EditController::setDimensionCoordinate(std::string_view text, int coordinate)
	{
		if (currentMode != EditMode::Select) return nothingSelected();
		return dimensions.setCoordinate(text, coordinate, editSettings.wallTool.inputUnit, activeLevel());
	}

	Status EditController::restoreAutomaticDimensions()
	{
		return dimensions.restoreAutomatic(activeLevel());
	}

	void EditController::reset()
	{
		dropDrags();
		tool.cancel();
		runDraft.clear();
		dimensions.clearDraft();
		clearSelected();
		refreshPlacements();
	}

	void EditController::refreshPlacements()
	{
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
			updateFurniturePlacement();
			updateColumnPlacement();
		}
		updateRunPointer();
	}

	WallId EditController::selection() const
	{
		return selectedOpeningId.isNil() && !selected.isNil() && document.project().findWall(selected) ? selected : WallId();
	}

	std::optional<OpeningRef> EditController::selectedOpening() const
	{
		const Wall* wall = selectedOpeningId.isNil() ? nullptr : document.project().findWall(selected);
		const Opening* opening = wall ? wall->findOpening(selectedOpeningId) : nullptr;
		return opening ? std::optional<OpeningRef>(OpeningRef{wall->id, *opening}) : std::nullopt;
	}

	Status EditController::select(const WallId& id)
	{
		if (id.isNil())
		{
			clearSelection();
			return Status::success();
		}
		if (!document.project().findWall(id))
		{
			return Error{ErrorCode::NotFound, "Wall not found: " + toString(id)};
		}
		setMode(EditMode::Select);
		changeLevel(document.project().levelOf(*document.project().findWall(id)));
		clearSelection();
		selected = id;
		return Status::success();
	}

	Status EditController::selectOpening(const WallId& wall, const OpeningId& opening)
	{
		const Wall* host = document.project().findWall(wall);
		if (!host || !host->findOpening(opening))
		{
			return Error{ErrorCode::NotFound, "Opening not found: " + toString(opening)};
		}
		setMode(EditMode::Select);
		changeLevel(document.project().levelOf(*host));
		clearSelection();
		selected = wall;
		selectedOpeningId = opening;
		return Status::success();
	}

	void EditController::clearSelection()
	{
		dropDrags();
		clearSelected();
	}

	Status EditController::changeSelection(std::string_view text, Command (*makeCommand)(const WallId&, std::int64_t))
	{
		const WallId current = selection();
		if (current.isNil())
		{
			return nothingSelected();
		}
		const Result<std::int64_t> value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value)
		{
			return value.error();
		}
		return toStatus(document.execute(makeCommand(current, value.value())));
	}

	Status EditController::setSelectionLength(std::string_view text)
	{
		return changeSelection(text, [](const WallId& wall, std::int64_t mm) -> Command { return ChangeWallLengthCommand{wall, mm}; });
	}

	Status EditController::setSelectionThickness(std::string_view text)
	{
		return changeSelection(text, [](const WallId& wall, std::int64_t mm) -> Command { return ChangeWallThicknessCommand{wall, mm}; });
	}

	Status EditController::setSelectionHeight(std::string_view text)
	{
		return changeSelection(text, [](const WallId& wall, std::int64_t mm) -> Command { return ChangeWallHeightCommand{wall, mm}; });
	}

	Status EditController::changeOpening(std::string_view text, void (*apply)(Opening&, std::int64_t))
	{
		const std::optional<OpeningRef> current = selectedOpening();
		if (!current)
		{
			return nothingSelected();
		}
		const Result<std::int64_t> value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value)
		{
			return value.error();
		}
		Opening changed = current->opening;
		apply(changed, value.value());
		return toStatus(document.execute(ChangeOpeningCommand{current->wall, changed}));
	}

	Status EditController::setOpeningWidth(std::string_view text)
	{
		return changeOpening(text, [](Opening& opening, std::int64_t mm) { opening.widthMm = mm; });
	}

	Status EditController::setOpeningHeight(std::string_view text)
	{
		return changeOpening(text, [](Opening& opening, std::int64_t mm) { opening.heightMm = mm; });
	}

	Status EditController::setOpeningSill(std::string_view text)
	{
		return changeOpening(text, [](Opening& opening, std::int64_t mm) { opening.sillMm = mm; });
	}

	Status EditController::setOpeningOffset(std::string_view text)
	{
		return changeOpening(text, [](Opening& opening, std::int64_t mm) { opening.offsetMm = mm; });
	}

	Status EditController::editSelectedOpening(const std::function<void(Opening&)>& edit)
	{
		const std::optional<OpeningRef> current = selectedOpening();
		if (!current)
		{
			return nothingSelected();
		}
		Opening changed = current->opening;
		edit(changed);
		return toStatus(document.execute(ChangeOpeningCommand{current->wall, changed}));
	}

	Status EditController::setOpeningGlazing(std::string_view finish)
	{
		return editSelectedOpening([finish](Opening& opening) { opening.glazing = std::string(finish); });
	}

	Status EditController::setOpeningFinish(std::string_view finish)
	{
		return editSelectedOpening([finish](Opening& opening) { opening.finish = std::string(finish); });
	}

	Status EditController::setOpeningOperation(OpeningOperation operation)
	{
		return editSelectedOpening([operation](Opening& opening)
		{
			opening.operation = operation;
			const auto* family = findOpeningFamily(opening.family);
			if (family && family->operation != operation) { opening.family.clear(); }
			if (operation == OpeningOperation::Fixed)
			{
				opening.openPercent = 0;
			}
		});
	}

	Status EditController::setNewOpeningOperation(OpeningKind kind, OpeningOperation operation)
	{
		if (kind == OpeningKind::Door && operation == OpeningOperation::Fixed)
		{
			return Error{ErrorCode::InvalidArgument, "A door cannot be fixed"};
		}
		// Placing it later also checks that everything it takes (a sliding door's pocket) fits the wall.
		Opening& defaults = kind == OpeningKind::Door ? editSettings.door : editSettings.window;
		defaults.operation = operation;
		const auto* family = findOpeningFamily(defaults.family);
		if (family && family->operation != operation) { defaults.family.clear(); }
		if (operation == OpeningOperation::Fixed) { defaults.openPercent = 0; }
		updatePlacement();
		return Status::success();
	}

	Status EditController::setOpeningFamily(std::string_view family)
	{
		const auto current = selectedOpening();
		if (!current) { return nothingSelected(); }
		Opening changed = current->opening;
		if (Status applied = applyOpeningFamily(changed, family); !applied) { return applied; }
		return toStatus(document.execute(ChangeOpeningCommand{current->wall, changed}));
	}

	Status EditController::setNewOpeningFamily(OpeningKind kind, std::string_view family)
	{
		Opening& defaults = kind == OpeningKind::Door ? editSettings.door : editSettings.window;
		if (Status applied = applyOpeningFamily(defaults, family); !applied) { return applied; }
		const auto* chosen = findOpeningFamily(family);
		defaults.widthMm = chosen->widthMm;
		defaults.heightMm = chosen->heightMm;
		defaults.sillMm = chosen->sillMm;
		updatePlacement();
		return Status::success();
	}

	Status EditController::setNewOpeningWidth(std::string_view text)
	{
		if (!isOpeningMode()) { return Error{ErrorCode::InvalidArgument, "Choose the Door or Window tool first"}; }
		const auto value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value) { return value.error(); }
		if (value.value() < 300 || value.value() > MaxAbsMillimeters)
		{
			return Error{ErrorCode::OutOfRange, "An opening must be at least 300 mm wide and within the model range"};
		}
		(currentMode == EditMode::Door ? editSettings.door : editSettings.window).widthMm = value.value();
		updatePlacement();
		return Status::success();
	}

	Status EditController::setNewOpeningHeight(std::string_view text)
	{
		if (!isOpeningMode()) { return Error{ErrorCode::InvalidArgument, "Choose the Door or Window tool first"}; }
		const auto value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value) { return value.error(); }
		if (value.value() <= 0 || value.value() > MaxAbsMillimeters)
		{
			return Error{ErrorCode::OutOfRange, "An opening needs a positive height within the model range"};
		}
		(currentMode == EditMode::Door ? editSettings.door : editSettings.window).heightMm = value.value();
		updatePlacement();
		return Status::success();
	}

	Status EditController::setNewOpeningSill(std::string_view text)
	{
		if (!isOpeningMode()) { return Error{ErrorCode::InvalidArgument, "Choose the Door or Window tool first"}; }
		const auto value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value) { return value.error(); }
		if (value.value() < 0 || value.value() > MaxAbsMillimeters || (currentMode == EditMode::Door && value.value() != 0))
		{
			return Error{ErrorCode::OutOfRange, "A door starts at the floor; a window needs a nonnegative sill within the model range"};
		}
		(currentMode == EditMode::Door ? editSettings.door : editSettings.window).sillMm = value.value();
		updatePlacement();
		return Status::success();
	}

	Status EditController::flipOpeningHinge()
	{
		return editSelectedOpening([](Opening& opening) { opening.hinge = oppositeEnd(opening.hinge); });
	}

	Status EditController::flipOpeningSide()
	{
		return editSelectedOpening([](Opening& opening) { opening.opensTo = oppositeSide(opening.opensTo); });
	}

	Status EditController::setOpeningOpenPercent(std::string_view text)
	{
		// "35", "35%", "35 %": a whole percentage.
		std::string digits;
		for (const char c : text)
		{
			if (c != ' ' && c != '%')
			{
				digits += c;
			}
		}
		const bool whole = !digits.empty() && digits.size() <= 3 &&
			std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; });
		int percent = whole ? 0 : -1;
		for (const char c : whole ? digits : std::string())
		{
			percent = percent * 10 + (c - '0');
		}
		if (percent < 0 || percent > 100)
		{
			return Error{ErrorCode::InvalidArgument, "Type how open it is shown: a whole number from 0 to 100 (%)"};
		}
		return editSelectedOpening([percent](Opening& opening) { opening.openPercent = percent; });
	}

	Status EditController::flip()
	{
		if (isOpeningMode())
		{
			// The doors or windows placed from now on: their hinges on the other jamb.
			Opening& defaults = currentMode == EditMode::Door ? editSettings.door : editSettings.window;
			defaults.hinge = oppositeEnd(defaults.hinge);
			updatePlacement();
			return Status::success();
		}
		if (currentMode == EditMode::Furniture)
		{
			// The pieces placed from now on (unless a wall turns them).
			editSettings.furnitureRotationCentidegrees = (editSettings.furnitureRotationCentidegrees + 9000) % FullTurnCentidegrees;
			updateFurniturePlacement();
			return Status::success();
		}
		if (currentMode == EditMode::Column)
		{
			// The columns placed from now on.
			editSettings.columnRotationCentidegrees = (editSettings.columnRotationCentidegrees + 9000) % FullTurnCentidegrees;
			updateColumnPlacement();
			return Status::success();
		}
		if (selectedFurniture())
		{
			return rotateFurniture();
		}
		if (selectedColumn())
		{
			return editSelectedColumn([](Column& column)
			{
				if (column.shape == ColumnShape::Rectangle)
				{
					column.rotationCentidegrees = (column.rotationCentidegrees + 9000) % FullTurnCentidegrees;
				}
				return Status::success();
			});
		}
		if (!selectedOpening())
		{
			return Error{ErrorCode::NotFound, "Select a piece of furniture, a column, a door or a window, or pick the door, "
				"window, furniture or column tool, to turn it"};
		}
		return flipOpeningHinge();
	}

	Status EditController::setFurnitureType(std::string_view type)
	{
		if (!furnitureSpec(document.project(), type))
		{
			return Error{ErrorCode::NotFound, "Unknown furniture type '" + std::string(type) + "'"};
		}
		editSettings.furnitureType = std::string(type);
		updateFurniturePlacement();
		return Status::success();
	}

	std::optional<Furniture> EditController::selectedFurniture() const
	{
		const Furniture* piece = selectedFurnitureId.isNil() ? nullptr : document.project().findFurniture(selectedFurnitureId);
		return piece ? std::optional<Furniture>(*piece) : std::nullopt;
	}

	Status EditController::selectFurniture(const FurnitureId& id)
	{
		const Furniture* piece = document.project().findFurniture(id);
		if (!piece)
		{
			return Error{ErrorCode::NotFound, "Furniture not found: " + toString(id)};
		}
		setMode(EditMode::Select);
		changeLevel(piece->level);
		clearSelection();
		selectedFurnitureId = id;
		return Status::success();
	}

	Status EditController::editSelectedFurniture(const std::function<Status(Furniture&)>& edit)
	{
		const std::optional<Furniture> current = selectedFurniture();
		if (!current)
		{
			return nothingSelected();
		}
		Furniture changed = *current;
		if (Status edited = edit(changed); !edited)
		{
			return edited;
		}
		furnitureDrag.reset();
		return toStatus(document.execute(ChangeFurnitureCommand{changed}));
	}

	Status EditController::changeFurnitureSize(std::string_view text, std::int64_t Furniture::*size)
	{
		const Result<std::int64_t> value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value)
		{
			return value.error();
		}
		return editSelectedFurniture([&value, size](Furniture& piece)
		{
			piece.*size = value.value();
			return Status::success();
		});
	}

	Status EditController::setFurnitureWidth(std::string_view text)
	{
		return changeFurnitureSize(text, &Furniture::widthMm);
	}

	Status EditController::setFurnitureDepth(std::string_view text)
	{
		return changeFurnitureSize(text, &Furniture::depthMm);
	}

	Status EditController::setFurnitureHeight(std::string_view text)
	{
		return changeFurnitureSize(text, &Furniture::heightMm);
	}

	Status EditController::setFurnitureElevation(std::string_view text)
	{
		return changeFurnitureSize(text, &Furniture::elevationMm);
	}

	Status EditController::setFurnitureLightEnabled(bool enabled)
	{
		return editSelectedFurniture([enabled](Furniture& piece)
		{
			if (!piece.light) return Status(Error{ErrorCode::InvalidArgument, "This furniture has no light"});
			piece.light->enabled = enabled;
			return Status::success();
		});
	}

	Status EditController::setFurnitureLightLumens(std::int64_t lumens)
	{
		return editSelectedFurniture([lumens](Furniture& piece)
		{
			if (!piece.light) return Status(Error{ErrorCode::InvalidArgument, "This furniture has no light"});
			piece.light->lumens = lumens;
			return Status::success();
		});
	}

	Status EditController::setFurnitureRotation(std::string_view text)
	{
		const std::optional<double> degrees = parseDegrees(text);
		if (!degrees)
		{
			return Error{ErrorCode::InvalidArgument, "Type the rotation in degrees, e.g. 90 or 22.5"};
		}
		return editSelectedFurniture([&degrees](Furniture& piece)
		{
			piece.rotationCentidegrees = toCentidegrees(*degrees);
			return Status::success();
		});
	}

	Status EditController::setFurnitureFinish(std::string_view finish)
	{
		return editSelectedFurniture([finish](Furniture& piece)
		{
			piece.finish = std::string(finish);
			return Status::success();
		});
	}

	Status EditController::rotateFurniture()
	{
		return editSelectedFurniture([](Furniture& piece)
		{
			piece.rotationCentidegrees = (piece.rotationCentidegrees + 9000) % FullTurnCentidegrees;
			return Status::success();
		});
	}

	Status EditController::setNewColumnShape(ColumnShape shape)
	{
		editSettings.columnShape = shape;
		updateColumnPlacement();
		return Status::success();
	}

	Status EditController::changeNewColumnSize(std::string_view text, std::int64_t EditSettings::*size)
	{
		const Result<std::int64_t> value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value)
		{
			return value.error();
		}
		if (value.value() < MinColumnSizeMm || value.value() > MaxColumnSizeMm)
		{
			return Error{ErrorCode::OutOfRange, "A column measures from " + std::to_string(MinColumnSizeMm) + " to " +
				std::to_string(MaxColumnSizeMm) + " mm"};
		}
		editSettings.*size = value.value();
		updateColumnPlacement();
		return Status::success();
	}

	Status EditController::setNewColumnWidth(std::string_view text)
	{
		return changeNewColumnSize(text, &EditSettings::columnWidthMm);
	}

	Status EditController::setNewColumnDepth(std::string_view text)
	{
		return changeNewColumnSize(text, &EditSettings::columnDepthMm);
	}

	std::optional<Column> EditController::selectedColumn() const
	{
		const Column* column = selectedColumnId.isNil() ? nullptr : document.project().findColumn(selectedColumnId);
		return column ? std::optional<Column>(*column) : std::nullopt;
	}

	Status EditController::selectColumn(const ColumnId& id)
	{
		const Column* column = document.project().findColumn(id);
		if (!column)
		{
			return Error{ErrorCode::NotFound, "Column not found: " + toString(id)};
		}
		setMode(EditMode::Select);
		changeLevel(column->level);
		clearSelection();
		selectedColumnId = id;
		return Status::success();
	}

	Status EditController::editSelectedColumn(const std::function<Status(Column&)>& edit)
	{
		const std::optional<Column> current = selectedColumn();
		if (!current)
		{
			return nothingSelected();
		}
		Column changed = *current;
		if (Status edited = edit(changed); !edited)
		{
			return edited;
		}
		columnDrag.reset();
		return toStatus(document.execute(ChangeColumnCommand{changed}));
	}

	Status EditController::changeColumnSize(std::string_view text, std::int64_t Column::*size)
	{
		const Result<std::int64_t> value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value)
		{
			return value.error();
		}
		return editSelectedColumn([&value, size](Column& column)
		{
			column.*size = value.value();
			if (column.shape == ColumnShape::Round)
			{
				column.widthMm = column.depthMm = value.value(); // its diameter
			}
			return Status::success();
		});
	}

	Status EditController::setColumnShape(ColumnShape shape)
	{
		return editSelectedColumn([shape](Column& column)
		{
			column.shape = shape;
			if (shape == ColumnShape::Round)
			{
				column.depthMm = column.widthMm;
				column.rotationCentidegrees = 0;
			}
			return Status::success();
		});
	}

	Status EditController::setColumnWidth(std::string_view text)
	{
		return changeColumnSize(text, &Column::widthMm);
	}

	Status EditController::setColumnDepth(std::string_view text)
	{
		return changeColumnSize(text, &Column::depthMm);
	}

	Status EditController::setColumnRotation(std::string_view text)
	{
		const std::optional<double> degrees = parseDegrees(text);
		if (!degrees)
		{
			return Error{ErrorCode::InvalidArgument, "Type the rotation in degrees, e.g. 90 or 22.5"};
		}
		return editSelectedColumn([&degrees](Column& column)
		{
			if (column.shape == ColumnShape::Round)
			{
				return Status(Error{ErrorCode::InvalidArgument, "A round column is not turned"});
			}
			column.rotationCentidegrees = toCentidegrees(*degrees);
			return Status::success();
		});
	}

	Status EditController::setColumnFinish(std::string_view finish)
	{
		return editSelectedColumn([finish](Column& column)
		{
			column.finish = std::string(finish);
			return Status::success();
		});
	}

	void EditController::setShowServices(bool show)
	{
		servicesShown = show;
		if (!show)
		{
			if (currentMode == EditMode::Services)
			{
				setMode(EditMode::Select);
			}
			runDrag.reset();
			selectedRunId = ServiceRunId();
		}
	}

	void EditController::setShowInteriorDimensions(bool show)
	{
		interiorDimensionsShown = show;
		if (!show)
		{
			dimensions.dropDrag();
			if (!dimensions.selected(activeLevel()))
			{
				dimensions.clearSelection(); // no longer shown, so no longer edited
			}
		}
	}

	void EditController::setServiceKindShown(ServiceKind kind, bool shown)
	{
		serviceKindsShown.set(kind, shown);
		const ServiceRun* selectedRun = selectedRunId.isNil() ? nullptr : document.project().findServiceRun(selectedRunId);
		if (!shown && selectedRun && selectedRun->kind == kind)
		{
			runDrag.reset();
			selectedRunId = ServiceRunId();
		}
	}

	Status EditController::setServiceKind(ServiceKind kind)
	{
		editSettings.serviceKind = kind;
		serviceKindsShown.set(kind, true); // what is drawn is seen
		return Status::success();
	}

	Status EditController::setServiceHeightMm(std::int64_t heightMm)
	{
		if (heightMm < MinServiceHeightMm || heightMm > MaxServiceHeightMm)
		{
			return Error{ErrorCode::OutOfRange, "A run goes from " + std::to_string(MinServiceHeightMm) + " to " +
				std::to_string(MaxServiceHeightMm) + " mm over the level's floor"};
		}
		editSettings.serviceHeightMm = heightMm;
		updateRunPointer();
		return Status::success();
	}

	Status EditController::setServiceHeight(std::string_view text)
	{
		const Result<std::int64_t> value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value)
		{
			return value.error();
		}
		return setServiceHeightMm(value.value());
	}

	std::optional<ServiceRun> EditController::selectedServiceRun() const
	{
		const ServiceRun* run = !servicesShown || selectedRunId.isNil() ? nullptr : document.project().findServiceRun(selectedRunId);
		return run && serviceKindsShown.contains(run->kind) ? std::optional<ServiceRun>(*run) : std::nullopt;
	}

	Status EditController::selectServiceRun(const ServiceRunId& id)
	{
		const ServiceRun* run = document.project().findServiceRun(id);
		if (!run)
		{
			return Error{ErrorCode::NotFound, "Run not found: " + toString(id)};
		}
		setMode(EditMode::Select);
		changeLevel(run->level);
		clearSelection();
		servicesShown = true;
		serviceKindsShown.set(run->kind, true);
		selectedRunId = id;
		return Status::success();
	}

	Status EditController::setServiceRunKind(ServiceKind kind)
	{
		std::optional<ServiceRun> changed = selectedServiceRun();
		if (!changed)
		{
			return nothingSelected();
		}
		changed->kind = kind;
		serviceKindsShown.set(kind, true); // it stays in sight
		runDrag.reset();
		return toStatus(document.execute(ChangeServiceRunCommand{*changed}));
	}

	std::optional<PaintTarget> EditController::resolvePaintTarget(const Project& levelWalls, const Ray3& ray) const
	{
		const bool forWalls = validateFinishIn(document.project(), editSettings.paintFinish, Surface::Wall).ok();
		const bool forFloors = validateFinishIn(document.project(), editSettings.paintFinish, Surface::Floor).ok();
		// The face of the wall under the pointer (the side of its axis the ray meets it on)...
		if (forWalls)
		{
			if (const std::optional<WallHit> hit = pickWall(levelWalls, ray))
			{
				const Wall& wall = *levelWalls.findWall(hit->wall);
				const PlanPoint at{ray.originX + ray.directionX * hit->distance, ray.originY + ray.directionY * hit->distance};
				return PaintTarget{wall.id, sideOf(wall, at), {}};
			}
		}
		// ...else the floor of the room under it: a finish only for floors looks past the walls, down to the floor.
		const std::optional<PlanPoint> floor = forFloors ? intersectFloor(ray) : std::nullopt;
		const std::optional<std::string> room = floor && roomLocator ? roomLocator(levelWalls, *floor) : std::nullopt;
		return room ? std::optional<PaintTarget>(PaintTarget{WallId(), WallSide::Left, *room}) : std::nullopt;
	}

	Status EditController::paint(const Project& project, const Ray3& ray)
	{
		paintAt = resolvePaintTarget(project, ray);
		if (!paintAt)
		{
			const bool forWalls = validateFinishIn(document.project(), editSettings.paintFinish, Surface::Wall).ok();
			const bool forFloors = validateFinishIn(document.project(), editSettings.paintFinish, Surface::Floor).ok();
			return Error{ErrorCode::InvalidArgument, forWalls && forFloors ? "Point at a wall or at the floor of a room"
				: forFloors ? "Point at the floor of a room: walls must close it all round"
				: "Point at a wall: this finish is for walls"};
		}
		if (!paintAt->wall.isNil())
		{
			return toStatus(document.execute(SetWallFinishCommand{paintAt->wall, paintAt->side, editSettings.paintFinish}));
		}
		const RoomFinish* current = document.project().findRoomFinish(paintAt->room);
		return toStatus(document.execute(SetRoomFinishCommand{paintAt->room, editSettings.paintFinish, current ? current->ceiling : FinishId()}));
	}

	Status EditController::setPaintFinish(std::string_view finish)
	{
		// Pintar covers wall faces and floors: a finish for either will do.
		const bool known = !finish.empty() && (validateFinishIn(document.project(), finish, Surface::Wall).ok() ||
			validateFinishIn(document.project(), finish, Surface::Floor).ok());
		if (!known)
		{
			return Error{ErrorCode::NotFound, "Unknown finish '" + std::string(finish) + "' for walls or floors"};
		}
		editSettings.paintFinish = std::string(finish);
		if (currentMode == EditMode::Paint && pointerRay)
		{
			paintAt = resolvePaintTarget(levelProject(), *pointerRay);
		}
		return Status::success();
	}

	const std::string& EditController::selectedRoom() const
	{
		static const std::string none;
		return currentMode == EditMode::Select ? selectedRoomKey : none;
	}

	Status EditController::selectRoom(const std::string& room)
	{
		setMode(EditMode::Select);
		clearSelection();
		selectedRoomKey = room;
		return Status::success();
	}

	Status EditController::setRoomFinishes(std::string_view floor, std::string_view ceiling)
	{
		if (selectedRoom().empty())
		{
			return nothingSelected();
		}
		return toStatus(document.execute(SetRoomFinishCommand{selectedRoomKey, std::string(floor), std::string(ceiling)}));
	}

	Status EditController::setSelectedRoomRoof(bool translucent)
	{
		if (selectedRoom().empty()) return nothingSelected();
		const auto* existing = document.project().findRoomRoof(selectedRoomKey);
		return toStatus(document.execute(SetRoomRoofCommand{existing ? *existing : RoomRoof{selectedRoomKey}, translucent}));
	}

	Status EditController::setSelectedRoofRise(std::string_view text)
	{
		if (selectedRoom().empty()) return nothingSelected();
		const auto* current = document.project().findRoomRoof(selectedRoomKey);
		if (!current) return Error{ErrorCode::InvalidArgument, "Select a translucent roof first"};
		const auto value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value) return value.error();
		RoomRoof roof = *current; roof.riseMm = value.value();
		return toStatus(document.execute(SetRoomRoofCommand{roof}));
	}

	Status EditController::rotateSelectedRoof()
	{
		if (selectedRoom().empty()) return nothingSelected();
		const auto* current = document.project().findRoomRoof(selectedRoomKey);
		if (!current) return Error{ErrorCode::InvalidArgument, "Select a translucent roof first"};
		RoomRoof roof = *current; roof.fallQuarterTurns = (roof.fallQuarterTurns + 1) % 4;
		return toStatus(document.execute(SetRoomRoofCommand{roof}));
	}

	Status EditController::editRoomBaseboard(const std::function<void(RoomBaseboard&)>& change)
	{
		if (selectedRoom().empty()) return nothingSelected();
		const RoomBaseboard* current = document.project().findRoomBaseboard(selectedRoomKey);
		RoomBaseboard entry = current ? *current : RoomBaseboard{selectedRoomKey};
		change(entry);
		return toStatus(document.execute(SetRoomBaseboardCommand{entry}));
	}

	Status EditController::setSelectedRoomBaseboard(bool enabled)
	{
		if (selectedRoom().empty()) return nothingSelected();
		if (!enabled && !document.project().findRoomBaseboard(selectedRoom())) return Status::success();
		return editRoomBaseboard([enabled](RoomBaseboard& entry) { entry.enabled = enabled; });
	}

	Status EditController::setBaseboardDimension(std::string_view text, bool height)
	{
		const auto value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value) return value.error();
		return editRoomBaseboard([&](RoomBaseboard& entry) { (height ? entry.heightMm : entry.thicknessMm) = value.value(); });
	}

	Status EditController::setSelectedBaseboardHeight(std::string_view text) { return setBaseboardDimension(text, true); }
	Status EditController::setSelectedBaseboardThickness(std::string_view text) { return setBaseboardDimension(text, false); }
	Status EditController::setSelectedBaseboardFinish(std::string_view finish)
	{
		return editRoomBaseboard([finish](RoomBaseboard& entry) { entry.finish = std::string(finish); });
	}

	Status EditController::setSelectedRoomName(std::string_view name)
	{
		if (selectedRoom().empty())
		{
			return nothingSelected();
		}
		return toStatus(document.execute(SetRoomNameCommand{selectedRoomKey, std::string(name)}));
	}

	Status EditController::setSelectedRoomFloor(std::string_view finish)
	{
		const RoomFinish* current = document.project().findRoomFinish(selectedRoomKey);
		return setRoomFinishes(finish, current ? std::string_view(current->ceiling) : std::string_view());
	}

	Status EditController::setSelectedRoomCeiling(std::string_view finish)
	{
		const RoomFinish* current = document.project().findRoomFinish(selectedRoomKey);
		return setRoomFinishes(current ? std::string_view(current->floor) : std::string_view(), finish);
	}

	RoomLabel EditController::selectedRoomLabel() const
	{
		if (selectedRoom().empty()) return RoomLabel{};
		const RoomLabel* stored = document.project().findRoomLabel(selectedRoomKey);
		return stored ? *stored : RoomLabel{selectedRoomKey};
	}

	Status EditController::editRoomLabel(const std::function<void(RoomLabel&)>& change)
	{
		if (selectedRoom().empty()) return nothingSelected();
		RoomLabel entry = selectedRoomLabel();
		change(entry);
		return toStatus(document.execute(SetRoomLabelCommand{entry}));
	}

	Status EditController::setRoomTextHeight(std::string_view text, bool area)
	{
		const auto value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value) return value.error();
		return editRoomLabel([&](RoomLabel& entry) { (area ? entry.areaHeightMm : entry.nameHeightMm) = value.value(); });
	}

	Status EditController::setSelectedRoomNameHeight(std::string_view text) { return setRoomTextHeight(text, false); }
	Status EditController::setSelectedRoomAreaHeight(std::string_view text) { return setRoomTextHeight(text, true); }

	Status EditController::resetSelectedRoomTextPositions()
	{
		return editRoomLabel([](RoomLabel& entry)
		{
			entry.nameOffset.reset();
			entry.areaOffset.reset();
		});
	}

	std::optional<RoomText> EditController::roomTextAt(PlanPoint point) const
	{
		if (!roomTexts) return std::nullopt;
		const std::vector<RoomText> texts = roomTexts(document.project(), activeLevel());
		// The last one is drawn on top. A quarter of its height around it still grabs it, as the letters reach past the
		// height of their capitals.
		for (auto text = texts.rbegin(); text != texts.rend(); ++text)
		{
			if (text->contains(point, text->heightMm * 0.25)) return *text;
		}
		return std::nullopt;
	}

	void EditController::updateRoomTextDrag()
	{
		RoomTextDrag& moving = *roomTextDrag;
		const PlanPoint delta{planPoint->x_mm - moving.grab.x_mm, planPoint->y_mm - moving.grab.y_mm};
		if (!moving.moved && std::hypot(delta.x_mm, delta.y_mm) < moving.startMm)
		{
			return; // a click that only shook: the text stays automatic
		}
		moving.moved = true;
		const auto component = [](double mm)
		{
			const double limit = static_cast<double>(MaxRoomTextOffsetMm);
			return static_cast<std::int64_t>(std::llround(std::clamp(mm, -limit, limit)));
		};
		const Vector2D offset{component(moving.centre.x_mm + delta.x_mm - moving.anchor.x_mm),
			component(moving.centre.y_mm + delta.y_mm - moving.anchor.y_mm)};
		(moving.area ? moving.label.areaOffset : moving.label.nameOffset) = offset;
	}

	Status EditController::setWallFinish(WallSide side, std::string_view finish)
	{
		const WallId current = selection();
		if (current.isNil())
		{
			return nothingSelected();
		}
		return toStatus(document.execute(SetWallFinishCommand{current, side, std::string(finish)}));
	}

	void EditController::changeLevel(const LevelId& level)
	{
		if (level == activeLevel())
		{
			return;
		}
		clearSelection();
		placement.reset();
		furnitureAt.reset();
		columnAt.reset();
		runDraft.clear();
		dimensions.clearDraft();
		runCursor.reset();
		tool.setLevel(level);
		planPoint.reset(); // it was measured on the other level; the next pointer move sets it again
		pointerRay.reset();
	}

	Status EditController::setActiveLevel(const LevelId& level)
	{
		if (!document.project().findLevel(level))
		{
			return Error{ErrorCode::NotFound, "No level with id " + toString(level)};
		}
		changeLevel(level);
		return Status::success();
	}

	Status EditController::levelUp()
	{
		const Project& project = document.project();
		const std::size_t index = project.levelIndex(activeLevel()).value_or(0);
		if (index + 1 >= project.levels.size())
		{
			return Error{ErrorCode::OutOfRange, "This is the top level"};
		}
		changeLevel(project.levels[index + 1].id);
		return Status::success();
	}

	Status EditController::levelDown()
	{
		const Project& project = document.project();
		const std::size_t index = project.levelIndex(activeLevel()).value_or(0);
		if (index == 0)
		{
			return Error{ErrorCode::OutOfRange, "This is the lowest level"};
		}
		changeLevel(project.levels[index - 1].id);
		return Status::success();
	}

	Status EditController::addLevel()
	{
		// Like the level below it: same height and slab.
		const Level& top = document.project().levels.back();
		const Result<Transaction> added = document.execute(AddLevelCommand{top.heightMm, top.slabThicknessMm, {}});
		if (!added)
		{
			return added.error();
		}
		changeLevel(added.value().levels.front().id);
		return Status::success();
	}

	Status EditController::deleteActiveLevel()
	{
		const Project& project = document.project();
		const std::size_t index = project.levelIndex(activeLevel()).value_or(0);
		const LevelId below = index > 0 ? project.levels[index - 1].id : LevelId();
		const Status deleted = toStatus(document.execute(DeleteLevelCommand{activeLevel()}));
		if (deleted)
		{
			changeLevel(below.isNil() ? document.project().levels.front().id : below);
		}
		return deleted;
	}

	Status EditController::changeActiveLevel(const std::function<Status(Level&)>& edit)
	{
		const Level* current = document.project().findLevel(activeLevel());
		F101_ASSERT(current != nullptr);
		Level changed = *current;
		if (Status edited = edit(changed); !edited)
		{
			return edited;
		}
		return toStatus(document.execute(ChangeLevelCommand{changed}));
	}

	Status EditController::setLevelHeight(std::string_view text)
	{
		const Result<std::int64_t> value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value)
		{
			return value.error();
		}
		return changeActiveLevel([&value](Level& level) { level.heightMm = value.value(); return Status::success(); });
	}

	Status EditController::setLevelSlabThickness(std::string_view text)
	{
		const Result<std::int64_t> value = parseLength(text, editSettings.wallTool.inputUnit);
		if (!value)
		{
			return value.error();
		}
		return changeActiveLevel([&value](Level& level) { level.slabThicknessMm = value.value(); return Status::success(); });
	}

	Status EditController::setLevelName(std::string_view text)
	{
		return changeActiveLevel([text](Level& level) { level.name = std::string(text); return Status::success(); });
	}

	EditView EditController::view() const
	{
		EditView result;
		result.activeLevel = activeLevel();
		result.showAllLevels = allLevels;
		result.selectedRoom = selectedRoom();
		result.showServices = servicesShown;
		result.serviceKinds = serviceKindsShown;
		result.showInteriorDimensions = interiorDimensionsShown;
		if (roomTextDrag && roomTextDrag->moved)
		{
			result.previewRoomLabel = roomTextDrag->label;
		}
		const Project& project = document.project();
		if (currentMode == EditMode::Column)
		{
			if (columnAt)
			{
				result.previewColumns.push_back(*columnAt);
			}
			return result;
		}
		if (currentMode == EditMode::Services)
		{
			if (const std::optional<ServiceRun> run = runPreview())
			{
				result.previewServiceRuns.push_back(*run);
			}
			if (runCursor)
			{
				result.handles.push_back(Point2D{runCursor->x_mm, runCursor->y_mm});
			}
			return result;
		}
		if (const std::optional<Column> column = selectedColumn())
		{
			result.selectedColumn = column->id;
			// While it is dragged, it is previewed where it would go.
			if (columnDrag && columnDrag->column.id == column->id && !(columnDrag->column == *column))
			{
				result.previewColumns.push_back(columnDrag->column);
			}
			return result;
		}
		if (const std::optional<ServiceRun> run = selectedServiceRun())
		{
			result.selectedServiceRun = run->id;
			// While it is dragged, it is previewed where it would go; a handle at each bend.
			const ServiceRun& shown = runDrag && runDrag->run.id == run->id ? runDrag->run : *run;
			if (!(shown == *run))
			{
				result.previewServiceRuns.push_back(shown);
			}
			for (std::size_t index = 0; index < shown.points.size(); ++index)
			{
				if (index == 0 || !sameSpot(shown.points[index - 1], shown.points[index]))
				{
					result.handles.push_back(Point2D{shown.points[index].x_mm, shown.points[index].y_mm});
				}
			}
			return result;
		}
		if (currentMode == EditMode::Wall)
		{
			if (const std::optional<Wall> preview = tool.preview())
			{
				result.previewWalls.push_back(*preview);
			}
			if (planPoint)
			{
				result.handles.push_back(tool.hoverPoint());
			}
			return result;
		}
		if (isOpeningMode())
		{
			const Wall* host = placement ? project.findWall(placement->wall) : nullptr;
			if (host)
			{
				Wall withOpening = *host;
				withOpening.openings.push_back(placement->opening);
				std::stable_sort(withOpening.openings.begin(), withOpening.openings.end(),
					[](const Opening& a, const Opening& b) { return a.offsetMm < b.offsetMm; });
				if (validateWallGeometry(withOpening).ok()) // not where it would overlap another opening
				{
					result.previewWalls.push_back(withOpening);
				}
				result.handles = {pointOnAxis(*host, placement->opening.offsetMm),
					pointOnAxis(*host, placement->opening.offsetMm + placement->opening.widthMm)};
			}
			return result;
		}
		if (currentMode == EditMode::Furniture)
		{
			if (furnitureAt)
			{
				result.previewFurniture.push_back(*furnitureAt);
			}
			return result;
		}
		if (const std::optional<Furniture> piece = selectedFurniture())
		{
			result.selectedFurniture = piece->id;
			// While it is dragged, it is previewed where it would go.
			if (furnitureDrag && furnitureDrag->furniture.id == piece->id && !(furnitureDrag->furniture == *piece))
			{
				result.previewFurniture.push_back(furnitureDrag->furniture);
			}
			return result;
		}

		if (const std::optional<OpeningRef> opening = selectedOpening())
		{
			const Wall& host = *project.findWall(opening->wall);
			result.selectedWall = host.id;
			// While it is dragged, the wall is previewed with the opening where it would go.
			const Opening shown = openingDrag && openingDrag->opening.id == opening->opening.id ? openingDrag->opening : opening->opening;
			if (!(shown == opening->opening))
			{
				Wall moved = host;
				*std::find_if(moved.openings.begin(), moved.openings.end(), [&shown](const Opening& o) { return o.id == shown.id; }) = shown;
				std::stable_sort(moved.openings.begin(), moved.openings.end(),
					[](const Opening& a, const Opening& b) { return a.offsetMm < b.offsetMm; });
				if (validateWallGeometry(moved).ok())
				{
					result.previewWalls.push_back(moved);
				}
			}
			result.handles = {pointOnAxis(host, shown.offsetMm), pointOnAxis(host, shown.offsetMm + shown.widthMm)};
			return result;
		}
		result.selectedWall = selection();
		const Wall* wall = result.selectedWall.isNil() ? nullptr : project.findWall(result.selectedWall);
		if (!wall)
		{
			return result;
		}
		result.handles = {wall->start, wall->end};
		if (wallBodyDrag && wallBodyDrag->wall.id == result.selectedWall)
		{
			const Vector2D delta = wallBodyDrag->delta;
			for (const WallId& id : wallBodyDrag->joined)
			{
				if (const Wall* joined = project.findWall(id))
				{
					Wall moved = *joined;
					for (Point2D* point : {&moved.start, &moved.end})
					{
						if (*point == wallBodyDrag->wall.start || *point == wallBodyDrag->wall.end)
						{
							point->x_mm += delta.dx_mm;
							point->y_mm += delta.dy_mm;
						}
					}
					result.previewWalls.push_back(moved);
				}
			}
			for (Point2D& point : result.handles)
			{
				point.x_mm += delta.dx_mm;
				point.y_mm += delta.dy_mm;
			}
		}
		if (drag && drag->wall == result.selectedWall)
		{
			for (const WallId& id : drag->joined)
			{
				if (const Wall* joinedWall = project.findWall(id))
				{
					Wall moved = *joinedWall;
					(moved.start == drag->origin ? moved.start : moved.end) = drag->position;
					result.previewWalls.push_back(moved);
				}
			}
			(drag->endpoint == WallEndpoint::Start ? result.handles[0] : result.handles[1]) = drag->position;
		}
		return result;
	}
}
