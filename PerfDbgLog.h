// PerfDbgLog.h
#pragma once

BOOL PerfDbgLog_IsEnabled(void);
void PerfDbgLog_Printf(const char* fmt, ...);

// Inline convenience macros so existing call sites (PerfDbgLog("%s", buf))
// compile unchanged.
#define _DbgIoLog        PerfDbgLog_Printf
#define _DbgIoLogEnabled PerfDbgLog_IsEnabled
