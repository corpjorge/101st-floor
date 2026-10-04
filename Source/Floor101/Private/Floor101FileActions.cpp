#include "Floor101FileActions.h"

#include "Floor101DocumentSubsystem.h"
#include "Floor101FileDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "Floor101GameViewportClient.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "Floor101Overlay"

namespace
{
	const TCHAR* PathsSection = TEXT("Floor101.Paths");
	const TCHAR* LastProjectKey = TEXT("LastProject");
	const TCHAR* LastTwinmotionKey = TEXT("LastTwinmotionFolder");
	const TCHAR* NewProjectName = TEXT("casa.json");

	FString ReadSetting(const TCHAR* Key)
	{
		FString Value;
		return GConfig && GConfig->GetString(PathsSection, Key, Value, GGameUserSettingsIni) ? Value : FString();
	}

	void WriteSetting(const TCHAR* Key, const FString& Value)
	{
		if (GConfig)
		{
			GConfig->SetString(PathsSection, Key, *Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	FText ProjectFiles()
	{
		return LOCTEXT("ProjectFiles", "Proyecto de 101st Floor (*.json)");
	}

	void GiveFocusBack()
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
}

FString FFloor101FileActions::GetLastProjectPath()
{
	return ReadSetting(LastProjectKey);
}

FString FFloor101FileActions::GetLastTwinmotionFolder()
{
	return ReadSetting(LastTwinmotionKey);
}

void FFloor101FileActions::RememberProjectPath(const FString& Path)
{
	auto Recent = GetRecentProjects();
	WriteSetting(LastProjectKey, Path);
	if (!GConfig || Path.IsEmpty()) { return; }
	FString Full = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Full);
	FPaths::CollapseRelativeDirectories(Full);
	Recent.RemoveAll([&Full](const FString& Existing) { return Existing.Equals(Full, ESearchCase::IgnoreCase); });
	Recent.Insert(Full, 0);
	if (Recent.Num() > 8) { Recent.SetNum(8); }
	GConfig->SetArray(PathsSection, TEXT("RecentProjects"), Recent, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

TArray<FString> FFloor101FileActions::GetRecentProjects()
{
	TArray<FString> Recent;
	if (GConfig) { GConfig->GetArray(PathsSection, TEXT("RecentProjects"), Recent, GGameUserSettingsIni); }
	if (Recent.IsEmpty() && !GetLastProjectPath().IsEmpty()) { Recent.Add(FPaths::ConvertRelativePathToFull(GetLastProjectPath())); }
	TArray<FString> Result;
	for (FString Path : Recent)
	{
		if (Path.IsEmpty()) { continue; }
		Path = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Path);
		FPaths::CollapseRelativeDirectories(Path);
		if (!Result.ContainsByPredicate([&Path](const FString& Existing) { return Existing.Equals(Path, ESearchCase::IgnoreCase); })) { Result.Add(Path); }
		if (Result.Num() == 8) { break; }
	}
	return Result;
}

void FFloor101FileActions::RememberTwinmotionFolder(const FString& Folder)
{
	WriteSetting(LastTwinmotionKey, Folder);
}

void FFloor101FileActions::Save(UFloor101DocumentSubsystem& Document)
{
	if (Document.GetProjectPath().IsEmpty())
	{
		SaveAs(Document);
		return;
	}
	if (Document.SaveProject(Document.GetProjectPath()))
	{
		RememberProjectPath(Document.GetProjectPath());
	}
	GiveFocusBack();
}

void FFloor101FileActions::SaveAs(UFloor101DocumentSubsystem& Document)
{
	// The current file; a new project: "casa.json" in the last folder used, else in Documentos/101st Floor.
	FString Proposed = Document.GetProjectPath();
	if (Proposed.IsEmpty())
	{
		const FString Last = GetLastProjectPath();
		Proposed = FPaths::Combine(FPaths::GetPath(Last.IsEmpty() ? UFloor101DocumentSubsystem::GetDefaultProjectPath() : Last), NewProjectName);
	}
	FString Path;
	if (FFloor101FileDialog::SaveFile(LOCTEXT("SaveAsTitle", "Guardar la casa como"), ProjectFiles().ToString(), TEXT("*.json"),
			TEXT("json"), Proposed, Path) &&
		Document.SaveProject(Path))
	{
		RememberProjectPath(Path);
	}
	GiveFocusBack();
}

void FFloor101FileActions::Open(UFloor101DocumentSubsystem& Document, FSimpleDelegate OnOpened)
{
	const FString Last = GetLastProjectPath();
	const FString Start = !Document.GetProjectPath().IsEmpty() ? Document.GetProjectPath()
		: !Last.IsEmpty() ? Last : UFloor101DocumentSubsystem::GetDefaultProjectPath();
	FString Path;
	if (FFloor101FileDialog::OpenFile(LOCTEXT("OpenTitle", "Abrir una casa"), ProjectFiles().ToString(), TEXT("*.json"), Path, Start))
	{
		OpenPath(Document, Path, OnOpened);
	}
	// A deferred confirmation owns focus until it is resolved.
	const auto* Client = Document.GetWorld() ? Cast<UFloor101GameViewportClient>(Document.GetWorld()->GetGameViewport()) : nullptr;
	if (!Client || !Client->IsInterfaceOpen()) { GiveFocusBack(); }
}

void FFloor101FileActions::OpenPath(UFloor101DocumentSubsystem& Document, const FString& Path, FSimpleDelegate OnOpened)
{
	RequestReplace(Document, FSimpleDelegate::CreateLambda([WeakDocument = TWeakObjectPtr<UFloor101DocumentSubsystem>(&Document), Path, OnOpened]()
	{
		if (auto* D = WeakDocument.Get(); D && D->OpenProject(Path))
		{
			RememberProjectPath(Path);
			GiveFocusBack();
			OnOpened.ExecuteIfBound();
		}
	}));
}

bool FFloor101FileActions::RequestReplace(UFloor101DocumentSubsystem& Document, FSimpleDelegate Proceed)
{
	if (!Document.HasUnsavedChanges()) { Proceed.ExecuteIfBound(); return true; }
	auto* Client = Document.GetWorld() ? Cast<UFloor101GameViewportClient>(Document.GetWorld()->GetGameViewport()) : nullptr;
	return Client && Client->RequestUnsavedAction(false, Proceed);
}

bool FFloor101FileActions::PrepareExit(UFloor101DocumentSubsystem& Document, TFunctionRef<EAppReturnType::Type()> Prompt,
	TFunctionRef<void()> SaveDocument)
{
	if (!Document.HasUnsavedChanges()) { return true; }
	const EAppReturnType::Type Choice = Prompt();
	if (Choice == EAppReturnType::No) { return true; }
	if (Choice != EAppReturnType::Yes) { return false; }
	SaveDocument();
	return !Document.HasUnsavedChanges();
}

void FFloor101FileActions::ExportPlan(UFloor101DocumentSubsystem& Document, const FString& Extension)
{
	if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().DismissAllMenus(); }
	ExportPlanToChosenPath(Document, Extension, [&Extension](const FString& Proposed, FString& Path)
	{
		const FText Title = FText::Format(LOCTEXT("PlanSaveTitle", "Guardar plano {0}"), FText::FromString(Extension.ToUpper()));
		return FFloor101FileDialog::SaveFile(Title, FString::Printf(TEXT("Plano %s (*.%s)"), *Extension.ToUpper(), *Extension),
			TEXT("*.") + Extension, Extension, Proposed, Path);
	});
	GiveFocusBack();
}

bool FFloor101FileActions::ExportPlanToChosenPath(UFloor101DocumentSubsystem& Document, const FString& Extension,
	TFunctionRef<bool(const FString&, FString&)> ChoosePath)
{
	if (Extension != TEXT("svg") && Extension != TEXT("png")) { return false; }
	const FString Default = Document.GetDefaultPlanPath(Extension);
	const FString Folder = ReadSetting(TEXT("LastPlanFolder"));
	const FString Proposed = Folder.IsEmpty() ? Default : FPaths::Combine(Folder, FPaths::GetCleanFilename(Default));
	FString Path;
	if (!ChoosePath(Proposed, Path) || Path.IsEmpty() || !Document.ExportPlan(Path, f101::cad::LevelId())) { return false; }
	WriteSetting(TEXT("LastPlanFolder"), FPaths::GetPath(FPaths::ConvertRelativePathToFull(Path)));
	return true;
}

void FFloor101FileActions::ExportTwinmotion(UFloor101DocumentSubsystem& Document)
{
	// "<project name>.glb", in the last folder a .glb went to, else next to the project.
	const FString Default = Document.GetDefaultGltfPath();
	const FString Folder = GetLastTwinmotionFolder();
	const FString Proposed = Folder.IsEmpty() ? Default : FPaths::Combine(Folder, FPaths::GetCleanFilename(Default));
	FString Path;
	if (FFloor101FileDialog::SaveFile(LOCTEXT("TwinmotionTitle", "Exportar para Twinmotion"),
			LOCTEXT("GlbFiles", "glTF binario para Twinmotion (*.glb)").ToString(), TEXT("*.glb"), TEXT("glb"), Proposed, Path) &&
		Document.ExportGltf(Path))
	{
		RememberTwinmotionFolder(FPaths::GetPath(Path));
	}
	GiveFocusBack();
}

#undef LOCTEXT_NAMESPACE
