// Regression coverage for close inspection and continuous camera navigation (ADR-0008).
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Floor101EditingPawn.h"

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float Tolerance = 1e-6f;
	constexpr double ScalarTolerance = 1e-6;

	struct FCameraTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101CameraNavigationTestWorld"));

		FCameraTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}

		~FCameraTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	const FBox Wall(FVector(0.0, -6.0, 0.0), FVector(355.0, 6.0, 240.0));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CameraCloseZoomTest, "Floor101.Interaction.CameraCloseZoom", TestFlags)
bool FFloor101CameraCloseZoomTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld TestWorld;
	AFloor101EditingPawn* Pawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	if (!TestNotNull(TEXT("pawn"), Pawn))
	{
		return false;
	}
	Pawn->Frame(Wall);
	Pawn->Zoom(30.0f);
	TestTrue(TEXT("wheel can inspect details closer than the old 50 cm limit"), Pawn->GetDistance() < 50.0);
	Pawn->Zoom(10000.0f);
	TestEqual(TEXT("close orbit distance"), Pawn->GetDistance(), 2.0, ScalarTolerance);
	const FVector Position = Pawn->GetActorLocation();
	const FVector Focus = Pawn->GetFocus();
	const FVector Forward = Pawn->GetActorForwardVector();
	Pawn->Zoom(1.0f);
	TestEqual(TEXT("another notch continues 5 cm past the old focus"), Pawn->GetActorLocation() - Position, Forward * 5.0, Tolerance);
	TestEqual(TEXT("the focus follows the dolly"), Pawn->GetFocus() - Focus, Forward * 5.0, Tolerance);
	TestEqual(TEXT("camera and focus keep the safe close distance"), Pawn->GetDistance(), 2.0, ScalarTolerance);
	const FVector ClosePosition = Pawn->GetActorLocation();
	Pawn->Zoom(-1.0f);
	TestTrue(TEXT("wheel out backs away after a dolly"), FVector::DotProduct(Pawn->GetActorLocation() - ClosePosition, Forward) < 0.0);
	const FVector BeforeLargeEvent = Pawn->GetActorLocation();
	Pawn->Zoom(10000.0f);
	TestTrue(TEXT("a large event cannot add more than 50 cm dolly plus remaining approach"),
		(Pawn->GetActorLocation() - BeforeLargeEvent).Size() < 51.0);

	const FVector BeforeLook = Pawn->GetActorLocation();
	const double Distance = Pawn->GetDistance();
	Pawn->Look(FVector2D(40.0, -400.0));
	TestEqual(TEXT("navigation look rotates around the camera, not the old orbit focus"), Pawn->GetActorLocation(), BeforeLook, Tolerance);
	TestTrue(TEXT("navigation look can see above the horizon"), Pawn->GetActorForwardVector().Z > 0.0);
	TestEqual(TEXT("navigation look retains the focus distance"), Pawn->GetDistance(), Distance, ScalarTolerance);
	const double AboveHorizonPitch = Pawn->GetActorRotation().Pitch;
	const FVector NavigationFocus = Pawn->GetFocus();
	Pawn->Orbit(FVector2D(0.0, 1.0));
	TestEqual(TEXT("resuming orbit gently lowers an above-horizon navigation pose without snapping"),
		Pawn->GetActorRotation().Pitch, AboveHorizonPitch - 0.25, ScalarTolerance);
	TestEqual(TEXT("resuming orbit retains the navigation focus"), Pawn->GetFocus(), NavigationFocus, Tolerance);
	Pawn->Look(FVector2D(0.0, -10000.0));
	TestTrue(TEXT("navigation look avoids flipping at the zenith"), Pawn->GetActorRotation().Pitch <= 89.0 + 1e-3);
	Pawn->Look(FVector2D(0.0, 10000.0));
	TestTrue(TEXT("navigation look avoids flipping at the nadir"), Pawn->GetActorRotation().Pitch >= -89.0 - 1e-3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CameraMovementTest, "Floor101.Interaction.CameraMovement", TestFlags)
