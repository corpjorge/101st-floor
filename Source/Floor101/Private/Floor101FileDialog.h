#pragma once

#include "CoreMinimal.h"

// The system's "open file" dialog (Windows common dialog): the game has none of the editor's dialogs (ADR-0017).
struct FFloor101FileDialog
{
	// Asks for one existing file among `Patterns` (e.g. "*.png;*.jpg"), described as `FilterName`. False if the user
	// cancels, or on a platform without the dialog.
	static bool OpenFile(const FText& Title, const FString& FilterName, const FString& Patterns, FString& OutPath);
};
