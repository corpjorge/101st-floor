#include "Floor101SceneNodeActor.h"

#include "Components/DynamicMeshComponent.h"
#include "F101/Scene/Scene.h"
#include "Floor101FinishLibrary.h"
#include "Floor101UnrealConversion.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

const FName AFloor101SceneNodeActor::ColorParameterName(TEXT("Tint"));

namespace
{
	// View-state colours (linear) tinting the base material. Normal nodes show their finishes (ADR-0012).
	FLinearColor StyleColor(f101::scene::NodeStyle Style)
	{
		switch (Style)
		{
		case f101::scene::NodeStyle::Selected: return FLinearColor::White;
		case f101::scene::NodeStyle::Preview: return FLinearColor(0.74f, 0.63f, 0.46f);
		case f101::scene::NodeStyle::Handle: return FLinearColor(0.67f, 0.42f, 0.19f);
		case f101::scene::NodeStyle::Context: return FLinearColor(0.3f, 0.32f, 0.36f); // levels below the edited one
		case f101::scene::NodeStyle::Symbol: return FLinearColor(0.02f, 0.03f, 0.05f); // plan symbols, e.g. door swings
		case f101::scene::NodeStyle::Ghost: return FLinearColor(0.62f, 0.68f, 0.76f); // only without the glass material
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

	// Hard reference so the generated material is cooked with the game (it is also in DirectoriesToAlwaysCook).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(UFloor101FinishLibrary::BaseMaterialPath);
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
	bStudioSelected = Node.style == f101::scene::NodeStyle::Selected;
	SelectionMaterials.Reset();
	Tags.Reset();
	Tags.Add(MakeNodeTag(SceneNodeId));
#if WITH_EDITOR
	SetActorLabel(UTF8_TO_TCHAR(Node.name.c_str()));
#endif

	MeshComponent->SetMesh(Floor101::Conversion::ToDynamicMesh(Node.mesh));
	// Symbols are drawn on the floor, not part of the building, and ghosts are seen through: they cast no shadow.
	const bool bGhost = Node.style == f101::scene::NodeStyle::Ghost;
	MeshComponent->SetCastShadow(Node.style != f101::scene::NodeStyle::Symbol && !bGhost);

	// Normal nodes show the finish of each section, Context ones the same finishes darker, Ghost ones the faint glass of
	// the services layer (ADR-0021); other view styles tint the whole node.
	UFloor101FinishLibrary* Finishes = UFloor101FinishLibrary::Get();
	TArray<UMaterialInterface*> Materials;
	for (std::size_t Section = 0; Section < Node.mesh.sections.size(); ++Section)
	{
		UMaterialInterface* Material = nullptr;
		const bool bContext = Node.style == f101::scene::NodeStyle::Context;
		if (bGhost && Finishes)
		{
			Material = Finishes->GetGhostMaterial();
		}
		else if ((Node.style == f101::scene::NodeStyle::Normal || bContext || bStudioSelected) && Finishes && Section < Node.finishes.size())
		{
			Material = Finishes->GetFinishMaterial(UTF8_TO_TCHAR(Node.finishes[Section].c_str()), bContext);
			if (bStudioSelected && Material)
			{
				auto* Selected = UMaterialInstanceDynamic::Create(Material->GetMaterial(), this);
				Selected->CopyMaterialUniformParameters(Material);
				// Selection is a contour, not a colour correction of the user's finish.
				SelectionMaterials.Add(Selected);
				Material = Selected;
			}
		}
		Materials.Add(Material ? Material : GetStyleMaterial(static_cast<uint8>(Node.style)));
	}
	MeshComponent->ConfigureMaterialSet(Materials);
}
