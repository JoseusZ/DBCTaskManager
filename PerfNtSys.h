// PerfNtSys.h
#pragma once

#include "stdafx.h"

// NtQuerySystemInformation disk monitor — non-admin fallback that works on
// every Windows version (Win7 through Win10) without any service, perf
// counter registration, or admin rights.

void PerfNtSys_Start(void);
void PerfNtSys_Stop(void);

// Getters for the most recent system-wide sample.
double PerfNtSys_GetReadBps(void);
double PerfNtSys_GetWriteBps(void);
double PerfNtSys_GetTotalPct(void);
LONG   PerfNtSys_GetReady(void);
