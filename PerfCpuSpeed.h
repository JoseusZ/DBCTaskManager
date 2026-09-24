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

// ---------------------------------------------------------------------------
//  Win10/11-style cascade (clean Ring-3 implementation)
//
//  Priority 1:  PDH "% Processor Performance"  (PdhAddEnglishCounterW)
//               * fBaseGhz.  On Windows 10/11 and modern CPUs (Intel Core
//               1st gen+, AMD Ryzen / Zen) the counter exceeds 100 % while
//               the cores are in Turbo / Boost state and is the
//               authoritative source.
//  Priority 2:  CallNtPowerInformation CurrentMhz (raw MHz / 1000 -> GHz).
//               On most platforms reports the active P-state; it can also
//               reach into Turbo (CurMhz > MaxMhz).
//  Priority 3:  WMI Win32_Processor.CurrentClockSpeed (3 s polling).
//  Priority 4:  fBaseGhz  (nominal base clock from registry ~MHz).
//
//  NOTE - Win 7 / Sandy Bridge and similar legacy paths:
//      Intel introduced the Invariant TSC in the 2nd Generation Core
//      family (Sandy Bridge). __rdtsc() therefore advances at a fixed
//      nominal rate (e.g. 2.50 GHz for the i5-2520M) regardless of P-state
//      or Turbo, so any Ring-3 RDTSC / QPC delta sampler will only ever
//      report the base clock and never surface Turbo Boost. Windows 7
//      itself exposes no Ring-3 API that recovers the Turbo frequency on
//      those CPUs, so the displayed speed is capped to the nominal clock
//      on this combination. On Windows 10/11 (modern PDH counter) the
//      full Turbo range is reported correctly.
// ---------------------------------------------------------------------------

// PDH "% Processor Performance" monitor (PRIMARY source). Uses
// PdhAddEnglishCounterW so the counter path resolves regardless of the
// display language (es-ES, es-MX, fr-FR, de-DE, ja-JP, ...).
void PerfPdhCpuPerf_Start(void);
void PerfPdhCpuPerf_Stop(void);

double PerfPdhCpuPerf_GetPct(void);   // 0.0 until ready; can exceed 100.0 for Turbo
LONG   PerfPdhCpuPerf_GetReady(void);

// WMI Win32_Processor.CurrentClockSpeed monitor (fallback). Polled every 3 s.
void PerfWmiCpu_Start(void);
void PerfWmiCpu_Stop(void);

LONG PerfWmiCpu_GetMhz(void);   // 0 = not ready yet
LONG PerfWmiCpu_GetReady(void);

// TRUE when the running OS reports Windows 7 (NT 6.1) via VerifyVersionInfo.
// Used both by the cascade (to gate the software Turbo estimator) and by the
// Performance view UI (to surface the "Turbo frequencies may be inaccurate
// on Windows 7" disclaimer under the CPU model name).
BOOL PerfIsWindows7(void);
