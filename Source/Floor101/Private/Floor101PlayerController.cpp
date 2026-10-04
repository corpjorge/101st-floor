#include "Floor101PlayerController.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101GameViewportClient.h"
#include "SFloor101Overlay.h"
#include "UiReviewCheck.h"
#include "Floor101Preferences.h"

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
