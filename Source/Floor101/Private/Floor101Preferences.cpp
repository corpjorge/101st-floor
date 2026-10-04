#include "Floor101Preferences.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace Floor101::Preferences
{
	bool ShowGrid()
	{
		FString Review;
		if (FParse::Value(FCommandLine::Get(), TEXT("F101UICapture="), Review))
		{
			FString Screen;
			FParse::Value(FCommandLine::Get(), TEXT("F101UIScreen="), Screen);
			return Screen == TEXT("grid"); // reproducible review; never changes the user's saved preference
		}
		bool bShow = false;
		if (GConfig) { GConfig->GetBool(TEXT("Floor101.Interface"), TEXT("ShowGrid"), bShow, GGameUserSettingsIni); }
		return bShow;
	}
	void SetShowGrid(bool bShow)
	{
		if (GConfig) { GConfig->SetBool(TEXT("Floor101.Interface"), TEXT("ShowGrid"), bShow, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
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
