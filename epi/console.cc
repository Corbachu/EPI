//----------------------------------------------------------------------------
//  EPI Console / Logging helpers
//----------------------------------------------------------------------------
//
// NOTE: DITD/FITD currently provides the active logging implementation in
// FitdLib/console.cc (built into FitdLib).
//
// This file is kept guarded so it won't cause duplicate symbols if the EPI
// sources are ever added to the build.

#include "epi.h"

#if defined(EPI_BUILD_CONSOLE_IMPL)

#include <cstdarg>
#include <cstdio>

#if defined(DREAMCAST)
extern "C" {
#include <kos.h>
}
#endif

static void I_VPrint(const char *prefix, const char *fmt, va_list ap)
{
	char buf[2048];
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	(void)n;

#if defined(DREAMCAST)
	if (prefix && prefix[0])
		dbgio_printf("%s%s", prefix, buf);
	else
		dbgio_printf("%s", buf);
#else
	FILE *out = (prefix && prefix[0]) ? stderr : stdout;
	if (prefix && prefix[0])
		fputs(prefix, out);
	fputs(buf, out);
	fflush(out);
#endif
}

void I_Printf(const char *message, ...)
{
	va_list ap;
	va_start(ap, message);
	I_VPrint(nullptr, message, ap);
	va_end(ap);
}

void I_Warning(const char *warning, ...)
{
	va_list ap;
	va_start(ap, warning);
	I_VPrint("WARNING: ", warning, ap);
	va_end(ap);
}

void I_Debugf(const char *message, ...)
{
	va_list ap;
	va_start(ap, message);
	I_VPrint("DEBUG: ", message, ap);
	va_end(ap);
}

#endif  // EPI_BUILD_CONSOLE_IMPL
