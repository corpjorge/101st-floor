#pragma once

#include "CoreMinimal.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Result.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/Function.h"
#include "Templates/PimplPtr.h"

#include "Floor101DocumentSubsystem.generated.h"

class FFloor101SceneReconciler;
class UMaterialInterface;
class UMeshComponent;

namespace f101::cad
{
	class Document;
	class EditController;
	struct Ray3;
}

namespace f101::drawing
{
	struct PlanDrawing;
}

namespace f101::assets
{
	class ModelCache;
	struct Library;
}

namespace f101::cad
{
	struct Asset;
	struct ModelAsset;
}

// A room as the UI shows it: where its label goes and its net floor area.
struct FFloor101RoomView
{
	FString Key;           // f101::geometry::Room::key
	FVector LabelLocation; // on the floor, Unreal space (cm)
	double AreaM2 = 0.0;
};

// What an export for Twinmotion wrote (ADR-0020).
struct FFloor101GltfSummary
{
	int32 Objects = 0; // walls, floors, slabs, doors and windows, pieces of furniture, columns, runs of the services
	int32 Materials = 0;
	int32 Images = 0;
	int64 Triangles = 0;
	int64 Bytes = 0;
	TArray<FString> Warnings; // e.g. an image that could not be read (its material keeps its colour)
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
	// Transient actor of a piece of furniture, or null (e.g. on a hidden level).
	AActor* FindFurnitureActor(const f101::cad::FurnitureId& Id) const;
	// Transient actor of a column, or of a run of the building services (null while their layer is hidden), ADR-0021.
	AActor* FindColumnActor(const f101::cad::ColumnId& Id) const;
	AActor* FindServiceRunActor(const f101::cad::ServiceRunId& Id) const;
	// Rooms of the edited level (of every level when all are shown) as last shown, following the current gesture.
	const TArray<FFloor101RoomView>& GetRooms() const { return Rooms; }

	// 2D plan (ADR-0015). While it is shown, every refresh also draws the edited level's plan with the editing marks
	// (selection, previews, handles).
	void SetPlanView(bool bShown);
	bool IsPlanView() const { return bPlanView; }
	const f101::drawing::PlanDrawing& GetPlanDrawing() const;
	// Exports the plan of a level (nil: the edited one), without editing marks (with the runs of the building services
	// of the networks shown while their layer is shown), as .svg or .png by the path's extension.
	// The status says where it went. PNG needs a renderer (not with -nullrhi).
	f101::Status ExportPlan(const FString& Path, const f101::cad::LevelId& Level);
	// Next to the project file (or the default one): "<file name>-<level>.<Extension>", e.g. "casa-planta-baja.svg".
	FString GetDefaultPlanPath(const FString& Extension) const;

	// Photo mode (ADR-0018): the scene is shown without editing marks (selection, previews, handles, door swings); the
	// viewport client path traces it (UFloor101GameViewportClient::SetPhotoMode).
	void SetPhotoMode(bool bPhoto);
	bool IsPhotoMode() const { return bPhotoMode; }
	// Writes the view as it is shown now to a PNG (without the UI) and says so in the status. The image is written a frame
	// later; a photo is best saved once its samples are complete.
	f101::Status SavePhoto(const FString& Path);
	// Next to the project file (or the default one): "<file name>-foto-<date>-<time>.png".
	FString GetDefaultPhotoPath() const;

	// Export for Twinmotion (ADR-0020): the whole project (every level with its slabs, doors and windows, furniture, columns
	// and the runs of the building services, the materials and their images) as binary glTF (.glb), names in the UI's
	// language. The status says where it went and what it holds.
	f101::Result<FFloor101GltfSummary> ExportGltf(const FString& Path);
	// Next to the project file (or the default one): "<file name>.glb".
	FString GetDefaultGltfPath() const;

	// Imported assets (ADR-0017). Their files live in the project's folder (the project file's, or the default one's)
	// under assets/<uuid>/, and move with the project when it is saved somewhere else.
	FString GetProjectFolder() const;
	// Copies a PNG or JPEG texture, or a glTF (.gltf, .glb) or OBJ model with the files it needs, into the project's
	// folder and adds it to the library (one undoable step). The status says what was imported, and any warning.
	f101::Result<f101::cad::AssetId> ImportAsset(const FString& SourcePath);
	// Changes an asset's name and settings (one undoable step). A model is read again with its new unit, up axis and
	// front to update its size.
	f101::Status ChangeAsset(const f101::cad::Asset& Asset);
	// Removes an asset nothing uses (its files stay on disk until the folder is cleaned up by hand).
	f101::Status RemoveAsset(const f101::cad::AssetId& Id);
	// The asset's file on disk.
	FString GetAssetFilePath(const f101::cad::Asset& Asset) const;
	// Why an imported model cannot be shown (its pieces show as boxes); empty if it can.
	FString GetAssetProblem(const f101::cad::AssetId& Id) const;

	// The built-in library (ADR-0019): furniture and textures shipped in <project>/Library, read once per run. Empty, with
	// an error logged, if it cannot be read.
	static const f101::assets::Library& GetLibrary();
	static FString GetLibraryFolder();
	// The project's asset made from a library item (furniture or texture): the one added before, else the item copied
	// into the project's folder and added (one undoable step), like an import.
	f101::Result<f101::cad::AssetId> AddFromLibrary(const FString& ItemId);

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
	// Tells the finish library how every asset of the project looks (textures, and each material of each model).
	void RegisterAssetFinishes();
	// Renders the drawing offscreen (fitted, 5 mm per pixel at most, 4096 px wide at most) and saves it as a PNG.
	f101::Status SavePlanPng(const f101::drawing::PlanDrawing& Drawing, const FString& Path) const;
	// The map's own ground (its static meshes, e.g. the template's floor) turns see-through like the architecture while
	// the services layer is shown, so runs under the ground floor show too (ADR-0021); off, its materials come back.
	void SetGroundGhosted(bool bGhosted);

	// TPimplPtr: UHT-generated code destroys members where these types are incomplete.
	TPimplPtr<f101::cad::Document> Document;
	TPimplPtr<f101::cad::EditController> Editor;
	TPimplPtr<FFloor101SceneReconciler> Reconciler;
	TPimplPtr<f101::drawing::PlanDrawing> PlanDrawing;
	TPimplPtr<f101::assets::ModelCache> Models;
	bool bPlanView = false;
	bool bPhotoMode = false;
	std::size_t ListenerHandle = 0;
	f101::cad::WallId ReferenceWallId;

	FText StatusMessage;
	bool bStatusIsError = false;
	FString ProjectPath;
	uint64 ChangeCount = 0;
	uint64 SavedChangeCount = 0;
	bool bInEdit = false; // inside Edit(): the document listener leaves the resync to Edit()
	TArray<FFloor101RoomView> Rooms;

	// The ground's material slots made see-through, and their own materials (kept referenced) to put back.
	struct FGhostedSlot
	{
		TWeakObjectPtr<UMeshComponent> Component;
		int32 Index = 0;
	};
	TArray<FGhostedSlot> GhostedSlots;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> GroundMaterials; // parallel to GhostedSlots
	bool bGroundGhosted = false;
};
