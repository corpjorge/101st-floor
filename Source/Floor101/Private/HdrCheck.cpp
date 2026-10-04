#include "HdrCheck.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Floor101.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101Display.h"
#include "Floor101GameViewportClient.h"
#include "Floor101Hdr.h"
#include "Floor101PathTracing.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HDRHelper.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

namespace Floor101::HDRCheck
{
namespace
{
	FTSTicker::FDelegateHandle Handle;
	struct FCheck
	{
		int32 Phase = 0;
		int32 Frames = 0;
		double Started = FPlatformTime::Seconds();
		FString ImagePath;
		bool bDisplayCheck = FParse::Param(FCommandLine::Get(), TEXT("F101DisplayCheck"));
		bool bExpectedHDR = false;
		bool bOriginalVSync = false;
		FIntPoint OriginalSize = FIntPoint::ZeroValue;
		FIntPoint NativeSize = FIntPoint::ZeroValue;
		FIntPoint LowerSize = FIntPoint::ZeroValue;
		FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HDR")));

		bool Finish(bool bPassed, const TCHAR* Detail)
		{
			UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: result=%s detail=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Detail);
			FPlatformMisc::RequestExit(false);
			return false;
		}

		bool ReadImage(bool bHDR, FIntPoint ExpectedSize = FIntPoint::ZeroValue)
		{
			FImage Image;
			if (!FImageUtils::LoadImage(*ImagePath, Image))
			{
				return false;
			}
			Image.ChangeFormat(ERawImageFormat::RGBA32F, EGammaSpace::Linear);
			float Maximum = 0.0f;
			double Sum = 0.0;
			for (const FLinearColor& Pixel : Image.AsRGBA32F())
			{
				Maximum = FMath::Max(Maximum, FMath::Max3(Pixel.R, Pixel.G, Pixel.B));
				Sum += Pixel.GetLuminance();
			}
			const double Mean = Sum / Image.GetNumPixels();
			UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: image=%s size=%dx%d max_linear=%.3f mean_linear=%.3f"),
				*ImagePath, Image.SizeX, Image.SizeY, Maximum, Mean);
			return Image.SizeX > 0 && Image.SizeY > 0 && Mean > 0.001 && (!bHDR || Maximum > 1.0f)
				&& (ExpectedSize == FIntPoint::ZeroValue || ExpectedSize == FIntPoint(Image.SizeX, Image.SizeY));
		}

