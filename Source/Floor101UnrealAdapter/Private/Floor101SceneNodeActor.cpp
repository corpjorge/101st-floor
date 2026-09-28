#include "Floor101SceneNodeActor.h"

#include "Components/DynamicMeshComponent.h"
#include "F101/Scene/Scene.h"
#include "Floor101UnrealConversion.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFloor101SceneNodeActor::AFloor101SceneNodeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetEnableRaytracing(true);
	RootComponent = MeshComponent;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	DefaultMaterial = Material.Object;
}

FBox AFloor101SceneNodeActor::GetMeshBounds() const
{
	return MeshComponent->Bounds.GetBox();
}

FName AFloor101SceneNodeActor::MakeNodeTag(const FString& NodeId)
{
	return FName(FString(TEXT("f101.node:")) + NodeId);
}

void AFloor101SceneNodeActor::ApplyNode(const f101::scene::SceneNode& Node)
{
	SceneNodeId = UTF8_TO_TCHAR(Node.id.value.c_str());
	Tags.Reset();
	Tags.Add(MakeNodeTag(SceneNodeId));
#if WITH_EDITOR
	SetActorLabel(UTF8_TO_TCHAR(Node.name.c_str()));
#endif

	MeshComponent->SetMesh(Floor101::Conversion::ToDynamicMesh(Node.mesh));

	TArray<UMaterialInterface*> Materials;
	Materials.Init(DefaultMaterial, static_cast<int32>(Node.mesh.sections.size()));
	MeshComponent->ConfigureMaterialSet(Materials);
}
