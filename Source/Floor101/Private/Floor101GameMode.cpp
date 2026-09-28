#include "Floor101GameMode.h"

#include "Floor101EditingPawn.h"
#include "Floor101PlayerController.h"

AFloor101GameMode::AFloor101GameMode()
{
	DefaultPawnClass = AFloor101EditingPawn::StaticClass();
	PlayerControllerClass = AFloor101PlayerController::StaticClass();
}
