#pragma once

#include "CoreMinimal.h"
#include "F101/Cad/Id.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"

#include "Floor101DocumentSubsystem.generated.h"

class FFloor101SceneReconciler;

namespace f101::cad
{
	class Document;
}

// Owns the document edited in a world and keeps the world's representation in sync with it:
// document change -> scene compilation -> Unreal adapter reconciliation (docs/architecture.md, "Flujo de un cambio").
UCLASS()
class FLOOR101_API UFloor101DocumentSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFloor101DocumentSubsystem();
	virtual ~UFloor101DocumentSubsystem() override;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	const f101::cad::Document& GetDocument() const { return *Document; }
	// Mutations resync the world automatically (document listener).
	f101::cad::Document& GetMutableDocument() { return *Document; }

	// Transient actor representing the wall, or null if it has none.
	AActor* FindWallActor(const f101::cad::WallId& Id) const;

	// MVP 0 reference wall: (0,0) -> (3550,0) mm, 120 mm thick, 2400 mm high. No-op if already loaded.
	void LoadReferenceWall();
	// Moves the reference wall's end to (LengthMm, 0). Returns false (and logs why) if rejected.
	bool SetReferenceWallLength(int64 LengthMm);
	void ClearReferenceWall();
	void LogStatus() const;

private:
	void Resync();

	// TPimplPtr: UHT-generated code destroys members where these types are incomplete.
	TPimplPtr<f101::cad::Document> Document;
	TPimplPtr<FFloor101SceneReconciler> Reconciler;
	std::size_t ListenerHandle = 0;
	f101::cad::WallId ReferenceWallId;
};