		bool Tick()
		{
			if (FPlatformTime::Seconds() - Started > 240.0)
			{
				return Finish(false, TEXT("timeout"));
			}
			UFloor101GameViewportClient* Client = GEngine ? Cast<UFloor101GameViewportClient>(GEngine->GameViewport) : nullptr;
			if (!Client || !Client->GetWorld() || !Client->GetWorld()->HasBegunPlay() || !Client->Viewport || !Client->GetWindow().IsValid())
			{
				return true;
			}
			UFloor101DocumentSubsystem* Document = Client->GetWorld()->GetSubsystem<UFloor101DocumentSubsystem>();
			if (!Document || PathTracing::GetPendingShaders() > 0)
			{
				return true;
			}
			switch (Phase)
			{
			case 0:
				if (++Frames < 120) { break; }
				IFileManager::Get().MakeDirectory(*Directory, true);
				if (bDisplayCheck)
				{
					const Display::FStatus State = Display::Query(Client);
					OriginalSize = State.Image;
					bOriginalVSync = Display::IsVSyncEnabled();
					NativeSize = State.Native;
					bExpectedHDR = HDR::Query(Client).bAvailable;
					const TArray<FIntPoint> Modes = Display::Resolutions(Client);
					LowerSize = Modes.Contains(FIntPoint(1920, 1080)) && NativeSize != FIntPoint(1920, 1080)
						? FIntPoint(1920, 1080) : FIntPoint(1280, 720);
					UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: display_native=%dx%d initial_image=%dx%d initial_render=%dx%d modes=%d"),
						NativeSize.X, NativeSize.Y, State.Image.X, State.Image.Y, State.Render.X, State.Render.Y, Modes.Num());
					if (State.bFullscreen || !Modes.Contains(LowerSize) || !Display::ToggleFullscreen(Client, false))
					{
						return Finish(false, TEXT("cannot_enter_native_fullscreen"));
					}
					if (bExpectedHDR) { HDR::SetEnabled(Client, true, false); }
					Phase = 10;
					Frames = 0;
					break;
				}
				UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: available=%d os_peak_nits=%.2f"), HDR::Query(Client).bAvailable, HDR::Query(Client).PeakNits);
				if (!HDR::Query(Client).bAvailable)
				{
					if (HDR::SetEnabled(Client, true, false)) { return Finish(false, TEXT("enabled_on_unavailable_display")); }
					HDR::SetEnabled(Client, false, false);
					Phase = 5;
				}
				else
				{
					HDR::SetEnabled(Client, true, false); // exactly the operation used by the UI button; no preference saved
					Phase = 1;
				}
				Frames = 0;
				break;
			case 1:
				if (++Frames < 90) { break; }
				if (!Client->Viewport->GetSceneHDREnabled() || !Client->GetWindow()->GetIsHDR())
				{
					return Finish(false, TEXT("HDR_swapchain_or_window_not_active"));
				}
				{
					FHDRMetaData Metadata{};
					const TSharedPtr<SWindow> Window = Client->GetWindow();
					HDRGetMetaDataForWindow(Metadata, Window->GetPositionInScreen(), Window->GetPositionInScreen() + Window->GetSizeInScreen(),
						Window->GetNativeWindow()->GetOSWindowHandle());
					UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: hdr_on=1 output_device=%d gamut=%d tonemap_peak_nits=%.2f paper_white_nits=%.2f"),
						static_cast<int32>(Metadata.DisplayOutputFormat), static_cast<int32>(Metadata.DisplayColorGamut),
						Metadata.MaximumLuminanceInNits, Metadata.HDRPaperWhiteInNits);
					if (!Metadata.bHDRSupported || Metadata.MaximumLuminanceInNits <= 0.0f) { return Finish(false, TEXT("invalid_HDR_metadata")); }
				}
				Client->SetPhotoSamples(64);
				Client->SetPhotoMode(true);
				Phase = 2;
				break;
			case 2:
				if (!PathTracing::IsComplete()) { break; }
				ImagePath = FPaths::Combine(Directory, TEXT("HDR-photo.exr"));
				IFileManager::Get().Delete(*ImagePath, false, true);
				if (!Document->GetDefaultPhotoPath().EndsWith(TEXT(".exr")) || !Document->SavePhoto(ImagePath).ok())
				{
					return Finish(false, TEXT("HDR_photo_request_failed"));
				}
				Frames = 0;
				Phase = 3;
				break;
			case 3:
				if (++Frames < 30 || IFileManager::Get().FileSize(*ImagePath) <= 0) { break; }
				if (!ReadImage(true)) { return Finish(false, TEXT("invalid_HDR_EXR")); }
				Client->SetPhotoMode(false);
				HDR::SetEnabled(Client, false, false);
				Frames = 0;
				Phase = 5;
				break;
			case 5:
				if (++Frames < 90) { break; }
				if (Client->Viewport->GetSceneHDREnabled() || Client->GetWindow()->GetIsHDR()) { return Finish(false, TEXT("SDR_not_restored")); }
				UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: hdr_off=1 default_photo_png=%d"), Document->GetDefaultPhotoPath().EndsWith(TEXT(".png")));
				ImagePath = FPaths::Combine(Directory, TEXT("SDR-ui.png"));
				IFileManager::Get().Delete(*ImagePath, false, true);
				FScreenshotRequest::RequestScreenshot(ImagePath, true, false, false);
				Phase = 6;
				Frames = 0;
				break;
			case 6:
				if (++Frames < 30 || IFileManager::Get().FileSize(*ImagePath) <= 0) { break; }
				return Finish(ReadImage(false), TEXT("toggle_and_capture"));
			case 10:
				if (++Frames < 90) { break; }
				{
					const Display::FStatus State = Display::Query(Client);
					UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: fullscreen=%d window_mode=%d native=%dx%d image=%dx%d render=%dx%d hdr=%d"),
						State.bFullscreen, static_cast<int32>(Client->GetWindow()->GetWindowMode()), State.Native.X, State.Native.Y,
						State.Image.X, State.Image.Y, State.Render.X, State.Render.Y, Client->Viewport->GetSceneHDREnabled());
					if (!State.bFullscreen || State.Image != NativeSize || State.Render != NativeSize || (bExpectedHDR && !Client->Viewport->GetSceneHDREnabled()))
					{
						return Finish(false, TEXT("fullscreen_not_native_or_HDR_lost"));
					}
					if (bExpectedHDR) { UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: hdr_on=1 fullscreen=1")); }
				}
				if (!Display::SetVSyncEnabled(true, false)) { return Finish(false, TEXT("cannot_enable_vsync")); }
				Frames = 0;
				Phase = 11;
				break;
			case 11:
				if (++Frames < 60) { break; }
				if (!Display::IsVSyncEnabled() || Display::Query(Client).Image != NativeSize || Display::Query(Client).Render != NativeSize
					|| (bExpectedHDR && !Client->Viewport->GetSceneHDREnabled())) { return Finish(false, TEXT("vsync_changed_display_or_HDR")); }
				UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: vsync_on=1 native_and_HDR_preserved=1"));
				if (!Display::SetVSyncEnabled(false, false)) { return Finish(false, TEXT("cannot_disable_vsync")); }
				HDR::SetEnabled(Client, false, false); // readable documentation screenshot; HDR was checked above
				Phase = 16;
				Frames = 0;
				break;
			case 16:
				if (++Frames < 60) { break; }
				if (Display::IsVSyncEnabled()) { return Finish(false, TEXT("vsync_not_disabled")); }
				UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: vsync_off=1"));
				ImagePath = FPaths::Combine(Directory, TEXT("Fullscreen-native.png"));
				IFileManager::Get().Delete(*ImagePath, false, true);
				FScreenshotRequest::RequestScreenshot(ImagePath, true, false, false);
				Phase = 12;
				Frames = 0;
				break;
			case 12:
				if (++Frames < 30 || IFileManager::Get().FileSize(*ImagePath) <= 0) { break; }
				if (!ReadImage(false, NativeSize) || !Display::SetResolution(Client, LowerSize, false)) { return Finish(false, TEXT("selected_resolution_request_failed")); }
				Phase = 13;
				Frames = 0;
				break;
			case 13:
				if (++Frames < 90) { break; }
				{
					const Display::FStatus State = Display::Query(Client);
					UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: selected=%dx%d image=%dx%d render=%dx%d native=%dx%d"),
						LowerSize.X, LowerSize.Y, State.Image.X, State.Image.Y, State.Render.X, State.Render.Y, State.Native.X, State.Native.Y);
					if (State.Image != LowerSize || State.Render != LowerSize || State.Native != NativeSize) { return Finish(false, TEXT("selected_resolution_not_applied")); }
				}
				Client->SetPhotoSamples(64);
				Client->SetPhotoMode(true);
				if (!Display::SetResolution(Client, NativeSize, false)) { return Finish(false, TEXT("native_resolution_request_failed")); }
				Phase = 14;
				Frames = 0;
				break;
			case 14:
				if (++Frames < 90 || !PathTracing::IsComplete()) { break; }
				if (Display::Query(Client).Image != NativeSize || Display::Query(Client).Render != NativeSize) { return Finish(false, TEXT("photo_native_resolution_not_restored")); }
				UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: photo_native=1 samples=64 image=%dx%d render=%dx%d"),
					Display::Query(Client).Image.X, Display::Query(Client).Image.Y, Display::Query(Client).Render.X, Display::Query(Client).Render.Y);
				Client->SetPhotoMode(false);
				if (!Display::ToggleFullscreen(Client, false)) { return Finish(false, TEXT("cannot_return_to_window")); }
				Phase = 15;
				Frames = 0;
				break;
			case 15:
				if (++Frames < 90) { break; }
				{
					const Display::FStatus State = Display::Query(Client);
					UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: window_restored=%d image=%dx%d render=%dx%d"), !State.bFullscreen,
						State.Image.X, State.Image.Y, State.Render.X, State.Render.Y);
					if (State.bFullscreen || State.Image != OriginalSize || State.Render != OriginalSize) { return Finish(false, TEXT("window_not_restored")); }
					Client->SetPlanView(true);
					const bool bPlanCorrect = !Display::Query(Client).bWorldRendering && Display::Describe(Display::Query(Client)).ToString().Contains(TEXT("plano 2D"));
					Client->SetPlanView(false);
					if (!Display::SetVSyncEnabled(bOriginalVSync, false)) { return Finish(false, TEXT("vsync_not_restored")); }
					return Finish(bPlanCorrect, TEXT("native_and_selected_resolution"));
				}
			}
			return true;
		}
	};
}

void StartIfRequested()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("F101HDRCheck")) || FParse::Param(FCommandLine::Get(), TEXT("F101DisplayCheck")))
	{
		const TSharedRef<FCheck> Check = MakeShared<FCheck>();
		Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Check](float) { return Check->Tick(); }));
	}
}

void Stop()
{
	if (Handle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Handle); Handle.Reset(); }
}
}
