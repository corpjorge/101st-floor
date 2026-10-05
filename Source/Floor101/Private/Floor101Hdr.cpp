#include "Floor101Hdr.h"

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Floor101.h"
#include "GameFramework/GameUserSettings.h"
#include "HDRHelper.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "Floor101HDR"

namespace Floor101::HDR
{
FStatus Query(const UGameViewportClient* Viewport)
{
	FStatus Status;
	if (!GDynamicRHI || !Viewport || !Viewport->GetWindow().IsValid())
	{
		return Status;
	}
	const TSharedPtr<SWindow> Window = Viewport->GetWindow();
	const FVector2D Position = Window->GetPositionInScreen();
	const FVector2D End = Position + Window->GetSizeInScreen();
	const FIntRect WindowRect(FMath::FloorToInt(Position.X), FMath::FloorToInt(Position.Y),
		FMath::CeilToInt(End.X), FMath::CeilToInt(End.Y));
	FDisplayInformationArray Displays;
	RHIGetDisplaysInformation(Displays);
	int64 BestArea = 0;
	for (const FDisplayInformation& Display : Displays)
	{
		const FIntRect& Bounds = Display.DesktopCoordinates;
		const int64 Width = FMath::Max(0, FMath::Min(WindowRect.Max.X, Bounds.Max.X) - FMath::Max(WindowRect.Min.X, Bounds.Min.X));
		const int64 Height = FMath::Max(0, FMath::Min(WindowRect.Max.Y, Bounds.Max.Y) - FMath::Max(WindowRect.Min.Y, Bounds.Min.Y));
		if (Width * Height > BestArea)
		{
			BestArea = Width * Height;
			Status.bAvailable = Display.bHDRSupported && IsHDRAllowed();
			Status.PeakNits = Display.MaximumLuminanceInNits;
		}
	}
	Status.bEnabled = Status.bAvailable && IsHDREnabled();
	return Status;
}

bool SetEnabled(UGameViewportClient* Viewport, bool bEnable, bool bSave)
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings || (bEnable && !Query(Viewport).bAvailable))
	{
		return false;
	}
	// 1000 selects an encoding profile, not panel brightness. UE 5.8 uses the OS peak/gamut unless overridden.
	Settings->SetMaximumHDRDisplayNits(1000);
	Settings->SetHDRCalibrationUsed(false);
	Settings->SetHDREnabled(bEnable);
	if (bSave)
	{
		Settings->SaveSettings();
	}
	const FStatus Status = Query(Viewport);
	F101_LOG(LogFloor101, Display, TEXT("F101HDR: requested=%d available=%d os_peak_nits=%.2f"), bEnable, Status.bAvailable, Status.PeakNits);
	return Settings->IsHDREnabled() == bEnable;
}

FText Tooltip(const FStatus& Status)
{
	return Status.bAvailable
		? LOCTEXT("Available", "Activa o desactiva HDR para esta aplicación. El brillo se adapta a la pantalla mediante Windows. La preferencia se guarda en este equipo.")
		: LOCTEXT("Unavailable", "Para usar HDR, activa «Usar HDR» en Configuración de Windows > Sistema > Pantalla y coloca esta ventana en un monitor HDR.");
}
}

#undef LOCTEXT_NAMESPACE
