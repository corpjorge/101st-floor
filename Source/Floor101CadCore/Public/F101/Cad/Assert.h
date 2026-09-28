#pragma once

#include "F101/Cad/Export.h"

namespace f101::detail
{
	// Reports a violated invariant and terminates the process.
	[[noreturn]] FLOOR101CADCORE_API void assertionFailed(const char* expression, const char* file, int line);
}

// Programmer errors (broken invariants), not user errors: those are returned as f101::Result / f101::Status.
// Always enabled: the checks are cheap and a corrupt model must never propagate.
#define F101_ASSERT(expression) \
	((expression) ? static_cast<void>(0) : ::f101::detail::assertionFailed(#expression, __FILE__, __LINE__))
