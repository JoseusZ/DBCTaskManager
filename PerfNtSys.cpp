// PerfNtSys.cpp
#include "stdafx.h"
#include "PerfNtSys.h"
#include "PerfDbgLog.h"
#include "PerfPidIoCache.h"

#include <stdlib.h>

static double        s_ReadBps   = 0.0;
static double        s_WriteBps  = 0.0;
static double        s_TotalPct  = 0.0;
static LONG          s_Ready     = 0;
static HANDLE        s_hThread   = NULL;
static volatile LONG s_Stop      = 0;

static unsigned __stdcall Thread_MonitorNtSysDisk(void*)
{
	typedef LONG (WINAPI *PFN_NtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
	HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
	if(!hNtdll) return 0;
	PFN_NtQuerySystemInformation pNtQuerySystemInformation =
		(PFN_NtQuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation");
	if(!pNtQuerySystemInformation) return 0;

	// SYSTEM_PROCESS_INFORMATION layout (Win7 / Win10):
	//   ULONG      NextEntryOffset;                       +0x000
	//   ULONG      NumberOfThreads;                       +0x004
	//   LARGE_INT  WorkingSetPrivateSize;                +0x008
	//   ULONG      HardFaultCount;                        +0x010
	//   ULONG      NumberOfThreadsHighWatermark;          +0x014
	//   ULONGLONG  CycleTime;                             +0x018
	//   LARGE_INT  CreateTime;                            +0x020
	//   LARGE_INT  UserTime;                              +0x028
	//   LARGE_INT  KernelTime;                            +0x030
	//   UNICODE_STR ImageName;                            x86:+0x038 / x64:+0x038 (padded 16)
	//   KPRIORITY  BasePriority;                          x86:+0x040 / x64:+0x048
	//   HANDLE     UniqueProcessId;                       x86:+0x044 / x64:+0x050
	//   LARGE_INT  ReadTransferCount;                     x86:+0x094 / x64:+0x0D0
	//   LARGE_INT  WriteTransferCount;                    x86:+0x09C / x64:+0x0D8
	//   LARGE_INT  OtherTransferCount;                    x86:+0x0A4 / x64:+0x0E0
	//
	// Calibrated at runtime against our own process's GetProcessIoCounters
	// so Win10 build variations don't shift our reads into the wrong slot.
	ULONG OFF_NEXT       = 0x000;
	ULONG OFF_PID;
	ULONG OFF_READ_XFER;
	ULONG OFF_WRITE_XFER;
	ULONG OFF_OTHER_XFER;
#ifdef _M_X64
	OFF_PID        = 0x050;
	OFF_READ_XFER  = 0x0D0;
	OFF_WRITE_XFER = 0x0D8;
	OFF_OTHER_XFER = 0x0E0;
#else
	OFF_PID        = 0x044;
	OFF_READ_XFER  = 0x094;
	OFF_WRITE_XFER = 0x09C;
	OFF_OTHER_XFER = 0x0A4;
#endif

	PerfPidIoCache_Init();

	// Rolling 100-sample (10 s) throughput buffer. The header % is the
	// average bytes/sec over the 10 s window mapped to a 5 MiB/s reference.
	LONGLONG bytesBuf[100] = {0};
	int      bytesIdx = 0;
	LONGLONG bytesSum = 0;

	ULONG bufSize = 256 * 1024;
	BYTE* buf = (BYTE*)malloc(bufSize);
	if(!buf) return 0;

	LONGLONG prevSystemRead  = 0;
	LONGLONG prevSystemWrite = 0;
	BOOL     havePrev        = FALSE;

	while(s_Stop == 0)
	{
		Sleep(100);

		LONG st = pNtQuerySystemInformation(/*SystemProcessInformation*/5,
			buf, bufSize, NULL);
		if(st == /*STATUS_INFO_LENGTH_MISMATCH*/0xC0000004)
		{
			free(buf);
			bufSize *= 2;
			if(bufSize > 4 * 1024 * 1024) bufSize = 4 * 1024 * 1024;
			buf = (BYTE*)malloc(bufSize);
			if(!buf) break;
			continue;
		}
		if(st != 0 /* STATUS_SUCCESS */)
			continue;

		static LONG s_calibrated = 0;
		if(InterlockedCompareExchange(&s_calibrated, 1, 0) == 0)
		{
			IO_COUNTERS realIo = {0};
			HANDLE hSelf = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
			BOOL gotReal = FALSE;
			if(hSelf)
			{
				gotReal = GetProcessIoCounters(hSelf, &realIo);
				CloseHandle(hSelf);
			}

			ULONGLONG realR = realIo.ReadTransferCount;
			ULONGLONG realW = realIo.WriteTransferCount;
			ULONGLONG realO = realIo.OtherTransferCount;

			BOOL found = FALSE;
			BYTE* ownEntry = NULL;
			if(gotReal && realR == 0 && realW == 0 && realO == 0)
			{
				_DbgIoLog("CALIBRATE: realR/W/O are all 0 - caller has done no I/O yet, "
					"keeping documented offsets OFF_PID=0x%lX OFF_READ=0x%lX",
					OFF_PID, OFF_READ_XFER);
				InterlockedExchange(&s_calibrated, 2);
			}
			else if(gotReal && bufSize >= 0x110)
			{
				const ULONGLONG SKEW_CAP = 1ull * 1024 * 1024;
				BYTE* q = buf;
				for(ULONG iters = 0; iters < 2048; iters++)
				{
					for(ULONG off2 = 0x040; off2 <= 0x108; off2 += 8)
					{
						ULONGLONG r = *(ULONGLONG*)(q + off2 + 0);
						ULONGLONG w = *(ULONGLONG*)(q + off2 + 8);
						ULONGLONG o = *(ULONGLONG*)(q + off2 + 16);
						BOOL rMatch = (r >= realR) && (r - realR) <= SKEW_CAP;
						if(rMatch && w == realW && o == realO)
						{
							OFF_READ_XFER  = off2 + 0;
							OFF_WRITE_XFER = off2 + 8;
							OFF_OTHER_XFER = off2 + 16;
							ownEntry       = q;
							found          = TRUE;
							break;
						}
					}
					if(found) break;
					ULONG next = *(ULONG*)(q + OFF_NEXT);
					if(next == 0) break;
					q += next;
				}

				if(found && ownEntry)
				{
					DWORD myPid = GetCurrentProcessId();
					for(ULONG off = 0x000; off < OFF_READ_XFER; off += 4)
					{
						if(*(DWORD*)(ownEntry + off) == myPid)
						{
							OFF_PID = off;
							break;
						}
					}
				}
			}

			_DbgIoLog("CALIBRATE: gotReal=%d found=%d realR=%llu realW=%llu realO=%llu -> "
				"OFF_PID=0x%lX OFF_READ=0x%lX OFF_WRITE=0x%lX OFF_OTHER=0x%lX",
				gotReal, found, realR, realW, realO,
				OFF_PID, OFF_READ_XFER, OFF_WRITE_XFER, OFF_OTHER_XFER);
		}

		LONGLONG sysRead  = 0;
		LONGLONG sysWrite = 0;
		LONGLONG sysOther = 0;

		DWORD myPid = GetCurrentProcessId();
		BOOL foundMyPid = FALSE;
		{
			BYTE* q = buf;
			for(ULONG iters = 0; iters < 2048; iters++)
			{
				if(*(DWORD*)(q + OFF_PID) == myPid)
				{
					foundMyPid = TRUE;
					break;
				}
				ULONG next = *(ULONG*)(q + OFF_NEXT);
				if(next == 0) break;
				q += next;
			}
		}
		static LONG s_offsetWarned = 0;
		if(!foundMyPid && InterlockedCompareExchange(&s_offsetWarned, 1, 0) == 0)
		{
			_DbgIoLog("WARNING: OFF_PID=0x%lX is WRONG for this OS! Could not find own PID=%lu anywhere in "
				"the SPI list. Per-process cache will contain garbage. arch=%s",
				OFF_PID, myPid,
#ifdef _M_X64
				"x64"
#else
				"x86"
#endif
				);
		}

		PerfPidIoCache_BeginUpdate();
		PerfPidIoCache_Clear();
		int slot = 0;
		BYTE* p = buf;
		for(;;)
		{
			DWORD pid = *(DWORD*)(p + OFF_PID);
			if(pid >= 1 && pid <= 0xFFFF)
			{
				LONGLONG r = *(LONGLONG*)(p + OFF_READ_XFER);
				LONGLONG w = *(LONGLONG*)(p + OFF_WRITE_XFER);
				LONGLONG o = *(LONGLONG*)(p + OFF_OTHER_XFER);
				sysRead  += r;
				sysWrite += w;
				sysOther += o;
				PerfPidIoCache_SetSlot(slot++, pid, (ULONGLONG)r, (ULONGLONG)w, (ULONGLONG)o);
			}
			ULONG next = *(ULONG*)(p + OFF_NEXT);
			if(next == 0) break;
			p += next;
		}
		PerfPidIoCache_EndUpdate();
		PerfPidIoCache_MarkReady();

		g_DbgWriterSlots = slot;
		InterlockedIncrement((LONG*)&g_DbgWriterTicks);

		_DbgIoLog("NtSys tick: procs=%d sysRead=%lld sysWrite=%lld busyPct=%.1f",
			slot, (long long)sysRead, (long long)sysWrite, s_TotalPct);

		if(!havePrev)
		{
			prevSystemRead  = sysRead;
			prevSystemWrite = sysWrite;
			havePrev = TRUE;
			continue;
		}

		LONGLONG dr = sysRead  - prevSystemRead;
		LONGLONG dw = sysWrite - prevSystemWrite;
		if(dr < 0) dr = 0;
		if(dw < 0) dw = 0;

		LONGLONG bytesThis = dr + dw;
		bytesSum -= bytesBuf[bytesIdx];
		bytesBuf[bytesIdx] = bytesThis;
		bytesSum += bytesThis;
		bytesIdx = (bytesIdx + 1) % 100;
		if(bytesSum < 0) bytesSum = 0;

		s_ReadBps  = (double)dr * 10.0;
		s_WriteBps = (double)dw * 10.0;
		double avgBps      = (double)bytesSum / 10.0;
		const double REF_BPS = 5.0 * 1024.0 * 1024.0;
		double pct          = (avgBps / REF_BPS) * 100.0;
		if(pct <   0.0) pct =   0.0;
		if(pct > 100.0) pct = 100.0;
		if(!_finite(pct)) pct = 0.0;
		s_TotalPct = pct;

		prevSystemRead  = sysRead;
		prevSystemWrite = sysWrite;
		s_Ready         = 1;

		static LONG dbgTickSeq = 0;
		LONG seq = InterlockedIncrement(&dbgTickSeq);
		if((seq % 10) == 0)
		{
			_DbgIoLog("STATUS: writerTicks=%ld cacheSlots=%ld lookupCalls=%ld hits=%ld misses=%ld ready=%ld",
				g_DbgWriterTicks,
				g_DbgWriterSlots,
				g_DbgLookupCalls, g_DbgLookupHits, g_DbgLookupMisses,
				g_DbgReadyFlag);
		}
	}

	if(buf) free(buf);
	return 0;
}

void PerfNtSys_Start(void)
{
	if(s_hThread != NULL) return;
	InterlockedExchange(&s_Stop, 0);
	s_hThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorNtSysDisk, NULL, 0, NULL);
}

void PerfNtSys_Stop(void)
{
	if(s_hThread == NULL) return;
	InterlockedExchange(&s_Stop, 1);
	WaitForSingleObject(s_hThread, 3000);
	CloseHandle(s_hThread);
	s_hThread = NULL;
}

double PerfNtSys_GetReadBps(void)  { return s_ReadBps;  }
double PerfNtSys_GetWriteBps(void) { return s_WriteBps; }
double PerfNtSys_GetTotalPct(void) { return s_TotalPct; }
LONG   PerfNtSys_GetReady(void)    { return s_Ready;    }
