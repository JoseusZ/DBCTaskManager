// PerfPdhDisk.h
#pragma once

#include "stdafx.h"
#include "PerfDiskShared.h"

// PDH-based per-disk monitor (Win7+ fallback when WMI perf counter provider
// is unavailable). Reads \PhysicalDisk(*)\* directly via PDH.

struct PdhDiskMetrics
{
	double ReadBps;
	double WriteBps;
	double Pct;
	LONG   Ready;
	LONG   EverNonZero;
};

void PerfPdhDisk_Start(void);
void PerfPdhDisk_Stop(void);

BOOL PerfPdhDisk_GetPerDisk(int idx, PdhDiskMetrics* out);
double PerfPdhDisk_GetTotalPct(void);
double PerfPdhDisk_GetTotalReadBps(void);
double PerfPdhDisk_GetTotalWriteBps(void);
LONG   PerfPdhDisk_GetTotalReady(void);
LONG   PerfPdhDisk_GetTotalEverNonZero(void);
