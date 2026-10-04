#pragma once

#include "CoreMinimal.h"

class UGameViewportClient;

namespace Floor101::Display
{
	struct FStatus
	{
		FString MonitorName;
		FIntPoint Native = FIntPoint::ZeroValue;
		FIntPoint Desktop = FIntPoint::ZeroValue;
		FIntPoint Image = FIntPoint::ZeroValue;
		FIntPoint Render = FIntPoint::ZeroValue; // measured scene-color rect before temporal upscaling
		bool bFullscreen = false;
		bool bWorldRendering = true;
	};
	void Tick(UGameViewportClient* Client);
	FStatus Query(const UGameViewportClient* Client);
	TArray<FIntPoint> Resolutions(const UGameViewportClient* Client);
	bool SetResolution(UGameViewportClient* Client, FIntPoint Size, bool bSave = true);
	bool ToggleFullscreen(UGameViewportClient* Client, bool bSave = true);
	bool IsVSyncEnabled(); // effective runtime state
	bool SetVSyncEnabled(bool bEnabled, bool bSave = true);
	FText Describe(const FStatus& Status);
}
