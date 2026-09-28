#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Floor101SceneNodeActor.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;

namespace f101::scene
{
	struct SceneNode;
}

// Unreal representation of one scene node (ADR-0005). Transient: generated from the model at runtime, never saved in a
// level and never placed by hand. Tagged "f101.node:<SceneNodeId>" so tests and MCP can find it.
UCLASS(Transient, NotPlaceable)
class FLOOR101UNREALADAPTER_API AFloor101SceneNodeActor : public AActor
{
	GENERATED_BODY()

public:
	AFloor101SceneNodeActor();

	// Rebuilds the mesh from the node. Geometry is regenerated, never scaled.
	void ApplyNode(const f101::scene::SceneNode& Node);

	UDynamicMeshComponent* GetMeshComponent() const { return MeshComponent; }
	const FString& GetSceneNodeId() const { return SceneNodeId; }

	// World-space bounds of the generated mesh, in centimetres.
	FBox GetMeshBounds() const;

	static FName MakeNodeTag(const FString& SceneNodeId);

private:
	UPROPERTY(VisibleAnywhere, Category = "101st Floor")
	TObjectPtr<UDynamicMeshComponent> MeshComponent;

	// Hard reference so the material is cooked with the game.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	UPROPERTY(VisibleAnywhere, Category = "101st Floor")
	FString SceneNodeId;
};
