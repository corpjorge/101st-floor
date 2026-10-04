#pragma once
#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

namespace Floor101::Startup
{
	// Shared with the development review capture; the actual boot uses PreLoadScreen.
	FLOOR101STARTUP_API TSharedRef<SWidget> MakeLoadingWidget();
}
