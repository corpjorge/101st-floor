#pragma once

#include "CoreMinimal.h"
#include "F101/Cad/EditController.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class UFloor101DocumentSubsystem;
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
	// Name, height and slab of the active level, and a button to delete it.
	TSharedRef<SWidget> MakeLevelSection();
	TSharedRef<SWidget> MakePropertiesPanel();
	// A length property: the value shown (mm, unset = nothing selected) and the controller call that sets it.
	TSharedRef<SWidget> MakePropertyRow(const FText& Label, TFunction<TOptional<double>()> Value, FSelectionSetter Setter);
	TSharedRef<SWidget> MakeStatusBar();
	TSharedRef<SWidget> MakeCursorLabel();

	// Runs an editing action through the subsystem and gives the keyboard back to the viewport.
	FReply Run(TFunctionRef<f101::Status(f101::cad::EditController&)> Action);
	const f101::cad::Wall* GetSelectedWall() const;
	std::optional<f101::cad::OpeningRef> GetSelectedOpening() const;
	// The active level, shown in the panel when nothing is selected (Select mode).
	const f101::cad::Level* GetActiveLevel() const;
	bool IsLevelPanelShown() const;
	FText GetHintText() const;
	FText GetCursorText() const;
	FString GetPathOrDefault() const;

	TWeakObjectPtr<UFloor101DocumentSubsystem> Document;
	TWeakObjectPtr<UFloor101GameViewportClient> ViewportClient;
	FString PathText;
	FVector2D CursorPosition = FVector2D::ZeroVector; // local to this widget
};
