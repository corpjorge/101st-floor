#include "F101/Cad/Project.h"

#include "F101/Cad/Units.h"

#include <algorithm>
#include <string>

namespace f101::cad
{
	Status validateDimension(const Project& project, const Dimension& dimension)
	{
		const auto within = [](std::int64_t value) { return value >= -MaxAbsMillimeters && value <= MaxAbsMillimeters; };
		if (dimension.id.empty() || dimension.id.size() > 512 || dimension.start == dimension.end ||
			!within(dimension.start.x_mm) || !within(dimension.start.y_mm) || !within(dimension.end.x_mm) ||
			!within(dimension.end.y_mm) || !within(dimension.offsetMm) || !project.findLevel(dimension.level))
			return Error{ErrorCode::InvalidArgument, "A dimension needs an id, existing level and distinct endpoints within the model range"};
		if (dimension.automatic != dimension.id.starts_with("auto/"))
			return Error{ErrorCode::InvalidArgument, "Automatic dimension identity mismatch"};
		return validateRoomName(dimension.text);
	}

	std::pair<PlanPoint, PlanPoint> dimensionLine(const Dimension& dimension)
	{
		const Vector2D axis = dimension.end - dimension.start;
		const double length = axis.lengthMm();
		const double nx = length > 0.0 ? -static_cast<double>(axis.dy_mm) / length : 0.0;
		const double ny = length > 0.0 ? static_cast<double>(axis.dx_mm) / length : 0.0;
		const double offset = static_cast<double>(dimension.offsetMm);
		return {{static_cast<double>(dimension.start.x_mm) + nx * offset, static_cast<double>(dimension.start.y_mm) + ny * offset},
			{static_cast<double>(dimension.end.x_mm) + nx * offset, static_cast<double>(dimension.end.y_mm) + ny * offset}};
	}

	PlanPoint dimensionLabelPoint(const Dimension& dimension)
	{
		const auto [a, b] = dimensionLine(dimension);
		const Vector2D axis = dimension.end - dimension.start;
		const double length = axis.lengthMm();
		const double sign = dimension.offsetMm < 0 ? -1.0 : 1.0;
		return {(a.x_mm + b.x_mm) / 2.0 - static_cast<double>(axis.dy_mm) / length * sign * 135.0,
			(a.y_mm + b.y_mm) / 2.0 + static_cast<double>(axis.dx_mm) / length * sign * 135.0};
	}

	Status validateRoomBaseboard(const Project& project, const RoomBaseboard& baseboard)
	{
		if (baseboard.room.empty() || baseboard.heightMm < 1 || baseboard.heightMm > 1000 ||
			baseboard.thicknessMm < 1 || baseboard.thicknessMm > 100)
		{
			return Error{ErrorCode::InvalidArgument, "Baseboards need a room, height 1..1000 mm and thickness 1..100 mm"};
		}
		return validateFinishIn(project, baseboard.finish, Surface::Carpentry);
	}

	Status validateRoomName(std::string_view name)
	{
		if (name.size() > MaxRoomNameBytes || std::any_of(name.begin(), name.end(), [](unsigned char ch) { return ch < 32 || ch == 127; }))
		{
			return Error{ErrorCode::InvalidArgument, "Room names must be a single line of at most 256 UTF-8 bytes"};
		}
		return Status::success();
	}

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
		for (const RoomBaseboard& entry : project.roomBaseboards) uses += entry.finish == ref ? 1u : 0u;
		for (const RoomFinish& finish : project.roomFinishes)
		{
			uses += static_cast<std::size_t>(finish.floor == ref) + static_cast<std::size_t>(finish.ceiling == ref);
		}
		for (const Furniture& piece : project.furniture)
		{
			uses += static_cast<std::size_t>(piece.type == ref) + static_cast<std::size_t>(piece.finish == ref);
		}
		for (const Column& column : project.columns)
		{
			uses += static_cast<std::size_t>(column.finish == ref);
		}
		return uses;
	}

	Status validateColumnIn(const Project& project, const Column& column)
	{
		if (Status valid = validateColumn(column); !valid)
		{
			return valid;
		}
		return validateFinishIn(project, column.finish, Surface::Wall);
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
