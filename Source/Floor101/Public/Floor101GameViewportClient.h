#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"

#include "Floor101GameViewportClient.generated.h"

class AFloor101EditingPawn;
class UFloor101DocumentSubsystem;

namespace f101::cad
{
	struct Ray3;
}

// What a key does while editing (keyboard map, ADR-0008).
enum class EFloor101KeyCommand : uint8
{
	None,
	Undo,
	Redo,
	Save,
	Open,
	Backspace,
	WallTool,
	SelectTool,
	DoorTool,
	WindowTool,
	PaintTool,
	Cancel,
	Confirm,
	Delete,
	Frame,
	TopView,
	LevelUp,
	LevelDown,
	AllLevels,
	Flip, // hinges of the door or window being placed or selected to the other jamb (ADR-0014)
};

// Translates raw input into editing calls (ADR-0008): the cursor becomes a ray in scene space for the edit controller,
// keys become commands, typed characters become the wall tool's length. Holds no editing logic of its own.
// Input it does not use (and all input while the console is open) goes to the engine as usual.
//
// Mouse: left = select / draw / drag a handle, right drag = orbit, right click = cancel, middle drag = pan, wheel = zoom.
// Keys: W wall, P door, O window, M paint, V select, Enter, Esc, Backspace, Delete, F frame all, T plan view,
// Page Up / Page Down level up / down, L show every level, Ctrl+Z undo, Ctrl+Y / Ctrl+Shift+Z redo, Ctrl+S save, Ctrl+O open.
// The camera follows the edited level: its focus moves up or down with the level's elevation.
UCLASS()
class FLOOR101_API UFloor101GameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputAxis(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputChar(FViewport* InViewport, int32 ControllerId, TCHAR Character) override;

	// Frames the walls of the levels in view: up to the edited one, or all of them (or the origin if there are none).
	void FrameProject();
	// Plan view (AFloor101EditingPawn::TopView).
	void ShowTopView();

	// Keyboard map. Held keys only repeat Backspace, undo and redo; with Ctrl, only the Ctrl shortcuts apply.
	static EFloor101KeyCommand MapKey(const FKey& Key, bool bControl, bool bShift, bool bRepeat);

private:
	// Null while the console is open, input is ignored or the world has no document.
	UFloor101DocumentSubsystem* GetEditingTarget();
	AFloor101EditingPawn* GetEditingPawn() const;
	// Ray under the cursor in scene space; false when the cursor is outside the viewport or there is no camera.
	bool GetCursorRay(f101::cad::Ray3& OutRay) const;
	bool HandleKey(UFloor101DocumentSubsystem& Document, const FKey& Key, bool bRepeat);

	FVector2D LastMousePosition = FVector2D(-1.0, -1.0);
	FVector LastRayOrigin = FVector::ZeroVector;
	FVector LastRayDirection = FVector::ZeroVector;
	bool bOrbiting = false;
	bool bPanning = false;
	// A right click that barely moved cancels, like Esc (in PIE the editor keeps Esc to stop playing).
	double RightDragPixels = 0.0;
	// Elevation of the edited level the camera last followed (mm); unset until the first tick with a document.
	TOptional<double> FollowedElevationMm;
};
