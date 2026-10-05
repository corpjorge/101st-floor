#pragma once

#include "CoreMinimal.h"

namespace Floor101::Diagnostics
{
	FLOOR101DIAGNOSTICS_API void SetVerbose(bool bEnabled);
	FLOOR101DIAGNOSTICS_API bool IsVerbose();
	FLOOR101DIAGNOSTICS_API void SetWindowVisible(bool bVisible);
	FLOOR101DIAGNOSTICS_API bool IsWindowVisible();
	FLOOR101DIAGNOSTICS_API FString LogPath();
	FLOOR101DIAGNOSTICS_API bool IsEnabled(ELogVerbosity::Type Verbosity);
	FLOOR101DIAGNOSTICS_API void Write(const TCHAR* Category, ELogVerbosity::Type Verbosity, const FString& Message);
}

// Do not change Unreal's NO_LOGGING ABI against the precompiled installed engine.
#if UE_BUILD_SHIPPING
#define F101_LOG(Category, Verbosity, Format, ...) do { \
	if (::Floor101::Diagnostics::IsEnabled(ELogVerbosity::Verbosity)) { \
		::Floor101::Diagnostics::Write(TEXT(#Category), ELogVerbosity::Verbosity, FString::Printf(Format, ##__VA_ARGS__)); \
	} } while (false)
#else
#define F101_LOG(Category, Verbosity, Format, ...) UE_LOG(Category, Verbosity, Format, ##__VA_ARGS__)
#endif
