#include "UiReviewCheck.h"

#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Floor101.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101GameViewportClient.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SFloor101Overlay.h"
#include "UnrealClient.h"

void Floor101::UIReview::Start(TSharedRef<SFloor101Overlay> Overlay, UFloor101GameViewportClient* Viewport)
{
	// Explicit opt-in capture also verifies the exact Shipping application; normal launches do nothing.
	FString Capture;
	if (!Viewport || !FParse::Value(FCommandLine::Get(), TEXT("F101UICapture="), Capture)) { return; }
	FString Screen;
	FString Project;
	FParse::Value(FCommandLine::Get(), TEXT("F101UIScreen="), Screen);
	FParse::Value(FCommandLine::Get(), TEXT("F101UIProject="), Project);
	if (!Project.IsEmpty())
	{
		if (auto* Document = Viewport->GetWorld()->GetSubsystem<UFloor101DocumentSubsystem>())
		{
			if (!Document->OpenProject(Project)) { RequestEngineExit(TEXT("UI review fixture could not be loaded")); return; }
		}
	}
	Viewport->FrameProject(true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Capture), true);
	const TWeakPtr<SFloor101Overlay> WeakOverlay = Overlay;
	const TWeakObjectPtr<UFloor101GameViewportClient> WeakViewport = Viewport;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakOverlay, WeakViewport, Capture, Screen, Elapsed = 0.0f, Phase = 0](float Delta) mutable
	{
		Elapsed += Delta;
		const auto Widget = WeakOverlay.Pin();
		if (!Widget || !WeakViewport.IsValid()) { return false; }
		if (Phase == 0 && Elapsed > 12.0f)
		{
			Widget->PrepareReview(Screen);
			Phase = 1;
		}
		if (Phase == 1 && Elapsed > 20.0f)
		{
			FScreenshotRequest::RequestScreenshot(Capture, true, false);
			F101_LOG(LogFloor101, Display, TEXT("F101UIReview: capture=%s screen=%s"), *Capture, *Screen);
			Phase = 2;
		}
		if (Elapsed > 25.0f)
		{
			RequestEngineExit(TEXT("F101 UI review complete"));
			return false;
		}
		return true;
	}));
}
