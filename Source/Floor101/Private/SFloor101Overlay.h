#pragma once

#include "CoreMinimal.h"
#include "F101/Cad/EditController.h"
#include "SFloor101Icon.h"
#include "SFloor101PlanView.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class UFloor101DocumentSubsystem;
struct FSlateBrush;
struct FFloor101RoomView;
class UFloor101GameViewportClient;
class SComboButton;
class SBox;

// Editing UI drawn over the game viewport (ADR-0008, ADR-0026, Cota Cero): a top bar, floating islands over the view (tools,
// view controls, levels, the inspector of the selection or of the active level, hints and status) and a label next to the
// cursor with the length being drawn or typed. It reads the document subsystem every frame and changes things only through
// it, so the keyboard, the mouse and this UI stay equivalent.
class SFloor101Overlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101Overlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UFloor101DocumentSubsystem>, Document)
		SLATE_ARGUMENT(TWeakObjectPtr<UFloor101GameViewportClient>, ViewportClient)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void ShowStartup();
	void ShowUnsavedPrompt(bool bExit, FSimpleDelegate Proceed);
	void ResolveUnsavedPrompt(EAppReturnType::Type Choice);
	bool IsUnsavedPromptOpen() const { return bUnsavedPromptOpen; }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	// Development-only command-line capture: exercises the actual Slate surface without changing user preferences.
	void PrepareReview(const FString& Screen);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	using FSelectionSetter = f101::Status (f101::cad::EditController::*)(std::string_view);

	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeToolRail();
	// View controls over the view: framing, top view, 2D plan, X-ray, the View menu, speed and, in narrow windows, the inspector.
	TSharedRef<SWidget> MakeViewBar();
	// The levels as a stack with their elevations (or a selector when the window is short), rebuilt when the levels change.
	TSharedRef<SWidget> MakeLevelIsland();
	void RefreshLevelIsland();
	bool IsLevelIslandCompact() const;
	TSharedPtr<SBox> LevelIslandHost;
	FString LevelSignature;
	// What the inspector is showing: its icon, title and one line under it.
	struct FInspectorHeading
	{
		EFloor101Icon Icon = EFloor101Icon::Levels;
		FText Title;
		FText Subtitle;
	};
	FInspectorHeading GetInspectorHeading() const;
	TSharedRef<SWidget> MakeInspectorHeader();
	bool IsInspectorDocked() const;
	// The hint pill (current tool, keys drawn as keycaps), rebuilt when the hint changes, and the status island.
	TSharedRef<SWidget> MakeHintBar();
	TSharedRef<SWidget> MakeHintContent(const FText& Hint, bool bDefault);
	TSharedRef<SWidget> MakeStatusIsland();
	TSharedPtr<SBox> HintHost;
	FString ShownHint;
	// The cursor label sits above the selected wall (anchored) or beside the pointer while drawing.
	bool bCursorAnchored = false;
	TSharedRef<SWidget> MakeFileMenu();
	TSharedRef<SWidget> MakeViewMenu();
	TSharedRef<SWidget> MakeUtilityMenu();
	TSharedRef<SWidget> MakeOptionsPanel();
	TSharedRef<SWidget> MakeHomePanel();
	TSharedRef<SWidget> MakeUnsavedPanel();
	// Updates (ADR-0039): Options → Acerca de, the notice on the home screen and the button in the bar while a new version
	// is available, downloading or ready to install.
	TSharedRef<SWidget> MakeAboutPage();
	TSharedRef<SWidget> MakeUpdateButton();
	TSharedRef<SWidget> MakeUpdateNotice();
	TSharedRef<SWidget> MakeUpdateChip();
	bool bUnsavedPromptOpen = false;
	bool bUnsavedExit = false;
	bool bResolvingUnsaved = false;
	bool bUnsavedSaveFailed = false;
	FSimpleDelegate PendingUnsavedAction;
	void OpenHome();
	void CloseHome();
	void EnterWorkspace(bool bFrame);
	void EnterNewProject();
	void RefreshModalInput();
	TArray<FString> RecentProjects() const;
	bool bHomeOpen = false;
	bool bHasEnteredWorkspace = false;
	void OpenOptions();
	void CloseOptions();
	bool bOptionsOpen = false;
	int32 OptionsPage = 0;
	bool bDisplayAdvanced = false; // Options > Pantalla: the monitor details are open
	bool IsDesignMode() const;
	FVector2D ViewSize = FVector2D(1920.0, 1080.0);
	TSharedRef<SWidget> MakeLevelRow();
	// Exports the edited level's plan after choosing its destination; Extension: "svg" or "png".
	TSharedRef<SWidget> MakePlanExportButton(const FString& Extension);
	// Name, height and slab of the active level, and a button to delete it.
	TSharedRef<SWidget> MakeLevelSection();
	// Finishes (ADR-0012): a drop-down of the finishes a surface allows, the Paint palette and the selected room.
	TSharedRef<SWidget> MakeFinishCombo(f101::cad::Surface Surface, TFunction<FString()> Current,
		TFunction<f101::Status(f101::cad::EditController&, const std::string&)> Apply);
	TSharedRef<SWidget> MakePaintPalette();
	// A face of the selected wall ("Cara norte") and its finish.
	TSharedRef<SWidget> MakeWallFaceRow(f101::cad::WallSide Side);
	TSharedRef<SWidget> MakeRoomSection();
	TSharedRef<SWidget> MakeDimensionSection();
	std::optional<f101::cad::Dimension> GetSelectedDimension() const;
	// Furniture (ADR-0016): the selected piece (sizes, rotation, finish) and the catalogue of the Furniture mode.
	TSharedRef<SWidget> MakeFurnitureSection();
	TSharedRef<SWidget> MakeFurniturePalette();
	// Columns and building services (ADR-0021): the selected column and run, and the palettes of the Column and Services
	// modes.
	TSharedRef<SWidget> MakeColumnSection();
	TSharedRef<SWidget> MakeColumnPalette();
	TSharedRef<SWidget> MakeServiceRunSection();
	TSharedRef<SWidget> MakeServicesPalette();
	// A button per column shape, and one per kind of run with its colour: the one `Current` gives is highlighted, a click
	// runs `Apply`.
	TSharedRef<SWidget> MakeColumnShapeButtons(TFunction<std::optional<f101::cad::ColumnShape>()> Current,
		TFunction<f101::Status(f101::cad::EditController&, f101::cad::ColumnShape)> Apply);
	TSharedRef<SWidget> MakeServiceKindButtons(TFunction<std::optional<f101::cad::ServiceKind>()> Current,
		TFunction<f101::Status(f101::cad::EditController&, f101::cad::ServiceKind)> Apply);
	// With the services layer: which networks are seen, in 3D, in the plan and in the exported plan.
	TSharedRef<SWidget> MakeNetworksRow();
	// The built-in library (ADR-0019). A tile of a palette grid: a picture with its name under it, framed in orange while
	// IsActive.
	TSharedRef<SWidget> MakeCatalogueTile(const TSharedRef<SWidget>& Picture, const FText& Name, const FText& ToolTip,
		TFunction<bool()> IsActive, TFunction<FReply()> OnClicked, const FString& Key, bool bModel);
	TSharedRef<SWidget> MakeCatalogueFilters(bool bModel);
	bool CatalogueHasResults(bool bModel) const;
	TSet<FString> FavoriteKeys;
	FString FurnitureQuery;
	FString MaterialQuery;
	bool bFurnitureFavoritesOnly = false;
	bool bMaterialFavoritesOnly = false;
	// The picture of an image file (library thumbnails and photographs), or a grey swatch if it cannot be read.
	const FSlateBrush* GetPictureBrush(const FString& Path);
	// Adds a library item to the project (once) and uses it: Amueblar places the model, Pintar applies the texture.
	FReply UseLibraryItem(const std::string& ItemId, bool bModel);
	// Whether Amueblar (bModel) or Pintar uses the project's asset made from that library item.
	bool IsLibraryItemInUse(const std::string& ItemId, bool bModel) const;
	// Imported assets (ADR-0017): the project's textures in Pintar and models in Amueblar (rebuilt when the library
	// changes), the buttons that import them, and the settings of the one in use.
	TSharedRef<SWidget> MakeImportedTextures();
	TSharedRef<SWidget> MakeImportedModels();
	void RebuildImportedLists();
	// Opens the system's file dialog and imports the chosen texture or model, then uses it (Pintar or Amueblar).
	void ImportFromDialog(bool bModel);
	// A row with a label and a text box: `Apply` gets what is typed (Enter or leaving the box).
	TSharedRef<SWidget> MakeEditRow(const FText& Label, TFunction<FText()> Current, TFunction<void(const FString&)> Apply, bool bNumeric = true);
	// The swatch of a finish: its colour, or the image of an imported texture.
	TSharedRef<SWidget> MakeSwatch(const std::string& FinishId, float Size);
	// The image an imported texture shows now (its brush follows it), or a grey swatch while it has none.
	const FSlateBrush* GetAssetBrush(const FString& FinishId);
	// The texture a catalogue finish is drawn with (its pattern), for swatches.
	const FSlateBrush* GetCatalogueBrush(const FString& FinishId);
	// The project being edited, or null.
	const f101::cad::Project* CurrentProject() const;
	// The imported texture Pintar applies, and the imported model Amueblar places; null when it is a catalogue one.
	const f101::cad::Asset* GetPaintAsset() const;
	const f101::cad::Asset* GetFurnitureAsset() const;
	// Changes an asset through the subsystem (one undoable step) and gives the keyboard back to the viewport.
	void ChangeAsset(const f101::cad::Asset& Asset);
	TSharedRef<SWidget> MakePropertiesPanel();
	// A length property: the value shown (mm, unset = nothing selected) and the controller call that sets it.
	TSharedRef<SWidget> MakePropertyRow(const FText& Label, TFunction<TOptional<double>()> Value, FSelectionSetter Setter);
	// A typed property: the text shown and the controller call that parses and sets what is typed.
	TSharedRef<SWidget> MakeTextRow(const FText& Label, TFunction<FText()> CurrentText, FSelectionSetter Setter, bool bNumeric = true);
	// How the selected door or window opens (ADR-0014): its type, its hinges, the face it opens to, how open it is shown.
	TSharedRef<SWidget> MakeOpeningTypeSection();
	// Door and Window modes: the type the next ones are placed with.
	TSharedRef<SWidget> MakeOpeningPalette();
	TSharedRef<SWidget> MakeOpeningCatalogue(bool bSelection);
	FString OpeningQuery;
	int32 OpeningOperationFilter = -1;
	// A row with a label, a value and an "Invertir" button running `Flip`.
	TSharedRef<SWidget> MakeFlipRow(TFunction<FText()> Label, TFunction<FText()> Value, f101::Status (f101::cad::EditController::*Flip)());
	// Photo mode (ADR-0018): its samples, quality, saving the image and leaving it.
	TSharedRef<SWidget> MakePhotoPanel();
	TSharedRef<SWidget> MakeHdrButton();
	TSharedRef<SWidget> MakeDisplayControls(bool bCompact = false);
	TSharedRef<SWidget> MakeNavigationButton();
	TSharedRef<SWidget> MakeNavigationSpeed();
	TSharedRef<SWidget> MakeVSyncButton();
	TSharedRef<SWidget> MakeExitButton(bool bCompact = false);
	TSharedRef<SWidget> MakeCursorLabel();

	// Runs an editing action through the subsystem and gives the keyboard back to the viewport.
	FReply Run(TFunctionRef<f101::Status(f101::cad::EditController&)> Action);
	const f101::cad::Wall* GetSelectedWall() const;
	std::optional<f101::cad::OpeningRef> GetSelectedOpening() const;
	std::optional<f101::cad::Furniture> GetSelectedFurniture() const;
	std::optional<f101::cad::Column> GetSelectedColumn() const;
	std::optional<f101::cad::ServiceRun> GetSelectedServiceRun() const;
	// The active level, shown in the panel when nothing is selected (Select mode).
	const f101::cad::Level* GetActiveLevel() const;
	// The selected room as shown (active level), or null.
	const FFloor101RoomView* GetSelectedRoom() const;
	bool IsLevelPanelShown() const;
	bool IsPhotoMode() const;
	FText GetHintText() const;
	FText GetCursorText() const;
	// Plan millimetres to this widget in the 2D plan view (camera straight down, north up); unset without a camera.
	TOptional<FFloor101PlanMapping> GetPlanMapping(const FGeometry& Geometry) const;

	TWeakObjectPtr<UFloor101DocumentSubsystem> Document;
	TWeakObjectPtr<UFloor101GameViewportClient> ViewportClient;
	TSharedPtr<SComboButton> PropertiesMenu;
	TSharedPtr<SComboButton> LevelMenu;
	TSharedPtr<SComboButton> DocumentMenu;
	TSharedPtr<SComboButton> ViewMenu;
	TSharedPtr<SComboButton> SpeedMenu;
	FVector2D CursorPosition = FVector2D::ZeroVector; // local to this widget
	// Finish ids each surface allows, for the drop-downs (on the heap: SComboBox keeps a pointer to the list).
	TMap<uint8, TSharedPtr<TArray<TSharedPtr<FString>>>> FinishOptions;
	// Imported assets (ADR-0017).
	TSharedPtr<SVerticalBox> ImportedTextureList;
	TSharedPtr<SVerticalBox> ImportedModelList;
	FString ImportedSignature; // ids and names of the assets the lists show
	TMap<FString, TSharedPtr<FSlateBrush>> AssetBrushes;
	// Pintar shows the finishes for walls, or those for floors (MVP 11).
	bool bPaintFloors = false;
	// The category Amueblar shows.
	f101::cad::FurnitureCategory FurnitureTab = f101::cad::FurnitureCategory::Seating;
	TSharedPtr<FSlateBrush> NoImageBrush;
	TArray<TSharedPtr<f101::cad::ModelUnit>> ModelUnits;
	// The types a door and a window can have, for their drop-downs.
	TArray<TSharedPtr<f101::cad::OpeningOperation>> DoorOperations;
	TArray<TSharedPtr<f101::cad::OpeningOperation>> WindowOperations;
};
