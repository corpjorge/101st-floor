#include "F101/Cad/Finishes.h"

#include "F101/Cad/Asset.h"

#include <utility>

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
		constexpr std::uint8_t S = static_cast<std::uint8_t>(Surface::Services);

		std::string_view surfaceName(Surface surface)
		{
			switch (surface)
			{
			case Surface::Wall: return "walls";
			case Surface::Floor: return "floors";
			case Surface::Ceiling: return "ceilings";
			case Surface::Carpentry: return "doors and window frames";
			case Surface::Furniture: return "furniture";
			case Surface::Services: return "building services";
			default: return "glazing";
			}
		}

		using Category = FinishCategory;

		FinishInfo plain(std::string_view id, Category category, std::uint8_t surfaces, Rgb8 color, double roughness)
		{
			FinishInfo finish;
			finish.id = id;
			finish.category = category;
			finish.surfaces = surfaces;
			finish.color = color;
			finish.roughness = roughness;
			return finish;
		}

		// Wall and ceiling paint: matt, one colour.
		FinishInfo paint(std::string_view id, Rgb8 color)
		{
			return plain(id, Category::Paint, W | C, color, 0.85);
		}

		FinishInfo speckle(std::string_view id, Category category, std::uint8_t surfaces, Rgb8 color, double roughness)
		{
			FinishInfo finish = plain(id, category, surfaces, color, roughness);
			finish.pattern = FinishPattern::Speckle;
			return finish;
		}

		// Bricks, tiles, planks: units of width x height (a plank: length x width) separated by joints.
		FinishInfo units(std::string_view id, Category category, std::uint8_t surfaces, FinishPattern pattern, Rgb8 color,
			Rgb8 joint, double roughness, std::int64_t widthMm, std::int64_t heightMm, std::int64_t jointMm)
		{
			FinishInfo finish = plain(id, category, surfaces, color, roughness);
			finish.pattern = pattern;
			finish.jointColor = joint;
			finish.unitWidthMm = widthMm;
			finish.unitHeightMm = heightMm;
			finish.jointMm = jointMm;
			return finish;
		}

		FinishInfo checker(std::string_view id, std::uint8_t surfaces, Rgb8 color, Rgb8 accent, Rgb8 joint, double roughness,
			std::int64_t sizeMm, std::int64_t jointMm)
		{
			FinishInfo finish =
				units(id, Category::Tiles, surfaces, FinishPattern::Checker, color, joint, roughness, sizeMm, sizeMm, jointMm);
			finish.accentColor = accent;
			return finish;
		}

		FinishInfo glass(std::string_view id, std::uint8_t surfaces, Rgb8 color, double roughness, double opacity, double metallic = 0.0)
		{
			FinishInfo finish = plain(id, Category::Glass, surfaces, color, roughness);
			finish.opacity = opacity;
			finish.metallic = metallic;
			return finish;
		}
	}

	namespace
	{
		constexpr std::pair<FinishCategory, std::string_view> FinishCategoryIds[] = {{FinishCategory::Paint, "paint"},
			{FinishCategory::Tiles, "tiles"}, {FinishCategory::Wood, "wood"}, {FinishCategory::Stone, "stone"},
			{FinishCategory::Textile, "textile"}, {FinishCategory::Joinery, "joinery"}, {FinishCategory::Fabric, "fabric"},
			{FinishCategory::Glass, "glass"}, {FinishCategory::Services, "services"}};
	}

	std::string_view finishCategoryId(FinishCategory category)
	{
		for (const auto& [value, id] : FinishCategoryIds)
		{
			if (value == category)
			{
				return id;
			}
		}
		return "paint";
	}

	std::optional<FinishCategory> parseFinishCategory(std::string_view id)
	{
		for (const auto& [value, name] : FinishCategoryIds)
		{
			if (name == id)
			{
				return value;
			}
		}
		return std::nullopt;
	}

	const std::vector<FinishInfo>& finishCatalogue()
	{
		using P = FinishPattern;
		static const std::vector<FinishInfo> catalogue{
			// Paints (the first four since MVP 5); every paint suits ceilings too.
			plain("plaster-white", Category::Paint, W | C, {238, 236, 230}, 0.9),
			paint("paint-sand", {221, 204, 178}),
			paint("paint-sage", {168, 184, 158}),
			paint("paint-slate", {118, 132, 150}),
			paint("paint-white-warm", {245, 241, 232}),
			paint("paint-ivory", {240, 232, 212}),
			paint("paint-cream", {238, 224, 196}),
			paint("paint-beige", {216, 196, 168}),
			paint("paint-greige", {196, 188, 176}),
			paint("paint-pearl", {206, 208, 206}),
			paint("paint-taupe", {150, 138, 126}),
			paint("paint-charcoal", {72, 76, 80}),
			paint("paint-black", {38, 38, 40}),
			paint("paint-mint", {190, 222, 204}),
			paint("paint-olive", {128, 132, 90}),
			paint("paint-forest", {54, 84, 64}),
			paint("paint-sky", {178, 206, 228}),
			paint("paint-denim", {88, 118, 152}),
			paint("paint-navy", {40, 56, 86}),
			paint("paint-teal", {40, 96, 104}),
			paint("paint-lavender", {192, 184, 214}),
			paint("paint-blush", {230, 196, 190}),
			paint("paint-terracotta", {184, 104, 74}),
			paint("paint-mustard", {214, 168, 62}),
			paint("paint-ochre", {196, 142, 74}),
			paint("paint-chocolate", {96, 66, 50}),
			// Tiles.
			units("tile-white", Category::Tiles, W | F, P::Tiles, {236, 238, 236}, {188, 188, 184}, 0.25, 200, 200, 3),
			units("tile-garage-grey", Category::Tiles, F, P::Tiles, {118, 121, 120}, {74, 77, 76}, 0.85, 300, 300, 4),
			units("tile-grey", Category::Tiles, F, P::Tiles, {148, 148, 144}, {104, 104, 100}, 0.5, 600, 600, 3),
			units("tile-metro-white", Category::Tiles, W, P::Bricks, {242, 242, 238}, {206, 206, 202}, 0.15, 150, 75, 2),
			units("tile-metro-green", Category::Tiles, W, P::Bricks, {98, 138, 118}, {220, 220, 214}, 0.15, 150, 75, 2),
			units("tile-metro-black", Category::Tiles, W, P::Bricks, {40, 42, 46}, {200, 200, 196}, 0.15, 150, 75, 2),
			units("tile-porcelain-beige", Category::Tiles, W | F, P::Tiles, {208, 194, 172}, {176, 166, 150}, 0.4, 600, 600, 2),
			units("tile-porcelain-anthracite", Category::Tiles, W | F, P::Tiles, {72, 74, 76}, {54, 56, 58}, 0.45, 600, 600, 2),
			units("tile-porcelain-white", Category::Tiles, W | F, P::Tiles, {230, 230, 226}, {200, 200, 196}, 0.3, 1200, 600, 2),
			units("tile-terracotta", Category::Tiles, F, P::Tiles, {178, 98, 64}, {190, 180, 166}, 0.7, 300, 300, 6),
			checker("tile-checker-bw", W | F, {236, 234, 228}, {40, 40, 42}, {180, 180, 176}, 0.3, 300, 2),
			checker("tile-checker-terracotta", F, {226, 212, 188}, {170, 92, 62}, {190, 182, 170}, 0.5, 200, 2),
			// Wood: floorboards, laminate, parquet, panelling.
			units("wood-oak", Category::Wood, F | C | K | U, P::Planks, {178, 130, 84}, {96, 66, 40}, 0.6, 1200, 180, 2),
			units("wood-walnut", Category::Wood, F | K | U, P::Planks, {112, 76, 52}, {58, 38, 26}, 0.55, 1200, 180, 2),
			units("laminate-oak-light", Category::Wood, F, P::Planks, {206, 170, 122}, {160, 128, 90}, 0.55, 1285, 192, 1),
			units("laminate-oak-natural", Category::Wood, F, P::Planks, {184, 140, 94}, {120, 88, 58}, 0.55, 1285, 192, 1),
			units("laminate-oak-grey", Category::Wood, F, P::Planks, {166, 156, 142}, {118, 110, 100}, 0.55, 1285, 192, 1),
			units("laminate-oak-white", Category::Wood, F, P::Planks, {224, 216, 202}, {184, 176, 162}, 0.5, 1285, 192, 1),
			units("laminate-walnut", Category::Wood, F, P::Planks, {124, 88, 62}, {76, 52, 36}, 0.5, 1285, 192, 1),
			units("laminate-wenge", Category::Wood, F, P::Planks, {70, 52, 42}, {40, 30, 24}, 0.5, 1285, 192, 1),
			units("parquet-herringbone-oak", Category::Wood, F, P::Herringbone, {188, 142, 94}, {120, 88, 56}, 0.5, 600, 100, 1),
			units("parquet-herringbone-walnut", Category::Wood, F, P::Herringbone, {118, 82, 58}, {70, 48, 32}, 0.5, 600, 100, 1),
			units("parquet-herringbone-grey", Category::Wood, F, P::Herringbone, {160, 150, 138}, {110, 102, 92}, 0.5, 600, 100, 1),
			units("boards-pine", Category::Wood, W | F, P::Planks, {214, 180, 130}, {150, 118, 80}, 0.6, 2000, 220, 3),
			units("panel-oak", Category::Wood, W | C, P::Planks, {178, 132, 86}, {110, 78, 50}, 0.55, 2400, 120, 2),
			// Stone, concrete and brick.
			units("brick-red", Category::Stone, W, P::Bricks, {152, 74, 54}, {204, 197, 184}, 0.9, 240, 55, 10),
			units("brick-white", Category::Stone, W, P::Bricks, {234, 230, 222}, {206, 202, 194}, 0.85, 240, 55, 10),
			speckle("concrete", Category::Stone, W | F | C, {162, 160, 154}, 0.8),
			speckle("screed-light", Category::Stone, F, {198, 196, 190}, 0.75),
			speckle("concrete-polished", Category::Stone, F, {150, 150, 146}, 0.35),
			speckle("microcement-grey", Category::Stone, W | F, {172, 170, 166}, 0.55),
			speckle("microcement-white", Category::Stone, W | F, {226, 224, 218}, 0.55),
			speckle("microcement-sand", Category::Stone, W | F, {208, 196, 178}, 0.55),
			speckle("stone-dark", Category::Stone, U, {62, 60, 58}, 0.3),
			// Carpet.
			speckle("carpet-grey", Category::Textile, F, {126, 126, 128}, 1.0),
			speckle("carpet-beige", Category::Textile, F, {198, 182, 158}, 1.0),
			speckle("carpet-blue", Category::Textile, F, {70, 88, 124}, 1.0),
			speckle("carpet-green", Category::Textile, F, {88, 112, 90}, 1.0),
			// Doors, windows and furniture.
			plain("lacquer-white", Category::Joinery, K | U, {240, 240, 236}, 0.35),
			plain("lacquer-black", Category::Joinery, K | U, {30, 30, 32}, 0.3),
			plain("lacquer-grey", Category::Joinery, K | U, {150, 152, 154}, 0.35),
			plain("aluminium-anthracite", Category::Joinery, K | U, {58, 60, 64}, 0.4),
			plain("steel-brushed", Category::Joinery, U, {176, 178, 180}, 0.35),
			plain("brass", Category::Joinery, K | U, {180, 146, 84}, 0.3),
			plain("ceramic-white", Category::Joinery, U, {245, 245, 243}, 0.12),
			// Upholstery and bedding (ADR-0016).
			speckle("fabric-grey", Category::Fabric, U, {118, 121, 126}, 0.95),
			speckle("fabric-sand", Category::Fabric, U, {196, 178, 150}, 0.95),
			speckle("fabric-blue", Category::Fabric, U, {74, 92, 128}, 0.95),
			speckle("fabric-green", Category::Fabric, U, {96, 120, 96}, 0.95),
			speckle("fabric-terracotta", Category::Fabric, U, {170, 98, 72}, 0.95),
			speckle("leather-brown", Category::Fabric, U, {110, 70, 44}, 0.5),
			speckle("leather-black", Category::Fabric, U, {36, 34, 34}, 0.45),
			plain("linen-white", Category::Fabric, U, {234, 230, 220}, 0.9),
			// Glass.
			glass("glass-clear", G, {214, 228, 230}, 0.03, 0.18),
			glass("glass-reflective", G, {172, 184, 190}, 0.06, 0.35, 0.65),
			glass("polycarbonate-opal", C | G, {236, 241, 239}, 0.42, 0.28),
			glass("glass-black", U, {22, 22, 24}, 0.08, 1.0),
			// Building services (ADR-0021): each kind of run in its colour.
			plain("service-electrical", Category::Services, S, {46, 156, 72}, 0.5),
			plain("service-water", Category::Services, S, {238, 238, 234}, 0.4),
			plain("service-sewage", Category::Services, S, {232, 186, 34}, 0.5),
			plain("service-gas", Category::Services, S, {212, 172, 64}, 0.3),
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
		if (isAssetRef(id))
		{
			return Status::success(); // an imported texture: the project checks it exists and allows the surface
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
