#include "F101/Cad/Project.h"

#include "F101/Cad/Units.h"

#include <algorithm>
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

	Status validateFinishIn(const Project& project, std::string_view finish, Surface surface)
	{
		if (Status valid = validateFinish(finish, surface); !valid)
		{
			return valid;
		}
		const std::optional<AssetId> id = parseAssetRef(finish);
		if (!id)
		{
			return Status::success();
		}
		const Asset* asset = project.findAsset(*id);
		if (!asset || asset->kind != AssetKind::Texture)
		{
			return Error{ErrorCode::NotFound, "No imported texture " + std::string(finish)};
		}
		if ((asset->texture.surfaces & static_cast<std::uint8_t>(surface)) == 0)
		{
			return Error{ErrorCode::InvalidArgument, "The texture '" + asset->name + "' is not for that surface"};
		}
		return Status::success();
	}

	std::optional<FurnitureType> furnitureSpec(const Project& project, std::string_view type)
	{
		if (const std::optional<AssetId> id = parseAssetRef(type))
		{
			const Asset* asset = project.findAsset(*id);
			if (!asset || asset->kind != AssetKind::Model)
			{
				return std::nullopt;
			}
			const auto size = [](std::int64_t mm) { return std::clamp(mm, MinModelSizeMm, MaxModelSizeMm); };
			return FurnitureType{type, size(asset->model.widthMm), size(asset->model.depthMm), size(asset->model.heightMm),
				MinModelSizeMm, MaxModelSizeMm, MinModelSizeMm, MaxModelSizeMm, MinModelSizeMm, MaxModelSizeMm,
				asset->model.againstWall, {}};
		}
		const FurnitureType* found = findFurnitureType(type);
		return found ? std::optional<FurnitureType>(*found) : std::nullopt;
	}

	Status validateFurnitureIn(const Project& project, const Furniture& furniture)
	{
		if (Status valid = validateFurniture(furniture); !valid)
		{
			return valid;
		}
		if (isAssetRef(furniture.type) && !furnitureSpec(project, furniture.type))
		{
			return Error{ErrorCode::NotFound, "No imported model " + furniture.type};
		}
		return validateFinishIn(project, furniture.finish, Surface::Furniture);
	}

	std::size_t assetUses(const Project& project, const AssetId& asset)
	{
		const std::string ref = assetRef(asset);
		std::size_t uses = 0;
		for (const Wall& wall : project.walls)
		{
			uses += static_cast<std::size_t>(wall.finishLeft == ref) + static_cast<std::size_t>(wall.finishRight == ref);
			for (const Opening& opening : wall.openings)
			{
				uses += static_cast<std::size_t>(opening.finish == ref);
			}
		}
		for (const RoomFinish& finish : project.roomFinishes)
		{
			uses += static_cast<std::size_t>(finish.floor == ref) + static_cast<std::size_t>(finish.ceiling == ref);
		}
		for (const Furniture& piece : project.furniture)
		{
			uses += static_cast<std::size_t>(piece.type == ref) + static_cast<std::size_t>(piece.finish == ref);
		}
		return uses;
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
