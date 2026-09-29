#pragma once

#include "F101/Cad/Document.h"
#include "F101/Cad/Export.h"
#include "F101/Cad/Picking.h"
#include "F101/Cad/WallTool.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace f101::cad
{
	enum class EditMode
	{
		Select,
		Wall,
	};

	struct EditSettings
	{
		WallToolSettings wallTool;
		// Draggable endpoint handles (and the wall tool cursor) are square posts of this size, drawn and picked alike.
		std::int64_t handleSizeMm = 240;
		std::int64_t handleHeightMm = 2500;
	};

	// What the view shows on top of the project. Not saved; the application maps it to scene::ViewOverlay.
	struct EditView
	{
		WallId selectedWall; // nil: none
		// Walls as the gesture would leave them: the wall being drawn (nil id), or every wall at a dragged corner
		// (their own ids). The application shows the project as if they were applied.
		std::vector<Wall> previewWalls;
		std::vector<Point2D> handles;
	};

	// Interactive editing of a document, independent of any engine (ADR-0008). The engine translates its input into
	// rays in scene space (mm, right-handed, Z up) and keys into these calls, then shows view(). Every model change is
	// one undoable command; a failed call leaves the document unchanged and returns why.
	//
	// Select mode: a click picks a wall (or clears the selection); dragging a handle of the selected wall moves that
	// corner: every wall ending there follows (MoveJointCommand on release; MoveWallEndpointCommand for a free end).
	// Wall mode: WallTool (click, move, type a length, Enter).
	class EditController
	{
	public:
		FLOOR101CADCORE_API EditController(Document& document, EditSettings settings);

		EditMode mode() const { return currentMode; }
		// Cancels any wall being drawn or endpoint being dragged.
		FLOOR101CADCORE_API void setMode(EditMode mode);

		FLOOR101CADCORE_API void pointerMoved(const Ray3& ray);
		FLOOR101CADCORE_API Status pointerPressed(const Ray3& ray);
		FLOOR101CADCORE_API Status pointerReleased(const Ray3& ray);

		// Enter: creates the previewed wall (Wall mode).
		FLOOR101CADCORE_API Status confirm();
		// Esc: stops the current gesture, then leaves Wall mode, then clears the selection.
		FLOOR101CADCORE_API void cancel();
		// Typed length of the wall being drawn.
		FLOOR101CADCORE_API void appendText(std::string_view text);
		FLOOR101CADCORE_API void backspace();

		FLOOR101CADCORE_API Status deleteSelection();
		// Cancel the current gesture first, so a tool never continues from undone geometry.
		FLOOR101CADCORE_API Status undo();
		FLOOR101CADCORE_API Status redo();
		// After the project was replaced (e.g. a file was opened): drops the selection and any gesture.
		FLOOR101CADCORE_API void reset();

		// Selected wall; nil if none or if it no longer exists (deleted, undone).
		FLOOR101CADCORE_API WallId selection() const;
		FLOOR101CADCORE_API Status select(const WallId& id);
		FLOOR101CADCORE_API void clearSelection();
		// Property edits of the selected wall, typed like the wall tool input ("355", "3.55 m"...).
		FLOOR101CADCORE_API Status setSelectionLength(std::string_view text);
		FLOOR101CADCORE_API Status setSelectionThickness(std::string_view text);
		FLOOR101CADCORE_API Status setSelectionHeight(std::string_view text);

		const WallTool& wallTool() const { return tool; }
		const EditSettings& settings() const { return editSettings; }
		bool isDragging() const { return drag.has_value(); }
		// Last cursor position in plan: under the point where the pointer ray meets a wall, else where it meets the floor;
		// nullopt when it meets neither (e.g. looking at the sky).
		const std::optional<PlanPoint>& pointerOnPlan() const { return planPoint; }

		FLOOR101CADCORE_API EditView view() const;

	private:
		struct EndpointDrag
		{
			WallId wall;
			WallEndpoint endpoint = WallEndpoint::End;
			Point2D origin;              // where the endpoint was
			Point2D position;            // where it goes
			std::vector<WallId> joined;  // every wall with an end at `origin` (the corner moves as one)
			PlanPoint grabOffset;        // origin minus the pointer at the press: the corner keeps its offset
		};

		// Parses `text` as a length and applies the command built from it to the selected wall.
		Status changeSelection(std::string_view text, Command (*makeCommand)(const WallId&, std::int64_t));
		void updateDrag();
		std::optional<PlanPoint> planPointOf(const Ray3& ray, const std::vector<WallId>& ignoreWalls) const;

		Document& document;
		EditSettings editSettings;
		WallTool tool;
		EditMode currentMode = EditMode::Select;
		WallId selected;
		std::optional<EndpointDrag> drag;
		std::optional<PlanPoint> planPoint;
	};
}
