#pragma once

#include "CoreMinimal.h"
#include "F101/Cad/EditController.h"
#include "SFloor101PlanView.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class UFloor101DocumentSubsystem;
struct FFloor101RoomView;
class UFloor101GameViewportClient;

// Editing UI drawn over the game viewport (ADR-0008): toolbar with the levels, properties of the selection (or of the
// active level when nothing is selected), status bar and a label next to the cursor with the length being drawn or typed. It reads the document subsystem every frame and changes
// things only through it, so the keyboard, the mouse and this UI stay equivalent.
class SFloor101Overlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101Overlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UFloor101DocumentSubsystem>, Document)
		SLATE_ARGUMENT(TWeakObjectPtr<UFloor101GameViewportClient>, ViewportClient)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	using FSelectionSetter = f101::Status (f101::cad::EditController::*)(std::string_view);

	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeLevelRow();
	// Exports the edited level's plan next to the project file (ADR-0015); Extension: "svg" or "png".
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
	TSharedRef<SWidget> MakePropertiesPanel();
	// A length property: the value shown (mm, unset = nothing selected) and the controller call that sets it.
	TSharedRef<SWidget> MakePropertyRow(const FText& Label, TFunction<TOptional<double>()> Value, FSelectionSetter Setter);
	// A typed property: the text shown and the controller call that parses and sets what is typed.
	TSharedRef<SWidget> MakeTextRow(const FText& Label, TFunction<FText()> CurrentText, FSelectionSetter Setter);
	// How the selected door or window opens (ADR-0014): its type, its hinges, the face it opens to, how open it is shown.
	TSharedRef<SWidget> MakeOpeningTypeSection();
	// A row with a label, a value and an "Invertir" button running `Flip`.
	TSharedRef<SWidget> MakeFlipRow(TFunction<FText()> Label, TFunction<FText()> Value, f101::Status (f101::cad::EditController::*Flip)());
	TSharedRef<SWidget> MakeStatusBar();
	TSharedRef<SWidget> MakeCursorLabel();

	// Runs an editing action through the subsystem and gives the keyboard back to the viewport.
	FReply Run(TFunctionRef<f101::Status(f101::cad::EditController&)> Action);
	const f101::cad::Wall* GetSelectedWall() const;
	std::optional<f101::cad::OpeningRef> GetSelectedOpening() const;
	// The active level, shown in the panel when nothing is selected (Select mode).
	const f101::cad::Level* GetActiveLevel() const;
	// The selected room as shown (active level), or null.
	const FFloor101RoomView* GetSelectedRoom() const;
	bool IsLevelPanelShown() const;
	FText GetHintText() const;
	FText GetCursorText() const;
	// Plan millimetres to this widget in the 2D plan view (camera straight down, north up); unset without a camera.
	TOptional<FFloor101PlanMapping> GetPlanMapping(const FGeometry& Geometry) const;
	FString GetPathOrDefault() const;

	TWeakObjectPtr<UFloor101DocumentSubsystem> Document;
	TWeakObjectPtr<UFloor101GameViewportClient> ViewportClient;
	FString PathText;
	FVector2D CursorPosition = FVector2D::ZeroVector; // local to this widget
	// Finish ids each surface allows, for the drop-downs (kept alive for SComboBox).
	TMap<uint8, TArray<TSharedPtr<FString>>> FinishOptions;
	// The types a door and a window can have, for their drop-downs.
	TArray<TSharedPtr<f101::cad::OpeningOperation>> DoorOperations;
	TArray<TSharedPtr<f101::cad::OpeningOperation>> WindowOperations;
};
