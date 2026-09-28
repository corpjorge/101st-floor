#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "Floor101PlayerController.generated.h"

class SWidget;

// Shows the cursor, keeps it visible while a mouse button is held (orbit, pan and endpoint drags need it) and puts the
// editing UI (SFloor101Overlay) on the viewport. Editing input itself goes through UFloor101GameViewportClient
// (ADR-0008).
UCLASS()
class FLOOR101_API AFloor101PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFloor101PlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<SWidget> Overlay;
};
