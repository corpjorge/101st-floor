#include "Floor101FileActions.h"

#include "Floor101DocumentSubsystem.h"
#include "Floor101FileDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
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

bool FFloor101FileActions::Open(UFloor101DocumentSubsystem& Document)
{
	const FString Last = GetLastProjectPath();
	const FString Start = !Document.GetProjectPath().IsEmpty() ? Document.GetProjectPath()
		: !Last.IsEmpty() ? Last : UFloor101DocumentSubsystem::GetDefaultProjectPath();
	FString Path;
	const bool Opened = FFloor101FileDialog::OpenFile(LOCTEXT("OpenTitle", "Abrir una casa"), ProjectFiles().ToString(), TEXT("*.json"), Path, Start)
		&& OpenPath(Document, Path);
	GiveFocusBack();
	return Opened;
}

bool FFloor101FileActions::OpenPath(UFloor101DocumentSubsystem& Document, const FString& Path)
{
	if (!ConfirmReplace(Document) || !Document.OpenProject(Path)) { return false; }
	RememberProjectPath(Path);
	GiveFocusBack();
	return true;
}

bool FFloor101FileActions::ConfirmReplace(UFloor101DocumentSubsystem& Document)
{
	return PrepareExit(Document, []()
	{
		return FMessageDialog::Open(EAppMsgType::YesNoCancel, EAppReturnType::Cancel,
			LOCTEXT("ReplaceUnsaved", "Hay cambios sin guardar. ¿Quieres guardarlos antes de cambiar de proyecto?\n\nSí: guardar y continuar.\nNo: descartar los cambios.\nCancelar: seguir trabajando."),
			LOCTEXT("ReplaceTitle", "Cambiar de proyecto"));
	}, [&Document]() { Save(Document); });
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

bool FFloor101FileActions::ConfirmExit(UFloor101DocumentSubsystem& Document)
{
	const bool bExit = PrepareExit(Document, []()
	{
		return FMessageDialog::Open(EAppMsgType::YesNoCancel, EAppReturnType::Cancel,
			LOCTEXT("ExitUnsaved", "Hay cambios sin guardar. ¿Quieres guardarlos antes de salir?\n\nSí: guardar y salir.\nNo: salir sin guardar.\nCancelar: seguir trabajando."),
			LOCTEXT("ExitTitle", "Salir de 101st Floor"));
	}, [&Document]() { Save(Document); });
	if (!bExit) { GiveFocusBack(); }
	return bExit;
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
