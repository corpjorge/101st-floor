#include "F101/Cad/Finishes.h"

namespace f101::cad
{
	namespace
	{
		constexpr std::uint8_t W = static_cast<std::uint8_t>(Surface::Wall);
		constexpr std::uint8_t F = static_cast<std::uint8_t>(Surface::Floor);
		constexpr std::uint8_t C = static_cast<std::uint8_t>(Surface::Ceiling);
		constexpr std::uint8_t K = static_cast<std::uint8_t>(Surface::Carpentry);
		constexpr std::uint8_t G = static_cast<std::uint8_t>(Surface::Glazing);
		constexpr std::uint8_t U = static_cast<std::uint8_t>(Surface::Furniture);

		std::string_view surfaceName(Surface surface)
		{
			switch (surface)
			{
			case Surface::Wall: return "walls";
			case Surface::Floor: return "floors";
			case Surface::Ceiling: return "ceilings";
			case Surface::Carpentry: return "doors and window frames";
			case Surface::Furniture: return "furniture";
			default: return "glazing";
			}
		}
	}

	const std::vector<FinishInfo>& finishCatalogue()
	{
		static const std::vector<FinishInfo> catalogue{
			{"plaster-white", W | C, {238, 236, 230}, {}, 0.9, FinishPattern::Plain, 0, 0, 0},
			{"paint-sand", W | C, {221, 204, 178}, {}, 0.85, FinishPattern::Plain, 0, 0, 0},
			{"paint-sage", W, {168, 184, 158}, {}, 0.85, FinishPattern::Plain, 0, 0, 0},
			{"paint-slate", W, {118, 132, 150}, {}, 0.85, FinishPattern::Plain, 0, 0, 0},
			{"brick-red", W, {152, 74, 54}, {204, 197, 184}, 0.9, FinishPattern::Bricks, 240, 55, 10},
			{"concrete", W | F | C, {162, 160, 154}, {}, 0.8, FinishPattern::Speckle, 0, 0, 0},
			{"tile-white", W | F, {236, 238, 236}, {188, 188, 184}, 0.25, FinishPattern::Tiles, 200, 200, 3},
			{"screed-light", F, {198, 196, 190}, {}, 0.75, FinishPattern::Speckle, 0, 0, 0},
			{"tile-grey", F, {148, 148, 144}, {104, 104, 100}, 0.5, FinishPattern::Tiles, 600, 600, 3},
			{"wood-oak", F | C | K | U, {178, 130, 84}, {96, 66, 40}, 0.6, FinishPattern::Planks, 1200, 180, 2},
			{"wood-walnut", F | K | U, {112, 76, 52}, {58, 38, 26}, 0.55, FinishPattern::Planks, 1200, 180, 2},
			{"lacquer-white", K | U, {240, 240, 236}, {}, 0.35, FinishPattern::Plain, 0, 0, 0},
			{"aluminium-anthracite", K | U, {58, 60, 64}, {}, 0.4, FinishPattern::Plain, 0, 0, 0},
			{"glass-clear", G, {214, 228, 230}, {}, 0.03, FinishPattern::Plain, 0, 0, 0, 0.18},
			// Furniture (ADR-0016).
			{"fabric-grey", U, {118, 121, 126}, {}, 0.95, FinishPattern::Speckle, 0, 0, 0},
			{"fabric-sand", U, {196, 178, 150}, {}, 0.95, FinishPattern::Speckle, 0, 0, 0},
			{"linen-white", U, {234, 230, 220}, {}, 0.9, FinishPattern::Plain, 0, 0, 0},
			{"ceramic-white", U, {245, 245, 243}, {}, 0.12, FinishPattern::Plain, 0, 0, 0},
			{"stone-dark", U, {62, 60, 58}, {}, 0.3, FinishPattern::Speckle, 0, 0, 0},
			{"steel-brushed", U, {176, 178, 180}, {}, 0.35, FinishPattern::Plain, 0, 0, 0},
			{"glass-black", U, {22, 22, 24}, {}, 0.08, FinishPattern::Plain, 0, 0, 0},
		};
		return catalogue;
	}

	const FinishInfo* findFinish(std::string_view id)
	{
		for (const FinishInfo& finish : finishCatalogue())
		{
			if (finish.id == id)
			{
				return &finish;
			}
		}
		return nullptr;
	}

	std::string_view resolveFinish(std::string_view id, Surface surface)
	{
		if (!id.empty())
		{
			return id;
		}
		switch (surface)
		{
		case Surface::Wall: return DefaultWallFinish;
		case Surface::Floor: return DefaultFloorFinish;
		case Surface::Ceiling: return DefaultCeilingFinish;
		case Surface::Carpentry: return DefaultCarpentryFinish;
		case Surface::Furniture: return DefaultCarpentryFinish; // each furniture type has its own default (ADR-0016)
		default: return DefaultGlazingFinish;
		}
	}

	Status validateFinish(std::string_view id, Surface surface)
	{
		if (id.empty())
		{
			return Status::success();
		}
		const FinishInfo* finish = findFinish(id);
		if (!finish)
		{
			return Error{ErrorCode::NotFound, "Unknown finish '" + std::string(id) + "'"};
		}
		if (!finish->allows(surface))
		{
			return Error{ErrorCode::InvalidArgument, "The finish '" + std::string(id) + "' is not for " + std::string(surfaceName(surface))};
		}
		return Status::success();
	}
}
