// Regression coverage for close inspection and continuous camera navigation (ADR-0008).
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Floor101EditingPawn.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace
{
	constexpr EAutomationTestFlags CameraTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float CameraTolerance = 1e-6f;
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

	const FBox CameraReferenceBounds(FVector(0.0, -6.0, 0.0), FVector(355.0, 6.0, 240.0));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CameraCloseZoomTest, "Floor101.Interaction.CameraCloseZoom", CameraTestFlags)
bool FFloor101CameraCloseZoomTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld TestWorld;
	AFloor101EditingPawn* Pawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	if (!TestNotNull(TEXT("pawn"), Pawn))
	{
		return false;
	}
	Pawn->Frame(CameraReferenceBounds);
	Pawn->Zoom(30.0f);
	TestTrue(TEXT("wheel can inspect details closer than the old 50 cm limit"), Pawn->GetDistance() < 50.0);
	Pawn->Zoom(10000.0f);
	TestEqual(TEXT("close orbit distance"), Pawn->GetDistance(), 2.0, ScalarTolerance);
	const FVector Position = Pawn->GetActorLocation();
	const FVector Focus = Pawn->GetFocus();
	const FVector Forward = Pawn->GetActorForwardVector();
	Pawn->Zoom(1.0f);
	TestEqual(TEXT("another notch continues 5 cm past the old focus"), Pawn->GetActorLocation() - Position, Forward * 5.0, CameraTolerance);
	TestEqual(TEXT("the focus follows the dolly"), Pawn->GetFocus() - Focus, Forward * 5.0, CameraTolerance);
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
	TestEqual(TEXT("navigation look rotates around the camera, not the old orbit focus"), Pawn->GetActorLocation(), BeforeLook, CameraTolerance);
	TestTrue(TEXT("navigation look can see above the horizon"), Pawn->GetActorForwardVector().Z > 0.0);
	TestEqual(TEXT("navigation look retains the focus distance"), Pawn->GetDistance(), Distance, ScalarTolerance);
	const double AboveHorizonPitch = Pawn->GetActorRotation().Pitch;
	const FVector NavigationFocus = Pawn->GetFocus();
	Pawn->Orbit(FVector2D(0.0, 1.0));
	TestEqual(TEXT("resuming orbit gently lowers an above-horizon navigation pose without snapping"),
		Pawn->GetActorRotation().Pitch, AboveHorizonPitch - 0.25, ScalarTolerance);
	TestEqual(TEXT("resuming orbit retains the navigation focus"), Pawn->GetFocus(), NavigationFocus, CameraTolerance);
	Pawn->Look(FVector2D(0.0, -10000.0));
	TestTrue(TEXT("navigation look avoids flipping at the zenith"), Pawn->GetActorRotation().Pitch <= 89.0 + 1e-3);
	Pawn->Look(FVector2D(0.0, 10000.0));
	TestTrue(TEXT("navigation look avoids flipping at the nadir"), Pawn->GetActorRotation().Pitch >= -89.0 - 1e-3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CameraMovementTest, "Floor101.Interaction.CameraMovement", CameraTestFlags)