bool FFloor101CameraMovementTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld TestWorld;
	AFloor101EditingPawn* Pawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	AFloor101EditingPawn* SplitStepPawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	if (!TestNotNull(TEXT("pawn"), Pawn) || !TestNotNull(TEXT("comparison pawn"), SplitStepPawn))
	{
		return false;
	}
	Pawn->Frame(Wall);
	SplitStepPawn->Frame(Wall);
	const FVector Start = Pawn->GetActorLocation();
	const FVector Focus = Pawn->GetFocus();
	const FRotator View = Pawn->GetActorRotation();
	const double Distance = Pawn->GetDistance();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, false);
	const FVector ForwardMove = Pawn->GetActorLocation() - Start;
	const double NormalSpeed = ForwardMove.Size();
	TestTrue(TEXT("W moves in the camera's direction"), FVector::DotProduct(ForwardMove.GetSafeNormal(), View.Vector()) > 0.9999);
	TestEqual(TEXT("movement translates the focus with the camera"), Pawn->GetFocus() - Focus, ForwardMove, Tolerance);
	TestEqual(TEXT("movement retains view direction"), Pawn->GetActorRotation(), View, 1e-3f);
	TestEqual(TEXT("movement retains orbit distance"), Pawn->GetDistance(), Distance, ScalarTolerance);
	SplitStepPawn->Move(FVector(1.0, 0.0, 0.0), 0.5f, false);
	SplitStepPawn->Move(FVector(1.0, 0.0, 0.0), 0.5f, false);
	TestEqual(TEXT("travel depends on elapsed time, not frame count"), Pawn->GetActorLocation(), SplitStepPawn->GetActorLocation(), Tolerance);

	Pawn->Frame(Wall);
	Pawn->Move(FVector(0.0, 1.0, 0.0), 1.0f, false);
	TestTrue(TEXT("D moves screen right"), FVector::DotProduct((Pawn->GetActorLocation() - Start).GetSafeNormal(),
		FRotationMatrix(View).GetUnitAxis(EAxis::Y)) > 0.9999);
	Pawn->Frame(Wall);
	Pawn->Move(FVector(1.0, 1.0, 0.0), 1.0f, false);
	TestEqual(TEXT("diagonal travel is not faster"), (Pawn->GetActorLocation() - Start).Size(), NormalSpeed, ScalarTolerance);
	Pawn->Frame(Wall);
	Pawn->Move(FVector(1.0, 0.0, 1.0), 1.0f, false);
	TestEqual(TEXT("forward plus world-up travel is normalized too"), (Pawn->GetActorLocation() - Start).Size(), NormalSpeed, ScalarTolerance);
	Pawn->Frame(Wall);
	Pawn->Move(FVector(0.0, 0.0, 1.0), 1.0f, false);
	TestEqual(TEXT("E rises in world space"), Pawn->GetActorLocation() - Start, FVector::UpVector * NormalSpeed, Tolerance);
	Pawn->Frame(Wall);
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, true);
	TestEqual(TEXT("Shift travels four times faster"), (Pawn->GetActorLocation() - Start).Size(), NormalSpeed * 4.0, ScalarTolerance);
	const FVector BeforeZeroTime = Pawn->GetActorLocation();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 0.0f, false);
	TestEqual(TEXT("zero elapsed time does not move"), Pawn->GetActorLocation(), BeforeZeroTime, Tolerance);
	Pawn->Zoom(10000.0f);
	const FVector CloseStart = Pawn->GetActorLocation();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, false);
	TestEqual(TEXT("close-up navigation gives fine 10 cm/s control"), (Pawn->GetActorLocation() - CloseStart).Size(), 10.0, ScalarTolerance);

	Pawn->SetPlanView(true);
	Pawn->Frame(Wall);
	const FVector PlanStart = Pawn->GetFocus();
	const FRotator PlanView = Pawn->GetActorRotation();
	const double PlanWidth = Pawn->GetViewWidth();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, false);
	const FVector PlanForward = Pawn->GetFocus() - PlanStart;
	TestTrue(TEXT("plan W pans north, up on screen"), PlanForward.Y < 0.0 && FMath::Abs(PlanForward.X) < Tolerance);
	TestEqual(TEXT("plan navigation stays at the same elevation"), Pawn->GetFocus().Z, PlanStart.Z, ScalarTolerance);
	Pawn->Frame(Wall);
	Pawn->Move(FVector(0.0, 1.0, 0.0), 1.0f, false);
	TestTrue(TEXT("plan D pans east, right on screen"), Pawn->GetFocus().X > PlanStart.X && FMath::Abs(Pawn->GetFocus().Y - PlanStart.Y) < Tolerance);
	const FVector BeforeUp = Pawn->GetFocus();
	Pawn->Move(FVector(0.0, 0.0, 1.0), 1.0f, false);
	TestEqual(TEXT("plan Q/E does not alter the elevation"), Pawn->GetFocus(), BeforeUp, Tolerance);
	Pawn->Look(FVector2D(200.0, 200.0));
	TestEqual(TEXT("plan cannot be tilted by navigation look"), Pawn->GetActorRotation(), PlanView, 1e-3f);
	TestEqual(TEXT("plan movement does not change zoom"), Pawn->GetViewWidth(), PlanWidth, ScalarTolerance);
	const FVector BeforePlanZoom = Pawn->GetFocus();
	Pawn->Zoom(10000.0f);
	Pawn->Zoom(1.0f);
	TestEqual(TEXT("plan zoom changes scale only, with no dolly"), Pawn->GetFocus(), BeforePlanZoom, Tolerance);
	return true;
}

#endif
