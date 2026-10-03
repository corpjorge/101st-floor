#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

// Building services (ADR-0021): runs of electrical conduit, clean water, sewage and gas pipe on a level, each a polyline
// through points in 3D (plan position plus height over the level's floor), so a run can go under the floor, up a wall and
// along the ceiling. Their kind gives their colour and diameter.
namespace f101::cad
{
	enum class ServiceKind
	{
		Electrical, // thin green conduit
		Water,      // thin white pipe (clean water)
		Sewage,     // thick yellow pipe (sewage)
		Gas,        // thin gold pipe
	};

	struct ServiceKindInfo
	{
		ServiceKind kind = ServiceKind::Electrical;
		std::string_view id;     // "electrical", "water", "sewage", "gas" (project.json)
		std::int64_t diameterMm = 25;
		std::string_view finish; // its colour: a catalogue finish for Surface::Services
	};

	// Every kind, in display order.
	FLOOR101CADCORE_API const std::vector<ServiceKindInfo>& serviceKinds();
	FLOOR101CADCORE_API const ServiceKindInfo& serviceKindInfo(ServiceKind kind);
	FLOOR101CADCORE_API std::optional<ServiceKind> parseServiceKind(std::string_view id);

	// Which kinds of run a view shows (the networks of the services layer): every kind unless told otherwise.
	struct ServiceKindSet
	{
		std::uint8_t bits = 0x0F; // bit i: ServiceKind i

		bool contains(ServiceKind kind) const { return ((bits >> static_cast<unsigned>(kind)) & 1U) != 0U; }
		void set(ServiceKind kind, bool shown)
		{
			const auto bit = static_cast<std::uint8_t>(1U << static_cast<unsigned>(kind));
			bits = shown ? static_cast<std::uint8_t>(bits | bit) : static_cast<std::uint8_t>(bits & ~bit);
		}

		friend bool operator==(const ServiceKindSet&, const ServiceKindSet&) = default;
	};

	// A point of a run: plan mm local to its level, and the height of the pipe's axis over the level's floor.
	struct ServicePoint
	{
		std::int64_t x_mm = 0;
		std::int64_t y_mm = 0;
		std::int64_t z_mm = 0;

		friend bool operator==(const ServicePoint&, const ServicePoint&) = default;
	};

	struct ServiceRun
	{
		ServiceRunId id;
		LevelId level;
		ServiceKind kind = ServiceKind::Electrical;
		std::vector<ServicePoint> points; // two or more; consecutive points differ

		friend bool operator==(const ServiceRun&, const ServiceRun&) = default;
	};

	inline constexpr std::size_t MaxServicePoints = 1000;
	// Heights a pipe may take over its level's floor: from under the slab of the level below up to well above the level.
	inline constexpr std::int64_t MinServiceHeightMm = -5000;
	inline constexpr std::int64_t MaxServiceHeightMm = 50'000;

	// 2 to MaxServicePoints points, consecutive ones different, positions in the model range and heights in
	// [MinServiceHeightMm, MaxServiceHeightMm].
	FLOOR101CADCORE_API Status validateServiceRun(const ServiceRun& run);
	// Its length along every segment, vertical ones included.
	FLOOR101CADCORE_API double serviceRunLengthMm(const ServiceRun& run);
}
