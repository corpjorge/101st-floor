#pragma once

#include "F101/Cad/Document.h"
#include "F101/Cad/Export.h"
#include "F101/Cad/Picking.h"
#include "F101/Cad/WallTool.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace f101::cad
{
	enum class EditMode
	{
		Select,
		Wall,
		Door,
		Window,
		Paint, // applies EditSettings::paintFinish to a wall face or a room floor (ADR-0012)
	};

	struct EditSettings
	{
		WallToolSettings wallTool;
		// Draggable endpoint handles (and the wall tool cursor) are square posts of this size, drawn and picked alike.
		std::int64_t handleSizeMm = 240;
		std::int64_t handleHeightMm = 2500;
		// What the Door and Window tools place (offset ignored), and how far from a wall's face the pointer still
		// targets it.
		Opening door{OpeningId{}, OpeningKind::Door, 0, 900, 2100, 0};
		Opening window{OpeningId{}, OpeningKind::Window, 0, 1200, 1200, 900};
		double openingToleranceMm = 150.0;
		// What the Paint mode applies.
		FinishId paintFinish = "paint-sand";
	};

	// The room whose floor contains a plan point, among one level's walls: its key (geometry::Room::key), or nullopt.
	// Rooms are derived in the Geometry Engine, which provides one (geometry::makeRoomLocator); without it rooms cannot be
	// picked.
	using RoomLocator = std::function<std::optional<std::string>(const Project& levelWalls, PlanPoint point)>;

	// A door or window with its host wall.
	struct OpeningRef
	{
		WallId wall;
		Opening opening;
	};

	// What the view shows on top of the project. Not saved; the application maps it to scene::ViewOverlay.
	struct EditView
	{
		LevelId activeLevel;        // the level being edited
		bool showAllLevels = false; // false: the levels above are hidden and the active one has no ceiling
		std::string selectedRoom;   // room key; empty: none
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
	// Door and Window modes: the pointer over a wall previews the opening centred on it (offset on the snap grid);
	// a click adds it. In Select mode a click on a wall where it has an opening selects that opening.
	// Paint mode: a click on a wall paints the face it hits (the side of the axis); elsewhere, the floor of the room under
	// the pointer. In Select mode a click on a room floor (not on a wall) selects the room.
	// Levels (ADR-0011): everything happens on the active level. Pointer rays are taken to that level's elevation, and
	// picking, snapping, handles and corner moves only see its walls. Changing level drops the selection and any gesture.
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

		// The selected door or window; nullopt if none or if it no longer exists. While one is selected, selection()
		// (the selected wall) is nil.
		FLOOR101CADCORE_API std::optional<OpeningRef> selectedOpening() const;
		FLOOR101CADCORE_API Status selectOpening(const WallId& wall, const OpeningId& opening);
		// Property edits of the selected opening, typed like lengths. The offset runs from the wall start to its edge.
		FLOOR101CADCORE_API Status setOpeningWidth(std::string_view text);
		FLOOR101CADCORE_API Status setOpeningHeight(std::string_view text);
		FLOOR101CADCORE_API Status setOpeningSill(std::string_view text);
		FLOOR101CADCORE_API Status setOpeningOffset(std::string_view text);
		// Door/Window mode: what a click would add (id nil); nullopt when the pointer is not over a wall that fits it.
		const std::optional<OpeningRef>& openingPlacement() const { return placement; }

		const WallTool& wallTool() const { return tool; }
		const EditSettings& settings() const { return editSettings; }
		bool isDragging() const { return drag.has_value(); }
		// Last cursor position in plan: under the point where the pointer ray meets a wall, else where it meets the floor;
		// nullopt when it meets neither (e.g. looking at the sky).
		const std::optional<PlanPoint>& pointerOnPlan() const { return planPoint; }

		// Finishes (ADR-0012).
		void setRoomLocator(RoomLocator locator) { roomLocator = std::move(locator); }
		const FinishId& paintFinish() const { return editSettings.paintFinish; }
		FLOOR101CADCORE_API Status setPaintFinish(std::string_view finish);
		// Selected room key (Select mode); empty if none. Whether the room still exists is up to the rooms of the scene.
		FLOOR101CADCORE_API const std::string& selectedRoom() const;
		FLOOR101CADCORE_API Status selectRoom(const std::string& room);
		FLOOR101CADCORE_API Status setSelectedRoomFloor(std::string_view finish);
		FLOOR101CADCORE_API Status setSelectedRoomCeiling(std::string_view finish);
		// A face of the selected wall; an empty finish restores the default.
		FLOOR101CADCORE_API Status setWallFinish(WallSide side, std::string_view finish);

		// The level being edited: the chosen one while it exists (an undo can remove it), else the first.
		FLOOR101CADCORE_API LevelId activeLevel() const;
		FLOOR101CADCORE_API Status setActiveLevel(const LevelId& level);
		FLOOR101CADCORE_API Status levelUp();
		FLOOR101CADCORE_API Status levelDown();
		// Adds a level on top, as tall as the current top one with the same slab, and makes it active.
		FLOOR101CADCORE_API Status addLevel();
		// Deletes the active level with its walls (one undoable step); the level below becomes active.
		FLOOR101CADCORE_API Status deleteActiveLevel();
		// Property edits of the active level; lengths typed like the wall tool input.
		FLOOR101CADCORE_API Status setLevelHeight(std::string_view text);
		FLOOR101CADCORE_API Status setLevelSlabThickness(std::string_view text);
		FLOOR101CADCORE_API Status setLevelName(std::string_view text);
		bool showsAllLevels() const { return allLevels; }
		void setShowAllLevels(bool show) { allLevels = show; }

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
		// Parses `text` as a length and applies it to a copy of the selected opening, then changes the opening.
		Status changeOpening(std::string_view text, void (*apply)(Opening&, std::int64_t));
		void updateDrag();
		void updatePlacement();
		bool isOpeningMode() const { return currentMode == EditMode::Door || currentMode == EditMode::Window; }
		std::optional<PlanPoint> planPointOf(const Ray3& ray, const std::vector<WallId>& ignoreWalls) const;
		// Pointer handling with a ray already in the active level's space.
		void movePointer(const Ray3& ray);
		// The scene ray moved down to the active level (its floor at z = 0).
		Ray3 toLevel(const Ray3& ray) const;
		// The project with only the active level's walls.
		Project levelProject() const;
		void changeLevel(const LevelId& level);
		Status paint(const Project& levelWalls, const Ray3& ray);
		Status setRoomFinishes(std::string_view floor, std::string_view ceiling);
		Status changeActiveLevel(const std::function<Status(Level&)>& edit);

		Document& document;
		EditSettings editSettings;
		WallTool tool;
		EditMode currentMode = EditMode::Select;
		WallId selected;
		OpeningId selectedOpeningId; // with `selected` as its wall
		std::optional<EndpointDrag> drag;
		std::optional<OpeningRef> placement;
		std::optional<PlanPoint> planPoint;
		bool allLevels = false;
		std::string selectedRoomKey;
		RoomLocator roomLocator;
	};
}
