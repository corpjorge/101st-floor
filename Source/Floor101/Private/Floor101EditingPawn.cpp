#include "Floor101EditingPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

namespace
{
	constexpr double DegreesPerPixel = 0.25;
	constexpr double MinPitch = -89.0;
	constexpr double MaxPitch = -5.0;
	constexpr double MinDistance = 50.0;     // cm
	constexpr double MaxDistance = 200000.0; // cm
	constexpr double ZoomFactorPerStep = 0.85;
	constexpr double PlanCameraHeight = 100000.0; // cm over the focus: above any building, so pointer rays start above it
}

AFloor101EditingPawn::AFloor101EditingPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	SetActorEnableCollision(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
}

void AFloor101EditingPawn::BeginPlay()
{
	Super::BeginPlay();
	ApplyView();
}

void AFloor101EditingPawn::Orbit(const FVector2D& DeltaPixels)
{
	if (bPlanView)
	{
		return;
	}
	Yaw = FRotator::NormalizeAxis(Yaw + DeltaPixels.X * DegreesPerPixel);
	Pitch = FMath::Clamp(Pitch - DeltaPixels.Y * DegreesPerPixel, MinPitch, MaxPitch);
	ApplyView();
}

void AFloor101EditingPawn::Pan(const FVector2D& DeltaPixels, const FVector2D& ViewportSize)
{
	// The focus follows the cursor: one viewport width of drag moves the visible width at the focus distance.
	const double WorldPerPixel = GetViewWidth() / FMath::Max(ViewportSize.X, 1.0);
	const FRotator View = bPlanView ? FRotator(-90.0, -90.0, 0.0) : FRotator(Pitch, Yaw, 0.0);
	const FVector Right = FRotationMatrix(View).GetUnitAxis(EAxis::Y);
	const FVector Up = FRotationMatrix(View).GetUnitAxis(EAxis::Z);
	Focus += (-Right * DeltaPixels.X + Up * DeltaPixels.Y) * WorldPerPixel;
	ApplyView();
}

void AFloor101EditingPawn::Zoom(float Steps)
{
	Distance = FMath::Clamp(Distance * FMath::Pow(ZoomFactorPerStep, static_cast<double>(Steps)), MinDistance, MaxDistance);
	ApplyView();
}

void AFloor101EditingPawn::Frame(const FBox& Box)
{
	if (!Box.IsValid)
	{
		return;
	}
	// FieldOfView is horizontal; a landscape view is narrower vertically, so the sphere around the box is fitted to the
	// vertical angle, with some margin.
	constexpr double Margin = 1.15;
	const double HalfHorizontal = FMath::DegreesToRadians(Camera->FieldOfView * 0.5);
	Focus = Box.GetCenter();
	if (bPlanView)
	{
		// The plan's footprint (with room for its dimensions) fits the width and the height of the view.
		const double Aspect = FMath::Max(static_cast<double>(Camera->AspectRatio), 1.0);
		const double Width = Margin * FMath::Max(Box.GetSize().X, Box.GetSize().Y * Aspect) + 400.0;
		Distance = FMath::Clamp(Width / (2.0 * FMath::Tan(HalfHorizontal)), MinDistance, MaxDistance);
		ApplyView();
		return;
	}
	const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / FMath::Max(static_cast<double>(Camera->AspectRatio), 1.0));
	const double Radius = FMath::Max(Box.GetExtent().Size(), 100.0);
	Distance = FMath::Clamp(Margin * Radius / FMath::Sin(HalfVertical), MinDistance, MaxDistance);
	ApplyView();
}

void AFloor101EditingPawn::SetPlanView(bool bPlan)
{
	bPlanView = bPlan;
	Camera->SetProjectionMode(bPlan ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective);
	ApplyView();
}

double AFloor101EditingPawn::GetViewWidth() const
{
	return 2.0 * Distance * FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5));
}

void AFloor101EditingPawn::TopView()
{
	// Unreal yaw -90 looks along -Y, which is scene north (the Y mirror of ADR-0003): north ends up at the top.
	Yaw = -90.0;
	Pitch = MinPitch;
	ApplyView();
}

void AFloor101EditingPawn::RaiseFocus(double DeltaCm)
{
	Focus.Z += DeltaCm;
	ApplyView();
}

void AFloor101EditingPawn::ApplyView()
{
	if (bPlanView)
	{
		// Yaw -90 looks along -Y, scene north (the Y mirror of ADR-0003): north ends up at the top, east on the right.
		Camera->SetOrthoWidth(static_cast<float>(GetViewWidth()));
		SetActorLocationAndRotation(Focus + FVector(0.0, 0.0, PlanCameraHeight), FRotator(-90.0, -90.0, 0.0));
		return;
	}
	const FRotator View(Pitch, Yaw, 0.0);
	SetActorLocationAndRotation(Focus - View.Vector() * Distance, View);
}
