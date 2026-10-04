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
	WriteSetting(LastProjectKey, Path);
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

void FFloor101FileActions::Open(UFloor101DocumentSubsystem& Document)
{
	const FString Last = GetLastProjectPath();
	const FString Start = !Document.GetProjectPath().IsEmpty() ? Document.GetProjectPath()
		: !Last.IsEmpty() ? Last : UFloor101DocumentSubsystem::GetDefaultProjectPath();
	FString Path;
	if (FFloor101FileDialog::OpenFile(LOCTEXT("OpenTitle", "Abrir una casa"), ProjectFiles().ToString(), TEXT("*.json"), Path, Start) &&
		Document.OpenProject(Path))
	{
		RememberProjectPath(Path);
	}
	GiveFocusBack();
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
