#pragma once

#include "F101/Cad/Wall.h"

#include <span>

namespace f101::cad
{
	enum class OpeningDesign { Solid, Panelled, Glazed };

	// Stable family ids persist; geometry and thumbnails derive from these dimensions and divisions.
	struct OpeningFamily
	{
		std::string_view id;
		std::string_view name;
		OpeningKind kind;
		OpeningOperation operation;
		OpeningDesign design;
		int columns;
		int rows;
		std::int64_t widthMm;
		std::int64_t heightMm;
		std::int64_t sillMm;
	};

	FLOOR101CADCORE_API std::span<const OpeningFamily> openingCatalogue();
	FLOOR101CADCORE_API const OpeningFamily* findOpeningFamily(std::string_view id);
	// Applies appearance and mechanism, preserving dimensions, finish, hinge and swing side.
	FLOOR101CADCORE_API Status applyOpeningFamily(Opening& opening, std::string_view id);
}
