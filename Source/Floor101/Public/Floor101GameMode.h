#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "Floor101GameMode.generated.h"

// Editing session: orbit camera pawn and a controller with a visible cursor (ADR-0008). Set as the global default
// game mode in Config/DefaultEngine.ini.
UCLASS()
class FLOOR101_API AFloor101GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFloor101GameMode();
};
