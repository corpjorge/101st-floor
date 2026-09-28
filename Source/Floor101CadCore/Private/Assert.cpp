#include "F101/Cad/Assert.h"

#include <cstdio>
#include <cstdlib>

namespace f101::detail
{
	void assertionFailed(const char* expression, const char* file, int line)
	{
		std::fprintf(stderr, "F101_ASSERT failed: %s (%s:%d)\n", expression, file, line);
		std::fflush(stderr);
		std::abort();
	}
}
