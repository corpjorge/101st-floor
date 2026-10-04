#pragma once

#include "CoreMinimal.h"

class UGameViewportClient;
class UWorld;

// Photo mode (ADR-0018): the game viewport rendered by Unreal's path tracer. It accumulates one sample per pixel each
// frame up to a target, starts over whenever anything in view changes (camera, geometry, materials) and denoises the last
// sample. Shared by the photo mode and the render check.
namespace Floor101::PathTracing
{
	// The path traced view, or back to the lit (Lumen) view.
	void SetEnabled(UGameViewportClient& Viewport, bool bEnabled);
	// Samples per pixel to accumulate (r.PathTracing.SamplesPerPixel).
	void SetTargetSamples(int32 Samples);
	int32 GetTargetSamples();
	// Samples the last path traced view has accumulated (0 before the first one, or right after a change).
	int32 GetAccumulatedSamples();
	// The view has all its samples (denoised) and has kept them for a few frames: the image is ready to save.
	bool IsComplete();
	// The viewport's actual output decides whether the next capture is HDR (EXR) or SDR (PNG).
	bool IsHDRCapture();
	void RequestImage(const FString& Path);
	// Shaders still compiling (the path tracer's own ones are compiled the first time it is used): meanwhile materials show
	// as the engine's default one.
	int32 GetPendingShaders();
	// To call every frame while path tracing: when the shaders finish compiling, the image (made with placeholder
	// materials) starts over.
	void Tick(UWorld* World);
}
