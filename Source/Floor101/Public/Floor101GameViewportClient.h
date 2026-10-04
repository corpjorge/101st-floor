#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "Floor101NavigationKeys.h"

#include "Floor101GameViewportClient.generated.h"

class AFloor101EditingPawn;
class UFloor101DocumentSubsystem;
DECLARE_DELEGATE_TwoParams(FFloor101UnsavedPrompt, bool, FSimpleDelegate);

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
	Save,   // the current file, or "save as" for a new project
	SaveAs, // Ctrl+Shift+S: asks where
	Open,
	Backspace,
	WallTool,
	SelectTool,
	DoorTool,
	WindowTool,
	PaintTool,
	FurnitureTool, // ADR-0016
	ColumnTool,    // ADR-0021
	ServicesTool,  // draws runs of the building services (ADR-0021)
	ServicesLayer, // the services layer on / off: the runs shown, the architecture seen through (ADR-0021)
	Cancel,
	Confirm,
	Delete,
	Frame,
	TopView,
	LevelUp,
	LevelDown,
	AllLevels,
	Flip, // hinges of the door or window being placed or selected to the other jamb (ADR-0014); turns furniture
	      // (ADR-0016) and columns (ADR-0021) 90 degrees
	PlanView, // 2D plan on / off (ADR-0015)
	PhotoMode, // path traced photo on / off (ADR-0018)
	Fullscreen, // dedicated fullscreen at native monitor resolution (F11)
	NavigationToggle, // N: WASD camera / editing shortcuts
	NavigationMove, // a held WASD/QE press, consumed before editing
};

// Translates raw input into editing calls (ADR-0008): the cursor becomes a ray in scene space for the edit controller,
// keys become commands, typed characters become the wall tool's length. Holds no editing logic of its own.
// Input it does not use (and all input while the console is open) goes to the engine as usual.
//
// Mouse: left = select / draw / drag a handle, right drag = orbit, right click = cancel, middle drag = pan, wheel = zoom.
// Keys: W wall, P door, O window, M paint, A furniture, C column, I services run, X services layer, V select, Enter, Esc,
// Backspace, Delete, F frame all, T top view,
// Tab 2D plan, R photo, Space flip hinges, Page Up / Page Down level up / down, L show every level, Ctrl+Z undo, Ctrl+Y / Ctrl+Shift+Z redo,
// Ctrl+S save, Ctrl+Shift+S save as, Ctrl+O open (the system's dialogs).
// N: navigation mode; WASD forward/sideways, Q/E vertical, Shift fast, right drag looks from the camera. Also right+WASD.
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
	virtual void LostFocus(FViewport* InViewport) override;
	void SetNavigationMode(bool bNavigate);
	bool IsNavigationMode() const { return bNavigationMode; }
	void SetNavigationSpeedLevel(int32 Level);
	int32 GetNavigationSpeedLevel() const;
	// Standard game exit (ends only the play session in PIE), after protecting unsaved work.
	bool RequestExit();
	// The overlay owns an asynchronous prompt inside the viewport, never a separate native window.
	FFloor101UnsavedPrompt UnsavedPrompt;
	bool RequestUnsavedAction(bool bExit, FSimpleDelegate Proceed);
	// A dedicated options/start screen owns input while open; CAD and held movement keys stay inactive.
	void SetInterfaceOpen(bool bOpen, FSimpleDelegate Close = FSimpleDelegate());
	bool IsInterfaceOpen() const { return bInterfaceOpen; }

	// Frames the walls of the levels in view: up to the edited one, or all of them (or the origin if there are none).
	void FrameProject();
	// Perspective looking down (AFloor101EditingPawn::TopView); leaves the 2D plan.
	void ShowTopView();
	// 2D plan (ADR-0015): orthographic camera straight down, no 3D rendering, the plan painted by the overlay. Editing
	// works as in 3D: the pointer rays come straight down. Off: back to the perspective view.
	void SetPlanView(bool bPlan);
	bool IsPlanView() const;
	// Photo mode (ADR-0018): the view path traced, without editing marks, accumulating samples while the camera and the
	// project stay still. The camera still moves (orbit, pan, zoom, F, T); clicks do not edit; R or Esc leave it. It leaves
	// the 2D plan.
	void SetPhotoMode(bool bPhoto);
	bool IsPhotoMode() const { return bPhotoMode; }
	// Samples per pixel the photo accumulates (64: draft .. 1024: high).
	void SetPhotoSamples(int32 Samples);
	int32 GetPhotoSamples() const { return PhotoSamples; }

	// Keyboard map. Editing repeats only Backspace/undo/redo; navigation keys are consumed while held. With Ctrl, only
	// the Ctrl shortcuts apply. While a
	// length is being typed, the letters of its units (m, cm, mm) are text, not commands.
	static EFloor101KeyCommand MapKey(const FKey& Key, bool bControl, bool bShift, bool bRepeat, bool bTypingLength = false, bool bNavigating = false);

private:
	bool CompleteExit();
	// Null while the console is open, input is ignored or the world has no document.
	UFloor101DocumentSubsystem* GetEditingTarget();
	AFloor101EditingPawn* GetEditingPawn() const;
	// Ray under the cursor in scene space; false when the cursor is outside the viewport or there is no camera.
	bool GetCursorRay(f101::cad::Ray3& OutRay) const;
	bool HandleKey(UFloor101DocumentSubsystem& Document, const FKey& Key, bool bRepeat);
	bool CanNavigate();
	void TickNavigation(float DeltaTime);

	FVector2D LastMousePosition = FVector2D(-1.0, -1.0);
	FVector LastRayOrigin = FVector::ZeroVector;
	FVector LastRayDirection = FVector::ZeroVector;
	bool bOrbiting = false;
	bool bPanning = false;
	bool bNavigationMode = false;
	bool bRightNavigationUsed = false;
	FFloor101NavigationKeys NavigationKeys;
	// A right click that barely moved cancels, like Esc (in PIE the editor keeps Esc to stop playing).
	double RightDragPixels = 0.0;
	// Elevation of the edited level the camera last followed (mm); unset until the first tick with a document.
	TOptional<double> FollowedElevationMm;
	bool bPhotoMode = false;
	bool bInterfaceOpen = false;
	FSimpleDelegate CloseInterface;
	int32 PhotoSamples = 256;
};
