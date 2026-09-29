#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "Floor101EditingPawn.generated.h"

class UCameraComponent;

// Orbit camera for editing (ADR-0008): it turns around a focus point on the floor. No movement or collision; the
// viewport client drives it from the mouse (right button orbits, middle button pans, wheel zooms, F frames).
UCLASS(NotPlaceable)
class FLOOR101_API AFloor101EditingPawn : public APawn
{
	GENERATED_BODY()

public:
	AFloor101EditingPawn();

	virtual void BeginPlay() override;

	// Mouse deltas in viewport pixels.
	void Orbit(const FVector2D& DeltaPixels);
	void Pan(const FVector2D& DeltaPixels, const FVector2D& ViewportSize);
	// Positive steps move closer.
	void Zoom(float Steps);
	// Looks at the box (Unreal space) from the current direction, at a distance that fits it.
	void Frame(const FBox& Box);
	// Plan view: looking straight down (as far as the orbit allows) with north (scene +Y) up. Keeps focus and distance.
	void TopView();

	UCameraComponent* GetCamera() const { return Camera; }
	const FVector& GetFocus() const { return Focus; }
	double GetDistance() const { return Distance; }

private:
	void ApplyView();

	UPROPERTY(VisibleAnywhere, Category = "101st Floor")
	TObjectPtr<UCameraComponent> Camera;

	FVector Focus = FVector::ZeroVector; // cm
	double Yaw = -45.0;                  // degrees, Unreal convention (0 = +X)
	double Pitch = -35.0;                // degrees, looking down
	double Distance = 1500.0;            // cm from the focus
};
