#include "F101/Cad/DimensionEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

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

		std::optional<Point2D> measurementPoint(PlanPoint point)
		{
			const auto x = quantizeMillimeters(point.x_mm);
			const auto y = quantizeMillimeters(point.y_mm);
			if (!x || !y) return std::nullopt;
			return Point2D{x.value(), y.value()};
		}

		// Signed distance of `point` from the measured segment of `dimension`, positive to its left (like its offset).
		double acrossMm(const Dimension& dimension, PlanPoint point)
		{
			const Vector2D axis = dimension.end - dimension.start;
			return ((point.y_mm - static_cast<double>(dimension.start.y_mm)) * static_cast<double>(axis.dx_mm) -
				(point.x_mm - static_cast<double>(dimension.start.x_mm)) * static_cast<double>(axis.dy_mm)) / axis.lengthMm();
		}
	}

	void DimensionEditor::setVisible(bool shown)
	{
		planShown = shown;
		if (!shown)
		{
			draft.clear();
			selectedId.clear();
			dropDrag();
		}
	}

	Result<std::optional<std::string>> DimensionEditor::place(PlanPoint pointer, const LevelId& level)
	{
		if (draft.size() < 2)
		{
			const auto point = measurementPoint(pointer);
			if (!point) return Error{ErrorCode::InvalidArgument, "Invalid measurement point"};
			if (!draft.empty() && *point == draft.front()) return Error{ErrorCode::InvalidArgument, "Choose two distinct points"};
			draft.push_back(*point);
			return std::optional<std::string>();
		}
		const auto placed = preview(pointer, level, true);
		if (!placed) return Error{ErrorCode::InvalidArgument, "Invalid dimension"};
		const auto applied = document.execute(SetDimensionCommand{*placed});
		if (!applied) return applied.error();
		draft.clear();
		selectedId = applied.value().dimensions.front().id;
		return std::optional<std::string>(selectedId);
	}

	std::optional<Dimension> DimensionEditor::preview(const std::optional<PlanPoint>& pointer, const LevelId& level, bool placing) const
	{
		if (drag) return drag;
		if (!placing || draft.empty() || !pointer) return std::nullopt;
		Dimension d;
		d.level = level;
		d.start = draft.front();
		const auto end = draft.size() < 2 ? measurementPoint(*pointer) : std::optional<Point2D>(draft[1]);
		if (!end) return std::nullopt;
		d.end = *end;
		if (d.start == d.end) return std::nullopt;
		if (draft.size() == 2)
		{
			const auto offset = quantizeMillimeters(acrossMm(d, *pointer));
			if (!offset) return std::nullopt;
			d.offsetMm = offset.value();
		}
		else d.offsetMm = 0;
		return d;
	}

	std::optional<Dimension> DimensionEditor::selected(const LevelId& level) const
	{
		if (!planShown || selectedId.empty()) return std::nullopt;
		if (drag) return drag;
		if (provider)
		{
			for (const Dimension& d : provider(document.project(), level)) if (d.id == selectedId && !d.hidden) return d;
			return std::nullopt;
		}
		const Dimension* d = document.project().findDimension(selectedId);
		return d && d->level == level && !d->hidden ? std::optional<Dimension>(*d) : std::nullopt;
	}

	Status DimensionEditor::select(std::string_view id, const LevelId& level)
	{
		selectedId = id;
		if (!selected(level)) { selectedId.clear(); return nothingSelected(); }
		return Status::success();
	}

	std::optional<DimensionEditor::Grab> DimensionEditor::hit(PlanPoint pointer, const LevelId& level) const
	{
		if (const auto current = selected(level))
		{
			std::array<PlanPoint, 4> handles;
			dimensionHandles(*current, handles);
			int grip = Grab::Line;
			double nearestGrip = pickToleranceMm;
			for (int i = 0; i < 4; ++i)
			{
				const double distance = std::hypot(pointer.x_mm - handles[i].x_mm, pointer.y_mm - handles[i].y_mm);
				if (distance < nearestGrip) { nearestGrip = distance; grip = i; }
			}
			if (grip != Grab::Line) return Grab{*current, grip};
		}
		if (!provider) return std::nullopt;
		std::optional<Grab> picked;
		double nearest = pickToleranceMm;
		for (const Dimension& d : provider(document.project(), level))
		{
			const auto [a, b] = dimensionLine(d);
			const double dx = b.x_mm - a.x_mm, dy = b.y_mm - a.y_mm;
			const double t = std::clamp(((pointer.x_mm - a.x_mm) * dx + (pointer.y_mm - a.y_mm) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
			const PlanPoint label = dimensionLabelPoint(d);
			const double distance = std::min(std::hypot(pointer.x_mm - a.x_mm - t * dx, pointer.y_mm - a.y_mm - t * dy),
				std::hypot(pointer.x_mm - label.x_mm, pointer.y_mm - label.y_mm));
			if (!d.hidden && distance < nearest) { nearest = distance; picked = Grab{d, Grab::Line}; }
		}
		return picked;
	}

	void DimensionEditor::startDrag(const Grab& grab, PlanPoint pointer)
	{
		selectedId = grab.dimension.id;
		drag = grab.dimension;
		dragOriginal = grab.dimension;
		dragGrip = grab.grip;
		if (grab.grip == Grab::Line) grabAcrossMm = acrossMm(grab.dimension, pointer);
		else grabPoint = pointer;
	}

	void DimensionEditor::moveDrag(PlanPoint pointer)
	{
		if (!drag || !dragOriginal) return;
		if (dragGrip == Grab::Line)
		{
			const auto offset = quantizeMillimeters(static_cast<double>(dragOriginal->offsetMm) + acrossMm(*dragOriginal, pointer) - grabAcrossMm);
			if (offset) drag->offsetMm = offset.value();
			return;
		}
		const Vector2D axis = dragOriginal->end - dragOriginal->start;
		const bool start = dragGrip % 2 == 0;
		const Point2D anchor = start ? dragOriginal->start : dragOriginal->end;
		double dx = pointer.x_mm - grabPoint.x_mm;
		double dy = pointer.y_mm - grabPoint.y_mm;
		if (dragGrip >= 2)
		{
			const double ux = static_cast<double>(axis.dx_mm) / axis.lengthMm();
			const double uy = static_cast<double>(axis.dy_mm) / axis.lengthMm();
			const double along = dx * ux + dy * uy;
			dx = along * ux; dy = along * uy;
		}
		const auto point = measurementPoint({static_cast<double>(anchor.x_mm) + dx, static_cast<double>(anchor.y_mm) + dy});
		if (!point) return;
		Dimension candidate = *dragOriginal;
		(start ? candidate.start : candidate.end) = *point;
		if (candidate.start == candidate.end) return;
		const Vector2D moved = candidate.end - candidate.start;
		if (dragGrip >= 2 && static_cast<double>(moved.dx_mm) * static_cast<double>(axis.dx_mm) +
			static_cast<double>(moved.dy_mm) * static_cast<double>(axis.dy_mm) <= 0.0) return;
		if (validateDimension(document.project(), candidate)) drag = candidate;
	}

	Status DimensionEditor::release()
	{
		if (!drag || !dragOriginal) return Status::success();
		const Dimension after = *drag;
		const bool changed = after != *dragOriginal;
		const bool convert = after.automatic && dragGrip != Grab::Line && changed;
		dropDrag();
		if (convert)
		{
			const auto applied = document.execute(MakeDimensionManualCommand{after});
			if (!applied) return applied.error();
			selectedId = applied.value().dimensions.back().id;
			return Status::success();
		}
		return changed ? toStatus(document.execute(SetDimensionCommand{after})) : Status::success();
	}

	void DimensionEditor::dropDrag()
	{
		drag.reset();
		dragOriginal.reset();
		dragGrip = Grab::Line;
	}

	Status DimensionEditor::setOffset(std::string_view text, LengthUnit unit, const LevelId& level)
	{
		auto d = selected(level);
		if (!d) return nothingSelected();
		const auto value = parseLength(text, unit);
		if (!value) return value.error();
		d->offsetMm = value.value();
		return toStatus(document.execute(SetDimensionCommand{*d}));
	}

	Status DimensionEditor::setText(std::string_view text, const LevelId& level)
	{
		auto d = selected(level);
		if (!d) return nothingSelected();
		d->text = text;
		return toStatus(document.execute(SetDimensionCommand{*d}));
	}

	Status DimensionEditor::setCoordinate(std::string_view text, int coordinate, LengthUnit unit, const LevelId& level)
	{
		auto d = selected(level);
		if (!d) return nothingSelected();
		if (d->automatic) return Error{ErrorCode::InvalidArgument, "Automatic endpoints follow the model"};
		const auto value = parseLength(text, unit);
		if (!value) return value.error();
		switch (coordinate)
		{
		case 0: d->start.x_mm = value.value(); break;
		case 1: d->start.y_mm = value.value(); break;
		case 2: d->end.x_mm = value.value(); break;
		case 3: d->end.y_mm = value.value(); break;
		default: return Error{ErrorCode::InvalidArgument, "Invalid dimension coordinate"};
		}
		return toStatus(document.execute(SetDimensionCommand{*d}));
	}

	std::optional<Status> DimensionEditor::deleteSelected(const LevelId& level)
	{
		auto d = selected(level);
		if (!d) return std::nullopt;
		d->hidden = true;
		const Status deleted = d->automatic ? toStatus(document.execute(SetDimensionCommand{*d}))
			: toStatus(document.execute(DeleteDimensionCommand{d->id}));
		if (deleted) selectedId.clear();
		return deleted;
	}

	Status DimensionEditor::restoreAutomatic(const LevelId& level)
	{
		return toStatus(document.execute(ResetDimensionsCommand{level}));
	}
}
