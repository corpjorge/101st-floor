#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Result.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Finishes (ADR-0012): the surface materials of walls, floors and ceilings, from a fixed catalogue. The model stores a
// finish id per surface; the appearance below is renderer-neutral (sRGB colours, roughness, a repeating pattern) so the
// Unreal adapter and future exporters build their materials from the same data. Ids are stable: new finishes are added,
// none is renamed or removed (projects refer to them).
namespace f101::cad
{
	// Catalogue id, e.g. "brick-red". Empty: the surface's default finish.
	using FinishId = std::string;

	enum class Surface : std::uint8_t
	{
		Wall = 1,
		Floor = 2,
		Ceiling = 4,
		Carpentry = 8, // frames and leaves of doors and windows (ADR-0013)
		Glazing = 16,  // window panes
		Furniture = 32, // furniture and fittings (ADR-0016)
		Services = 64,  // pipes and conduits of the building services (ADR-0021): their kind's colour, never chosen
	};

	enum class FinishPattern
	{
		Plain,       // one colour
		Speckle,     // fine noise (concrete, screed, carpet)
		Bricks,      // running bond: rows of units offset by half a unit
		Tiles,       // grid of units
		Planks,      // rows of long units with staggered ends and a grain
		Herringbone, // planks at right angles stepping diagonally (length a whole number of widths)
		Checker,     // grid of units alternating the colour and the accent colour
	};

	// What kind of material a finish is, to group the catalogue (ADR-0012; MVP 11).
	enum class FinishCategory
	{
		Paint,    // wall and ceiling paints
		Tiles,    // ceramic and porcelain tiles
		Wood,     // floorboards, laminate, parquet, panelling
		Stone,    // concrete, screed, microcement, brick
		Textile,  // carpet
		Joinery,  // lacquers and metals for doors, windows and furniture
		Fabric,   // upholstery and bedding
		Glass,
		Services, // the colours of the building services' pipes (ADR-0021)
	};

	struct Rgb8
	{
		std::uint8_t r = 0;
		std::uint8_t g = 0;
		std::uint8_t b = 0;

		friend bool operator==(const Rgb8&, const Rgb8&) = default;
	};

	struct FinishInfo
	{
		std::string_view id;
		FinishCategory category = FinishCategory::Paint;
		std::uint8_t surfaces = 0; // Surface flags it may be applied to
		Rgb8 color;                // base colour (sRGB)
		Rgb8 jointColor;           // mortar, grout or the gap between planks (patterns with units)
		double roughness = 0.8;    // 0 mirror .. 1 matte
		FinishPattern pattern = FinishPattern::Plain;
		std::int64_t unitWidthMm = 0; // size of one brick, tile or plank (patterns with units; a plank's length)
		std::int64_t unitHeightMm = 0;
		std::int64_t jointMm = 0;
		double opacity = 1.0; // 1: opaque; less: see-through (glass)
		double metallic = 0.0; // reflective coatings, portable PBR approximation
		Rgb8 accentColor;     // Checker: every other unit's colour

		bool allows(Surface surface) const { return (surfaces & static_cast<std::uint8_t>(surface)) != 0; }
	};

	inline constexpr std::string_view DefaultWallFinish = "plaster-white";
	inline constexpr std::string_view DefaultFloorFinish = "screed-light";
	inline constexpr std::string_view DefaultCeilingFinish = "plaster-white";
	inline constexpr std::string_view DefaultCarpentryFinish = "lacquer-white";
	inline constexpr std::string_view DefaultGlazingFinish = "glass-clear";
	// Door handles and other fittings (not chosen by the user).
	inline constexpr std::string_view HardwareFinish = "aluminium-anthracite";

	// "paint", "tiles", "wood", "stone", "textile", "joinery", "fabric", "glass", "services" (library files).
	FLOOR101CADCORE_API std::string_view finishCategoryId(FinishCategory category);
	FLOOR101CADCORE_API std::optional<FinishCategory> parseFinishCategory(std::string_view id);

	// Every finish, in display order.
	FLOOR101CADCORE_API const std::vector<FinishInfo>& finishCatalogue();
	// Null if the id is not in the catalogue.
	FLOOR101CADCORE_API const FinishInfo* findFinish(std::string_view id);
	// The finish a surface shows: `id`, or the surface's default when it is empty.
	FLOOR101CADCORE_API std::string_view resolveFinish(std::string_view id, Surface surface);
	// Empty (the default), a catalogue finish that allows the surface, or an imported texture ("asset:<uuid>", ADR-0017),
	// which only the project can check (validateFinishIn).
	FLOOR101CADCORE_API Status validateFinish(std::string_view id, Surface surface);
}
