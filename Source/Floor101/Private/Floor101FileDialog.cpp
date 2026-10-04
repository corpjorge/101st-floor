#include "Floor101FileDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	#include <commdlg.h>
	#include "Windows/HideWindowsPlatformTypes.h"
	#undef LoadImage
#endif

#if PLATFORM_WINDOWS
namespace
{
	// What both dialogs share: the filter (pairs of NUL-terminated strings, ended by an extra NUL), the file buffer
	// (with `InitialPath`'s name), the folder to start in, the owner window and the title.
	struct FDialogSetup
	{
		TArray<TCHAR> Filter;
		TCHAR File[4096] = {};
		FString Folder;
		FString Title;
		OPENFILENAMEW Dialog = {};

		FDialogSetup(const FText& InTitle, const FString& FilterName, const FString& Patterns, const FString& InitialPath)
			: Title(InTitle.ToString())
		{
			for (const FString& Part : {FilterName, Patterns})
			{
				Filter.Append(*Part, Part.Len());
				Filter.Add(TEXT('\0'));
			}
			Filter.Add(TEXT('\0'));
			if (!InitialPath.IsEmpty())
			{
				Folder = FPaths::ConvertRelativePathToFull(FPaths::GetPath(InitialPath));
				FPaths::MakePlatformFilename(Folder);
				FCString::Strncpy(File, *FPaths::GetCleanFilename(InitialPath), UE_ARRAY_COUNT(File));
			}
			Dialog.lStructSize = sizeof(Dialog);
			Dialog.hwndOwner = FSlateApplication::IsInitialized()
				? static_cast<HWND>(const_cast<void*>(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)))
				: nullptr;
			Dialog.lpstrFilter = Filter.GetData();
			Dialog.lpstrFile = File;
			Dialog.nMaxFile = UE_ARRAY_COUNT(File);
			Dialog.lpstrInitialDir = Folder.IsEmpty() ? nullptr : *Folder;
			Dialog.lpstrTitle = *Title;
		}
	};
}
#endif

bool FFloor101FileDialog::OpenFile(const FText& Title, const FString& FilterName, const FString& Patterns, FString& OutPath,
	const FString& InitialPath)
{
#if PLATFORM_WINDOWS
	FDialogSetup Setup(Title, FilterName, Patterns, InitialPath);
	Setup.File[0] = TEXT('\0'); // open: only the folder is proposed
	Setup.Dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
	if (!::GetOpenFileNameW(&Setup.Dialog))
	{
		return false;
	}
	OutPath = Setup.File;
	FPaths::NormalizeFilename(OutPath);
	return true;
#else
	return false;
#endif
}

bool FFloor101FileDialog::SaveFile(const FText& Title, const FString& FilterName, const FString& Patterns,
	const FString& DefaultExtension, const FString& InitialPath, FString& OutPath)
{
#if PLATFORM_WINDOWS
	FDialogSetup Setup(Title, FilterName, Patterns, InitialPath);
	Setup.Dialog.lpstrDefExt = *DefaultExtension;
	Setup.Dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
	if (!::GetSaveFileNameW(&Setup.Dialog))
	{
		return false;
	}
	OutPath = Setup.File;
	FPaths::NormalizeFilename(OutPath);
	return true;
#else
	return false;
#endif
}
