#pragma once

#include "CoreMinimal.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/Function.h"
#include "Templates/PimplPtr.h"

#include "Floor101DocumentSubsystem.generated.h"

class FFloor101SceneReconciler;

namespace f101::cad
{
	class Document;
	class EditController;
	struct Ray3;
}

// A room as the UI shows it: where its label goes and its net floor area.
struct FFloor101RoomView
{
	FString Key;           // f101::geometry::Room::key
	FVector LabelLocation; // on the floor, Unreal space (cm)
	double AreaM2 = 0.0;
};

// Owns the document edited in a world and keeps the world's representation in sync with it:
// document change -> scene compilation -> Unreal adapter reconciliation (docs/architecture.md, "Flujo de un cambio").
// Also owns the interactive editing state (selection, tools; ADR-0008), shown on top of the document.
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

	const f101::cad::EditController& GetEditController() const { return *Editor; }
	// Runs an editing action, then refreshes the view. A failure becomes the status message; a success clears it.
	f101::Status Edit(TFunctionRef<f101::Status(f101::cad::EditController&)> Action);
	// Pointer moves only change the view (previews, snapped cursor); they leave the status message alone.
	void UpdatePointer(const f101::cad::Ray3& Ray);

	// Last outcome worth telling the user (an error, a saved file...); empty when there is nothing to say.
	const FText& GetStatusMessage() const { return StatusMessage; }
	bool IsStatusError() const { return bStatusIsError; }

	// project.json (ADR-0007). Opening replaces the document and clears the undo history.
	f101::Status SaveProject(const FString& Path);
	f101::Status OpenProject(const FString& Path);
	// File the document was last saved to or opened from; empty if none.
	const FString& GetProjectPath() const { return ProjectPath; }
	// True after any change since the last save or open (an undo back to the saved state still counts as a change).
	bool HasUnsavedChanges() const { return ChangeCount != SavedChangeCount; }
	// <Documents>/101st Floor/project.json
	static FString GetDefaultProjectPath();

	// Transient actor representing the wall, or null if it has none.
	AActor* FindWallActor(const f101::cad::WallId& Id) const;
	// Transient actor of a room's floor, or null.
	AActor* FindRoomFloorActor(const FString& RoomKey) const;
	// Transient actor of the slab over a room, or null (e.g. while its level is being edited, or hidden).
	AActor* FindRoomSlabActor(const FString& RoomKey) const;
	// Rooms of the edited level (of every level when all are shown) as last shown, following the current gesture.
	const TArray<FFloor101RoomView>& GetRooms() const { return Rooms; }

	// MVP 0 reference wall: (0,0) -> (3550,0) mm, 120 mm thick, 2400 mm high. No-op if already loaded.
	void LoadReferenceWall();
	// Moves the reference wall's end to (LengthMm, 0). Returns false (and logs why) if rejected.
	bool SetReferenceWallLength(int64 LengthMm);
	void ClearReferenceWall();
	void LogStatus() const;

private:
	// Compiles the document plus the editing view state and reconciles the world. Document changes log what changed.
	void Resync(bool bDocumentChanged);
	void SetStatus(const FText& Message, bool bIsError);

	// TPimplPtr: UHT-generated code destroys members where these types are incomplete.
	TPimplPtr<f101::cad::Document> Document;
	TPimplPtr<f101::cad::EditController> Editor;
	TPimplPtr<FFloor101SceneReconciler> Reconciler;
	std::size_t ListenerHandle = 0;
	f101::cad::WallId ReferenceWallId;

	FText StatusMessage;
	bool bStatusIsError = false;
	FString ProjectPath;
	uint64 ChangeCount = 0;
	uint64 SavedChangeCount = 0;
	bool bInEdit = false; // inside Edit(): the document listener leaves the resync to Edit()
	TArray<FFloor101RoomView> Rooms;
};
