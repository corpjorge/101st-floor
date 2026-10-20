#pragma once

#include "F101/Cad/DimensionEditor.h"
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
		Furniture, // places pieces of EditSettings::furnitureType (ADR-0016)
		Column,    // places columns of EditSettings::columnShape (ADR-0021)
		Dimension, // plan annotations: two measured endpoints and a placement click
		Services,  // draws runs of EditSettings::serviceKind point by point (ADR-0021)
	};

	struct EditSettings
	{
		WallToolSettings wallTool;
		// Draggable endpoint handles (and the wall tool cursor) are square posts of this size, drawn and picked alike.
		std::int64_t handleSizeMm = 240;
		std::int64_t handleHeightMm = 2500;
		// What the Door and Window tools place (offset ignored; it opens towards the side the pointer is on), and how far
		// from a wall's face the pointer still targets it.
		Opening door{OpeningId{}, OpeningKind::Door, 0, 900, 2100, 0};
		Opening window{OpeningId{}, OpeningKind::Window, 0, 1200, 1200, 900, {}, OpeningOperation::DoubleHinged};
		double openingToleranceMm = 150.0;
		// Dragging an edge of a door or window never makes it narrower than this.
		std::int64_t minOpeningWidthMm = 300;
		// What the Paint mode applies.
		FinishId paintFinish = "paint-sand";
		// What the Furniture mode places, turned by furnitureRotationCentidegrees unless it goes against a wall; a piece that
		// does snaps its back to the face of the nearest wall when its centre is within its depth plus this reach.
		std::string furnitureType = "sofa";
		std::int64_t furnitureRotationCentidegrees = 0;
		double furnitureWallReachMm = 400.0;
		// What the Column mode places (ADR-0021): a column of this shape and size (a round one columnWidthMm across), turned
		// by columnRotationCentidegrees.
		ColumnShape columnShape = ColumnShape::Rectangle;
		std::int64_t columnWidthMm = DefaultColumnSizeMm;
		std::int64_t columnDepthMm = DefaultColumnSizeMm;
		std::int64_t columnRotationCentidegrees = 0;
		// What the Services mode draws: runs of this kind, their next point this high over the level's floor (negative:
		// under it).
		ServiceKind serviceKind = ServiceKind::Electrical;
		std::int64_t serviceHeightMm = 300;
		// How far from a pipe the pointer still picks it.
		double servicePickToleranceMm = 80.0;
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

	// What the Paint tool would change: a face of a wall, or the floor of a room (wall nil).
	struct PaintTarget
	{
		WallId wall;
		WallSide side = WallSide::Left;
		std::string room; // room key, when it is a floor

		friend bool operator==(const PaintTarget&, const PaintTarget&) = default;
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
		FurnitureId selectedFurniture; // nil: none
		// The piece being placed (nil id) or the selected piece where a drag would leave it (its own id).
		std::vector<Furniture> previewFurniture;
		ColumnId selectedColumn; // nil: none
		// The column being placed (nil id) or the selected one where a drag would leave it (its own id).
		std::vector<Column> previewColumns;
		// The building-services layer (ADR-0021): the runs shown, the architecture seen through.
		bool showServices = false;
		// Which networks the layer shows (in 3D, in the plan and in the exported plan).
		ServiceKindSet serviceKinds;
		ServiceRunId selectedServiceRun; // nil: none
		// The run being drawn (nil id) or the selected one where a drag would leave it (its own id).
		std::vector<ServiceRun> previewServiceRuns;
		// The plan's interior dimensions (ADR-0034), in the view and in the exported plan.
		bool showInteriorDimensions = true;
		// The selected room's label layout while one of its texts is dragged, as the drag would leave it (ADR-0035).
		std::optional<RoomLabel> previewRoomLabel;
	};

	// Interactive editing of a document, independent of any engine (ADR-0008). The engine translates its input into
	// rays in scene space (mm, right-handed, Z up) and keys into these calls, then shows view(). Every model change is
	// one undoable command; a failed call leaves the document unchanged and returns why.
	//
	// Select mode: a click picks a wall (or clears the selection); dragging a handle of the selected wall moves that
	// corner: every wall ending there follows (MoveJointCommand on release; MoveWallEndpointCommand for a free end).
	// Dragging its solid body translates both joints together (TranslateWallCommand), preserving its length and angle.
	// Wall mode: WallTool (click, move, type a length, Enter).
	// Door and Window modes: the pointer over a wall previews the opening centred on it (offset on the snap grid) in the
	// free stretch nearest to it, clear of the walls joined to its host and of its other openings (ADR-0013), opening
	// towards the side of the wall the pointer is on (ADR-0014), of the type setNewOpeningOperation chose (a sliding door
	// with room for its pocket); flip() moves its hinges to the other jamb; a click adds it. In Select mode a click on a wall where it has an opening selects that opening; dragging the selected opening moves
	// it along its wall, dragging one of its edge handles resizes it (one ChangeOpeningCommand on release).
	// Paint mode: a click on a wall paints the face it hits (the side of the axis); elsewhere, the floor of the room under
	// the pointer. A finish only for floors looks past the walls, down to the floor. paintTarget() tells what a click would
	// paint. In Select mode a click on a room floor (not on a wall) selects the room.
	// Furniture mode (ADR-0016): the pointer previews a piece of the chosen type centred on it, on the snap grid; a type
	// that goes against walls, near one, turns its back to the nearest wall face and touches it, sliding along it on the
	// grid and stopping at the walls it would run into. Space turns the next pieces 90 degrees; a click adds one. In Select
	// mode a click on a piece (nearer than any wall) selects it, dragging it moves it with the same snapping (one
	// ChangeFurnitureCommand on release), and Space turns it 90 degrees.
	// Column mode (ADR-0021): the pointer previews a column of the chosen shape and size centred on it, snapped to wall
	// corners, then wall axes, then the grid; Space turns the next ones 90 degrees; a click adds one. In Select mode a click
	// on a column (nearer than any wall or piece) selects it, dragging it moves it with the same snapping, Space turns it.
	// Services mode (ADR-0021): it shows the services layer. The pointer, taken to the chosen height, previews the next
	// point, snapped like a wall's ends (from the last point: its angle and length); each click adds a point, Enter, a
	// double click or a second click on the last point finishes the run (one AddServiceRunCommand), Backspace takes the
	// last point back and Esc drops the run. A height changed while drawing rises or drops on the spot from the last
	// point. With the layer
	// shown, in Select mode a click on a run (seen through the walls) selects it: dragging it moves it on the grid,
	// dragging one of the handles at its bends moves that bend. The layer shows the networks chosen
	// (setServiceKindShown); drawing, selecting or retyping a run shows its network.
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
		// The second press of a double click (the first one came as pointerPressed): in Services mode it finishes the run
		// being drawn, wherever snapping would put a next point; elsewhere it is a press.
		FLOOR101CADCORE_API Status pointerDoubleClicked(const Ray3& ray);

		// Enter: creates the previewed wall (Wall mode), or finishes the run being drawn (Services mode).
		FLOOR101CADCORE_API Status confirm();
		// Esc: stops the current gesture (a run being drawn is dropped), then leaves the tool's mode, then clears the
		// selection.
		FLOOR101CADCORE_API void cancel();
		// Typed length of the wall being drawn.
		FLOOR101CADCORE_API void appendText(std::string_view text);
		// The typed length's last character; in Services mode, the last point of the run being drawn.
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
		// Plan annotations (ADR-0031), edited by DimensionEditor: placed in Dimension mode, picked and edited in Select mode,
		// only while the 2D plan is shown.
		using DimensionProvider = DimensionEditor::Provider;
		void setDimensionProvider(DimensionProvider provider) { dimensions.setProvider(std::move(provider)); }
		void setDimensionsVisible(bool visible)
		{
			dimensions.setVisible(visible);
			if (!visible) roomTextDrag.reset();
		}
		void setDimensionPickTolerance(double mm) { dimensions.setPickTolerance(mm); }
		double dimensionHandleSizeMm() const { return dimensions.handleSizeMm(); }
		FLOOR101CADCORE_API std::optional<Dimension> selectedDimension() const;
		FLOOR101CADCORE_API std::optional<Dimension> dimensionPreview() const;
		FLOOR101CADCORE_API Status selectDimension(std::string_view id);
		FLOOR101CADCORE_API Status setDimensionOffset(std::string_view text);
		FLOOR101CADCORE_API Status setDimensionText(std::string_view text);
		FLOOR101CADCORE_API Status setDimensionCoordinate(std::string_view text, int coordinate);
		FLOOR101CADCORE_API Status restoreAutomaticDimensions();
		// The plan's interior dimensions (ADR-0034), shown by default; the provider leaves them out while hidden, so they
		// cannot be picked. Hiding them lets go of a selected one.
		bool showsInteriorDimensions() const { return interiorDimensionsShown; }
		FLOOR101CADCORE_API void setShowInteriorDimensions(bool show);
		std::size_t dimensionPointCount() const { return dimensions.draftPointCount(); }
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
		// Its carpentry (frame and leaf, ADR-0013); empty restores the default.
		FLOOR101CADCORE_API Status setOpeningFinish(std::string_view finish);
		// How it opens (ADR-0014). Fixed closes it. A sliding door needs room for its pocket beside it.
		FLOOR101CADCORE_API Status setOpeningOperation(OpeningOperation operation);
		// Hinges (or where a sliding leaf goes) to the other end; leaves swinging towards the other face.
		FLOOR101CADCORE_API Status flipOpeningHinge();
		FLOOR101CADCORE_API Status flipOpeningSide();
		// How open it is shown, typed as a whole percentage ("35", "35 %").
		FLOOR101CADCORE_API Status setOpeningOpenPercent(std::string_view text);
		// Space: in Door/Window mode, the next openings get their hinges on the other jamb; in Furniture and Column modes
		// the next pieces or columns turn 90 degrees; otherwise the selected piece or column turns 90 degrees, or the
		// selected opening flips its hinges.
		FLOOR101CADCORE_API Status flip();
		// Door/Window mode: what a click would add (id nil); nullopt when the pointer is not over a wall that fits it.
		const std::optional<OpeningRef>& openingPlacement() const { return placement; }
		// How the next doors or windows the Door and Window tools place open (settings().door / .window); the preview
		// follows. A door cannot be fixed.
		FLOOR101CADCORE_API Status setNewOpeningOperation(OpeningKind kind, OpeningOperation operation);
		FLOOR101CADCORE_API Status setOpeningFamily(std::string_view family);
		FLOOR101CADCORE_API Status setNewOpeningFamily(OpeningKind kind, std::string_view family);
		// Template dimensions in the active Door/Window tool, parsed in the current input unit.
		FLOOR101CADCORE_API Status setNewOpeningWidth(std::string_view text);
		FLOOR101CADCORE_API Status setNewOpeningHeight(std::string_view text);
		FLOOR101CADCORE_API Status setNewOpeningSill(std::string_view text);

		// Furniture (ADR-0016).
		// A catalogue type for the Furniture mode.
		FLOOR101CADCORE_API Status setFurnitureType(std::string_view type);
		// Furniture mode: what a click would add (id nil); nullopt while the pointer is off the floor.
		const std::optional<Furniture>& furniturePlacement() const { return furnitureAt; }
		// The selected piece; nullopt if none or if it no longer exists. While one is selected nothing else is.
		FLOOR101CADCORE_API std::optional<Furniture> selectedFurniture() const;
		FLOOR101CADCORE_API Status selectFurniture(const FurnitureId& id);
		// Property edits of the selected piece: sizes typed like lengths, the rotation in degrees ("90", "22.5").
		FLOOR101CADCORE_API Status setFurnitureWidth(std::string_view text);
		FLOOR101CADCORE_API Status setFurnitureDepth(std::string_view text);
		FLOOR101CADCORE_API Status setFurnitureHeight(std::string_view text);
		FLOOR101CADCORE_API Status setFurnitureRotation(std::string_view text);
		// Its main finish; empty restores its type's.
		FLOOR101CADCORE_API Status setFurnitureFinish(std::string_view finish);
		// Turns the selected piece 90 degrees counter-clockwise about its centre.
		FLOOR101CADCORE_API Status rotateFurniture();

		// Columns (ADR-0021).
		// Column mode: what a click would add (id nil); nullopt while the pointer is off the floor.
		const std::optional<Column>& columnPlacement() const { return columnAt; }
		// The shape and size of the next columns (settings().column*); the preview follows. Sizes typed like lengths.
		FLOOR101CADCORE_API Status setNewColumnShape(ColumnShape shape);
		FLOOR101CADCORE_API Status setNewColumnWidth(std::string_view text);
		FLOOR101CADCORE_API Status setNewColumnDepth(std::string_view text);
		// The selected column; nullopt if none or if it no longer exists. While one is selected nothing else is.
		FLOOR101CADCORE_API std::optional<Column> selectedColumn() const;
		FLOOR101CADCORE_API Status selectColumn(const ColumnId& id);
		// Property edits of the selected column: sizes typed like lengths, the rotation in degrees. A round one takes its
		// width as its diameter and is never turned.
		FLOOR101CADCORE_API Status setColumnShape(ColumnShape shape);
		FLOOR101CADCORE_API Status setColumnWidth(std::string_view text);
		FLOOR101CADCORE_API Status setColumnDepth(std::string_view text);
		FLOOR101CADCORE_API Status setColumnRotation(std::string_view text);
		// A finish for walls; empty restores concrete.
		FLOOR101CADCORE_API Status setColumnFinish(std::string_view finish);

		// Building services (ADR-0021).
		bool showsServices() const { return servicesShown; }
		// Hiding the layer leaves the Services mode and drops a selected run.
		FLOOR101CADCORE_API void setShowServices(bool show);
		// Which networks the layer shows: hiding one drops a selected run of that kind; the runs of a hidden kind are not
		// picked.
		const ServiceKindSet& shownServiceKinds() const { return serviceKindsShown; }
		FLOOR101CADCORE_API void setServiceKindShown(ServiceKind kind, bool shown);
		// The kind of the next runs, and of the one being drawn; its network is shown.
		FLOOR101CADCORE_API Status setServiceKind(ServiceKind kind);
		// The height of the next points over the level's floor, typed like a length ("-10" goes under the floor) or in mm.
		FLOOR101CADCORE_API Status setServiceHeight(std::string_view text);
		FLOOR101CADCORE_API Status setServiceHeightMm(std::int64_t heightMm);
		// Services mode: the points of the run being drawn (empty: none yet), and where a click would put the next one
		// (nullopt while the pointer points at nothing).
		const std::vector<ServicePoint>& runPoints() const { return runDraft; }
		const std::optional<ServicePoint>& runPointer() const { return runCursor; }
		// The selected run; nullopt if none, if it no longer exists or if the layer or its network is hidden.
		FLOOR101CADCORE_API std::optional<ServiceRun> selectedServiceRun() const;
		// Shows the layer and the run's network.
		FLOOR101CADCORE_API Status selectServiceRun(const ServiceRunId& id);
		FLOOR101CADCORE_API Status setServiceRunKind(ServiceKind kind);

		const WallTool& wallTool() const { return tool; }
		const EditSettings& settings() const { return editSettings; }
		bool isDragging() const
		{
			return dimensions.dragging() || drag.has_value() || wallBodyDrag.has_value() || openingDrag.has_value() || furnitureDrag.has_value() || columnDrag.has_value() ||
				runDrag.has_value() || roomTextDrag.has_value();
		}
		bool isDraggingOpening() const { return openingDrag.has_value(); }
		bool isDraggingFurniture() const { return furnitureDrag.has_value(); }
		// Last cursor position in plan: under the point where the pointer ray meets a wall, else where it meets the floor;
		// nullopt when it meets neither (e.g. looking at the sky).
		const std::optional<PlanPoint>& pointerOnPlan() const { return planPoint; }

		// Finishes (ADR-0012).
		void setRoomLocator(RoomLocator locator) { roomLocator = std::move(locator); }
		const FinishId& paintFinish() const { return editSettings.paintFinish; }
		FLOOR101CADCORE_API Status setPaintFinish(std::string_view finish);
		// Paint mode: what a click would paint with the chosen finish; nullopt when the pointer is over nothing it suits
		// (e.g. a floor that no walls close into a room).
		const std::optional<PaintTarget>& paintTarget() const { return paintAt; }
		// Selected room key (Select mode); empty if none. Whether the room still exists is up to the rooms of the scene.
		FLOOR101CADCORE_API const std::string& selectedRoom() const;
		FLOOR101CADCORE_API Status selectRoom(const std::string& room);
		FLOOR101CADCORE_API Status setSelectedRoomBaseboard(bool enabled);
		FLOOR101CADCORE_API Status setSelectedBaseboardHeight(std::string_view text);
		FLOOR101CADCORE_API Status setSelectedBaseboardThickness(std::string_view text);
		FLOOR101CADCORE_API Status setSelectedBaseboardFinish(std::string_view finish);
		FLOOR101CADCORE_API Status setSelectedRoomName(std::string_view name);
		FLOOR101CADCORE_API Status setSelectedRoomFloor(std::string_view finish);
		FLOOR101CADCORE_API Status setSelectedRoomCeiling(std::string_view finish);
		// Room names and areas on the plan (ADR-0035). In Select mode with the plan shown, a press on one of them selects
		// its room and drags that text (the selected dimension's grips come first, then texts, then dimensions and the
		// rest); once the pointer has moved a fifth of the text's height, the text follows it, and releasing moves it from
		// the room's label point (one SetRoomLabelCommand). The provider gives the texts of a level where the plan writes
		// them (Floor101Drawing: PlanDrawing::roomTexts); without it texts cannot be picked.
		using RoomTextProvider = std::function<std::vector<RoomText>(const Project&, const LevelId&)>;
		void setRoomTextProvider(RoomTextProvider provider) { roomTexts = std::move(provider); }
		// The selected room's layout as stored, or the defaults; an empty room when none is selected.
		FLOOR101CADCORE_API RoomLabel selectedRoomLabel() const;
		// Heights of its texts' capitals on the plan, typed like lengths; positions back to automatic (sizes stay).
		FLOOR101CADCORE_API Status setSelectedRoomNameHeight(std::string_view text);
		FLOOR101CADCORE_API Status setSelectedRoomAreaHeight(std::string_view text);
		FLOOR101CADCORE_API Status resetSelectedRoomTextPositions();
		bool isDraggingRoomText() const { return roomTextDrag.has_value(); }
		// A face of the selected wall; an empty finish restores the default.
		FLOOR101CADCORE_API Status setWallFinish(WallSide side, std::string_view finish);

		// The level being edited: the chosen one while it exists (an undo can remove it), else the first.
		FLOOR101CADCORE_API LevelId activeLevel() const;
		FLOOR101CADCORE_API Status setActiveLevel(const LevelId& level);
		FLOOR101CADCORE_API Status levelUp();
		FLOOR101CADCORE_API Status levelDown();
		// Adds a level on top, as tall as the current top one with the same slab, and makes it active.
		FLOOR101CADCORE_API Status addLevel();
		// Deletes the active level with its walls and furniture (one undoable step); the level below becomes active.
		FLOOR101CADCORE_API Status deleteActiveLevel();
		// Property edits of the active level; lengths typed like the wall tool input.
		FLOOR101CADCORE_API Status setLevelHeight(std::string_view text);
		FLOOR101CADCORE_API Status setLevelSlabThickness(std::string_view text);
		FLOOR101CADCORE_API Status setLevelName(std::string_view text);
		bool showsAllLevels() const { return allLevels; }
		void setShowAllLevels(bool show) { allLevels = show; }

		FLOOR101CADCORE_API EditView view() const;

	private:
		// A selected door or window being moved (Body) or resized (an edge) along its wall.
		struct OpeningDrag
		{
			enum class Part
			{
				Body,
				StartEdge,
				EndEdge,
			};

			WallId wall;
			Opening opening; // where it would go (the original until the pointer moves)
			Part part = Part::Body;
			double grab = 0.0; // pointer minus the dragged edge at the press, along the wall
		};

		struct EndpointDrag
		{
			WallId wall;
			WallEndpoint endpoint = WallEndpoint::End;
			Point2D origin;              // where the endpoint was
			Point2D position;            // where it goes
			std::vector<WallId> joined;  // every wall with an end at `origin` (the corner moves as one)
			PlanPoint grabOffset;        // origin minus the pointer at the press: the corner keeps its offset
		};

		struct WallBodyDrag
		{
			Wall wall;
			PlanPoint grab;
			Vector2D delta;
			std::vector<WallId> joined;
		};

		// The selected piece being moved.
		struct FurnitureDrag
		{
			Furniture furniture;  // where it would go (the original until the pointer moves)
			PlanPoint grabOffset; // its centre minus the pointer at the press
		};

		// The selected column being moved.
		struct ColumnDrag
		{
			Column column;        // where it would go (the original until the pointer moves)
			PlanPoint grabOffset; // its centre minus the pointer at the press
		};

		// A room's name or area being moved on the plan.
		struct RoomTextDrag
		{
			RoomLabel label;     // as it would be left (the stored one until the pointer has moved far enough)
			bool area = false;   // false: the name
			PlanPoint centre;    // the text's centre at the press
			PlanPoint grab;      // the pointer at the press
			PlanPoint anchor;    // the room's label point
			double startMm = 0.0; // how far the pointer must move before the text follows
			bool moved = false;
		};

		// The selected run being moved (bend nullopt) or one of its bends.
		struct RunDrag
		{
			ServiceRun run;                  // where it would go (the original until the pointer moves)
			std::optional<std::size_t> bend; // index of a point at the bend (every point there moves with it)
			double planeZ = 0.0;             // the pointer is followed on this horizontal plane
			PlanPoint grab;                  // where the pointer met it at the press
		};

		// Parses `text` as a length and applies the command built from it to the selected wall.
		Status changeSelection(std::string_view text, Command (*makeCommand)(const WallId&, std::int64_t));
		// Parses `text` as a length and applies it to a copy of the selected opening, then changes the opening.
		Status changeOpening(std::string_view text, void (*apply)(Opening&, std::int64_t));
		// Edits a copy of the selected opening and changes it (one undoable step).
		Status editSelectedOpening(const std::function<void(Opening&)>& edit);
		void updateDrag();
		void updateWallBodyDrag();
		void updatePlacement();
		void updateOpeningDrag();
		std::int64_t gridMm() const;
		// Where an opening of `wall` must not go: walls joined to it and its other openings (except `except`).
		std::vector<AxisSpan> blockedSpans(const Project& levelWalls, const Wall& wall, const OpeningId& except) const;
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
		std::optional<PaintTarget> resolvePaintTarget(const Project& levelWalls, const Ray3& ray) const;
		Status editRoomBaseboard(const std::function<void(RoomBaseboard&)>& change);
		Status setBaseboardDimension(std::string_view text, bool height);
		Status setRoomFinishes(std::string_view floor, std::string_view ceiling);
		Status editRoomLabel(const std::function<void(RoomLabel&)>& change);
		Status setRoomTextHeight(std::string_view text, bool area);
		// The room text the plan writes under `point` on the active level (the topmost), if any.
		std::optional<RoomText> roomTextAt(PlanPoint point) const;
		void updateRoomTextDrag();
		Status changeActiveLevel(const std::function<Status(Level&)>& edit);
		// Where `piece` goes for a wanted centre on the active level (snapping, ADR-0016); `freeRotation` when it does not
		// go against a wall. It goes on the side of the wall the pointer is on: while dragging, the wanted centre (the
		// pointer plus where the piece was grabbed) can fall across the wall.
		Furniture placeFurniture(Furniture piece, PlanPoint wanted, PlanPoint pointer, std::int64_t freeRotation) const;
		void updateFurniturePlacement();
		void updateFurnitureDrag();
		// Edits a copy of the selected piece and changes it (one undoable step).
		Status editSelectedFurniture(const std::function<Status(Furniture&)>& edit);
		Status changeFurnitureSize(std::string_view text, std::int64_t Furniture::*size);
		// Where a column goes for a wanted centre on the active level: a wall corner, a wall axis or the grid.
		Column placeColumn(Column column, PlanPoint wanted) const;
		void updateColumnPlacement();
		void updateColumnDrag();
		Status editSelectedColumn(const std::function<Status(Column&)>& edit);
		Status changeColumnSize(std::string_view text, std::int64_t Column::*size);
		Status changeNewColumnSize(std::string_view text, std::int64_t EditSettings::*size);
		void updateRunPointer();
		void updateRunDrag();
		// Adds the pointed point to the run being drawn (or starts one), or finishes it on its last point.
		Status addRunPoint();
		Status finishRun();
		// The run being drawn as it would be with the pointed point: nullopt with fewer than two points.
		std::optional<ServiceRun> runPreview() const;
		// Drops every drag.
		void dropDrags();
		// The tools' previews for the last pointer, after the project changed.
		void refreshPlacements();
		// Drops every selection.
		void clearSelected();

		Document& document;
		EditSettings editSettings;
		WallTool tool;
		DimensionEditor dimensions;
		EditMode currentMode = EditMode::Select;
		WallId selected;
		OpeningId selectedOpeningId; // with `selected` as its wall
		std::optional<EndpointDrag> drag;
		std::optional<WallBodyDrag> wallBodyDrag;
		std::optional<OpeningDrag> openingDrag;
		std::optional<OpeningRef> placement;
		std::optional<PlanPoint> planPoint;
		bool allLevels = false;
		std::string selectedRoomKey;
		RoomLocator roomLocator;
		RoomTextProvider roomTexts;
		std::optional<RoomTextDrag> roomTextDrag;
		FurnitureId selectedFurnitureId;
		std::optional<FurnitureDrag> furnitureDrag;
		std::optional<Furniture> furnitureAt; // Furniture mode placement
		std::optional<PaintTarget> paintAt;  // Paint mode target
		std::optional<Ray3> pointerRay;      // the last pointer ray, in the active level's space
		ColumnId selectedColumnId;
		std::optional<ColumnDrag> columnDrag;
		std::optional<Column> columnAt; // Column mode placement
		bool servicesShown = false;
		ServiceKindSet serviceKindsShown;
		bool interiorDimensionsShown = true;
		ServiceRunId selectedRunId;
		std::optional<RunDrag> runDrag;
		std::vector<ServicePoint> runDraft;     // Services mode: the run being drawn
		std::optional<ServicePoint> runCursor;  // Services mode: the next point
	};
}
