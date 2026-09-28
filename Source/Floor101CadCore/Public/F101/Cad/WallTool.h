#pragma once

#include "F101/Cad/Document.h"
#include "F101/Cad/Export.h"
#include "F101/Cad/Snapping.h"
#include "F101/Cad/Units.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace f101::cad
{
	struct WallToolSettings
	{
		std::int64_t thicknessMm = 120;
		std::int64_t heightMm = 2400;
		LengthUnit inputUnit = LengthUnit::Centimeter; // unit of a typed length without suffix
		SnapSettings snap;
	};

	// Interactive wall drawing, independent of any engine (master prompt §19): click the start, move the cursor to
	// preview, optionally type an exact length ("355", "355 cm", "3.55 m", "3550 mm"), Enter or click to create.
	// Walls chain: the next one starts where the previous ended, until cancel(). Each created wall is one undoable
	// CreateWallCommand.
	class WallTool
	{
	public:
		enum class State
		{
			Idle,
			Drafting,
		};

		FLOOR101CADCORE_API WallTool(Document& document, WallToolSettings settings);

		State state() const { return currentState; }
		const WallToolSettings& settings() const { return toolSettings; }

		// Updates the snapped hover point (Idle) or the preview wall (Drafting).
		FLOOR101CADCORE_API void pointerMoved(PlanPoint cursor);
		// Idle: starts a wall at the snapped point. Drafting: creates the previewed wall. Returns the created wall, if any.
		FLOOR101CADCORE_API Result<std::optional<WallId>> click(PlanPoint cursor);
		// Enter: creates the previewed wall; a typed length wins over the cursor distance.
		FLOOR101CADCORE_API Result<WallId> confirm();
		// Esc: stops drafting. Walls already created stay (undo removes them).
		FLOOR101CADCORE_API void cancel();

		// Typed length. Only digits, '.', ',', blanks and unit letters (m, c) are kept.
		FLOOR101CADCORE_API void appendText(std::string_view text);
		FLOOR101CADCORE_API void backspace();
		const std::string& typedText() const { return text; }
		// Why the typed text cannot be used; empty when it parses (or nothing is typed).
		const std::string& typedTextError() const { return textError; }

		// Snapped cursor: where the next wall would start (Idle) or end (Drafting).
		Point2D hoverPoint() const { return hover; }
		SnapKind snapKind() const { return kind; }
		// Wall that click()/confirm() would create. Not part of the document; its id is nil.
		FLOOR101CADCORE_API std::optional<Wall> preview() const;

	private:
		void update();
		Result<WallId> commit();

		Document& document;
		WallToolSettings toolSettings;
		State currentState = State::Idle;
		PlanPoint cursor;
		Point2D start;
		Point2D hover;
		SnapKind kind = SnapKind::Free;
		std::string text;
		std::string textError;
	};
}
