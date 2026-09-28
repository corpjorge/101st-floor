#include "F101/Cad/Wall.h"

#include "F101/Cad/Units.h"

namespace f101::cad
{
	namespace
	{
		bool inModelRange(std::int64_t value)
		{
			return value >= -MaxAbsMillimeters && value <= MaxAbsMillimeters;
		}
	}

	Status validateWall(const Wall& wall)
	{
		if (wall.id.isNil())
		{
			return Error{ErrorCode::InvalidArgument, "Wall has no identity"};
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
		return Status::success();
	}
}
