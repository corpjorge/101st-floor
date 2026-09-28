#include "Floor101UnrealConversion.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"

namespace Floor101::Conversion
{
	UE::Geometry::FDynamicMesh3 ToDynamicMesh(const f101::geometry::MeshData& Mesh)
	{
		using namespace UE::Geometry;

		check(Mesh.normals.size() == Mesh.positions.size() && Mesh.uvs.size() == Mesh.positions.size());

		FDynamicMesh3 Result;
		Result.EnableAttributes();
		Result.EnableTriangleGroups();
		Result.Attributes()->EnableMaterialID();
		FDynamicMeshNormalOverlay* Normals = Result.Attributes()->PrimaryNormals();
		FDynamicMeshUVOverlay* UVs = Result.Attributes()->PrimaryUV();
		FDynamicMeshMaterialAttribute* MaterialIds = Result.Attributes()->GetMaterialID();

		const int32 VertexCount = static_cast<int32>(Mesh.positions.size());
		TArray<int32> VertexIds, NormalIds, UVIds;
		VertexIds.SetNumUninitialized(VertexCount);
		NormalIds.SetNumUninitialized(VertexCount);
		UVIds.SetNumUninitialized(VertexCount);
		for (int32 Index = 0; Index < VertexCount; ++Index)
		{
			VertexIds[Index] = Result.AppendVertex(ToUnrealPosition(Mesh.positions[Index]));
			NormalIds[Index] = Normals->AppendElement(ToUnrealNormal(Mesh.normals[Index]));
			const f101::geometry::Vec2& UV = Mesh.uvs[Index];
			UVIds[Index] = UVs->AppendElement(FVector2f(static_cast<float>(UV.u), static_cast<float>(UV.v)));
		}

		for (int32 SectionIndex = 0; SectionIndex < static_cast<int32>(Mesh.sections.size()); ++SectionIndex)
		{
			const std::vector<uint32>& Indices = Mesh.sections[SectionIndex].indices;
			for (size_t Corner = 0; Corner + 2 < Indices.size(); Corner += 3)
			{
				const int32 A = static_cast<int32>(Indices[Corner]);
				const int32 B = static_cast<int32>(Indices[Corner + 1]);
				const int32 C = static_cast<int32>(Indices[Corner + 2]);
				const int32 TriangleId = Result.AppendTriangle(FIndex3i(VertexIds[A], VertexIds[B], VertexIds[C]), SectionIndex);
				checkf(TriangleId >= 0, TEXT("Scene mesh triangle rejected by FDynamicMesh3 (error %d)"), TriangleId);
				Normals->SetTriangle(TriangleId, FIndex3i(NormalIds[A], NormalIds[B], NormalIds[C]));
				UVs->SetTriangle(TriangleId, FIndex3i(UVIds[A], UVIds[B], UVIds[C]));
				MaterialIds->SetValue(TriangleId, SectionIndex);
			}
		}
		return Result;
	}
}
