#include "RenderCheck.h"
#include "Floor101.h"

#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RHIGlobals.h"
#include "RHIStrings.h"
#include "RenderUtils.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

namespace Floor101::RenderCheck
{
namespace
{
	// Console variables whose effective runtime values are reported. They are read, never written.
	const TCHAR* const ReportedCVars[] = {
		TEXT("r.RayTracing"),
		TEXT("r.RayTracing.Enable"),
		TEXT("r.SkinCache.CompileShaders"),
		TEXT("r.DynamicGlobalIlluminationMethod"),
		TEXT("r.ReflectionMethod"),
		TEXT("r.Lumen.HardwareRayTracing"),
		TEXT("r.Lumen.HardwareRayTracing.LightingMode"),
		TEXT("r.GenerateMeshDistanceFields"),
		TEXT("r.Shadow.Virtual.Enable"),
		TEXT("r.Nanite"),
		TEXT("r.Nanite.ProjectEnabled"),
		TEXT("r.Substrate"),
		TEXT("r.PathTracing"),
		TEXT("r.AllowStaticLighting"),
		TEXT("r.AntiAliasingMethod"),
		TEXT("r.ScreenPercentage"),
		TEXT("r.ScreenPercentage.Default.Desktop.Mode"),
		TEXT("r.VSync"),
		TEXT("t.MaxFPS"),
	};

	enum class EPhase : uint8
	{
		WaitForWorld,
		WaitForShaders,
		Warmup,
		Sample,
		Profile,
		Done,
	};

	struct FSettings
	{
		int32 WarmupFrames = 120;
		int32 SampleFrames = 300;
		int32 ProfileWaitFrames = 30;
		double TimeoutSeconds = 1800.0;
	};

	struct FFrameStats
	{
		double Avg = 0.0;
		double P95 = 0.0;
		double Max = 0.0;
	};

	FFrameStats ComputeStats(TArray<double> Samples)
	{
		FFrameStats Stats;
		if (Samples.IsEmpty())
		{
			return Stats;
		}
		Samples.Sort();
		double Sum = 0.0;
		for (const double Sample : Samples)
		{
			Sum += Sample;
		}
		Stats.Avg = Sum / Samples.Num();
		Stats.P95 = Samples[FMath::Clamp(FMath::FloorToInt32(0.95 * (Samples.Num() - 1)), 0, Samples.Num() - 1)];
		Stats.Max = Samples.Last();
		return Stats;
	}

	void Report(const TCHAR* Key, const FString& Value)
	{
		UE_LOG(LogFloor101, Display, TEXT("F101RenderCheck: %s=%s"), Key, *Value);
	}

	void Report(const TCHAR* Key, bool bValue)
	{
		Report(Key, FString(bValue ? TEXT("1") : TEXT("0")));
	}

	void Report(const TCHAR* Key, const FFrameStats& Stats)
	{
		Report(Key, FString::Printf(TEXT("avg=%.2fms p95=%.2fms max=%.2fms"), Stats.Avg, Stats.P95, Stats.Max));
	}

	UWorld* GetGameWorld()
	{
		if (GEngine && GEngine->GameViewport)
		{
			return GEngine->GameViewport->GetWorld();
		}
		return nullptr;
	}

	class FRenderCheck
	{
	public:
		explicit FRenderCheck(const FSettings& InSettings)
			: Settings(InSettings)
			, StartSeconds(FPlatformTime::Seconds())
		{
		}

		// Returns false once the check is finished so the ticker unregisters it.
		bool Tick()
		{
			if (Phase != EPhase::Done && FPlatformTime::Seconds() - StartSeconds > Settings.TimeoutSeconds)
			{
				Report(TEXT("result"), FString::Printf(TEXT("timeout phase=%d"), static_cast<int32>(Phase)));
				Finish();
				return false;
			}

			switch (Phase)
			{
			case EPhase::WaitForWorld:
				if (UWorld* World = GetGameWorld(); World && World->HasBegunPlay())
				{
					Report(TEXT("map"), World->GetMapName());
					Phase = EPhase::WaitForShaders;
				}
				break;

			case EPhase::WaitForShaders:
				if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
				{
					LogShaderProgressThrottled();
				}
				else
				{
					Report(TEXT("shaders_ready_after"), FString::Printf(TEXT("%.1fs"), FPlatformTime::Seconds() - StartSeconds));
					Phase = EPhase::Warmup;
				}
				break;

			case EPhase::Warmup:
				if (++FrameCounter >= Settings.WarmupFrames)
				{
					FrameCounter = 0;
					Phase = EPhase::Sample;
				}
				break;

			case EPhase::Sample:
				FrameTimesMs.Add(FApp::GetDeltaTime() * 1000.0);
				GpuTimesMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
				if (FrameTimesMs.Num() >= Settings.SampleFrames)
				{
					ReportState();
					if (UWorld* World = GetGameWorld())
					{
						// Next frame: Lumen scene card counts (surface cache coverage) and a GPU pass profile.
						GEngine->Exec(World, TEXT("r.LumenScene.DumpStats 1"));
						GEngine->Exec(World, TEXT("ProfileGPU"));
					}
					Phase = EPhase::Profile;
				}
				break;

			case EPhase::Profile:
				if (++FrameCounter >= Settings.ProfileWaitFrames)
				{
					Report(TEXT("result"), FString(TEXT("complete")));
					Finish();
					return false;
				}
				break;

			case EPhase::Done:
				return false;
			}
			return true;
		}

