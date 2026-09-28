#pragma once

#include "F101/Cad/Wall.h"

#include <vector>

namespace f101::cad
{
	// The editable architectural model: the source of truth of 101st Floor.
	struct Project
	{
		std::vector<Wall> walls; // creation order

		const Wall* findWall(const WallId& id) const
		{
			for (const Wall& wall : walls)
			{
				if (wall.id == id)
				{
					return &wall;
				}
			}
			return nullptr;
		}
	};
}
