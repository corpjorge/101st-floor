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
	Yaw = FRotator::NormalizeAxis(Yaw + DeltaPixels.X * DegreesPerPixel);
	Pitch = FMath::Clamp(Pitch - DeltaPixels.Y * DegreesPerPixel, MinPitch, MaxPitch);
	ApplyView();
}

void AFloor101EditingPawn::Pan(const FVector2D& DeltaPixels, const FVector2D& ViewportSize)
{
	// The focus follows the cursor: one viewport height of drag moves about the visible height at the focus distance.
	const double HalfFovRadians = FMath::DegreesToRadians(Camera->FieldOfView * 0.5);
	const double WorldPerPixel = 2.0 * Distance * FMath::Tan(HalfFovRadians) / FMath::Max(ViewportSize.X, 1.0);
	const FRotator View(Pitch, Yaw, 0.0);
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
	const double HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / FMath::Max(static_cast<double>(Camera->AspectRatio), 1.0));
	const double Radius = FMath::Max(Box.GetExtent().Size(), 100.0);
	Focus = Box.GetCenter();
	Distance = FMath::Clamp(Margin * Radius / FMath::Sin(HalfVertical), MinDistance, MaxDistance);
	ApplyView();
}

void AFloor101EditingPawn::TopView()
{
	// Unreal yaw -90 looks along -Y, which is scene north (the Y mirror of ADR-0003): north ends up at the top.
	Yaw = -90.0;
	Pitch = MinPitch;
	ApplyView();
}

void AFloor101EditingPawn::ApplyView()
{
	const FRotator View(Pitch, Yaw, 0.0);
	SetActorLocationAndRotation(Focus - View.Vector() * Distance, View);
}
