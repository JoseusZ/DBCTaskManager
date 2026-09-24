// PerfPidIoCache.h
#pragma once

#include "stdafx.h"

#define PID_IO_CACHE_MAX 512

struct PidIoEntry
{
	DWORD     Pid;
	ULONGLONG ReadXfer;
	ULONGLONG WriteXfer;
	ULONGLONG OtherXfer;
};

void    PerfPidIoCache_Init(void);
void    PerfPidIoCache_BeginUpdate(void);
void    PerfPidIoCache_EndUpdate(void);
void    PerfPidIoCache_SetSlot(int slot, DWORD pid, ULONGLONG r, ULONGLONG w, ULONGLONG o);
void    PerfPidIoCache_Clear(void);
void    PerfPidIoCache_MarkReady(void);
LONG    PerfPidIoCache_GetReady(void);

// Read-only peek of slot `s`. Caller must already hold the update lock
// (between BeginUpdate / EndUpdate) since the slot Pid field is plain DWORD.
DWORD   PerfPidIoCache_PeekPid(int s);

// Diagnostic counters used by the debug log writer thread.
extern volatile LONG g_DbgLookupCalls;
extern volatile LONG g_DbgLookupHits;
extern volatile LONG g_DbgLookupMisses;
extern volatile LONG g_DbgWriterSlots;
extern volatile LONG g_DbgWriterTicks;
extern volatile LONG g_DbgReadyFlag;
