// PerfDbgLog.cpp
#include "stdafx.h"
#include "PerfDbgLog.h"

#include <stdio.h>
#include <stdarg.h>

BOOL PerfDbgLog_IsEnabled(void)
{
#ifdef DBC_DBG_IO_LOG
	return TRUE;
#else
	static LONG cached = -1;
	LONG v = InterlockedCompareExchange(&cached, 0, 0);
	if(v >= 0) return (BOOL)v;
	BOOL enabled = (GetFileAttributesA("dbc_dbg_io.log") != INVALID_FILE_ATTRIBUTES);
	InterlockedExchange(&cached, enabled ? 1 : 0);
	return enabled;
#endif
}

void PerfDbgLog_Printf(const char* fmt, ...)
{
	if(!PerfDbgLog_IsEnabled()) return;
	FILE* f = NULL;
	if(fopen_s(&f, "dbc_dbg_io.log", "a") != 0 || !f) return;
	va_list ap; va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}
