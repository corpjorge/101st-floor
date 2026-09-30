#include "F101/Cad/Furniture.h"

#include "F101/Cad/Units.h"

#include <cmath>
#include <string>

namespace f101::cad
{
	namespace
	{
		std::string range(std::int64_t low, std::int64_t high)
		{
			return std::to_string(low) + " to " + std::to_string(high) + " mm";
		}
	}

	const std::vector<FurnitureType>& furnitureCatalogue()
	{
		// id, default W D H, W range, D range, H range, against a wall, body finish.
		static const std::vector<FurnitureType> catalogue{
			{"bed-double", 1500, 2000, 900, 1350, 1800, 1900, 2200, 600, 1300, true, "wood-oak"},
			{"bed-single", 900, 2000, 900, 800, 1200, 1900, 2200, 600, 1300, true, "wood-oak"},
			{"sofa", 2000, 900, 850, 1400, 3200, 800, 1100, 700, 1000, true, "fabric-grey"},
			{"table", 1600, 900, 750, 700, 3000, 600, 1400, 650, 1100, false, "wood-oak"},
			{"chair", 450, 500, 900, 400, 600, 400, 600, 750, 1100, false, "wood-oak"},
			{"wardrobe", 1200, 600, 2200, 500, 3600, 450, 700, 1600, 2700, true, "lacquer-white"},
			{"kitchen-counter", 2400, 600, 900, 600, 6000, 550, 700, 850, 950, true, "lacquer-white"},
			{"fridge", 600, 650, 1850, 550, 950, 600, 750, 1400, 2100, true, "steel-brushed"},
			{"toilet", 380, 700, 780, 350, 450, 600, 800, 700, 850, true, "ceramic-white"},
			{"washbasin", 600, 460, 850, 400, 1600, 400, 600, 800, 950, true, "lacquer-white"},
			{"bathtub", 1700, 750, 550, 1400, 1900, 700, 900, 450, 650, true, "ceramic-white"},
			{"shower-tray", 900, 900, 40, 700, 1800, 700, 1200, 20, 120, true, "ceramic-white"},
		};
		return catalogue;
	}

	const FurnitureType* findFurnitureType(std::string_view id)
	{
		for (const FurnitureType& type : furnitureCatalogue())
		{
			if (type.id == id)
			{
				return &type;
			}
		}
		return nullptr;
	}

	Furniture makeFurniture(std::string_view type, Point2D position, std::int64_t rotationCentidegrees)
	{
		Furniture furniture;
		furniture.type = std::string(type);
		furniture.position = position;
		furniture.rotationCentidegrees = rotationCentidegrees;
		if (const FurnitureType* found = findFurnitureType(type))
		{
			furniture.widthMm = found->widthMm;
			furniture.depthMm = found->depthMm;
			furniture.heightMm = found->heightMm;
		}
		return furniture;
	}

	std::string_view resolveFurnitureFinish(const Furniture& furniture)
	{
		if (!furniture.finish.empty())
		{
			return furniture.finish;
		}
		const FurnitureType* type = findFurnitureType(furniture.type);
		return type ? type->bodyFinish : DefaultCarpentryFinish;
	}

	Status validateFurniture(const Furniture& furniture)
	{
		const FurnitureType* type = findFurnitureType(furniture.type);
		if (!type)
		{
			return Error{ErrorCode::NotFound, "Unknown furniture type '" + furniture.type + "'"};
		}
		if (furniture.widthMm < type->minWidthMm || furniture.widthMm > type->maxWidthMm)
		{
			return Error{ErrorCode::OutOfRange, "Its width goes from " + range(type->minWidthMm, type->maxWidthMm)};
		}
		if (furniture.depthMm < type->minDepthMm || furniture.depthMm > type->maxDepthMm)
		{
			return Error{ErrorCode::OutOfRange, "Its depth goes from " + range(type->minDepthMm, type->maxDepthMm)};
		}
		if (furniture.heightMm < type->minHeightMm || furniture.heightMm > type->maxHeightMm)
		{
			return Error{ErrorCode::OutOfRange, "Its height goes from " + range(type->minHeightMm, type->maxHeightMm)};
		}
		if (furniture.rotationCentidegrees < 0 || furniture.rotationCentidegrees >= FullTurnCentidegrees)
		{
			return Error{ErrorCode::OutOfRange, "A rotation goes from 0 to 359.99 degrees"};
		}
		for (const std::int64_t coordinate : {furniture.position.x_mm, furniture.position.y_mm})
		{
			if (coordinate < -MaxAbsMillimeters || coordinate > MaxAbsMillimeters)
			{
				return Error{ErrorCode::OutOfRange, "Furniture position outside the model range"};
			}
		}
		return validateFinish(furniture.finish, Surface::Furniture);
	}

	std::int64_t toCentidegrees(double degrees)
	{
		const auto hundredths = static_cast<std::int64_t>(std::llround(degrees * 100.0));
		return ((hundredths % FullTurnCentidegrees) + FullTurnCentidegrees) % FullTurnCentidegrees;
	}

	FurnitureAxes furnitureAxes(const Furniture& furniture)
	{
		// Quarter turns exactly, so a piece square to the axes stays on whole millimetres.
		switch (furniture.rotationCentidegrees)
		{
		case 0: return {{1.0, 0.0}, {0.0, 1.0}};
		case 9000: return {{0.0, 1.0}, {-1.0, 0.0}};
		case 18000: return {{-1.0, 0.0}, {0.0, -1.0}};
		case 27000: return {{0.0, -1.0}, {1.0, 0.0}};
		default: break;
		}
		constexpr double RadiansPerCentidegree = 3.14159265358979323846 / 18000.0;
		const double angle = static_cast<double>(furniture.rotationCentidegrees) * RadiansPerCentidegree;
		const double c = std::cos(angle);
		const double s = std::sin(angle);
		return {{c, s}, {-s, c}};
	}

	PlanPoint furnitureToPlan(const Furniture& furniture, double x, double y)
	{
		const FurnitureAxes axes = furnitureAxes(furniture);
		return PlanPoint{static_cast<double>(furniture.position.x_mm) + axes.x.x_mm * x + axes.y.x_mm * y,
			static_cast<double>(furniture.position.y_mm) + axes.x.y_mm * x + axes.y.y_mm * y};
	}

	PlanPoint planToFurniture(const Furniture& furniture, PlanPoint point)
	{
		const FurnitureAxes axes = furnitureAxes(furniture);
		const double dx = point.x_mm - static_cast<double>(furniture.position.x_mm);
		const double dy = point.y_mm - static_cast<double>(furniture.position.y_mm);
		return PlanPoint{dx * axes.x.x_mm + dy * axes.x.y_mm, dx * axes.y.x_mm + dy * axes.y.y_mm};
	}

	std::array<PlanPoint, 4> furnitureFootprint(const Furniture& furniture)
	{
		const double w = static_cast<double>(furniture.widthMm) / 2.0;
		const double d = static_cast<double>(furniture.depthMm) / 2.0;
		return {furnitureToPlan(furniture, -w, -d), furnitureToPlan(furniture, w, -d), furnitureToPlan(furniture, w, d),
			furnitureToPlan(furniture, -w, d)};
	}

	bool furnitureContains(const Furniture& furniture, PlanPoint point, double marginMm)
	{
		const PlanPoint local = planToFurniture(furniture, point);
		return std::abs(local.x_mm) <= static_cast<double>(furniture.widthMm) / 2.0 + marginMm &&
			std::abs(local.y_mm) <= static_cast<double>(furniture.depthMm) / 2.0 + marginMm;
	}
}
