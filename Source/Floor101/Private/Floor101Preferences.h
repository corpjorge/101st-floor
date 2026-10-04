#pragma once
#include "CoreMinimal.h"
#include "Scalability.h"

namespace Floor101::Preferences
{
	int32 GraphicsQuality();
	Scalability::FQualityLevels QualityLevels(int32 Profile);
	void SetGraphicsQuality(int32 Profile, bool bSave = true);
	void ApplySavedGraphics();
	bool DiagnosticMode();
	bool ShowLogWindow();
	void SetDiagnosticMode(bool bEnabled);
	void SetShowLogWindow(bool bShow, bool bApply = true);
	void ApplySavedDiagnostics();
	bool ShowGrid();
	void SetShowGrid(bool bShow);
}
