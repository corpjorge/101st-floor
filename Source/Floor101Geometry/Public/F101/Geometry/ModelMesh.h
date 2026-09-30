#pragma once

#include "F101/Cad/Furniture.h"
#include "F101/Geometry/MeshData.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// Imported models (ADR-0017): a model's triangles once read, in the frame of a piece of furniture, and placed for each
// piece that uses it.
namespace f101::geometry
{
	// A model in the frame of furniture: x across its front, centred; y from its front (-) to its back (+), centred; z up
	// from 0; millimetres. One section per material, named "m<index>" (the index into the model's materials).
	struct ModelMesh
	{
		MeshData mesh;
		double widthMm = 0.0; // its extent on each axis
		double depthMm = 0.0;
		double heightMm = 0.0;
	};

	// Section name of a model material, and back.
	FLOOR101GEOMETRY_API std::string modelSectionName(std::size_t material);
	FLOOR101GEOMETRY_API std::optional<std::size_t> modelMaterialIndex(std::string_view section);

	// The model stretched on each axis to the piece's width, depth and height, turned and placed like buildFurnitureMesh
	// (standing on a room floor, z = FloorThicknessMm). Normals follow the stretch; UVs are the model's own.
	FLOOR101GEOMETRY_API MeshData buildModelFurnitureMesh(const cad::Furniture& furniture, const ModelMesh& model);

	// A plain box of the piece's size, section "body": stands in for a model whose file is missing or unreadable.
	FLOOR101GEOMETRY_API MeshData buildPlaceholderFurnitureMesh(const cad::Furniture& furniture);
}
