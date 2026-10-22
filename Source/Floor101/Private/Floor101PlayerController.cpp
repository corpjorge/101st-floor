#include "Floor101PlayerController.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101GameViewportClient.h"
#include "SFloor101Overlay.h"
#include "UiReviewCheck.h"
#include "Floor101Preferences.h"
#include "PackageCheck.h"
#include "Floor101Update.h"

AFloor101PlayerController::AFloor101PlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void AFloor101PlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	Floor101::Preferences::ApplySavedGraphics();
	if (UFloor101DocumentSubsystem* Subsystem = GetWorld()->GetSubsystem<UFloor101DocumentSubsystem>())
	{
		// The application's own view preference (ADR-0034); tests and the editor's toolset keep the default.
		Subsystem->SetInteriorDimensionsShown(Floor101::Preferences::ShowInteriorDimensions());
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
	if (Viewport)
	{
		Overlay = SNew(SFloor101Overlay)
			.Document(GetWorld()->GetSubsystem<UFloor101DocumentSubsystem>())
			.ViewportClient(Cast<UFloor101GameViewportClient>(Viewport));
		Viewport->AddViewportWidgetContent(Overlay.ToSharedRef());
		Floor101::UIReview::Start(StaticCastSharedRef<SFloor101Overlay>(Overlay.ToSharedRef()), Cast<UFloor101GameViewportClient>(Viewport));
		StaticCastSharedRef<SFloor101Overlay>(Overlay.ToSharedRef())->ShowStartup();
		Floor101::PackageCheck::Start(Cast<UFloor101GameViewportClient>(Viewport));
		Floor101::Update::Startup(Cast<UFloor101GameViewportClient>(Viewport)); // a newer release, if allowed (ADR-0039)
	}
}

void AFloor101PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Overlay)
	{
		const ULocalPlayer* LocalPlayer = GetLocalPlayer();
		if (UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
		}
		Overlay.Reset();
	}
	Super::EndPlay(EndPlayReason);
}
