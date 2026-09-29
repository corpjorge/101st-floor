#include "F101/Cad/Project.h"

#include "F101/Cad/Units.h"

#include <string>

namespace f101::cad
{
	Status validateLevel(const Level& level)
	{
		if (level.id.isNil())
		{
			return Error{ErrorCode::InvalidArgument, "Level id is nil"};
		}
		if (level.heightMm <= 0 || level.heightMm > MaxAbsMillimeters)
		{
			return Error{ErrorCode::OutOfRange, "Level height must be positive"};
		}
		if (level.slabThicknessMm <= 0 || level.slabThicknessMm >= level.heightMm)
		{
			return Error{ErrorCode::OutOfRange, "The slab must be thinner than the level (" + std::to_string(level.heightMm) +
				" mm)"};
		}
		if (level.name.size() > MaxLevelNameBytes)
		{
			return Error{ErrorCode::OutOfRange, "Level name longer than " + std::to_string(MaxLevelNameBytes) + " bytes"};
		}
		return Status::success();
	}

	std::int64_t levelElevationMm(const Project& project, const LevelId& level)
	{
		std::int64_t elevation = 0;
		for (const Level& candidate : project.levels)
		{
			if (candidate.id == level)
			{
				return elevation;
			}
			elevation += candidate.heightMm;
		}
		return 0;
	}

	Project wallsOnLevel(const Project& project, const LevelId& level)
	{
		Project filtered;
		filtered.levels = project.levels;
		for (const Wall& wall : project.walls)
		{
			if (project.levelOf(wall) == level)
			{
				filtered.walls.push_back(wall);
			}
		}
		return filtered;
	}

	Project snapReference(const Project& project, const LevelId& level)
	{
		const std::size_t index = project.levelIndex(level).value_or(0);
		const LevelId below = index > 0 ? project.levels[index - 1].id : LevelId();
		Project reference;
		reference.levels = project.levels;
		for (const Wall& wall : project.walls)
		{
			const LevelId on = project.levelOf(wall);
			if (on == level || (!below.isNil() && on == below))
			{
				reference.walls.push_back(wall);
			}
		}
		return reference;
	}
}
