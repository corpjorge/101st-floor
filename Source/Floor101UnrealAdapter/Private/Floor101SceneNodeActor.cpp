#include "Floor101SceneNodeActor.h"

#include "Components/DynamicMeshComponent.h"
#include "F101/Scene/Scene.h"
#include "Floor101UnrealConversion.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

const FName AFloor101SceneNodeActor::ColorParameterName(TEXT("Color"));

namespace
{
	// View-state colours (linear). Normal nodes use the untinted default material.
	FLinearColor StyleColor(f101::scene::NodeStyle Style)
	{
		switch (Style)
		{
		case f101::scene::NodeStyle::Selected: return FLinearColor(1.0f, 0.42f, 0.08f);
		case f101::scene::NodeStyle::Preview: return FLinearColor(0.22f, 0.58f, 1.0f);
		case f101::scene::NodeStyle::Handle: return FLinearColor(1.0f, 0.85f, 0.1f);
		default: return FLinearColor::White;
		}
	}
}

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

UMaterialInterface* AFloor101SceneNodeActor::GetDisplayMaterial() const
{
	return MeshComponent->GetMaterial(0);
}

FName AFloor101SceneNodeActor::MakeNodeTag(const FString& NodeId)
{
	return FName(FString(TEXT("f101.node:")) + NodeId);
}

UMaterialInterface* AFloor101SceneNodeActor::GetStyleMaterial(uint8 Style)
{
	if (static_cast<f101::scene::NodeStyle>(Style) == f101::scene::NodeStyle::Normal || !DefaultMaterial)
	{
		return DefaultMaterial;
	}
	if (TObjectPtr<UMaterialInterface>* Cached = StyleMaterials.Find(Style))
	{
		return *Cached;
	}
	UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(DefaultMaterial, this);
	Tinted->SetVectorParameterValue(ColorParameterName, StyleColor(static_cast<f101::scene::NodeStyle>(Style)));
	StyleMaterials.Add(Style, Tinted);
	return Tinted;
}

void AFloor101SceneNodeActor::ApplyNode(const f101::scene::SceneNode& Node)
{
	SceneNodeId = UTF8_TO_TCHAR(Node.id.value.c_str());
	Tags.Reset();
	Tags.Add(MakeNodeTag(SceneNodeId));
#if WITH_EDITOR
	SetActorLabel(UTF8_TO_TCHAR(Node.name.c_str()));
#endif

	SetActorHiddenInGame(Node.style == f101::scene::NodeStyle::Hidden);
	MeshComponent->SetMesh(Floor101::Conversion::ToDynamicMesh(Node.mesh));

	TArray<UMaterialInterface*> Materials;
	Materials.Init(GetStyleMaterial(static_cast<uint8>(Node.style)), static_cast<int32>(Node.mesh.sections.size()));
	MeshComponent->ConfigureMaterialSet(Materials);
}
