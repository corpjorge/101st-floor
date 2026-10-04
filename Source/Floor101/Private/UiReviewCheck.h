#pragma once

#include "CoreMinimal.h"

class SFloor101Overlay;
class UFloor101GameViewportClient;

namespace Floor101::UIReview
{
	void Start(TSharedRef<SFloor101Overlay> Overlay, UFloor101GameViewportClient* Viewport);
}
