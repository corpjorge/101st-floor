#pragma once

#include "CoreMinimal.h"

class UFloor101DocumentSubsystem;

// Saving, opening and exporting through the system's dialogs, shared by the toolbar and the keyboard. The dialogs start
// where the user last chose: the places are remembered between sessions in the user's settings (GameUserSettings.ini,
// section [Floor101.Paths]). Each action gives the keyboard back to the viewport; the subsystem's status says how it went.
struct FFloor101FileActions
{
	// Ctrl+S: the current file, or "save as" while the project has never been saved.
	static void Save(UFloor101DocumentSubsystem& Document);
	// Ctrl+Shift+S: asks where (proposing the current file, else the last folder used).
	static void SaveAs(UFloor101DocumentSubsystem& Document);
	// Ctrl+O: asks which project.json to open.
	static void Open(UFloor101DocumentSubsystem& Document, FSimpleDelegate OnOpened = FSimpleDelegate());
	static void OpenPath(UFloor101DocumentSubsystem& Document, const FString& Path, FSimpleDelegate OnOpened = FSimpleDelegate());
	static bool RequestReplace(UFloor101DocumentSubsystem& Document, FSimpleDelegate Proceed);
	// Clean documents close immediately. Dirty documents offer save, discard or cancel; a cancelled/failed save stays open.
	static bool PrepareExit(UFloor101DocumentSubsystem& Document, TFunctionRef<EAppReturnType::Type()> Prompt,
		TFunctionRef<void()> SaveDocument);
	// Destination selection precedes export; cancelling never writes a file.
	static void ExportPlan(UFloor101DocumentSubsystem& Document, const FString& Extension);
	static bool ExportPlanToChosenPath(UFloor101DocumentSubsystem& Document, const FString& Extension,
		TFunctionRef<bool(const FString&, FString&)> ChoosePath);
	// Asks where to write the .glb for Twinmotion (the last folder used for it, else next to the project).
	static void ExportTwinmotion(UFloor101DocumentSubsystem& Document);

	// The remembered places; empty while there is none.
	static FString GetLastProjectPath();
	static TArray<FString> GetRecentProjects();
	static FString GetLastTwinmotionFolder();
	static void RememberProjectPath(const FString& Path);
	static void RememberTwinmotionFolder(const FString& Folder);
};
