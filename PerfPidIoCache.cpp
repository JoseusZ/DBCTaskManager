// PerfPidIoCache.cpp
#include "stdafx.h"
#include "PerfPidIoCache.h"

static CRITICAL_SECTION s_PidIoLock;
static BOOL             s_PidIoLockInit = FALSE;
static PidIoEntry       s_PidIoCache[PID_IO_CACHE_MAX];
static LONG             s_PidIoReady    = 0;

volatile LONG g_DbgLookupCalls    = 0;
volatile LONG g_DbgLookupHits     = 0;
volatile LONG g_DbgLookupMisses   = 0;
volatile LONG g_DbgWriterSlots    = 0;
volatile LONG g_DbgWriterTicks    = 0;
volatile LONG g_DbgReadyFlag      = 0;

static void EnsureLockInit(void)
{
	if(s_PidIoLockInit) return;
	InitializeCriticalSection(&s_PidIoLock);
	s_PidIoLockInit = TRUE;
}

void PerfPidIoCache_Init(void)
{
	EnsureLockInit();
	memset(s_PidIoCache, 0, sizeof(s_PidIoCache));
	InterlockedExchange(&s_PidIoReady, 0);
}

void PerfPidIoCache_BeginUpdate(void)
{
	EnsureLockInit();
	EnterCriticalSection(&s_PidIoLock);
}

void PerfPidIoCache_EndUpdate(void)
{
	LeaveCriticalSection(&s_PidIoLock);
}

void PerfPidIoCache_SetSlot(int slot, DWORD pid, ULONGLONG r, ULONGLONG w, ULONGLONG o)
{
	if(slot < 0 || slot >= PID_IO_CACHE_MAX) return;
	s_PidIoCache[slot].Pid       = pid;
	s_PidIoCache[slot].ReadXfer  = r;
	s_PidIoCache[slot].WriteXfer = w;
	s_PidIoCache[slot].OtherXfer = o;
}

void PerfPidIoCache_Clear(void)
{
	for(int i = 0; i < PID_IO_CACHE_MAX; i++) s_PidIoCache[i].Pid = 0;
}

void PerfPidIoCache_MarkReady(void)
{
	InterlockedExchange(&s_PidIoReady, 1);
}

LONG PerfPidIoCache_GetReady(void)
{
	return InterlockedCompareExchange(&s_PidIoReady, 0, 0);
}

DWORD PerfPidIoCache_PeekPid(int s)
{
	if(s < 0 || s >= PID_IO_CACHE_MAX) return 0;
	return s_PidIoCache[s].Pid;
}

// Public accessor exported across the DLL/exe boundary for ProcessInfo.cpp.
extern "C" BOOL ApiGetProcessDiskIoFromCache(DWORD pid, ULONGLONG* pDiskBytes, ULONGLONG* pOther)
{
	InterlockedIncrement(&g_DbgLookupCalls);
	if(pDiskBytes) *pDiskBytes = 0;
	if(pOther)     *pOther = 0;
	LONG readyObserved = InterlockedCompareExchange(&s_PidIoReady, 0, 0);
	g_DbgReadyFlag = readyObserved;
	if(readyObserved == 0)
	{
		InterlockedIncrement(&g_DbgLookupMisses);
		return FALSE;
	}
	if(pid == 0)
	{
		InterlockedIncrement(&g_DbgLookupMisses);
		return FALSE;
	}
	if(!s_PidIoLockInit)
	{
		InterlockedIncrement(&g_DbgLookupMisses);
		return FALSE;
	}

	EnterCriticalSection(&s_PidIoLock);
	BOOL found = FALSE;
	for(int i = 0; i < PID_IO_CACHE_MAX; i++)
	{
		if(s_PidIoCache[i].Pid == pid)
		{
			if(pDiskBytes) *pDiskBytes = s_PidIoCache[i].ReadXfer + s_PidIoCache[i].WriteXfer;
			if(pOther)     *pOther     = s_PidIoCache[i].OtherXfer;
			found = TRUE;
			break;
		}
	}
	LeaveCriticalSection(&s_PidIoLock);
	if(found) InterlockedIncrement(&g_DbgLookupHits);
	else      InterlockedIncrement(&g_DbgLookupMisses);
	return found;
}
