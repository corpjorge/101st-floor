#include "F101/Cad/OpeningCatalogue.h"

#include <array>

namespace f101::cad
{
	namespace
	{
		using K = OpeningKind;
		using O = OpeningOperation;
		using D = OpeningDesign;
		constexpr std::array<OpeningFamily, 24> Families{{
			{"door-flush", "Abatible lisa", K::Door, O::Hinged, D::Solid, 1, 1, 900, 2100, 0},
			{"door-two-panels", "Abatible de dos paneles", K::Door, O::Hinged, D::Panelled, 1, 2, 900, 2100, 0},
			{"door-four-panels", "Abatible de cuatro paneles", K::Door, O::Hinged, D::Panelled, 2, 2, 900, 2100, 0},
			{"door-six-panels", "Abatible de seis paneles", K::Door, O::Hinged, D::Panelled, 2, 3, 900, 2100, 0},
			{"door-glass", "Abatible acristalada", K::Door, O::Hinged, D::Glazed, 1, 1, 900, 2100, 0},
			{"door-glass-grid", "Abatible acristalada con retícula", K::Door, O::Hinged, D::Glazed, 2, 3, 900, 2100, 0},
			{"door-double-flush", "Doble lisa", K::Door, O::DoubleHinged, D::Solid, 1, 1, 1600, 2100, 0},
			{"door-double-panels", "Doble panelada", K::Door, O::DoubleHinged, D::Panelled, 1, 3, 1600, 2100, 0},
			{"door-double-glass", "Doble acristalada", K::Door, O::DoubleHinged, D::Glazed, 1, 2, 1600, 2100, 0},
			{"door-pocket-flush", "Corredera empotrada lisa", K::Door, O::Sliding, D::Solid, 1, 1, 900, 2100, 0},
			{"door-pocket-panels", "Corredera empotrada panelada", K::Door, O::Sliding, D::Panelled, 1, 3, 900, 2100, 0},
			{"door-pocket-glass", "Corredera empotrada acristalada", K::Door, O::Sliding, D::Glazed, 1, 1, 900, 2100, 0},
			{"window-fixed", "Fija panorámica", K::Window, O::Fixed, D::Glazed, 1, 1, 1200, 1200, 900},
			{"window-fixed-two", "Fija de dos paños", K::Window, O::Fixed, D::Glazed, 2, 1, 1600, 1200, 900},
			{"window-fixed-grid", "Fija de seis paños", K::Window, O::Fixed, D::Glazed, 3, 2, 1800, 1200, 900},
			{"window-casement", "Practicable sencilla", K::Window, O::Hinged, D::Glazed, 1, 1, 800, 1200, 900},
			{"window-casement-two", "Practicable con travesaño", K::Window, O::Hinged, D::Glazed, 1, 2, 800, 1200, 900},
			{"window-casement-grid", "Practicable con retícula", K::Window, O::Hinged, D::Glazed, 2, 3, 800, 1200, 900},
			{"window-double", "Doble practicable", K::Window, O::DoubleHinged, D::Glazed, 1, 1, 1200, 1200, 900},
			{"window-double-two", "Doble con travesaños", K::Window, O::DoubleHinged, D::Glazed, 1, 2, 1200, 1200, 900},
			{"window-double-grid", "Doble con retícula", K::Window, O::DoubleHinged, D::Glazed, 2, 3, 1200, 1200, 900},
			{"window-sliding", "Corredera sencilla", K::Window, O::Sliding, D::Glazed, 1, 1, 1600, 1200, 900},
			{"window-sliding-two", "Corredera con travesaños", K::Window, O::Sliding, D::Glazed, 1, 2, 1600, 1200, 900},
			{"window-sliding-grid", "Corredera con retícula", K::Window, O::Sliding, D::Glazed, 2, 3, 1600, 1200, 900},
		}};
	}

	std::span<const OpeningFamily> openingCatalogue() { return Families; }

	const OpeningFamily* findOpeningFamily(std::string_view id)
	{
		for (const auto& family : Families)
		{
			if (family.id == id) { return &family; }
		}
		return nullptr;
	}

	Status applyOpeningFamily(Opening& opening, std::string_view id)
	{
		const auto* family = findOpeningFamily(id);
		if (!family || family->kind != opening.kind)
		{
			return Error{ErrorCode::InvalidArgument, "Choose a catalogue family for this kind of opening"};
		}
		opening.family = family->id;
		opening.operation = family->operation;
		if (opening.operation == OpeningOperation::Fixed) { opening.openPercent = 0; }
		return Status::success();
	}
}
