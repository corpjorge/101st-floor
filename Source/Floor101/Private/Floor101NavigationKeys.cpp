#include "Floor101NavigationKeys.h"

bool FFloor101NavigationKeys::IsMovementKey(const FKey& Key)
{
	return Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D || Key == EKeys::Q || Key == EKeys::E;
}

bool FFloor101NavigationKeys::HandleKey(const FKey& Key, EInputEvent Event, bool bNavigationActive, bool bShortcutModifier)
{
	if (bShortcutModifier)
	{
		Reset();
		return false;
	}
	if (Event == IE_Released)
	{
		return Accepted.Remove(Key) > 0;
	}
	if (!bNavigationActive || !IsMovementKey(Key))
	{
		return false;
	}
	if (Event == IE_Pressed || Event == IE_DoubleClick)
	{
		Accepted.Add(Key);
	}
	return Event == IE_Pressed || Event == IE_DoubleClick || Event == IE_Repeat;
}

void FFloor101NavigationKeys::Reset()
{
	Accepted.Reset();
}

FVector FFloor101NavigationKeys::Direction(TFunctionRef<bool(const FKey&)> IsDown) const
{
	auto Down = [this, &IsDown](const FKey& Key) { return Accepted.Contains(Key) && IsDown(Key) ? 1.0 : 0.0; };
	return FVector(Down(EKeys::W) - Down(EKeys::S), Down(EKeys::D) - Down(EKeys::A), Down(EKeys::E) - Down(EKeys::Q));
}
