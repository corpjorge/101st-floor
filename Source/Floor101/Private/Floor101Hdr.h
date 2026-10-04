#pragma once

#include "CoreMinimal.h"

class UGameViewportClient;

namespace Floor101::HDR
{
	struct FStatus
	{
		bool bAvailable = false; // Windows HDR on the display containing this window
		bool bEnabled = false;   // application's requested output
		float PeakNits = 0.0f;   // OS metadata, not measured brightness
	};
	FStatus Query(const UGameViewportClient* Viewport);
	bool SetEnabled(UGameViewportClient* Viewport, bool bEnable, bool bSave = true);
	FText Tooltip(const FStatus& Status);
}