	private:
		void LogShaderProgressThrottled()
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - LastProgressLogSeconds >= 10.0)
			{
				LastProgressLogSeconds = Now;
				UE_LOG(LogFloor101, Display, TEXT("F101RenderCheck: waiting for shaders, %d jobs remaining"),
					GShaderCompilingManager->GetNumRemainingJobs());
			}
		}

		void ReportState() const
		{
			const EShaderPlatform Platform = GMaxRHIShaderPlatform;
			FString FeatureLevelName;
			GetFeatureLevelName(GMaxRHIFeatureLevel, FeatureLevelName);

			Report(TEXT("rhi"), FString(GDynamicRHI ? GDynamicRHI->GetName() : TEXT("none")));
			Report(TEXT("feature_level"), FeatureLevelName);
			Report(TEXT("shader_platform"), LegacyShaderPlatformToShaderFormat(Platform).ToString());
			Report(TEXT("adapter"), GRHIAdapterName);
			Report(TEXT("vendor_id"), FString::Printf(TEXT("0x%04X"), GRHIVendorId));
			Report(TEXT("driver"), GRHIAdapterUserDriverVersion);

			Report(TEXT("rhi_ray_tracing"), GRHISupportsRayTracing);
			Report(TEXT("rhi_ray_tracing_shaders"), GRHISupportsRayTracingShaders);
			Report(TEXT("rhi_inline_ray_tracing"), GRHISupportsInlineRayTracing);
			Report(TEXT("rhi_atomic64"), GRHISupportsAtomicUInt64);
			Report(TEXT("ray_tracing_enabled"), IsRayTracingEnabled(Platform));
			Report(TEXT("nanite"), UseNanite(Platform));
			Report(TEXT("virtual_shadow_maps"), UseVirtualShadowMaps(Platform, GMaxRHIFeatureLevel));
			Report(TEXT("lumen_gi_supported"), DoesPlatformSupportLumenGI(Platform));

			for (const TCHAR* Name : ReportedCVars)
			{
				const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
				Report(*FString::Printf(TEXT("cvar.%s"), Name), CVar ? CVar->GetString() : FString(TEXT("<missing>")));
			}

			// Mirrors Lumen::UseHardwareRayTracing() (Renderer-private) minus the per-view flag;
			// ProfileGPU output is the runtime proof that the HWRT passes actually ran.
			const IConsoleVariable* LumenHwrt = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"));
			const bool bLumenHwrtExpected = IsRayTracingEnabled(Platform)
				&& (GRHISupportsInlineRayTracing || (GRHISupportsRayTracingShaders && GRHISupportsRayTracingDispatchIndirect))
				&& LumenHwrt && LumenHwrt->GetInt() != 0;
			Report(TEXT("lumen_hwrt_expected"), bLumenHwrtExpected);

			if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
			{
				const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
				Report(TEXT("resolution"), FString::Printf(TEXT("%dx%d"), Size.X, Size.Y));
			}
			Report(TEXT("frame_time"), ComputeStats(FrameTimesMs));
			Report(TEXT("gpu_time"), ComputeStats(GpuTimesMs));
		}

		void Finish()
		{
			Phase = EPhase::Done;
			RequestEngineExit(TEXT("F101RenderCheck finished"));
		}

		FSettings Settings;
		EPhase Phase = EPhase::WaitForWorld;
		double StartSeconds = 0.0;
		double LastProgressLogSeconds = 0.0;
		int32 FrameCounter = 0;
		TArray<double> FrameTimesMs;
		TArray<double> GpuTimesMs;
	};

	TUniquePtr<FRenderCheck> GRenderCheck;
	FTSTicker::FDelegateHandle GTickerHandle;
} // namespace

void StartIfRequested()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("F101RenderCheck")))
	{
		return;
	}

	FSettings Settings;
	FParse::Value(FCommandLine::Get(), TEXT("F101RenderCheckWarmupFrames="), Settings.WarmupFrames);
	FParse::Value(FCommandLine::Get(), TEXT("F101RenderCheckSampleFrames="), Settings.SampleFrames);
	FParse::Value(FCommandLine::Get(), TEXT("F101RenderCheckTimeout="), Settings.TimeoutSeconds);

	UE_LOG(LogFloor101, Display, TEXT("F101RenderCheck: started warmup=%d samples=%d timeout=%.0fs"),
		Settings.WarmupFrames, Settings.SampleFrames, Settings.TimeoutSeconds);

	GRenderCheck = MakeUnique<FRenderCheck>(Settings);
	GTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
	{
		return GRenderCheck && GRenderCheck->Tick();
	}));
}

void Stop()
{
	if (GTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GTickerHandle);
		GTickerHandle.Reset();
	}
	GRenderCheck.Reset();
}
} // namespace Floor101::RenderCheck
