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
		FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
		if (bOwnConsole)
		{
			DWORD Written = 0;
			WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), *Line, Line.Len(), &Written, nullptr);
		}
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
#endif
#endif
	}
};
IMPLEMENT_MODULE(FFloor101DiagnosticsModule, Floor101Diagnostics);