bool FFloor101CameraMovementTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld TestWorld;
	AFloor101EditingPawn* Pawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	AFloor101EditingPawn* SplitStepPawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	if (!TestNotNull(TEXT("pawn"), Pawn) || !TestNotNull(TEXT("comparison pawn"), SplitStepPawn))
	{
		return false;
	}
	Pawn->Frame(CameraReferenceBounds);
	SplitStepPawn->Frame(CameraReferenceBounds);
	const FVector Start = Pawn->GetActorLocation();
	const FVector Focus = Pawn->GetFocus();
	const FRotator View = Pawn->GetActorRotation();
	const double Distance = Pawn->GetDistance();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, false);
	const FVector ForwardMove = Pawn->GetActorLocation() - Start;
	const double NormalSpeed = ForwardMove.Size();
	TestEqual(TEXT("default level 5 travels at 3 m/s"), NormalSpeed, 300.0, ScalarTolerance);
	TestTrue(TEXT("W moves in the camera's direction"), FVector::DotProduct(ForwardMove.GetSafeNormal(), View.Vector()) > 0.9999);
	TestEqual(TEXT("movement translates the focus with the camera"), Pawn->GetFocus() - Focus, ForwardMove, CameraTolerance);
	TestEqual(TEXT("movement retains view direction"), Pawn->GetActorRotation(), View, 1e-3f);
	TestEqual(TEXT("movement retains orbit distance"), Pawn->GetDistance(), Distance, ScalarTolerance);
	SplitStepPawn->Move(FVector(1.0, 0.0, 0.0), 0.5f, false);
	SplitStepPawn->Move(FVector(1.0, 0.0, 0.0), 0.5f, false);
	TestEqual(TEXT("travel depends on elapsed time, not frame count"), Pawn->GetActorLocation(), SplitStepPawn->GetActorLocation(), CameraTolerance);

	Pawn->Frame(CameraReferenceBounds);
	Pawn->Move(FVector(0.0, 1.0, 0.0), 1.0f, false);
	TestTrue(TEXT("D moves screen right"), FVector::DotProduct((Pawn->GetActorLocation() - Start).GetSafeNormal(),
		FRotationMatrix(View).GetUnitAxis(EAxis::Y)) > 0.9999);
	Pawn->Frame(CameraReferenceBounds);
	Pawn->Move(FVector(1.0, 1.0, 0.0), 1.0f, false);
	TestEqual(TEXT("diagonal travel is not faster"), (Pawn->GetActorLocation() - Start).Size(), NormalSpeed, ScalarTolerance);
	Pawn->Frame(CameraReferenceBounds);
	Pawn->Move(FVector(1.0, 0.0, 1.0), 1.0f, false);
	TestEqual(TEXT("forward plus world-up travel is normalized too"), (Pawn->GetActorLocation() - Start).Size(), NormalSpeed, ScalarTolerance);
	Pawn->Frame(CameraReferenceBounds);
	Pawn->Move(FVector(0.0, 0.0, 1.0), 1.0f, false);
	TestEqual(TEXT("E rises in world space"), Pawn->GetActorLocation() - Start, FVector::UpVector * NormalSpeed, CameraTolerance);
	Pawn->Frame(CameraReferenceBounds);
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, true);
	TestEqual(TEXT("Shift travels four times faster"), (Pawn->GetActorLocation() - Start).Size(), NormalSpeed * 4.0, ScalarTolerance);
	const FVector BeforeZeroTime = Pawn->GetActorLocation();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 0.0f, false);
	TestEqual(TEXT("zero elapsed time does not move"), Pawn->GetActorLocation(), BeforeZeroTime, CameraTolerance);
	Pawn->Zoom(10000.0f);
	const FVector CloseStart = Pawn->GetActorLocation();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, false);
	TestEqual(TEXT("zoom cannot slow the selected navigation speed"), (Pawn->GetActorLocation() - CloseStart).Size(), NormalSpeed, ScalarTolerance);
	Pawn->SetNavigationSpeedLevel(1);
	const FVector FineStart = Pawn->GetActorLocation();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, false);
	TestEqual(TEXT("level 1 permits precise 10 cm/s travel"), (Pawn->GetActorLocation() - FineStart).Size(), 10.0, ScalarTolerance);
	Pawn->SetNavigationSpeedLevel(8);
	const FVector FastStart = Pawn->GetActorLocation();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 0.5f, true);
	TestEqual(TEXT("chosen level 8 responds immediately and combines with Shift"),
		(Pawn->GetActorLocation() - FastStart).Size(), 4000.0, ScalarTolerance);
	Pawn->SetNavigationSpeedLevel(-1);
	TestEqual(TEXT("invalid low level clamps to precision"), Pawn->GetNavigationSpeedLevel(), 1);
	Pawn->SetNavigationSpeedLevel(100);
	TestEqual(TEXT("invalid high level clamps to the fastest choice"), Pawn->GetNavigationSpeedLevel(), 10);
	Pawn->SetNavigationSpeedLevel(5);

	Pawn->SetPlanView(true);
	Pawn->Frame(CameraReferenceBounds);
	const FVector PlanStart = Pawn->GetFocus();
	const FRotator PlanView = Pawn->GetActorRotation();
	const double PlanWidth = Pawn->GetViewWidth();
	Pawn->Move(FVector(1.0, 0.0, 0.0), 1.0f, false);
	const FVector PlanForward = Pawn->GetFocus() - PlanStart;
	TestEqual(TEXT("plan uses the selected speed too"), PlanForward.Size(), 300.0, ScalarTolerance);
	TestTrue(TEXT("plan W pans north, up on screen"), PlanForward.Y < 0.0 && FMath::Abs(PlanForward.X) < CameraTolerance);
	TestEqual(TEXT("plan navigation stays at the same elevation"), Pawn->GetFocus().Z, PlanStart.Z, ScalarTolerance);
	Pawn->Frame(CameraReferenceBounds);
	Pawn->Move(FVector(0.0, 1.0, 0.0), 1.0f, false);
	TestTrue(TEXT("plan D pans east, right on screen"), Pawn->GetFocus().X > PlanStart.X && FMath::Abs(Pawn->GetFocus().Y - PlanStart.Y) < CameraTolerance);
	const FVector BeforeUp = Pawn->GetFocus();
	Pawn->Move(FVector(0.0, 0.0, 1.0), 1.0f, false);
	TestEqual(TEXT("plan Q/E does not alter the elevation"), Pawn->GetFocus(), BeforeUp, CameraTolerance);
	Pawn->Look(FVector2D(200.0, 200.0));
	TestEqual(TEXT("plan cannot be tilted by navigation look"), Pawn->GetActorRotation(), PlanView, 1e-3f);
	TestEqual(TEXT("plan movement does not change zoom"), Pawn->GetViewWidth(), PlanWidth, ScalarTolerance);
	const FVector BeforePlanZoom = Pawn->GetFocus();
	Pawn->Zoom(10000.0f);
	Pawn->Zoom(1.0f);
	TestEqual(TEXT("plan zoom changes scale only, with no dolly"), Pawn->GetFocus(), BeforePlanZoom, CameraTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CameraSpeedPreferenceTest, "Floor101.Interaction.CameraSpeedPreference", CameraTestFlags)
