#pragma once

#include "CoreMinimal.h"

// The system's "open file" and "save as" dialogs (Windows common dialogs): the game has none of the editor's dialogs
// (ADR-0017).
struct FFloor101FileDialog
{
	// Asks for one existing file among `Patterns` (e.g. "*.png;*.jpg"), described as `FilterName`, starting in the folder
	// of `InitialPath` (empty: the system's choice). False if the user cancels, or on a platform without the dialog.
	static bool OpenFile(const FText& Title, const FString& FilterName, const FString& Patterns, FString& OutPath,
		const FString& InitialPath = FString());

	// Asks where to save a file of `Patterns`, proposing `InitialPath` (its folder and name); `DefaultExtension` (no dot)
	// is added when the name typed has none, and replacing an existing file asks first. False if the user cancels.
	static bool SaveFile(const FText& Title, const FString& FilterName, const FString& Patterns, const FString& DefaultExtension,
		const FString& InitialPath, FString& OutPath);
};
