#include "F101/Cad/Wall.h"

#include "F101/Cad/Units.h"

#include <cmath>
#include <string>

namespace f101::cad
{
	namespace
	{
		bool inModelRange(std::int64_t value)
		{
			return value >= -MaxAbsMillimeters && value <= MaxAbsMillimeters;
		}

		std::string describe(const Opening& opening)
		{
			return std::string(opening.kind == OpeningKind::Door ? "The door" : "The window");
		}

		Status validateOpenings(const Wall& wall)
		{
			// Openings must fit the axis length rounded down to whole millimetres.
			const auto length = static_cast<std::int64_t>(std::floor(wall.lengthMm()));
			std::int64_t previousEnd = 0;
			for (std::size_t index = 0; index < wall.openings.size(); ++index)
			{
				const Opening& opening = wall.openings[index];
				if (opening.widthMm <= 0 || opening.heightMm <= 0)
				{
					return Error{ErrorCode::OutOfRange, describe(opening) + " needs a positive width and height"};
				}
				if (opening.sillMm < 0 || opening.sillMm > MaxAbsMillimeters ||
					(opening.kind == OpeningKind::Door && opening.sillMm != 0))
				{
					return Error{ErrorCode::OutOfRange, opening.kind == OpeningKind::Door
						? "A door starts at the floor (sill 0)" : "The window sill cannot be negative"};
				}
				if (opening.offsetMm < 0 || opening.widthMm > MaxAbsMillimeters || opening.offsetMm > length - opening.widthMm)
				{
					return Error{ErrorCode::OutOfRange, describe(opening) + " does not fit along the wall (" +
						std::to_string(length) + " mm)"};
				}
				if (opening.heightMm > MaxAbsMillimeters || opening.sillMm + opening.heightMm >= wall.heightMm)
				{
					return Error{ErrorCode::OutOfRange, describe(opening) + " must stay below the top of the wall (" +
						std::to_string(wall.heightMm) + " mm)"};
				}
				if (index > 0 && opening.offsetMm < previousEnd)
				{
					return Error{ErrorCode::InvalidArgument, describe(opening) + " overlaps another opening"};
				}
				previousEnd = opening.offsetMm + opening.widthMm;
			}
			return Status::success();
		}
	}

	std::string_view kindName(OpeningKind kind)
	{
		return kind == OpeningKind::Door ? "door" : "window";
	}

	Status validateWall(const Wall& wall)
	{
		if (wall.id.isNil())
		{
			return Error{ErrorCode::InvalidArgument, "Wall has no identity"};
		}
		for (const Opening& opening : wall.openings)
		{
			if (opening.id.isNil())
			{
				return Error{ErrorCode::InvalidArgument, "Opening has no identity"};
			}
			if (Status valid = validateFinish(opening.finish, Surface::Carpentry); !valid)
			{
				return valid;
			}
		}
		for (const WallSide side : {WallSide::Left, WallSide::Right})
		{
			if (Status valid = validateFinish(wall.finish(side), Surface::Wall); !valid)
			{
				return valid;
			}
		}
		return validateWallGeometry(wall);
	}

	Status validateWallGeometry(const Wall& wall)
	{
		if (!inModelRange(wall.start.x_mm) || !inModelRange(wall.start.y_mm) || !inModelRange(wall.end.x_mm) ||
			!inModelRange(wall.end.y_mm))
		{
			return Error{ErrorCode::OutOfRange, "Wall endpoint outside the model range"};
		}
		if (wall.start == wall.end)
		{
			return Error{ErrorCode::InvalidArgument, "Wall start and end coincide"};
		}
		if (wall.thicknessMm <= 0 || wall.thicknessMm > MaxAbsMillimeters)
		{
			return Error{ErrorCode::OutOfRange, "Wall thickness must be positive"};
		}
		if (wall.heightMm <= 0 || wall.heightMm > MaxAbsMillimeters)
		{
			return Error{ErrorCode::OutOfRange, "Wall height must be positive"};
		}
		return validateOpenings(wall);
	}
}
