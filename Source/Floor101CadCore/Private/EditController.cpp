#include "F101/Cad/EditController.h"

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
		if (mode == EditMode::Wall)
		{
			selected = WallId();
		}
		currentMode = mode;
		if (floorPoint)
		{
			tool.pointerMoved(*floorPoint);
		}
	}

	void EditController::pointerMoved(const Ray3& ray)
	{
		floorPoint = intersectFloor(ray);
		if (!floorPoint)
		{
			return; // off the floor: keep the last preview
		}
		if (currentMode == EditMode::Wall)
		{
			tool.pointerMoved(*floorPoint);
		}
		else if (drag)
		{
			updateDrag();
		}
	}

	void EditController::updateDrag()
	{
		const Project& project = document.project();
		const Wall* wall = project.findWall(drag->wall);
		if (!wall)
		{
			drag.reset(); // the wall disappeared under the gesture
			return;
		}
		// Angle and length snap relative to the fixed end; other walls' endpoints attract.
		const Point2D fixed = endpointOf(*wall, opposite(drag->endpoint));
		drag->position = snapFromAnchor(project, fixed, *floorPoint, editSettings.wallTool.snap, drag->wall).point;
	}

	Status EditController::pointerPressed(const Ray3& ray)
	{
		pointerMoved(ray);
		if (currentMode == EditMode::Wall)
		{
			if (!floorPoint)
			{
				return Error{ErrorCode::InvalidArgument, "Point at the floor to draw"};
			}
			const Result<std::optional<WallId>> clicked = tool.click(*floorPoint);
			return clicked ? Status::success() : Status(clicked.error());
		}

		// A handle of the selected wall starts a drag. The endpoint only moves once the pointer moves.
		const WallId current = selection();
		if (const Wall* wall = current.isNil() ? nullptr : document.project().findWall(current))
		{
			std::optional<double> nearest;
			for (const WallEndpoint endpoint : {WallEndpoint::Start, WallEndpoint::End})
			{
				const std::optional<double> hit = intersectPost(ray, endpointOf(*wall, endpoint),
					static_cast<double>(editSettings.handleSizeMm), static_cast<double>(editSettings.handleHeightMm));
				if (hit && (!nearest || *hit < *nearest))
				{
					nearest = hit;
					drag = EndpointDrag{current, endpoint, endpointOf(*wall, endpoint)};
				}
			}
			if (drag)
			{
				return Status::success();
			}
		}

		const std::optional<WallHit> hit = pickWall(document.project(), ray);
		selected = hit ? hit->wall : WallId();
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
		else if (currentMode == EditMode::Wall)
		{
			if (tool.state() == WallTool::State::Drafting)
			{
				tool.cancel();
			}
			else
			{
				setMode(EditMode::Select);
			}
		}
		else
		{
			selected = WallId();
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
		const WallId current = selection();
		if (current.isNil())
		{
			return nothingSelected();
		}
		drag.reset();
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
		if (floorPoint)
		{
			tool.pointerMoved(*floorPoint);
		}
		return undone;
	}

	Status EditController::redo()
	{
		drag.reset();
		tool.cancel();
		const Status redone = document.redo();
		if (floorPoint)
		{
			tool.pointerMoved(*floorPoint);
		}
		return redone;
	}

	void EditController::reset()
	{
		drag.reset();
		tool.cancel();
		selected = WallId();
		if (floorPoint)
		{
			tool.pointerMoved(*floorPoint);
		}
	}

	WallId EditController::selection() const
	{
		return !selected.isNil() && document.project().findWall(selected) ? selected : WallId();
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
		drag.reset();
		selected = id;
		return Status::success();
	}

	void EditController::clearSelection()
	{
		drag.reset();
		selected = WallId();
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

	EditView EditController::view() const
	{
		EditView result;
		const Project& project = document.project();
		if (currentMode == EditMode::Wall)
		{
			result.preview = tool.preview();
			if (floorPoint)
			{
				result.handles.push_back(tool.hoverPoint());
			}
			return result;
		}

		result.selectedWall = selection();
		const Wall* wall = result.selectedWall.isNil() ? nullptr : project.findWall(result.selectedWall);
		if (!wall)
		{
			return result;
		}
		if (drag && drag->wall == result.selectedWall)
		{
			Wall moved = *wall;
			(drag->endpoint == WallEndpoint::Start ? moved.start : moved.end) = drag->position;
			result.hiddenWall = wall->id;
			result.preview = moved;
			result.handles = {moved.start, moved.end};
		}
		else
		{
			result.handles = {wall->start, wall->end};
		}
		return result;
	}
}
