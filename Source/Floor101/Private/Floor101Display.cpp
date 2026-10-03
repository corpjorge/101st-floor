#include "Floor101Display.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Floor101.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

#include <atomic>

#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	#include <windows.h>
	#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "Floor101Display"

namespace Floor101::Display
{
namespace
{
	FDisplayMetrics Metrics;
	double LastMetrics = -1.0;
	FIntPoint WindowedSize(1280, 720);

	class FRenderSize : public FSceneViewExtensionBase
	{
	public:
		explicit FRenderSize(const FAutoRegister& Register) : FSceneViewExtensionBase(Register) {}
		std::atomic<const FRenderTarget*> Target{nullptr};
		std::atomic<uint64> Pixels{0};
		virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View,
			FPostProcessingPassDelegateArray& Callbacks, bool bEnabled) override
		{
			if (Pass == EPostProcessingPass::BeforeDOF && View.Family && View.Family->RenderTarget == Target.load())
			{
				Callbacks.Add(FAfterPassCallbackDelegate::CreateLambda([this](FRDGBuilder& GraphBuilder, const FSceneView&,
					const FPostProcessMaterialInputs& Inputs)
				{
					const FScreenPassTextureSlice Color = Inputs.GetInput(EPostProcessMaterialInput::SceneColor);
					const FIntPoint Size = Color.ViewRect.Size();
					Pixels = (static_cast<uint64>(Size.X) << 32) | static_cast<uint32>(Size.Y);
					return FScreenPassTexture::CopyFromSlice(GraphBuilder, Color);
				}));
			}
		}
	};
	TSharedPtr<FRenderSize, ESPMode::ThreadSafe> Probe;

	int32 MonitorIndex(const UGameViewportClient* Client)
	{
		if (!Client || !Client->GetWindow().IsValid()) { return INDEX_NONE; }
		const FVector2D Start = Client->GetWindow()->GetPositionInScreen();
		const FVector2D End = Start + Client->GetWindow()->GetSizeInScreen();
		double Best = 0.0;
		int32 Index = INDEX_NONE;
		for (int32 I = 0; I < Metrics.MonitorInfo.Num(); ++I)
		{
			const FPlatformRect& Rect = Metrics.MonitorInfo[I].DisplayRect;
			const double Area = FMath::Max(0.0, FMath::Min(End.X, static_cast<double>(Rect.Right)) - FMath::Max(Start.X, static_cast<double>(Rect.Left)))
				* FMath::Max(0.0, FMath::Min(End.Y, static_cast<double>(Rect.Bottom)) - FMath::Max(Start.Y, static_cast<double>(Rect.Top)));
			if (Area > Best) { Best = Area; Index = I; }
		}
		return Index;
	}

	void FullResolution(UGameUserSettings& Settings)
	{
		Settings.SetDynamicResolutionEnabled(false);
		Settings.SetResolutionScaleValueEx(100.0f);
		for (const TCHAR* Name : {TEXT("r.ScreenPercentage"), TEXT("r.SecondaryScreenPercentage.GameViewport")})
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name)) { Variable->Set(100.0f, ECVF_SetByCode); }
		}
		if (IConsoleVariable* Dynamic = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.OperationMode"))) { Dynamic->Set(0, ECVF_SetByCode); }
	}

	FText SizeText(FIntPoint Size)
	{
		return Size.X > 0 && Size.Y > 0 ? FText::FromString(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y)) : LOCTEXT("Waiting", "comprobando…");
	}
}

void Tick(UGameViewportClient* Client)
{
	if (!Client || !Client->Viewport || !Client->GetWindow().IsValid()) { return; }
	if (!Probe)
	{
		Probe = FSceneViewExtensions::NewExtension<FRenderSize>();
		if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr) { FullResolution(*Settings); }
	}
	Probe->Target = Client->Viewport;
	const double Now = FPlatformTime::Seconds();
	if (LastMetrics < 0.0 || Now - LastMetrics > 1.0)
	{
		FDisplayMetrics::RebuildDisplayMetrics(Metrics);
		LastMetrics = Now;
	}
}

