#pragma once

#include "CoreMinimal.h"
#include "F101/Scene/Scene.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AFloor101SceneNodeActor;
class UWorld;

struct FFloor101ReconcileStats
{
	int32 Created = 0;
	int32 Updated = 0;
	int32 Unchanged = 0;
	int32 Removed = 0;
};

// Keeps a world's actors in line with a scene, matching them by SceneNodeId (ADR-0003):
// new nodes spawn actors, nodes whose content hash changed are rebuilt in place (same actor), missing nodes are
// destroyed. The scene is the only input; the adapter never reads or writes the CAD model.
class FLOOR101UNREALADAPTER_API FFloor101SceneReconciler
{
public:
	explicit FFloor101SceneReconciler(UWorld* InWorld);

	FFloor101ReconcileStats Reconcile(const f101::scene::Scene& Scene);
	void DestroyAll();

	AFloor101SceneNodeActor* FindActor(const f101::scene::SceneNodeId& Id) const;
	int32 Num() const { return Actors.Num(); }

private:
	TWeakObjectPtr<UWorld> World;
	TMap<FString, TWeakObjectPtr<AFloor101SceneNodeActor>> Actors;
	TMap<FString, uint64> ContentHashes;
};
