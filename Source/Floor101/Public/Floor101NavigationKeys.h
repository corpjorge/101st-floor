#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"

// Only fresh presses with available viewport camera input may move; shortcuts/focus changes discard held presses.
class FLOOR101_API FFloor101NavigationKeys
{
public:
	static bool IsMovementKey(const FKey& Key);
	static bool IsMovementCharacter(TCHAR Character);
	bool HandleKey(const FKey& Key, EInputEvent Event, bool bNavigationActive, bool bShortcutModifier);
	void Reset();
	FVector Direction(TFunctionRef<bool(const FKey&)> IsDown) const;

private:
	TSet<FKey> Accepted;
};