bool FFloor101CameraSpeedPreferenceTest::RunTest(const FString& Parameters)
{
	FCameraTestWorld TestWorld;
	AFloor101EditingPawn* Pawn = TestWorld.World->SpawnActor<AFloor101EditingPawn>();
	if (!TestNotNull(TEXT("pawn"), Pawn) || !TestNotNull(TEXT("config"), GConfig)) { return false; }
	const FString OriginalIni = GGameUserSettingsIni;
	const FString TestIni = FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("CameraSpeed"), TEXT(".ini")));
	GGameUserSettingsIni = TestIni;
	ON_SCOPE_EXIT
	{
		GGameUserSettingsIni = OriginalIni;
		GConfig->UnloadFile(TestIni);
		IFileManager::Get().Delete(*TestIni);
	};
	if (!TestTrue(TEXT("temporary preferences created"), FFileHelper::SaveStringToFile(TEXT("[SectionsToSave]\nbCanSaveAllSections=true\n"), *TestIni))) { return false; }
	GConfig->LoadFile(TestIni);
	Pawn->LoadNavigationSpeedLevel();
	TestEqual(TEXT("new installs use level 5"), Pawn->GetNavigationSpeedLevel(), 5);
	Pawn->SetNavigationSpeedLevel(7, true);
	GConfig->UnloadFile(TestIni);
	GConfig->LoadFile(TestIni);
	Pawn->SetNavigationSpeedLevel(1);
	Pawn->LoadNavigationSpeedLevel();
	TestEqual(TEXT("speed survives reloading preferences from disk"), Pawn->GetNavigationSpeedLevel(), 7);
	GConfig->SetInt(TEXT("Floor101.Camera"), TEXT("NavigationSpeedLevel"), -99, TestIni);
	Pawn->LoadNavigationSpeedLevel();
	TestEqual(TEXT("corrupt saved level is bounded safely"), Pawn->GetNavigationSpeedLevel(), 1);
	return true;
}

#endif
