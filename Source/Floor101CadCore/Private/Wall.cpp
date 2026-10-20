#include "F101/Cad/Wall.h"

#include "F101/Cad/Units.h"
#include "F101/Cad/OpeningCatalogue.h"

#include <algorithm>
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

		// How it opens suits its kind, and how open it is shown is a percentage (ADR-0014).
		Status validateOperation(const Opening& opening)
		{
			if (!opening.family.empty())
			{
				const auto* family = findOpeningFamily(opening.family);
				if (!family || family->kind != opening.kind || family->operation != opening.operation)
				{
					return Error{ErrorCode::InvalidArgument, "Opening family does not match its kind and operation"};
				}
			}
			if (opening.kind == OpeningKind::Door && opening.operation == OpeningOperation::Fixed)
			{
				return Error{ErrorCode::InvalidArgument, "A door cannot be fixed"};
			}
			if (opening.openPercent < 0 || opening.openPercent > 100)
			{
				return Error{ErrorCode::OutOfRange, describe(opening) + " is open from 0 to 100 %"};
			}
			if (opening.operation == OpeningOperation::Fixed && opening.openPercent != 0)
			{
				return Error{ErrorCode::InvalidArgument, "A fixed window does not open"};
			}
			return Status::success();
		}

		Status validateOpenings(const Wall& wall)
		{
			// Openings must fit the axis length rounded down to whole millimetres.
			const auto length = static_cast<std::int64_t>(std::floor(wall.lengthMm()));
			std::int64_t previousEnd = 0;   // end of the previous hole
			std::int64_t previousTaken = 0; // furthest end of what the previous openings take (pockets included)
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
				if (Status valid = validateOperation(opening); !valid)
				{
					return valid;
				}
				const AxisSpan taken = occupiedSpan(opening);
				if (taken.fromMm < 0 || taken.toMm > length)
				{
					return Error{ErrorCode::OutOfRange, "The sliding door needs " + std::to_string(opening.widthMm) +
						" mm of wall beside it for its pocket"};
				}
				if (index > 0 && opening.offsetMm < previousEnd)
				{
					return Error{ErrorCode::InvalidArgument, describe(opening) + " overlaps another opening"};
				}
				if (index > 0 && taken.fromMm < previousTaken)
				{
					return Error{ErrorCode::InvalidArgument, opening.offsetMm < previousTaken
						? describe(opening) + " is in the pocket of a sliding door"
						: std::string("The sliding door's pocket overlaps another opening")};
				}
				previousEnd = opening.offsetMm + opening.widthMm;
				previousTaken = std::max(previousTaken, taken.toMm);
			}
			return Status::success();
		}
	}

	std::string_view kindName(OpeningKind kind)
	{
		return kind == OpeningKind::Door ? "door" : "window";
	}

	std::string_view operationName(OpeningOperation operation)
	{
		switch (operation)
		{
		case OpeningOperation::DoubleHinged: return "double";
		case OpeningOperation::Sliding: return "sliding";
		case OpeningOperation::Fixed: return "fixed";
		default: return "hinged";
		}
	}

	AxisSpan occupiedSpan(const Opening& opening)
	{
		AxisSpan span{opening.offsetMm, opening.offsetMm + opening.widthMm};
		if (opening.kind == OpeningKind::Door && opening.operation == OpeningOperation::Sliding)
		{
			if (opening.hinge == OpeningEnd::Start)
			{
				span.fromMm -= opening.widthMm;
			}
			else
			{
				span.toMm += opening.widthMm;
			}
		}
		return span;
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
