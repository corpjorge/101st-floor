#include "F101/Geometry/ModelMesh.h"

#include "F101/Geometry/Rooms.h"
#include "MeshBuilding.h"

#include <charconv>
#include <cmath>
#include <string>
#include <system_error>

namespace f101::geometry
{
	using namespace detail;

	std::string modelSectionName(std::size_t material)
	{
		return "m" + std::to_string(material);
	}

	std::optional<std::size_t> modelMaterialIndex(std::string_view section)
	{
		if (section.size() < 2 || section.front() != 'm')
		{
			return std::nullopt;
		}
		std::size_t index = 0;
		const char* first = section.data() + 1;
		const char* last = section.data() + section.size();
		const auto [end, error] = std::from_chars(first, last, index);
		if (error != std::errc() || end != last)
		{
			return std::nullopt;
		}
		return index;
	}

	MeshData buildModelFurnitureMesh(const cad::Furniture& furniture, const ModelMesh& model)
	{
		// Per-axis stretch from the model's size to the piece's; a flat axis stays flat.
		auto stretch = [](double target, double extent) { return extent > 1e-9 ? target / extent : 1.0; };
		const double sx = stretch(static_cast<double>(furniture.widthMm), model.widthMm);
		const double sy = stretch(static_cast<double>(furniture.depthMm), model.depthMm);
		const double sz = stretch(static_cast<double>(furniture.heightMm), model.heightMm);
		const cad::FurnitureAxes axes = cad::furnitureAxes(furniture);
		const double cx = static_cast<double>(furniture.position.x_mm);
		const double cy = static_cast<double>(furniture.position.y_mm);

		MeshData mesh = model.mesh;
		for (Vec3& p : mesh.positions)
		{
			const double x = p.x * sx;
			const double y = p.y * sy;
			p = Vec3{cx + axes.x.x_mm * x + axes.y.x_mm * y, cy + axes.x.y_mm * x + axes.y.y_mm * y, FloorThicknessMm + p.z * sz};
		}
		for (Vec3& n : mesh.normals)
		{
			// Normals of a stretched surface: divide by the stretch (inverse transpose), then turn.
			const double x = n.x / sx;
			const double y = n.y / sy;
			const double z = n.z / sz;
			const double length = std::sqrt(x * x + y * y + z * z);
			const double nx = length > 0.0 ? x / length : 0.0;
			const double ny = length > 0.0 ? y / length : 0.0;
			n = Vec3{axes.x.x_mm * nx + axes.y.x_mm * ny, axes.x.y_mm * nx + axes.y.y_mm * ny, length > 0.0 ? z / length : 1.0};
		}
		return mesh;
	}

	MeshData buildPlaceholderFurnitureMesh(const cad::Furniture& furniture)
	{
		MeshData mesh;
		const cad::FurnitureAxes axes = cad::furnitureAxes(furniture);
		const double w = static_cast<double>(furniture.widthMm) / 2.0;
		const double d = static_cast<double>(furniture.depthMm) / 2.0;
		addWallBox(mesh, "body", toPlan(furniture.position), axes.x, axes.y, -w, w, -d, d, FloorThicknessMm,
			FloorThicknessMm + static_cast<double>(furniture.heightMm));
		return mesh;
	}
}
