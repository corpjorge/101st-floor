#pragma once

#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Project.h"

#include <cstdint>
#include <string>
#include <vector>

#ifndef FLOOR101DRAWING_API
	#define FLOOR101DRAWING_API
#endif

// Plan drawings (ADR-0015): a level of the project as an architectural floor plan, in plan millimetres (X east, Y north).
// Derived from the model on demand, never saved; the 2D view, the SVG writer and the PNG export all draw this.
namespace f101::drawing
{
	// What a drawing element is, which decides how it is drawn. Consumers map inks to colours; widths come in plan mm.
	enum class Ink
	{
		Cut,       // walls cut by the plan: solid dark fill
		Carpentry, // window lines, door leaves
		Swing,     // door swings
		Hidden,    // what the cut hides (a sliding door's pocket): dashed, drawn light over the cut walls
		Dimension, // dimension lines, ticks and figures
		DimensionSelected, // dark amber on white paper: a selected annotation remains readable
		Label,     // room names and areas
		Furniture, // outlines of furniture and their details (ADR-0016)
		Selected,  // the selected wall (editing view)
		Preview,   // walls as the current gesture would leave them (editing view)
		Handle,    // draggable handles (editing view)
		// Runs of the building services (ADR-0021): a dark casing under a core in the colour of their kind, so a white
		// water pipe still shows on white paper.
		ServiceCasing,
		ServiceElectrical,
		ServiceWater,
		ServiceSewage,
		ServiceGas,
	};

	struct Rgb
	{
		std::uint8_t r = 0;
		std::uint8_t g = 0;
		std::uint8_t b = 0;
	};

	// The colour of an ink on white paper (sRGB), the same for every consumer.
	FLOOR101DRAWING_API Rgb inkColor(Ink ink);

	// The core ink of a kind of run.
	FLOOR101DRAWING_API Ink serviceInk(cad::ServiceKind kind);

	// A filled simple polygon, counter-clockwise, with its triangulation (index triples into the outline).
	struct Fill
	{
		std::vector<cad::PlanPoint> outline;
		std::vector<std::uint32_t> triangles;
		Ink ink = Ink::Cut;
	};

	struct Stroke
	{
		std::vector<cad::PlanPoint> points; // a polyline
		bool closed = false;
		bool dashed = false;
		Ink ink = Ink::Carpentry;
		double widthMm = 10.0; // on the plan: at 1:100, 10 mm print 0.1 mm
	};

	// A line of text centred on `at`, turned `angleDegrees` counter-clockwise (kept so it reads upright).
	struct Label
	{
		cad::PlanPoint at;
		double angleDegrees = 0.0;
		double heightMm = 150.0; // height of its capitals on the plan
		std::string text;         // UTF-8
		Ink ink = Ink::Label;
	};

	struct PlanDrawing
	{
		std::vector<Fill> fills;     // drawn first
		std::vector<Stroke> strokes; // then these
		std::vector<Label> labels;   // on top
		std::vector<cad::Dimension> dimensions; // visible, resolved annotations for CAD picking
		cad::PlanPoint min;          // bounds of everything (labels by their anchor and height); zero when empty
		cad::PlanPoint max;

		bool empty() const { return fills.empty() && strokes.empty() && labels.empty(); }
	};

	// Editing state drawn on the plan, like scene::ViewOverlay for the 3D view. Never saved; exports use none.
	struct PlanOverlay
	{
		std::string selectedDimension;
		std::optional<cad::Dimension> previewDimension;
		cad::WallId selectedWall; // nil: none
		// Walls as the current gesture would leave them: one with an existing id replaces it, one with a nil id is added.
		std::vector<cad::Wall> previewWalls;
		cad::FurnitureId selectedFurniture; // nil: none
		// Furniture as the current gesture would leave it: one with an existing id replaces it, one with a nil id is added.
		std::vector<cad::Furniture> previewFurniture;
		cad::ColumnId selectedColumn; // nil: none
		// Columns as the current gesture would leave them: one with an existing id replaces it, one with a nil id is added.
		std::vector<cad::Column> previewColumns;
		bool services = false; // draw the runs of the building services (the layer of the editing view)...
		cad::ServiceKindSet serviceKinds; // ...of these networks
		cad::ServiceRunId selectedServiceRun; // nil: none
		// Runs as the current gesture would leave them, drawn even with the layer off: one with an existing id replaces
		// it, one with a nil id is added.
		std::vector<cad::ServiceRun> previewServiceRuns;
		std::vector<cad::Point2D> handles;
		double handleSizeMm = 240.0;
	};

	// The words and number format of the drawing, in the application's language.
	struct DrawingTexts
	{
		std::string roomName = "Room {n}"; // {n}: 1, 2... in the rooms' key order
		std::string areaUnit = " m2";
		char decimalSeparator = '.';
	};

	// Openings are cut (drawn as gaps with their symbol) where the plan's cutting height crosses them: doors always,
	// windows whose sill is below it and whose head is above it.
	inline constexpr double CutHeightMm = 1200.0;
	// Dimension lines stand this far out from a wall's face: the chain with its openings, then its overall length.
	inline constexpr double ChainOffsetMm = 600.0;
	inline constexpr double OverallOffsetMm = 1100.0;
	inline constexpr double FigureHeightMm = 150.0;
	inline constexpr double RoomNameHeightMm = 200.0;
	inline constexpr double RoomAreaHeightMm = 150.0;
	// A run's core is drawn as wide as its pipe, never thinner than this, inside a casing this much wider on each side.
	inline constexpr double MinServiceCoreMm = 40.0;
	inline constexpr double ServiceCasingMm = 10.0;

	// The plan of one level (ADR-0015):
	// - walls cut at CutHeightMm as solid fills (joint fills included), with gaps where openings are cut;
	// - windows: their outline across the gap on both faces and the glass (two lines when sliding); doors: each leaf open
	//   at 90 degrees and the arc its free edge sweeps, towards the face it opens to; a sliding door: its leaf in the gap
	//   and its pocket dashed along the axis;
	// - dimensions, measured along the wall axes (the lengths the model is edited in): for each wall, on the side
	//   without a room (the left one when both or neither have one), a chain through its openings' edges and its overall
	//   length; walls with rooms on both sides only get their overall length. Figures in metres with two decimals;
	// - each room's custom name or automatic name ("Room 1"...) and net area at its label point;
	// - each piece of furniture: its outline and the details of its type (a mattress, a basin, a hob) as thin lines;
	// - each column: cut, as a solid fill of its outline (ADR-0021);
	// - with the overlay's services layer, each run on the level of the networks it shows: its plan path (a casing, then
	//   its kind's colour on top), and a ring where it rises or drops on the spot.
	// The overlay tints the selected and previewed walls, furniture, columns and runs and adds the handles.
	FLOOR101DRAWING_API PlanDrawing buildPlanDrawing(const cad::Project& project, const cad::LevelId& level,
		const PlanOverlay& overlay = {}, const DrawingTexts& texts = {});

	// "3.55" (or "3,55"): metres with two decimals, rounded half away from zero.
	FLOOR101DRAWING_API std::string formatMeters(double millimeters, char decimalSeparator);
}
