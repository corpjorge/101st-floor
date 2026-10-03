#include "F101/Cad/Services.h"

#include "F101/Cad/Units.h"

#include <cmath>
#include <string>

namespace f101::cad
{
	const std::vector<ServiceKindInfo>& serviceKinds()
	{
		// Thin green electrical conduit, thin white water pipe, thick yellow sewage pipe, thin gold gas pipe.
		static const std::vector<ServiceKindInfo> kinds{
			{ServiceKind::Electrical, "electrical", 25, "service-electrical"},
			{ServiceKind::Water, "water", 25, "service-water"},
			{ServiceKind::Sewage, "sewage", 110, "service-sewage"},
			{ServiceKind::Gas, "gas", 22, "service-gas"},
		};
		return kinds;
	}

	const ServiceKindInfo& serviceKindInfo(ServiceKind kind)
	{
		for (const ServiceKindInfo& info : serviceKinds())
		{
			if (info.kind == kind)
			{
				return info;
			}
		}
		return serviceKinds().front();
	}

	std::optional<ServiceKind> parseServiceKind(std::string_view id)
	{
		for (const ServiceKindInfo& info : serviceKinds())
		{
			if (info.id == id)
			{
				return info.kind;
			}
		}
		return std::nullopt;
	}

	Status validateServiceRun(const ServiceRun& run)
	{
		if (run.points.size() < 2 || run.points.size() > MaxServicePoints)
		{
			return Error{ErrorCode::InvalidArgument, "A run goes through 2 to " + std::to_string(MaxServicePoints) + " points"};
		}
		for (std::size_t index = 0; index < run.points.size(); ++index)
		{
			const ServicePoint& point = run.points[index];
			for (const std::int64_t coordinate : {point.x_mm, point.y_mm})
			{
				if (coordinate < -MaxAbsMillimeters || coordinate > MaxAbsMillimeters)
				{
					return Error{ErrorCode::OutOfRange, "A run's point is outside the model range"};
				}
			}
			if (point.z_mm < MinServiceHeightMm || point.z_mm > MaxServiceHeightMm)
			{
				return Error{ErrorCode::OutOfRange, "A run goes from " + formatLength(static_cast<double>(MinServiceHeightMm), LengthUnit::Meter) +
					" to " + formatLength(static_cast<double>(MaxServiceHeightMm), LengthUnit::Meter) + " over its level's floor"};
			}
			if (index > 0 && point == run.points[index - 1])
			{
				return Error{ErrorCode::InvalidArgument, "A run cannot go through the same point twice in a row"};
			}
		}
		return Status::success();
	}

	double serviceRunLengthMm(const ServiceRun& run)
	{
		double length = 0.0;
		for (std::size_t index = 1; index < run.points.size(); ++index)
		{
			const ServicePoint& a = run.points[index - 1];
			const ServicePoint& b = run.points[index];
			length += std::sqrt(std::pow(static_cast<double>(b.x_mm - a.x_mm), 2.0) + std::pow(static_cast<double>(b.y_mm - a.y_mm), 2.0) +
				std::pow(static_cast<double>(b.z_mm - a.z_mm), 2.0));
		}
		return length;
	}
}
