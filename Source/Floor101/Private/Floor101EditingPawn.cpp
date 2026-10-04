#include "Floor101EditingPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	constexpr double DegreesPerPixel = 0.25;
	constexpr double MinPitch = -89.0;
	constexpr double MaxPitch = -5.0;
	constexpr double MinDistance = 2.0;      // cm; the configured 1 cm near plane permits inspecting small details
	constexpr double MaxDistance = 200000.0; // cm
	constexpr double ZoomFactorPerStep = 0.85;
	constexpr double CloseDollyPerStep = 5.0; // cm once the camera has reached its orbit focus
	constexpr double MaxCloseDollySteps = 10.0; // bound a single unusually large wheel event
	constexpr double PlanCameraHeight = 100000.0; // cm over the focus: above any building, so pointer rays start above it
	constexpr double NavigationSpeeds[] = {10.0, 25.0, 50.0, 100.0, 300.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0}; // cm/s
	constexpr double FastMoveMultiplier = 4.0;
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
	LoadNavigationSpeedLevel();
	ApplyView();
}

void AFloor101EditingPawn::LoadNavigationSpeedLevel()
{
	int32 SavedLevel = 5;
	if (GConfig) { GConfig->GetInt(TEXT("Floor101.Camera"), TEXT("NavigationSpeedLevel"), SavedLevel, GGameUserSettingsIni); }
	SetNavigationSpeedLevel(SavedLevel);
}

double AFloor101EditingPawn::NavigationSpeedForLevel(int32 Level)
{
	return NavigationSpeeds[FMath::Clamp(Level, 1, 10) - 1];
}

void AFloor101EditingPawn::SetNavigationSpeedLevel(int32 Level, bool bSave)
{
	NavigationSpeedLevel = FMath::Clamp(Level, 1, 10);
	if (bSave && GConfig)
	{
		GConfig->SetInt(TEXT("Floor101.Camera"), TEXT("NavigationSpeedLevel"), NavigationSpeedLevel, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void AFloor101EditingPawn::Orbit(const FVector2D& DeltaPixels)
{
	if (bPlanView)
	{
		return;
	}
	Yaw = FRotator::NormalizeAxis(Yaw + DeltaPixels.X * DegreesPerPixel);
	// A navigation look may leave the camera above the horizon. Resume orbiting from that pose without snapping
	// downward; ordinary downward-facing orbits still cannot tilt past their usual upper limit.
	const double UpperPitch = FMath::Max(MaxPitch, Pitch);
	Pitch = FMath::Clamp(Pitch - DeltaPixels.Y * DegreesPerPixel, MinPitch, UpperPitch);
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

void AFloor101EditingPawn::Look(const FVector2D& DeltaPixels)
{
	if (bPlanView)
	{
		return;
	}
	const FVector Position = GetActorLocation();
	Yaw = FRotator::NormalizeAxis(Yaw + DeltaPixels.X * DegreesPerPixel);
	Pitch = FMath::Clamp(Pitch - DeltaPixels.Y * DegreesPerPixel, MinPitch, -MinPitch);
	Focus = Position + FRotator(Pitch, Yaw, 0.0).Vector() * Distance;
	ApplyView();
}

void AFloor101EditingPawn::Zoom(float Steps)
{
	if (!FMath::IsFinite(Steps))
	{
		return;
	}
	if (!bPlanView && Steps > 0.0f)
	{
		// Once the old focus is reached, move both camera and focus forward instead of getting stuck there. Account
		// for any notches spent approaching the focus before dollying; a large event cannot leap through a building.
		const double StepsToClose = FMath::Loge(MinDistance / Distance) / FMath::Loge(ZoomFactorPerStep);
		const double DollySteps = FMath::Clamp(static_cast<double>(Steps) - StepsToClose, 0.0, MaxCloseDollySteps);
		Focus += FRotator(Pitch, Yaw, 0.0).Vector() * (CloseDollyPerStep * DollySteps);
	}
	Distance = FMath::Clamp(Distance * FMath::Pow(ZoomFactorPerStep, static_cast<double>(Steps)), MinDistance, MaxDistance);
	ApplyView();
}

void AFloor101EditingPawn::Move(const FVector& LocalDirection, float DeltaTime, bool bFast)
{
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f || LocalDirection.ContainsNaN())
	{
		return;
	}
	const FRotationMatrix Basis(FRotator(Pitch, Yaw, 0.0));
	// The plan is cardinal by definition. Exact axes avoid tiny height/easting drift from rotating by -90 degrees.
	const FVector Forward = bPlanView ? FVector(0.0, -1.0, 0.0) : Basis.GetUnitAxis(EAxis::X);
	const FVector Right = bPlanView ? FVector(1.0, 0.0, 0.0) : Basis.GetUnitAxis(EAxis::Y);
	const FVector Up = bPlanView ? FVector::ZeroVector : FVector::UpVector;
	const FVector Direction = (Forward * LocalDirection.X + Right * LocalDirection.Y + Up * LocalDirection.Z).GetSafeNormal();
	if (Direction.IsZero())
	{
		return;
	}
	const double Speed = NavigationSpeedForLevel(NavigationSpeedLevel) * (bFast ? FastMoveMultiplier : 1.0);
	Focus += Direction * Speed * static_cast<double>(DeltaTime);
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
