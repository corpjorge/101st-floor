#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "Floor101EditingPawn.generated.h"

class UCameraComponent;

// Camera for editing (ADR-0008): orbit a movable focus or fly with the keyboard. The viewport client drives the
// mouse/keyboard; movement has no collision and changes only the view, never the CAD model.
UCLASS(NotPlaceable)
class FLOOR101_API AFloor101EditingPawn : public APawn
{
	GENERATED_BODY()

public:
	AFloor101EditingPawn();

	virtual void BeginPlay() override;

	// Mouse deltas in viewport pixels.
	void Orbit(const FVector2D& DeltaPixels);
	// Navigation look: rotate around the camera's position, including above the horizon. Does nothing in the 2D plan.
	void Look(const FVector2D& DeltaPixels);
	void Pan(const FVector2D& DeltaPixels, const FVector2D& ViewportSize);
	// Continuous movement: X = camera forward, Y = camera right, Z = world up. In the 2D plan, X/Y pan up/right on
	// screen and Z is ignored. DeltaTime is seconds; diagonals have the same speed and Shift selects fast travel.
	void Move(const FVector& LocalDirection, float DeltaTime, bool bFast);
	// Explicit travel speed, independent of zoom. Saved locally only when changed through the UI.
	void SetNavigationSpeedLevel(int32 Level, bool bSave = false);
	void LoadNavigationSpeedLevel();
	int32 GetNavigationSpeedLevel() const { return NavigationSpeedLevel; }
	static double NavigationSpeedForLevel(int32 Level); // cm/s; levels 1..10, default 5 = 3 m/s
	// Positive steps move closer; after the close limit, continue forward in small bounded dolly steps. In 2D only
	// the orthographic width changes. Negative steps back away from the current focus.
	void Zoom(float Steps);
	// Looks at the box (Unreal space) from the current direction, at a distance that fits it.
	void Frame(const FBox& Box);
	// Plan view: looking straight down (as far as the orbit allows) with north (scene +Y) up. Keeps focus and distance.
	void TopView();
	// Moves the focus (and the camera with it) up by DeltaCm; e.g. to follow the edited level.
	void RaiseFocus(double DeltaCm);
	// 2D plan (ADR-0015): an orthographic camera straight down, north up, high above everything, as wide as the
	// perspective view was at the focus. Orbiting does nothing meanwhile; zoom and pan keep working.
	void SetPlanView(bool bPlan);
	bool IsPlanView() const { return bPlanView; }

	UCameraComponent* GetCamera() const { return Camera; }
	const FVector& GetFocus() const { return Focus; }
	double GetDistance() const { return Distance; }
	// Visible width at the focus (cm): the ortho width in the plan view.
	double GetViewWidth() const;

private:
	void ApplyView();

	UPROPERTY(VisibleAnywhere, Category = "101st Floor")
	TObjectPtr<UCameraComponent> Camera;

	FVector Focus = FVector::ZeroVector; // cm
	double Yaw = -45.0;                  // degrees, Unreal convention (0 = +X)
	double Pitch = -35.0;                // degrees, looking down
	double Distance = 1500.0;            // cm from the focus
	bool bPlanView = false;
	int32 NavigationSpeedLevel = 5;
};
