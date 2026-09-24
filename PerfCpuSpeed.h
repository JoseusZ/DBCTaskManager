// PerfCpuSpeed.h
#pragma once

#include "stdafx.h"

// PROCESSOR_POWER_INFORMATION used by CallNtPowerInformation
// (ProcessorInformation). Defined here because the only consumer is the
// speed-monitor code that historically lived next to it.
typedef struct _PROCESSOR_POWER_INFORMATION {
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz;
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

void PerfCpuSpeed_Start(void);   // RDTSC + QPC measurement thread
void PerfCpuSpeed_Stop(void);
void PerfWmiCpu_Start(void);     // WMI Win32_Processor.CurrentClockSpeed thread
void PerfWmiCpu_Stop(void);

LONG PerfCpuSpeed_GetMhz(void);  // 0 = not ready yet
LONG PerfCpuSpeed_GetReady(void);

LONG PerfWmiCpu_GetMhz(void);
LONG PerfWmiCpu_GetReady(void);
