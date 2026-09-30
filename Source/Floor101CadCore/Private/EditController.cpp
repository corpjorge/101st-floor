#include "F101/Cad/EditController.h"

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
	}

	EditController::EditController(Document& targetDocument, EditSettings settings)
		: document(targetDocument)
		, editSettings(std::move(settings))
		, tool(targetDocument, editSettings.wallTool)
	{
	}

	void EditController::setMode(EditMode mode)
	{
		if (mode == currentMode)
		{
			return;
		}
		drag.reset();
		openingDrag.reset();
		furnitureDrag.reset();
		tool.cancel();
		placement.reset();
		furnitureAt.reset();
		if (mode != EditMode::Select)
		{
			clearSelected();
		}
		currentMode = mode;
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
			updateFurniturePlacement();
		}
	}

	void EditController::clearSelected()
	{
		selected = WallId();
		selectedOpeningId = OpeningId();
		selectedRoomKey.clear();
		selectedFurnitureId = FurnitureId();
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
		planPoint = planPointOf(ray, drag ? drag->joined : std::vector<WallId>{});
		if (!planPoint)
		{
			return; // off the floor: keep the last preview
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
		else if (drag)
		{
			updateDrag();
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

		// A piece of furniture in front of any wall: selected, and grabbed (it moves once the pointer moves).
		clearSelected();
		const std::optional<WallHit> wallHit = pickWall(project, ray);
		const std::optional<FurnitureHit> pieceHit = pickFurniture(document.project(), ray, activeLevel());
		if (pieceHit && (!wallHit || pieceHit->distance < wallHit->distance))
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
		}
		else if (planPoint && roomLocator)
		{
			selectedRoomKey = roomLocator(project, *planPoint).value_or(std::string());
		}
		return Status::success();
	}

	Status EditController::pointerReleased(const Ray3&)
	{
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
		if (currentMode != EditMode::Wall)
		{
			return Status::success();
		}
		const Result<WallId> created = tool.confirm();
		return created ? Status::success() : Status(created.error());
	}

	void EditController::cancel()
	{
		if (drag || openingDrag || furnitureDrag)
		{
			drag.reset();
			openingDrag.reset();
			furnitureDrag.reset();
		}
		else if (currentMode == EditMode::Wall && tool.state() == WallTool::State::Drafting)
		{
			tool.cancel();
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
	}

	Status EditController::deleteSelection()
	{
		drag.reset();
		openingDrag.reset();
		furnitureDrag.reset();
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
		drag.reset();
		openingDrag.reset();
		furnitureDrag.reset();
		tool.cancel();
		const Status undone = document.undo();
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
			updateFurniturePlacement();
		}
		return undone;
	}

	Status EditController::redo()
	{
		drag.reset();
		openingDrag.reset();
		furnitureDrag.reset();
		tool.cancel();
		const Status redone = document.redo();
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
			updateFurniturePlacement();
		}
		return redone;
	}

	void EditController::reset()
	{
		drag.reset();
		openingDrag.reset();
		furnitureDrag.reset();
		tool.cancel();
		clearSelected();
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
			updateFurniturePlacement();
		}
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
		drag.reset();
		openingDrag.reset();
		furnitureDrag.reset();
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

	Status EditController::setOpeningFinish(std::string_view finish)
	{
		return editSelectedOpening([finish](Opening& opening) { opening.finish = std::string(finish); });
	}

	Status EditController::setOpeningOperation(OpeningOperation operation)
	{
		return editSelectedOpening([operation](Opening& opening)
		{
			opening.operation = operation;
			if (operation == OpeningOperation::Fixed)
			{
				opening.openPercent = 0;
			}
		});
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
		if (selectedFurniture())
		{
			return rotateFurniture();
		}
		if (!selectedOpening())
		{
			return Error{ErrorCode::NotFound,
				"Select a piece of furniture, a door or a window, or pick the door, window or furniture tool, to turn it"};
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

	Status EditController::paint(const Project& project, const Ray3& ray)
	{
		// The face of the wall under the pointer (the side of its axis the ray meets it on), else the room floor.
		if (const std::optional<WallHit> hit = pickWall(project, ray))
		{
			const Wall& wall = *project.findWall(hit->wall);
			const WallSide side = sideOf(wall, PlanPoint{ray.originX + ray.directionX * hit->distance, ray.originY + ray.directionY * hit->distance});
			return toStatus(document.execute(SetWallFinishCommand{wall.id, side, editSettings.paintFinish}));
		}
		const std::optional<std::string> room = planPoint && roomLocator ? roomLocator(project, *planPoint) : std::nullopt;
		if (!room)
		{
			return Error{ErrorCode::InvalidArgument, "Point at a wall or at the floor of a room"};
		}
		const RoomFinish* current = document.project().findRoomFinish(*room);
		return toStatus(document.execute(SetRoomFinishCommand{*room, editSettings.paintFinish, current ? current->ceiling : FinishId()}));
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
		tool.setLevel(level);
		planPoint.reset(); // it was measured on the other level; the next pointer move sets it again
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
		const Project& project = document.project();
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
