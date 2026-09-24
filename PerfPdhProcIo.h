// PerfPdhProcIo.h
#pragma once

#include "stdafx.h"

// PDH-based per-process disk I/O monitor (Win7+ non-admin path). Reads
// \Process(*)\ID Process and \Process(*)\IO Data Bytes via the documented
// Performance Data Helper API, populates the per-process cache shared with
// PerfNtSys, and derives a system-wide bytes/sec + busy% from the per-process
// sum (used as a fallback when PhysicalDisk PDH counters return 0).

void PerfPdhProcIo_Start(void);
void PerfPdhProcIo_Stop(void);

double PerfPdhProcIo_GetSystemBps(void);
double PerfPdhProcIo_GetSystemBusyPct(void);
LONG   PerfPdhProcIo_GetReady(void);