FStatus Query(const UGameViewportClient* Client)
{
	FStatus Status;
	const int32 Index = MonitorIndex(Client);
	if (Index != INDEX_NONE)
	{
		const FMonitorInfo& Monitor = Metrics.MonitorInfo[Index];
		Status.MonitorName = Monitor.FriendlyName;
		Status.Native = FIntPoint(Monitor.NativeWidth, Monitor.NativeHeight);
		Status.Desktop = FIntPoint(Monitor.DisplayRect.Right - Monitor.DisplayRect.Left, Monitor.DisplayRect.Bottom - Monitor.DisplayRect.Top);
	}
	if (Client && Client->Viewport) { Status.Image = Client->Viewport->GetSizeXY(); }
	Status.bWorldRendering = Client && !Client->bDisableWorldRendering;
	if (Client && Client->GetWindow().IsValid()) { Status.bFullscreen = Client->GetWindow()->GetWindowMode() != EWindowMode::Windowed; }
	if (Probe)
	{
		const uint64 Pixels = Probe->Pixels.load();
		Status.Render = FIntPoint(static_cast<int32>(Pixels >> 32), static_cast<int32>(Pixels & 0xffffffff));
	}
	return Status;
}

TArray<FIntPoint> Resolutions(const UGameViewportClient* Client)
{
	TArray<FIntPoint> Result;
	const int32 Index = MonitorIndex(Client);
	if (Index == INDEX_NONE) { return Result; }
	const FIntPoint Native = Query(Client).Native;
#if PLATFORM_WINDOWS
	MONITORINFOEXW Info{};
	Info.cbSize = sizeof(Info);
	if (GetMonitorInfoW(static_cast<HMONITOR>(Metrics.MonitorInfo[Index].NativeHandle), reinterpret_cast<MONITORINFO*>(&Info)))
	{
		DEVMODEW Mode{};
		Mode.dmSize = sizeof(Mode);
		for (DWORD I = 0; EnumDisplaySettingsW(Info.szDevice, I, &Mode); ++I)
		{
			if (Mode.dmPelsWidth >= 1280 && Mode.dmPelsHeight >= 720 && Mode.dmPelsWidth <= static_cast<uint32>(Native.X) && Mode.dmPelsHeight <= static_cast<uint32>(Native.Y))
			{
				Result.AddUnique(FIntPoint(Mode.dmPelsWidth, Mode.dmPelsHeight));
			}
		}
	}
#endif
	if (Native.X > 0 && Native.Y > 0) { Result.AddUnique(Native); }
	Result.Sort([](FIntPoint A, FIntPoint B) { return static_cast<int64>(A.X) * A.Y > static_cast<int64>(B.X) * B.Y; });
	return Result;
}

bool SetResolution(UGameViewportClient* Client, FIntPoint Size, bool bSave)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const int32 Index = MonitorIndex(Client);
	if (!Settings || !Client || Index == INDEX_NONE || !Resolutions(Client).Contains(Size)) { return false; }
	Settings->SetDisplayProperties(Metrics.MonitorInfo[Index].ID, Index);
	Settings->SetFullscreenMode(Query(Client).bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);
	Settings->SetScreenResolution(Size);
	FullResolution(*Settings);
	Settings->ApplyResolutionSettings(false); // explicit UI choices supersede startup command-line overrides
	if (bSave) { Settings->ConfirmVideoMode(); Settings->SaveSettings(); }
	UE_LOG(LogFloor101, Display, TEXT("F101Display: requested=%dx%d fullscreen=%d render_scale=100"), Size.X, Size.Y, Query(Client).bFullscreen);
	return true;
}

bool ToggleFullscreen(UGameViewportClient* Client, bool bSave)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const FStatus Status = Query(Client);
	const int32 Index = MonitorIndex(Client);
	if (!Settings || !Client || Index == INDEX_NONE || Status.Native.X <= 0 || Status.Native.Y <= 0) { return false; }
	Settings->SetDisplayProperties(Metrics.MonitorInfo[Index].ID, Index);
	if (Status.bFullscreen)
	{
		Settings->SetFullscreenMode(EWindowMode::Windowed);
		Settings->SetScreenResolution(WindowedSize);
	}
	else
	{
		WindowedSize = Status.Image;
		Settings->SetFullscreenMode(EWindowMode::Fullscreen);
		Settings->SetScreenResolution(Status.Native);
	}
	FullResolution(*Settings);
	Settings->ApplyResolutionSettings(false);
	if (bSave) { Settings->ConfirmVideoMode(); Settings->SaveSettings(); }
	return true;
}

FText Describe(const FStatus& Status)
{
	return FText::Format(LOCTEXT("ResolutionState", "Pantalla: {0} · Imagen: {1} · Render: {2}"),
		SizeText(Status.Native), SizeText(Status.Image), Status.bWorldRendering ? SizeText(Status.Render) : LOCTEXT("PlanRendering", "plano 2D"));
}
}

#undef LOCTEXT_NAMESPACE
