#pragma once

#include "F101/Cad/Project.h"

namespace f101::cad
{
	// Operates on a prospective project only. The caller records the complete result in one transaction.
	Status splitJoinedWalls(Project& project, const std::vector<WallId>& edited, const UuidSource& newUuid);
}
