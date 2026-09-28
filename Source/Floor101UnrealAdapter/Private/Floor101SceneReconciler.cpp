#include "Floor101SceneReconciler.h"

#include "Engine/World.h"
#include "Floor101SceneNodeActor.h"

namespace
{
	FString ToKey(const f101::scene::SceneNodeId& Id)
	{
		return UTF8_TO_TCHAR(Id.value.c_str());
	}
}

FFloor101SceneReconciler::FFloor101SceneReconciler(UWorld* InWorld)
	: World(InWorld)
{
	check(InWorld);
}

FFloor101ReconcileStats FFloor101SceneReconciler::Reconcile(const f101::scene::Scene& Scene)
{
	FFloor101ReconcileStats Stats;
	UWorld* TargetWorld = World.Get();
	if (!ensure(TargetWorld))
	{
		return Stats;
	}

	TSet<FString> Present;
	for (const f101::scene::SceneNode& Node : Scene.nodes)
	{
		const FString Key = ToKey(Node.id);
		Present.Add(Key);

		AFloor101SceneNodeActor* Actor = Actors.FindRef(Key).Get();
		if (!Actor)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = TargetWorld->SpawnActor<AFloor101SceneNodeActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			check(Actor);
			Actor->ApplyNode(Node);
			Actors.Add(Key, Actor);
			ContentHashes.Add(Key, Node.contentHash);
			++Stats.Created;
		}
		else if (ContentHashes.FindRef(Key) != Node.contentHash)
		{
			Actor->ApplyNode(Node);
			ContentHashes.Add(Key, Node.contentHash);
			++Stats.Updated;
		}
		else
		{
			++Stats.Unchanged;
		}
	}

	for (auto It = Actors.CreateIterator(); It; ++It)
	{
		if (!Present.Contains(It.Key()))
		{
			if (AFloor101SceneNodeActor* Actor = It.Value().Get())
			{
				Actor->Destroy();
			}
			ContentHashes.Remove(It.Key());
			It.RemoveCurrent();
			++Stats.Removed;
		}
	}
	return Stats;
}

void FFloor101SceneReconciler::DestroyAll()
{
	for (const TPair<FString, TWeakObjectPtr<AFloor101SceneNodeActor>>& Entry : Actors)
	{
		if (AFloor101SceneNodeActor* Actor = Entry.Value.Get())
		{
			Actor->Destroy();
		}
	}
	Actors.Reset();
	ContentHashes.Reset();
}

AFloor101SceneNodeActor* FFloor101SceneReconciler::FindActor(const f101::scene::SceneNodeId& Id) const
{
	return Actors.FindRef(ToKey(Id)).Get();
}
