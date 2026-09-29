#include "F101/Cad/EditController.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
		tool.cancel();
		placement.reset();
		if (mode != EditMode::Select)
		{
			selected = WallId();
			selectedOpeningId = OpeningId();
		}
		currentMode = mode;
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
		}
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
		else if (drag)
		{
			updateDrag();
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
		const auto length = host ? static_cast<std::int64_t>(std::floor(host->lengthMm())) : 0;
		if (!host || length < defaults.widthMm)
		{
			return;
		}
		// Centred on the pointer, its edge on the snap grid, inside the wall.
		const std::int64_t grid = editSettings.wallTool.snap.gridMm > 0 ? editSettings.wallTool.snap.gridMm : 1;
		const double edge = hostAlong - static_cast<double>(defaults.widthMm) / 2.0;
		const std::int64_t offset = std::llround(edge / static_cast<double>(grid)) * grid;
		Opening opening = defaults;
		opening.id = OpeningId();
		opening.offsetMm = std::clamp<std::int64_t>(offset, 0, length - defaults.widthMm);
		placement = OpeningRef{host->id, opening};
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

		// A wall, or the opening in it where the pointer is along the wall.
		selected = WallId();
		selectedOpeningId = OpeningId();
		if (const std::optional<WallHit> hit = pickWall(project, ray))
		{
			selected = hit->wall;
			const Wall& wall = *project.findWall(hit->wall);
			const PlanPoint where{ray.originX + ray.directionX * hit->distance, ray.originY + ray.directionY * hit->distance};
			if (const Opening* opening = openingAt(wall, locate(wall, where).along))
			{
				selectedOpeningId = opening->id;
			}
		}
		return Status::success();
	}

	Status EditController::pointerReleased(const Ray3&)
	{
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
		if (drag)
		{
			drag.reset();
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
			selected = WallId();
			selectedOpeningId = OpeningId();
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
		tool.cancel();
		const Status undone = document.undo();
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
		}
		return undone;
	}

	Status EditController::redo()
	{
		drag.reset();
		tool.cancel();
		const Status redone = document.redo();
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
		}
		return redone;
	}

	void EditController::reset()
	{
		drag.reset();
		tool.cancel();
		selected = WallId();
		selectedOpeningId = OpeningId();
		if (planPoint)
		{
			tool.pointerMoved(*planPoint);
			updatePlacement();
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
		drag.reset();
		selected = id;
		selectedOpeningId = OpeningId();
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
		drag.reset();
		selected = wall;
		selectedOpeningId = opening;
		return Status::success();
	}

	void EditController::clearSelection()
	{
		drag.reset();
		selected = WallId();
		selectedOpeningId = OpeningId();
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

	void EditController::changeLevel(const LevelId& level)
	{
		if (level == activeLevel())
		{
			return;
		}
		drag.reset();
		placement.reset();
		selected = WallId();
		selectedOpeningId = OpeningId();
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

		if (const std::optional<OpeningRef> opening = selectedOpening())
		{
			const Wall& host = *project.findWall(opening->wall);
			result.selectedWall = host.id;
			result.handles = {pointOnAxis(host, opening->opening.offsetMm),
				pointOnAxis(host, opening->opening.offsetMm + opening->opening.widthMm)};
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
