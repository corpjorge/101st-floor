#pragma once

#include "F101/Drawing/PlanDrawing.h"

#include <string>

namespace f101::drawing
{
	struct SvgOptions
	{
		double scale = 100.0;     // 1:scale; the document is sized in paper millimetres
		double marginMm = 1000.0; // plan millimetres of white paper around the drawing
	};

	// A standalone SVG document of the drawing (ADR-0015): white paper, user units in plan millimetres (north up), fills
	// and strokes in their inks' colours, dashed hidden lines, labels as text. Deterministic; an empty drawing gives an
	// empty sheet.
	FLOOR101DRAWING_API std::string writeSvg(const PlanDrawing& drawing, const SvgOptions& options = {});
}
