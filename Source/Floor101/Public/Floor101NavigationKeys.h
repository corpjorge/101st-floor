#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"

// Only fresh presses with available viewport camera input may move; shortcuts/focus changes discard held presses.
class FLOOR101_API FFloor101NavigationKeys
{
public:
	// Top-row digits select speed; dimensions and modified shortcuts take priority. Zero selects level 10.
	static int32 SpeedLevelForKey(const FKey& Key, bool bTypingLength, bool bShortcutModifier);
	static bool IsMovementKey(const FKey& Key);
	static bool IsMovementCharacter(TCHAR Character);
	bool HandleKey(const FKey& Key, EInputEvent Event, bool bNavigationActive, bool bShortcutModifier);
	void Reset();
	FVector Direction(TFunctionRef<bool(const FKey&)> IsDown) const;

private:
	TSet<FKey> Accepted;
};
