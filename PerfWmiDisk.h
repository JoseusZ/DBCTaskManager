// PerfWmiDisk.h
#pragma once

#include "stdafx.h"
#include "PerfDiskShared.h"

// WMI-based disk metrics monitor (Win7+, no admin required). Polls
// Win32_PerfRawData_PerfDisk_PhysicalDisk once per second on a background
// thread and caches per-disk + _Total counters.

struct WmiDiskMetrics
{
	double ReadBps;
	double WriteBps;
	double Pct;
	LONG   Ready;
	LONG   EverNonZero;
};

void PerfWmiDisk_Start(void);
void PerfWmiDisk_Stop(void);

BOOL PerfWmiDisk_GetPerDisk(int idx, WmiDiskMetrics* out);
double PerfWmiDisk_GetTotalPct(void);
LONG   PerfWmiDisk_GetTotalReady(void);
LONG   PerfWmiDisk_GetTotalEverNonZero(void);
