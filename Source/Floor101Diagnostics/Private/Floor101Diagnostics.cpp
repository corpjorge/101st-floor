#include "Floor101Diagnostics.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceConsole.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "CoreGlobals.h"
#include <atomic>

#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
#include "Windows/WindowsHWrapper.h"
#endif

namespace Floor101::Diagnostics
{
	namespace
	{
		std::atomic<bool> bVerbose = false;
		FCriticalSection Lock;
		FString FilePath;
		FDelegateHandle FatalHandle;
#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
		bool bOwnConsole = false;
		// The product log stays open for appending: opening and closing it for every line stalled the game thread on each
		// edit. Shared for writing and renaming, like the per-line appends it replaces, so another instance still logs.
		HANDLE LogFile = INVALID_HANDLE_VALUE;
		bool bLogFileOpened = false;
		constexpr int64 MaxLogFileBytes = 16 * 1024 * 1024;

		HANDLE OpenForAppend(const FString& Path)
		{
			return CreateFileW(*Path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
				OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		}

		// Under Lock, once per session: a log that reached MaxLogFileBytes first moves to <name>.old.log, replacing the
		// previous one, so the logs of past sessions stay bounded.
		void OpenLogFile(const FString& Path)
		{
			bLogFileOpened = true;
			LogFile = OpenForAppend(Path);
			LARGE_INTEGER Size{};
			if (LogFile == INVALID_HANDLE_VALUE || !GetFileSizeEx(LogFile, &Size) || Size.QuadPart < MaxLogFileBytes) { return; }
			CloseHandle(LogFile);
			const FString Old = FPaths::Combine(FPaths::GetPath(Path), FPaths::GetBaseFilename(Path) + TEXT(".old.log"));
			MoveFileExW(*Path, *Old, MOVEFILE_REPLACE_EXISTING); // best effort: otherwise keep appending
			LogFile = OpenForAppend(Path);
		}

		void CloseLogFile()
		{
			FScopeLock Guard(&Lock);
			if (LogFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(LogFile);
				LogFile = INVALID_HANDLE_VALUE;
			}
		}
#endif
	}
	void SetVerbose(bool bEnabled) { bVerbose.store(bEnabled); }
	bool IsVerbose() { return bVerbose.load(); }
	bool IsEnabled(ELogVerbosity::Type Verbosity) { return Verbosity <= ELogVerbosity::Log || IsVerbose(); }
	FString LogPath()
	{
#if UE_BUILD_SHIPPING
		FScopeLock Guard(&Lock);
		if (FilePath.IsEmpty())
		{
			if (!FParse::Value(FCommandLine::Get(), TEXT("ABSLOG="), FilePath))
			{
				FilePath = FPaths::Combine(FPaths::ProjectLogDir(), TEXT("Floor101-product.log"));
			}
			FilePath = FPaths::ConvertRelativePathToFull(FilePath);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
		}
		return FilePath;
#else
		return FPaths::ConvertRelativePathToFull(FPlatformOutputDevices::GetAbsoluteLogFilename());
#endif
	}
	void Write(const TCHAR* Category, ELogVerbosity::Type Verbosity, const FString& Message)
	{
		if (!IsEnabled(Verbosity)) { return; }
		const FString Path = LogPath();
		const TCHAR* Severity = Verbosity <= ELogVerbosity::Error ? TEXT("Error") : Verbosity == ELogVerbosity::Warning ? TEXT("Warning") : TEXT("Info");
		const FString Line = FString::Printf(TEXT("[%s] %s %s: %s\n"), *FDateTime::UtcNow().ToIso8601(), Category, Severity, *Message);
		FScopeLock Guard(&Lock);
#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
		if (!bLogFileOpened) { OpenLogFile(Path); }
		if (LogFile != INVALID_HANDLE_VALUE)
		{
			const FTCHARToUTF8 Utf8(*Line);
			DWORD Appended = 0;
			WriteFile(LogFile, Utf8.Get(), static_cast<DWORD>(Utf8.Length()), &Appended, nullptr);
		}
		else
		{
			// It could not be kept open: append line by line, as before.
			FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		}
		if (bOwnConsole)
		{
			DWORD Written = 0;
			WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), *Line, Line.Len(), &Written, nullptr);
		}
#else
		FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
#endif
	}
	void SetWindowVisible(bool bVisible)
	{
#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
		FScopeLock Guard(&Lock);
		if (bVisible && !bOwnConsole && !GetConsoleWindow())
		{
			bOwnConsole = AllocConsole() != 0;
			if (bOwnConsole)
			{
				SetConsoleTitleW(L"101st Floor · Diagnóstico");
				// Closing this optional window hides it; it must never terminate the CAD session.
				if (HMENU Menu = GetSystemMenu(GetConsoleWindow(), 0)) { DeleteMenu(Menu, SC_CLOSE, MF_BYCOMMAND); }
			}
		}
		if (bOwnConsole) { ShowWindow(GetConsoleWindow(), bVisible ? SW_SHOWNOACTIVATE : SW_HIDE); }
#else
		if (GLogConsole) { GLogConsole->Show(bVisible); }
#endif
	}
	bool IsWindowVisible()
	{
#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
		return bOwnConsole && ::IsWindowVisible(GetConsoleWindow()) != 0;
#else
		return GLogConsole && GLogConsole->IsShown();
#endif
	}
}

class FFloor101DiagnosticsModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if UE_BUILD_SHIPPING
		Floor101::Diagnostics::SetVerbose(FParse::Param(FCommandLine::Get(), TEXT("F101Debug")));
		Floor101::Diagnostics::Write(TEXT("Startup"), ELogVerbosity::Display, TEXT("101st Floor Shipping: product diagnostics initialized"));
		Floor101::Diagnostics::FatalHandle = FCoreDelegates::OnHandleSystemError.AddLambda([]()
		{
			Floor101::Diagnostics::Write(TEXT("Runtime"), ELogVerbosity::Error, FString(GErrorHist));
		});
#endif
	}
	virtual void ShutdownModule() override
	{
#if UE_BUILD_SHIPPING
		FCoreDelegates::OnHandleSystemError.Remove(Floor101::Diagnostics::FatalHandle);
#if PLATFORM_WINDOWS
		if (Floor101::Diagnostics::bOwnConsole) { FreeConsole(); }
		Floor101::Diagnostics::CloseLogFile();
#endif
#endif
	}
};
IMPLEMENT_MODULE(FFloor101DiagnosticsModule, Floor101Diagnostics);
