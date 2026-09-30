#include "Floor101PathTracing.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ShaderCompiler.h"
#include "ShowFlags.h"
#include "UnrealClient.h"

#include <atomic>

namespace Floor101::PathTracing
{
namespace
{
	// The view state of the game viewport is private to its local player: the sample count is read from the path traced
	// views as they are set up, one frame behind the renderer.
	class FProgress : public FSceneViewExtensionBase
	{
	public:
		explicit FProgress(const FAutoRegister& AutoRegister)
			: FSceneViewExtensionBase(AutoRegister)
		{
		}

		virtual void SetupView(FSceneViewFamily& Family, FSceneView& View) override
		{
#if RHI_RAYTRACING
			if (Family.EngineShowFlags.PathTracing && View.State)
			{
				const int32 Index = static_cast<int32>(View.State->GetPathTracingSampleIndex());
				const int32 Count = static_cast<int32>(View.State->GetPathTracingSampleCount());
				Samples = Index;
				Target = Count;
				// Complete once accumulation has stopped at the target for a few frames (early on, auto exposure can still
				// restart it).
				CompleteFrames = Count > 0 && Index >= Count ? CompleteFrames + 1 : 0;
			}
#endif
		}

		std::atomic<int32> Samples{0};
		std::atomic<int32> Target{0};
		std::atomic<int32> CompleteFrames{0};
	};

	constexpr int32 StableFrames = 3;

	TSharedPtr<FProgress, ESPMode::ThreadSafe> Progress;
	bool bWasCompiling = false;

	IConsoleVariable* SamplesPerPixel()
	{
		return IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.SamplesPerPixel"));
	}
}

void SetEnabled(UGameViewportClient& Viewport, bool bEnabled)
{
	if (!Progress)
	{
		Progress = FSceneViewExtensions::NewExtension<FProgress>();
	}
	Progress->Samples = 0;
	Progress->CompleteFrames = 0;
	if (IConsoleVariable* Display = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.ProgressDisplay")))
	{
		Display->Set(0, ECVF_SetByCode); // the photo panel shows the progress; saved images never carry the engine's bar
	}
	Viewport.ViewModeIndex = bEnabled ? VMI_PathTracing : VMI_Lit;
	ApplyViewMode(bEnabled ? VMI_PathTracing : VMI_Lit, true, Viewport.EngineShowFlags);
}

void SetTargetSamples(int32 Samples)
{
	if (IConsoleVariable* Variable = SamplesPerPixel())
	{
		Variable->Set(FMath::Max(1, Samples), ECVF_SetByCode);
	}
}

int32 GetTargetSamples()
{
	const IConsoleVariable* Variable = SamplesPerPixel();
	return Variable ? Variable->GetInt() : 0;
}

int32 GetAccumulatedSamples()
{
	return Progress ? FMath::Min(Progress->Samples.load(), FMath::Max(GetTargetSamples(), 0)) : 0;
}

bool IsComplete()
{
	return Progress && Progress->Target.load() == GetTargetSamples() && Progress->CompleteFrames.load() >= StableFrames;
}

void RequestImage(const FString& Path)
{
	FScreenshotRequest::RequestScreenshot(Path, false, false);
}

int32 GetPendingShaders()
{
	return GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
}

void Tick(UWorld* World)
{
	const bool bCompiling = GShaderCompilingManager && GShaderCompilingManager->IsCompiling();
	if (bWasCompiling && !bCompiling && World && World->Scene)
	{
		World->Scene->InvalidatePathTracedOutput();
		if (Progress)
		{
			Progress->Samples = 0;
			Progress->CompleteFrames = 0;
		}
	}
	bWasCompiling = bCompiling;
}
}
