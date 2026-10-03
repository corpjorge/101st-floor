#include "F101/Geometry/ServicesBuilder.h"

#include "F101/Geometry/PolygonGeometry.h"
#include "MeshBuilding.h"

#include <cmath>

namespace f101::geometry
{
	using namespace detail;

	std::vector<cad::PlanPoint> columnOutline(const cad::Column& column)
	{
		if (column.shape == cad::ColumnShape::Rectangle)
		{
			const std::array<cad::PlanPoint, 4> corners = cad::columnFootprint(column);
			return std::vector<cad::PlanPoint>(corners.begin(), corners.end());
		}
		constexpr double Tau = 6.28318530717958647692;
		const double radius = static_cast<double>(column.widthMm) / 2.0;
		std::vector<cad::PlanPoint> outline;
		for (int i = 0; i < RoundColumnSides; ++i)
		{
			const double angle = Tau * static_cast<double>(i) / static_cast<double>(RoundColumnSides);
			outline.push_back(cad::PlanPoint{static_cast<double>(column.position.x_mm) + radius * std::cos(angle),
				static_cast<double>(column.position.y_mm) + radius * std::sin(angle)});
		}
		return outline;
	}

	MeshData buildColumnMesh(const cad::Column& column, double heightMm)
	{
		if (heightMm <= 0.0)
		{
			return {};
		}
		if (column.shape == cad::ColumnShape::Rectangle)
		{
			return buildPrismMesh(columnOutline(column), 0.0, heightMm,
				PrismSlots{services_slots::Column, services_slots::Column, services_slots::Column});
		}
		MeshData mesh;
		const Vec3 foot{static_cast<double>(column.position.x_mm), static_cast<double>(column.position.y_mm), 0.0};
		addTube(mesh, services_slots::Column, foot, Vec3{foot.x, foot.y, heightMm}, static_cast<double>(column.widthMm) / 2.0,
			RoundColumnSides, 0.0, true, true);
		return mesh;
	}

	MeshData buildServiceRunMesh(const cad::ServiceRun& run)
	{
		MeshData mesh;
		if (run.points.size() < 2)
		{
			return mesh;
		}
		const double radius = static_cast<double>(cad::serviceKindInfo(run.kind).diameterMm) / 2.0;
		const auto at = [](const cad::ServicePoint& p)
		{
			return Vec3{static_cast<double>(p.x_mm), static_cast<double>(p.y_mm), static_cast<double>(p.z_mm)};
		};
		double along = 0.0;
		for (std::size_t index = 1; index < run.points.size(); ++index)
		{
			const Vec3 a = at(run.points[index - 1]);
			const Vec3 b = at(run.points[index]);
			addTube(mesh, services_slots::Pipe, a, b, radius, PipeSides, along, index == 1, index + 1 == run.points.size());
			const Vec3 step = subtract(b, a);
			along += std::sqrt(dot(step, step));
			if (index + 1 < run.points.size())
			{
				addBall(mesh, services_slots::Pipe, b, radius, PipeSides / 2, PipeSides);
			}
		}
		return mesh;
	}
}
