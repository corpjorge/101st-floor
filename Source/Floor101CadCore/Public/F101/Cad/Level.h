#pragma once

#include "F101/Cad/Export.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"

#include <cstdint>
#include <string>

namespace f101::cad
{
	inline constexpr std::int64_t DefaultLevelHeightMm = 2800;
	inline constexpr std::int64_t DefaultSlabThicknessMm = 250;
	inline constexpr std::size_t MaxLevelNameBytes = 200;

	// A floor of the building (ADR-0011). Levels are stacked in Project::levels from the bottom up: the first one
	// stands at elevation 0 and each next one starts where the previous one ends, so elevations are derived, never
	// stored. The slab of a level is the top band of its height: the ceiling of its rooms and the floor of the next
	// level.
	struct Level
	{
		LevelId id;
		std::string name; // UTF-8; empty: the application shows a default name ("Planta baja", "Planta 1"...)
		std::int64_t heightMm = DefaultLevelHeightMm; // floor to floor
		std::int64_t slabThicknessMm = DefaultSlabThicknessMm;

		friend bool operator==(const Level&, const Level&) = default;

		// What new walls on this level get: the full floor-to-floor height, so walls run up to the next level and the
		// slab fits between them.
		std::int64_t wallHeightMm() const { return heightMm; }
		// Clear height of its rooms, under the slab.
		std::int64_t clearHeightMm() const { return heightMm - slabThicknessMm; }
	};

	// Non-nil id, height within the model range, a slab thinner than the level, a name of at most 200 bytes.
	FLOOR101CADCORE_API Status validateLevel(const Level& level);
}
