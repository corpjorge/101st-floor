#include "Floor101FileDialog.h"

#include "Framework/Application/SlateApplication.h"

#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	#include <commdlg.h>
	#include "Windows/HideWindowsPlatformTypes.h"
#endif

bool FFloor101FileDialog::OpenFile(const FText& Title, const FString& FilterName, const FString& Patterns, FString& OutPath)
{
#if PLATFORM_WINDOWS
	// The filter is pairs of NUL-terminated strings, ended by an extra NUL.
	TArray<TCHAR> Filter;
	for (const FString& Part : {FilterName, Patterns})
	{
		Filter.Append(*Part, Part.Len());
		Filter.Add(TEXT('\0'));
	}
	Filter.Add(TEXT('\0'));
	TCHAR File[4096] = {};
	const FString TitleText = Title.ToString();

	OPENFILENAMEW Dialog = {};
	Dialog.lStructSize = sizeof(Dialog);
	Dialog.hwndOwner = FSlateApplication::IsInitialized()
		? static_cast<HWND>(const_cast<void*>(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)))
		: nullptr;
	Dialog.lpstrFilter = Filter.GetData();
	Dialog.lpstrFile = File;
	Dialog.nMaxFile = UE_ARRAY_COUNT(File);
	Dialog.lpstrTitle = *TitleText;
	Dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
	if (!::GetOpenFileNameW(&Dialog))
	{
		return false;
	}
	OutPath = File;
	return true;
#else
	return false;
#endif
}
