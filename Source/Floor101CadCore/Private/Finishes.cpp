#include "F101/Cad/Finishes.h"

namespace f101::cad
{
	namespace
	{
		constexpr std::uint8_t W = static_cast<std::uint8_t>(Surface::Wall);
		constexpr std::uint8_t F = static_cast<std::uint8_t>(Surface::Floor);
		constexpr std::uint8_t C = static_cast<std::uint8_t>(Surface::Ceiling);

		std::string_view surfaceName(Surface surface)
		{
			switch (surface)
			{
			case Surface::Wall: return "walls";
			case Surface::Floor: return "floors";
			default: return "ceilings";
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
			{"wood-oak", F | C, {178, 130, 84}, {96, 66, 40}, 0.6, FinishPattern::Planks, 1200, 180, 2},
			{"wood-walnut", F, {112, 76, 52}, {58, 38, 26}, 0.55, FinishPattern::Planks, 1200, 180, 2},
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
		default: return DefaultCeilingFinish;
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
