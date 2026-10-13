#include "Floor101Preferences.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Floor101.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceConsole.h"

namespace Floor101::Preferences
{
	bool DiagnosticMode()
	{
		bool bEnabled = false;
		if (GConfig) { GConfig->GetBool(TEXT("Floor101.Diagnostics"), TEXT("DebugMode"), bEnabled, GGameUserSettingsIni); }
		return bEnabled;
	}
	bool ShowLogWindow()
	{
		bool bShow = false;
		if (GConfig) { GConfig->GetBool(TEXT("Floor101.Diagnostics"), TEXT("ShowLogWindow"), bShow, GGameUserSettingsIni); }
		return bShow;
	}
	void SetDiagnosticMode(bool bEnabled)
	{
		if (GConfig)
		{
			GConfig->SetBool(TEXT("Floor101.Diagnostics"), TEXT("DebugMode"), bEnabled, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		Diagnostics::SetVerbose(bEnabled);
		#if !UE_BUILD_SHIPPING
		LogFloor101.SetVerbosity(bEnabled ? ELogVerbosity::VeryVerbose : ELogVerbosity::Log);
		#endif
		F101_LOG(LogFloor101, Display, TEXT("F101: diagnostic mode %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
	}
	void SetShowLogWindow(bool bShow, bool bApply)
	{
		if (GConfig)
		{
			GConfig->SetBool(TEXT("Floor101.Diagnostics"), TEXT("ShowLogWindow"), bShow, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		if (bApply) { Diagnostics::SetWindowVisible(bShow); }
	}
	void ApplySavedDiagnostics()
	{
		if (GIsEditor || IsRunningCommandlet()) { return; }
		const bool bQuiet = FParse::Param(FCommandLine::Get(), TEXT("F101Quiet"));
		const bool bDebug = !bQuiet && (DiagnosticMode() || FParse::Param(FCommandLine::Get(), TEXT("F101Debug")));
		const bool bShow = !bQuiet && (ShowLogWindow() || FParse::Param(FCommandLine::Get(), TEXT("LOG")));
		Diagnostics::SetVerbose(bDebug);
		#if !UE_BUILD_SHIPPING
		LogFloor101.SetVerbosity(bDebug ? ELogVerbosity::VeryVerbose : ELogVerbosity::Log);
		#endif
		Diagnostics::SetWindowVisible(bShow);
		F101_LOG(LogFloor101, Display, TEXT("F101: diagnostics=%s log window=%s; file logging remains enabled"), bDebug ? TEXT("on") : TEXT("off"), bShow ? TEXT("on") : TEXT("off"));
	}
	bool ShowGrid()
	{
		FString Review;
		if (FParse::Value(FCommandLine::Get(), TEXT("F101UICapture="), Review))
		{
			return true; // reproducible review; never changes the user's saved preference
		}
		bool bShow = true;
		if (GConfig) { GConfig->GetBool(TEXT("Floor101.Interface"), TEXT("ShowGrid"), bShow, GGameUserSettingsIni); }
		return bShow;
	}
	void SetShowGrid(bool bShow)
	{
		if (GConfig) { GConfig->SetBool(TEXT("Floor101.Interface"), TEXT("ShowGrid"), bShow, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	}
	bool ShowInteriorDimensions()
	{
		FString Review;
		if (FParse::Value(FCommandLine::Get(), TEXT("F101UICapture="), Review))
		{
			return true; // reproducible review; never changes the user's saved preference
		}
		bool bShow = true;
		if (GConfig) { GConfig->GetBool(TEXT("Floor101.Interface"), TEXT("ShowInteriorDimensions"), bShow, GGameUserSettingsIni); }
		return bShow;
	}
	void SetShowInteriorDimensions(bool bShow)
	{
		if (GConfig) { GConfig->SetBool(TEXT("Floor101.Interface"), TEXT("ShowInteriorDimensions"), bShow, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	}
	int32 GraphicsQuality()
	{
		int32 Profile = FMath::Clamp(Scalability::GetQualityLevels().TextureQuality - 1, 0, 2);
		if (GConfig) { GConfig->GetInt(TEXT("Floor101.Interface"), TEXT("GraphicsQuality"), Profile, GGameUserSettingsIni); }
		return FMath::Clamp(Profile, 0, 2);
	}

	Scalability::FQualityLevels QualityLevels(int32 Profile)
	{
		const int32 Level = FMath::Clamp(Profile, 0, 2) + 1;
		Scalability::FQualityLevels Quality;
		Quality.SetFromSingleQualityLevel(Level);
		// Preserve Lumen at every profile; native render resolution remains the display policy (ADR-0023).
		Quality.GlobalIlluminationQuality = FMath::Max(2, Level);
		Quality.ReflectionQuality = FMath::Max(2, Level);
		Quality.ResolutionQuality = 100.0f;
		return Quality;
	}

	void SetGraphicsQuality(int32 Profile, bool bSave)
	{
		Profile = FMath::Clamp(Profile, 0, 2);
		// Apply scalability alone, never ApplyNonResolutionSettings: it also rewrites HDR and VSync.
		Scalability::SetQualityLevels(QualityLevels(Profile));
		if (bSave && GConfig)
		{
			GConfig->SetInt(TEXT("Floor101.Interface"), TEXT("GraphicsQuality"), Profile, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	void ApplySavedGraphics()
	{
		int32 Profile;
		if (GConfig && GConfig->GetInt(TEXT("Floor101.Interface"), TEXT("GraphicsQuality"), Profile, GGameUserSettingsIni)) { SetGraphicsQuality(Profile, false); }
	}
}
