#pragma once

// Symbol export for the pure modules (ADR-0002).
// UnrealBuildTool defines FLOOR101CADCORE_API as DLLEXPORT/DLLIMPORT in modular (editor) builds, and those two macros
// normally come from Unreal's platform headers, which pure code does not include. They are defined here with the exact
// same tokens Unreal uses on Windows, so either definition order is a legal identical redefinition.
// Outside Unreal (CMake unit tests) the API macros expand to nothing.
#if defined(_MSC_VER)
	#ifndef DLLEXPORT
		#define DLLEXPORT __declspec(dllexport)
	#endif
	#ifndef DLLIMPORT
		#define DLLIMPORT __declspec(dllimport)
	#endif
#endif

#ifndef FLOOR101CADCORE_API
	#define FLOOR101CADCORE_API
#endif
