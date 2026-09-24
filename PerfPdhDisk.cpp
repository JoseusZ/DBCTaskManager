// PerfPdhDisk.cpp
#include "stdafx.h"
#include "PerfPdhDisk.h"
#include "PerfDbgLog.h"

#include <pdh.h>
#pragma comment(lib, "pdh.lib")

#include <stdlib.h>

#define PDH_DISK_MAX PERF_DISK_MAX

static PdhDiskMetrics s_PdhDisk[PDH_DISK_MAX];
static double         s_TotalPct        = 0.0;
static double         s_TotalReadBps    = 0.0;
static double         s_TotalWriteBps   = 0.0;
static LONG           s_TotalReady      = 0;
static LONG           s_TotalEverNonZero= 0;
static HANDLE         s_hThread         = NULL;
static volatile LONG  s_Stop            = 0;

static BOOL _InitPdhDiskQuery(HQUERY* hQuery,
							  HCOUNTER* pctCtr, HCOUNTER* readCtr, HCOUNTER* writeCtr,
							  WCHAR instNames[PDH_DISK_MAX][MAX_PATH],
							  int* numInst)
{
	*hQuery = NULL;
	*numInst = 0;
	memset(pctCtr,   0, sizeof(HCOUNTER) * PDH_DISK_MAX);
	memset(readCtr,  0, sizeof(HCOUNTER) * PDH_DISK_MAX);
	memset(writeCtr, 0, sizeof(HCOUNTER) * PDH_DISK_MAX);
	memset(instNames, 0, sizeof(WCHAR) * MAX_PATH * PDH_DISK_MAX);

	if(PdhOpenQuery(NULL, 0, hQuery) != ERROR_SUCCESS || !*hQuery)
		return FALSE;

	// Always try the hardcoded _Total path first. This is the same path
	// Windows Task Manager uses for its "Disk" header.
	if(*numInst < PDH_DISK_MAX)
	{
		int slot = *numInst;
		wcsncpy_s(instNames[slot], MAX_PATH, L"_Total", _TRUNCATE);
		WCHAR path[MAX_PATH];
		PDH_STATUS s;
		swprintf_s(path, MAX_PATH, L"\\PhysicalDisk(_Total)\\%% Disk Time");
		s = PdhAddCounterW(*hQuery, path, 0, &pctCtr[slot]);
		swprintf_s(path, MAX_PATH, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec");
		PdhAddCounterW(*hQuery, path, 0, &readCtr[slot]);
		swprintf_s(path, MAX_PATH, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec");
		PdhAddCounterW(*hQuery, path, 0, &writeCtr[slot]);
		if(s == ERROR_SUCCESS && pctCtr[slot] != NULL)
			(*numInst)++;
	}

	DWORD cbListSize = 0;
	DWORD detailLevel = PERF_DETAIL_STANDARD;
	PdhEnumObjectItemsW(NULL, NULL, L"PhysicalDisk",
						NULL, &cbListSize,
						NULL, NULL,
						detailLevel, 0);
	if(cbListSize == 0)
	{
		detailLevel = PERF_DETAIL_WIZARD;
		PdhEnumObjectItemsW(NULL, NULL, L"PhysicalDisk",
							NULL, &cbListSize,
							NULL, NULL,
							detailLevel, 0);
	}
	if(cbListSize > 0 && *numInst < PDH_DISK_MAX)
	{
		WCHAR* counterBuf = (WCHAR*)malloc(cbListSize * sizeof(WCHAR));
		WCHAR* instBuf    = (WCHAR*)malloc(cbListSize * sizeof(WCHAR));
		if(counterBuf && instBuf)
		{
			counterBuf[0] = 0;
			instBuf[0]    = 0;
			DWORD instBufLen = cbListSize;
			if(PdhEnumObjectItemsW(NULL, NULL, L"PhysicalDisk",
								   counterBuf, &cbListSize,
								   instBuf, &instBufLen,
								   detailLevel, 0) == ERROR_SUCCESS)
			{
				WCHAR* p = instBuf;
				while(*p && *numInst < PDH_DISK_MAX)
				{
					if(_wcsicmp(p, L"_Total") == 0)
					{
						p += wcslen(p) + 1;
						continue;
					}
					int slot = *numInst;
					wcsncpy_s(instNames[slot], MAX_PATH, p, _TRUNCATE);
					WCHAR path[MAX_PATH];
					swprintf_s(path, MAX_PATH, L"\\PhysicalDisk(%s)\\%% Disk Time",   instNames[slot]);
					PdhAddCounterW(*hQuery, path, 0, &pctCtr[slot]);
					swprintf_s(path, MAX_PATH, L"\\PhysicalDisk(%s)\\Disk Read Bytes/sec",  instNames[slot]);
					PdhAddCounterW(*hQuery, path, 0, &readCtr[slot]);
					swprintf_s(path, MAX_PATH, L"\\PhysicalDisk(%s)\\Disk Write Bytes/sec", instNames[slot]);
					PdhAddCounterW(*hQuery, path, 0, &writeCtr[slot]);
					(*numInst)++;
					p += wcslen(p) + 1;
				}
			}
		}
		if(counterBuf) free(counterBuf);
		if(instBuf)    free(instBuf);
	}

	PdhCollectQueryData(*hQuery);

	if(*numInst == 0)
	{
		PdhCloseQuery(*hQuery);
		*hQuery = NULL;
		return FALSE;
	}

	{
		char ascii[2048] = "PDH-DISK init: numInst=";
		char numBuf[16];
		sprintf_s(numBuf, sizeof(numBuf), "%d", *numInst);
		strcat_s(ascii, sizeof(ascii), numBuf);
		strcat_s(ascii, sizeof(ascii), " [");
		for(int i = 0; i < *numInst; i++)
		{
			if(i > 0) strcat_s(ascii, sizeof(ascii), ",");
			strcat_s(ascii, sizeof(ascii), "\"");
			const WCHAR* w = instNames[i];
			for(; *w && (strlen(ascii) + 8) < sizeof(ascii); w++)
			{
				char c = (char)(*w & 0x7F);
				if(*w >= 0x20 && *w <= 0x7E && c == *w)
				{
					char tmp[2] = { c, 0 };
					strcat_s(ascii, sizeof(ascii), tmp);
				}
				else
				{
					char tmp[16];
					sprintf_s(tmp, sizeof(tmp), "\\u%04x", (unsigned)*w);
					strcat_s(ascii, sizeof(ascii), tmp);
				}
			}
			strcat_s(ascii, sizeof(ascii), "\"");
			if(pctCtr[i] == NULL) strcat_s(ascii, sizeof(ascii), ":NO_PCT");
		}
		strcat_s(ascii, sizeof(ascii), "]");
		_DbgIoLog("%s", ascii);
	}
	return TRUE;
}

static unsigned __stdcall Thread_MonitorPdhDisk(void*)
{
	HQUERY     hQuery      = NULL;
	HCOUNTER   pctCtr[PDH_DISK_MAX];
	HCOUNTER   readCtr[PDH_DISK_MAX];
	HCOUNTER   writeCtr[PDH_DISK_MAX];
	WCHAR      instNames[PDH_DISK_MAX][MAX_PATH];
	int        numInst     = 0;
	int        idxMap[PDH_DISK_MAX];

	memset(idxMap, -1, sizeof(idxMap));

	for(int attempt = 0; attempt < 10 && s_Stop == 0; attempt++)
	{
		if(_InitPdhDiskQuery(&hQuery, pctCtr, readCtr, writeCtr, instNames, &numInst))
			break;
		Sleep(500);
	}
	if(!hQuery || numInst == 0)
		return 0;

	for(int i = 0; i < numInst; i++)
	{
		if(_wcsicmp(instNames[i], L"_Total") == 0)
		{
			idxMap[i] = -2;
			continue;
		}
		LPWSTR p = instNames[i];
		while(*p == L' ' || *p == L'\t') p++;
		int n = -1;
		if(*p >= L'0' && *p <= L'9')
		{
			n = 0;
			while(*p >= L'0' && *p <= L'9')
			{
				n = n * 10 + (*p - L'0');
				p++;
			}
		}
		idxMap[i] = n;
	}

	PDH_FMT_COUNTERVALUE value;

	while(s_Stop == 0)
	{
		PdhCollectQueryData(hQuery);

		PdhDiskMetrics local[PDH_DISK_MAX];
		memset(local, 0, sizeof(local));
		double localTotalPct = 0.0, localTotalRead = 0.0, localTotalWrite = 0.0;
		BOOL  gotTotal = FALSE;

		for(int i = 0; i < numInst; i++)
		{
			int diskIdx = idxMap[i];
			if(diskIdx == -2)
			{
				if(pctCtr[i]   && PdhGetFormattedCounterValue(pctCtr[i],   PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
					localTotalPct   = value.doubleValue;
				if(readCtr[i]  && PdhGetFormattedCounterValue(readCtr[i],  PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
					localTotalRead  = value.doubleValue;
				if(writeCtr[i] && PdhGetFormattedCounterValue(writeCtr[i], PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
					localTotalWrite = value.doubleValue;
				gotTotal = TRUE;
			}
			else if(diskIdx >= 0 && diskIdx < PDH_DISK_MAX)
			{
				if(pctCtr[i] && PdhGetFormattedCounterValue(pctCtr[i], PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
				{
					double v = value.doubleValue;
					if(v < 0)        v = 0;
					if(v > 100.0)    v = 100;
					local[diskIdx].Pct = v;
				}
				if(readCtr[i]  && PdhGetFormattedCounterValue(readCtr[i],  PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
					local[diskIdx].ReadBps  = value.doubleValue;
				if(writeCtr[i] && PdhGetFormattedCounterValue(writeCtr[i], PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
					local[diskIdx].WriteBps = value.doubleValue;
				local[diskIdx].Ready = 1;
				if(local[diskIdx].Pct > 0.0
					|| local[diskIdx].ReadBps > 0.0
					|| local[diskIdx].WriteBps > 0.0)
				{
					local[diskIdx].EverNonZero = 1;
				}
			}
		}

		for(int i = 0; i < PDH_DISK_MAX; i++)
		{
			if(local[i].Ready)
			{
				s_PdhDisk[i].ReadBps  = local[i].ReadBps;
				s_PdhDisk[i].WriteBps = local[i].WriteBps;
				s_PdhDisk[i].Pct      = local[i].Pct;
				s_PdhDisk[i].Ready    = 1;
				if(local[i].EverNonZero)
				{
					InterlockedExchange(&s_PdhDisk[i].EverNonZero, 1);
					InterlockedExchange(&g_AnyPerDiskSourceEverLive, 1);
				}
			}
		}
		if(gotTotal)
		{
			s_TotalPct        = localTotalPct;
			s_TotalReadBps    = localTotalRead;
			s_TotalWriteBps   = localTotalWrite;
			s_TotalReady      = 1;
			if(localTotalPct > 0.0 || localTotalRead > 0.0 || localTotalWrite > 0.0)
				InterlockedExchange(&s_TotalEverNonZero, 1);
		}

		for(int i = 0; i < 10 && s_Stop == 0; i++) Sleep(100);
	}

	PdhCloseQuery(hQuery);
	return 0;
}

void PerfPdhDisk_Start(void)
{
	if(s_hThread != NULL) return;
	PerfDiskShared_Init();
	InterlockedExchange(&s_Stop, 0);
	s_hThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorPdhDisk, NULL, 0, NULL);
}

void PerfPdhDisk_Stop(void)
{
	if(s_hThread == NULL) return;
	InterlockedExchange(&s_Stop, 1);
	WaitForSingleObject(s_hThread, 3000);
	CloseHandle(s_hThread);
	s_hThread = NULL;
}

BOOL PerfPdhDisk_GetPerDisk(int idx, PdhDiskMetrics* out)
{
	if(idx < 0 || idx >= PDH_DISK_MAX || out == NULL) return FALSE;
	*out = s_PdhDisk[idx];
	return TRUE;
}

double PerfPdhDisk_GetTotalPct(void)         { return s_TotalPct;         }
double PerfPdhDisk_GetTotalReadBps(void)     { return s_TotalReadBps;     }
double PerfPdhDisk_GetTotalWriteBps(void)    { return s_TotalWriteBps;    }
LONG   PerfPdhDisk_GetTotalReady(void)       { return s_TotalReady;       }
LONG   PerfPdhDisk_GetTotalEverNonZero(void) { return s_TotalEverNonZero; }
