#include "HdrCheck.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Floor101.h"
#include "Floor101DocumentSubsystem.h"
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
		FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HDR")));

		bool Finish(bool bPassed, const TCHAR* Detail)
		{
			UE_LOG(LogFloor101, Display, TEXT("F101HDRCheck: result=%s detail=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Detail);
			FPlatformMisc::RequestExit(false);
			return false;
		}

		bool ReadImage(bool bHDR)
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
			return Image.SizeX > 0 && Image.SizeY > 0 && Mean > 0.001 && (!bHDR || Maximum > 1.0f);
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
			}
			return true;
		}
	};
}

void StartIfRequested()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("F101HDRCheck")))
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
