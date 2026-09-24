// PerfPdhProcIo.cpp
#include "stdafx.h"
#include "PerfPdhProcIo.h"
#include "PerfDbgLog.h"
#include "PerfPidIoCache.h"

#include <pdh.h>
#pragma comment(lib, "pdh.lib")

#include <stdlib.h>

// PDH_MORE_DATA is documented but not defined in the Win SDK pdh.h.
#ifndef PDH_MORE_DATA
#define PDH_MORE_DATA 0x800007D0L
#endif

static HANDLE        s_hThread = NULL;
static volatile LONG s_Stop    = 0;

static double   s_SystemBps      = 0.0;
static double   s_SystemBusyPct  = 0.0;
static LONG     s_SystemReady    = 0;
static double   s_BusyBuf[10]    = {0};
static int      s_BusyIdx        = 0;
static double   s_BusySum        = 0;
static LONGLONG s_PrevBytes      = 0;

static unsigned __stdcall Thread_MonitorPdhPerProcIo(void*)
{
	HQUERY hQuery = NULL;
	for(int attempt = 0; attempt < 10 && s_Stop == 0; attempt++)
	{
		if(PdhOpenQuery(NULL, 0, &hQuery) == ERROR_SUCCESS && hQuery)
			break;
		hQuery = NULL;
		Sleep(500);
	}
	if(!hQuery) return 0;

	HCOUNTER hPidCounter = NULL;
	HCOUNTER hIoCounter  = NULL;
	PDH_STATUS sPid = PdhAddCounterW(hQuery, L"\\Process(*)\\ID Process", 0, &hPidCounter);
	PDH_STATUS sIo  = PdhAddCounterW(hQuery, L"\\Process(*)\\IO Data Bytes", 0, &hIoCounter);
	if(sPid != ERROR_SUCCESS || sIo != ERROR_SUCCESS || !hPidCounter || !hIoCounter)
	{
		_DbgIoLog("PDH-PROCIO init failed: sPid=0x%lX sIo=0x%lX hPid=%p hIo=%p",
			sPid, sIo, hPidCounter, hIoCounter);
		PdhCloseQuery(hQuery);
		return 0;
	}

	PdhCollectQueryData(hQuery);

	while(s_Stop == 0)
	{
		Sleep(1000);
		PdhCollectQueryData(hQuery);

		PDH_FMT_COUNTERVALUE_ITEM* pidItems = NULL;
		PDH_FMT_COUNTERVALUE_ITEM* ioItems  = NULL;
		DWORD pidBufSize = 0;
		DWORD ioBufSize  = 0;
		DWORD pidItemCount = 0;
		DWORD ioItemCount  = 0;

		PDH_STATUS g1 = PdhGetFormattedCounterArray(hPidCounter, PDH_FMT_LARGE,
			&pidBufSize, &pidItemCount, pidItems);
		if(g1 == PDH_MORE_DATA)
		{
			pidItems = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(pidBufSize);
			if(pidItems) memset(pidItems, 0, pidBufSize);
			g1 = PdhGetFormattedCounterArray(hPidCounter, PDH_FMT_LARGE,
				&pidBufSize, &pidItemCount, pidItems);
		}
		PDH_STATUS g2 = PdhGetFormattedCounterArray(hIoCounter, PDH_FMT_LARGE,
			&ioBufSize, &ioItemCount, ioItems);
		if(g2 == PDH_MORE_DATA)
		{
			ioItems = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(ioBufSize);
			if(ioItems) memset(ioItems, 0, ioBufSize);
			g2 = PdhGetFormattedCounterArray(hIoCounter, PDH_FMT_LARGE,
				&ioBufSize, &ioItemCount, ioItems);
		}

		if(g1 == ERROR_SUCCESS && g2 == ERROR_SUCCESS && pidItems && ioItems)
		{
			DWORD nPid = pidItemCount;
			DWORD nIo  = ioItemCount;
			DWORD n    = (nPid < nIo) ? nPid : nIo;

			PerfPidIoCache_BeginUpdate();
			PerfPidIoCache_Clear();
			int slot = 0;
			int matched = 0, mismatched = 0;
			LONGLONG sysTotal = 0;
			for(DWORD k = 0; k < n && slot < PID_IO_CACHE_MAX; k++)
			{
				DWORD pid = (DWORD)pidItems[k].FmtValue.largeValue;
				LONGLONG bytes = ioItems[k].FmtValue.largeValue;
				if(pid < 1 || pid > 0xFFFF) continue;
				PerfPidIoCache_SetSlot(slot++, pid, (ULONGLONG)bytes, 0, 0);
				matched++;
				sysTotal += bytes;
			}
			if(nPid != nIo)
			{
				for(DWORD k = 0; k < nPid && slot < PID_IO_CACHE_MAX; k++)
				{
					DWORD pid = (DWORD)pidItems[k].FmtValue.largeValue;
					if(pid < 1 || pid > 0xFFFF) continue;
					BOOL already = FALSE;
					for(int s = 0; s < slot; s++)
						if(PerfPidIoCache_PeekPid(s) == pid) { already = TRUE; break; }
					if(already) continue;
					LPCWSTR nm = pidItems[k].szName;
					for(DWORD j = 0; j < nIo; j++)
					{
						if(_wcsicmp(ioItems[j].szName, nm) == 0)
						{
							LONGLONG bytes = ioItems[j].FmtValue.largeValue;
							PerfPidIoCache_SetSlot(slot++, pid, (ULONGLONG)bytes, 0, 0);
							matched++;
							break;
						}
					}
					mismatched++;
				}
			}

			{
							LONGLONG delta = (s_PrevBytes > 0 && sysTotal >= s_PrevBytes)
								? (sysTotal - s_PrevBytes)
								: 0;
				if(delta < 0) delta = 0;
				s_SystemBps = (double)delta;
				s_BusySum -= s_BusyBuf[s_BusyIdx];
				s_BusyBuf[s_BusyIdx] = (double)delta;
				s_BusySum += s_BusyBuf[s_BusyIdx];
				s_BusyIdx = (s_BusyIdx + 1) % 10;
				if(s_BusySum < 0) s_BusySum = 0;
				double avgBps = s_BusySum / 10.0;
				const double REF_BPS = 5.0 * 1024.0 * 1024.0;
				double pct = (avgBps / REF_BPS) * 100.0;
				if(pct <   0.0) pct =   0.0;
				if(pct > 100.0) pct = 100.0;
				if(!_finite(pct)) pct = 0.0;
				s_SystemBusyPct = pct;
				s_PrevBytes = sysTotal;
				InterlockedExchange(&s_SystemReady, 1);
			}

			PerfPidIoCache_EndUpdate();
			PerfPidIoCache_MarkReady();

			static LONG seq = 0;
			LONG s = InterlockedIncrement(&seq);
			if((s % 5) == 0)
			{
				_DbgIoLog("PDH-PROCIO tick: instances=%lu slot=%d matched=%d mismatched=%d "
					"sysTotal=%lld sysBps=%.0f sysBusyPct=%.1f",
					n, slot, matched, mismatched,
					(long long)sysTotal, s_SystemBps, s_SystemBusyPct);
			}
		}
		else
		{
			static LONG errSeq = 0;
			LONG s = InterlockedIncrement(&errSeq);
			if((s % 10) == 1)
			{
				_DbgIoLog("PDH-PROCIO read failed: g1=0x%lX g2=0x%lX pidItems=%p ioItems=%p",
					g1, g2, pidItems, ioItems);
			}
		}

		if(pidItems) free(pidItems);
		if(ioItems)  free(ioItems);
	}

	PdhCloseQuery(hQuery);
	return 0;
}

void PerfPdhProcIo_Start(void)
{
	if(s_hThread != NULL) return;
	InterlockedExchange(&s_Stop, 0);
	s_hThread = (HANDLE)_beginthreadex(NULL, 0,
		Thread_MonitorPdhPerProcIo, NULL, 0, NULL);
}

void PerfPdhProcIo_Stop(void)
{
	if(s_hThread == NULL) return;
	InterlockedExchange(&s_Stop, 1);
	WaitForSingleObject(s_hThread, 3000);
	CloseHandle(s_hThread);
	s_hThread = NULL;
}

double PerfPdhProcIo_GetSystemBps(void)     { return s_SystemBps;     }
double PerfPdhProcIo_GetSystemBusyPct(void) { return s_SystemBusyPct; }
LONG   PerfPdhProcIo_GetReady(void)         { return s_SystemReady;    }
