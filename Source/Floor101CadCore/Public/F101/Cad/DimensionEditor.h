#pragma once

#include "F101/Cad/Document.h"
#include "F101/Cad/Export.h"
#include "F101/Cad/Geometry2D.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Project.h"
#include "F101/Cad/Result.h"
#include "F101/Cad/Units.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace f101::cad
{
	// Plan annotations in the 2D view (ADR-0031): placing a manual dimension (two measured points, then a click that
	// places its line), picking one, dragging its line or its grips, and editing the selected one. Each change is one
	// undoable Document command; the gesture is not part of the document. EditController drives it like WallTool: it
	// decides when it applies (the plan is shown; Dimension mode places, Select mode picks and edits) and passes the
	// active level.
	class DimensionEditor
	{
	public:
		// The dimensions of a level as the plan resolves them (automatic ones from the model, then manual ones).
		using Provider = std::function<std::vector<Dimension>(const Project&, const LevelId&)>;

		// What a press grabs: a grip of the selected dimension (0/1: its measured points; 2/3: resize along its axis), or
		// the line or figure of a dimension (Line), whose offset then follows the pointer.
		struct Grab
		{
			static constexpr int Line = -1;
			Dimension dimension;
			int grip = Line;
		};

		explicit DimensionEditor(Document& target) : document(target) {}

		// Picking needs it; without it, only the stored dimensions of the level can be selected.
		void setProvider(Provider dimensions) { provider = std::move(dimensions); }
		bool hasProvider() const { return static_cast<bool>(provider); }
		// Whether the plan is shown. Hiding it forgets the draft, the selection and any drag.
		FLOOR101CADCORE_API void setVisible(bool shown);
		bool visible() const { return planShown; }
		void setPickTolerance(double mm) { pickToleranceMm = mm; }
		double handleSizeMm() const { return pickToleranceMm * 1.25; }

		// Placing: the first two clicks set the measured points, the third places the line, creates the dimension and
		// selects it; it returns its id.
		FLOOR101CADCORE_API Result<std::optional<std::string>> place(PlanPoint pointer, const LevelId& level);
		std::size_t draftPointCount() const { return draft.size(); }
		bool hasDraft() const { return !draft.empty(); }
		void clearDraft() { draft.clear(); }
		// The dimension being dragged, else the one being placed (`placing`) as the next click would create it.
		FLOOR101CADCORE_API std::optional<Dimension> preview(const std::optional<PlanPoint>& pointer, const LevelId& level,
			bool placing) const;

		// The selected dimension of the level as it is shown (while dragged, as the drag leaves it); hidden ones are not.
		FLOOR101CADCORE_API std::optional<Dimension> selected(const LevelId& level) const;
		FLOOR101CADCORE_API Status select(std::string_view id, const LevelId& level);
		void clearSelection() { selectedId.clear(); }

		// What a press at `pointer` grabs: a grip of the selected dimension first, else the nearest visible dimension
		// within the pick tolerance (its line or its figure).
		FLOOR101CADCORE_API std::optional<Grab> hit(PlanPoint pointer, const LevelId& level) const;
		// Selects the grabbed dimension and starts dragging it from `pointer`.
		FLOOR101CADCORE_API void startDrag(const Grab& grab, PlanPoint pointer);
		bool dragging() const { return drag.has_value(); }
		// Offset drags move the line across its axis; grips move a measured point (2/3: only along the axis, never past
		// the other end). A position the dimension cannot take keeps the last valid one.
		FLOOR101CADCORE_API void moveDrag(PlanPoint pointer);
		// One undoable change, or none if nothing moved. Moving a grip of an automatic dimension hides it and creates a
		// manual copy, which becomes the selection.
		FLOOR101CADCORE_API Status release();
		FLOOR101CADCORE_API void dropDrag();

		// Edits of the selected dimension; lengths typed like the wall tool input ("355", "3.55 m"...).
		FLOOR101CADCORE_API Status setOffset(std::string_view text, LengthUnit unit, const LevelId& level);
		FLOOR101CADCORE_API Status setText(std::string_view text, const LevelId& level);
		// 0/1: start x/y, 2/3: end x/y. Only manual dimensions: automatic endpoints follow the model.
		FLOOR101CADCORE_API Status setCoordinate(std::string_view text, int coordinate, LengthUnit unit, const LevelId& level);
		// Hides an automatic dimension or deletes a manual one; nullopt when no dimension is selected.
		FLOOR101CADCORE_API std::optional<Status> deleteSelected(const LevelId& level);
		// Forgets the stored changes to the automatic dimensions of the level.
		FLOOR101CADCORE_API Status restoreAutomatic(const LevelId& level);

	private:
		Document& document;
		Provider provider;
		bool planShown = false;
		double pickToleranceMm = 150.0;
		std::string selectedId;
		std::vector<Point2D> draft;
		std::optional<Dimension> drag;         // as the gesture leaves it
		std::optional<Dimension> dragOriginal; // as it was grabbed
		int dragGrip = Grab::Line;
		double grabAcrossMm = 0.0; // line drags: where the pointer grabbed it, across its axis
		PlanPoint grabPoint;       // grip drags
	};
}
