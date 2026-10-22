#include "Floor101SceneReconciler.h"

#include "Engine/World.h"
#include "Floor101SceneNodeActor.h"
#include "Floor101SceneSpace.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"

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
			AppliedState.Add(Key, FApplied{Node.contentHash, Node.style});
			++Stats.Created;
		}
		else if (const FApplied* Applied = AppliedState.Find(Key); !Applied || Applied->ContentHash != Node.contentHash || Applied->Style != Node.style)
		{
			Actor->ApplyNode(Node);
			AppliedState.Add(Key, FApplied{Node.contentHash, Node.style});
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
			AppliedState.Remove(It.Key());
			It.RemoveCurrent();
			++Stats.Removed;
		}
	}
	TSet<FString> PresentLights;
	for (const auto& Light : Scene.lights)
	{
		const FString Key = ToKey(Light.id);
		PresentLights.Add(Key);
		APointLight* Actor = Lights.FindRef(Key).Get();
		const bool bNew = !Actor;
		if (bNew)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = TargetWorld->SpawnActor<APointLight>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			check(Actor);
			Actor->Tags.Add(FName(*(TEXT("f101.light:") + Key)));
			Lights.Add(Key, Actor);
		}
		const auto* Applied = AppliedLights.Find(Key);
		if (bNew || !Applied || !(*Applied == Light))
		{
			UPointLightComponent* Component = CastChecked<UPointLightComponent>(Actor->GetLightComponent());
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetIntensityUnits(ELightUnits::Lumens);
			Component->SetIntensity(static_cast<float>(Light.lumens));
			Component->SetAttenuationRadius(static_cast<float>(Floor101::Conversion::ToUnrealLength(static_cast<double>(Light.radiusMm))));
			Component->SetLightColor(FLinearColor::FromSRGBColor(FColor(Light.color.r, Light.color.g, Light.color.b)));
			Component->SetSourceRadius(2.0f); // 20 mm physical source, soft shadows
			Actor->SetActorLocation(Floor101::Conversion::ToUnrealPosition(Light.position));
			AppliedLights.Add(Key, Light);
			if (bNew) ++Stats.Created; else ++Stats.Updated;
		}
		else ++Stats.Unchanged;
	}
	for (auto It = Lights.CreateIterator(); It; ++It)
	{
		if (!PresentLights.Contains(It.Key()))
		{
			if (APointLight* Actor = It.Value().Get()) Actor->Destroy();
			AppliedLights.Remove(It.Key());
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
	AppliedState.Reset();
	for (const auto& Entry : Lights) if (APointLight* Actor = Entry.Value.Get()) Actor->Destroy();
	Lights.Reset();
	AppliedLights.Reset();
}

AFloor101SceneNodeActor* FFloor101SceneReconciler::FindActor(const f101::scene::SceneNodeId& Id) const
{
	return Actors.FindRef(ToKey(Id)).Get();
}

APointLight* FFloor101SceneReconciler::FindLight(const f101::scene::SceneNodeId& Id) const
{
	return Lights.FindRef(ToKey(Id)).Get();
}
