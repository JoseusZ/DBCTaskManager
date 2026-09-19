// PerformanceView.cpp : implementation file
//

#include "stdafx.h"
#include "DBCTaskman.h"
#include "PerformanceBox.h"
#include "MemCompBox.h"
#include "SMBIOS.h"
#include <intrin.h>      // __rdtsc
#include <process.h>     // _beginthreadex


extern "C" {
#include <powrprof.h>
}
#pragma comment(lib , "PowrProf.lib") // 




#include <atlbase.h>

// #include <Iphlpapi.h>
#pragma comment(lib , "Iphlpapi.lib") //��������

// WLAN API (Win7+) — provides PHY type (802.11ac/n/...), SSID, signal quality.
#include <wlanapi.h>
#include <rpc.h>
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "rpcrt4.lib")

// PDH (Performance Data Helper) — fallback disk activity source for Win7
// systems where the WMI perf counter provider (wmiapsrv) is unavailable.
// This is the same native API Windows Task Manager uses to read disk
// counters; it requires no service, no admin, and works on every Win7
// build regardless of UI language. Included here (not in stdafx.h)
// because <pdh.h> declares a typedef HLOG that clashes with the HLOG
// typedef from <lmerrlog.h> which stdafx transitively pulls in.
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
// CPerformanceBox

static  CPerformanceBox* pThisBoxView;

static   BOOL FlagStartDiskMon = FALSE;


typedef struct _PROCESSOR_POWER_INFORMATION {
	ULONG  Number;
	ULONG  MaxMhz;
	ULONG  CurrentMhz;
	ULONG  MhzLimit;
	ULONG  MaxIdleState;
	ULONG  CurrentIdleState;
} PROCESSOR_POWER_INFORMATION , *PPROCESSOR_POWER_INFORMATION;

// ----------------------------------------------------------------------------
// CPU actual-speed monitor
// ----------------------------------------------------------------------------
// On Win7 + modern CPUs (Ryzen, Intel K-series, etc.) with no P-state driver,
// CallNtPowerInformation(ProcessorInformation) returns MaxMhz as CurrentMhz,
// so we never see the real turbo / boost frequency the way Win10 task manager
// does. To get the *actual* current frequency we measure it ourselves with
// RDTSC + QueryPerformanceCounter on a dedicated background thread. This
// detects turbo boost AND throttling regardless of whether the OS exposes
// P-state info. TSC is invariant on every CPU we care about (any AMD since
// K10, any Intel since Nehalem), so a delta over time yields true MHz.
static volatile LONG  g_CpuSpeedMhz = 0;     // last measured MHz (0 = not ready)
static volatile LONG  g_CpuSpeedReady = 0;   // 1 once first sample is valid
static HANDLE         g_hCpuSpeedThread = NULL;
static volatile LONG  g_CpuSpeedStop = 0;    // 1 = ask worker to exit

static UINT __stdcall Thread_MonitorCpuSpeed(LPVOID lparam)
{
	(void)lparam;

	// Pin the measurement thread to a single logical processor so TSC delta is
	// not perturbed by migration between cores that may not have a perfectly
	// synchronized TSC on older systems. On modern CPUs TSC is invariant and
	// synchronized, so this is a harmless extra safety.
	HANDLE hThread = GetCurrentThread();
	DWORD_PTR affinityMask = 1;
	if(theApp.PerformanceInfo.nLogicalProcessor > 0)
		affinityMask = (DWORD_PTR)1;
	SetThreadAffinityMask(hThread, affinityMask);

	LARGE_INTEGER qpcFreq;
	if(!QueryPerformanceFrequency(&qpcFreq) || qpcFreq.QuadPart <= 0)
		return 0;

	// First sample baseline (no timing yet, just seed).
	LARGE_INTEGER startQpc; QueryPerformanceCounter(&startQpc);
	unsigned __int64 startTsc = __rdtsc();

	const DWORD SampleMs = 250; // 4 Hz is plenty for a UI display
	while(g_CpuSpeedStop == 0)
	{
		Sleep(SampleMs);

		LARGE_INTEGER endQpc; QueryPerformanceCounter(&endQpc);
		unsigned __int64 endTsc = __rdtsc();

		LONGLONG deltaQpc = endQpc.QuadPart - startQpc.QuadPart;
		unsigned __int64 deltaTsc = endTsc - startTsc;

		if(deltaQpc <= 0 || deltaTsc == 0)
			continue;

		double seconds = (double)deltaQpc / (double)qpcFreq.QuadPart;
		if(seconds <= 0)
			continue;

		double mhz = (double)deltaTsc / (seconds * 1.0e6);

		// Sanity: reject obviously bad samples (sleep was interrupted, etc).
		if(_finite(mhz) == 0) continue;
		if(mhz < 100.0) continue;       // < 100 MHz = garbage
		if(mhz > 20000.0) continue;    // > 20 GHz = garbage

		// Store as integer MHz via InterlockedExchange so the read on the UI
		// thread sees a consistent value.
		LONG mhzInt = (LONG)(mhz + 0.5);
		InterlockedExchange(&g_CpuSpeedMhz, mhzInt);
		InterlockedExchange(&g_CpuSpeedReady, 1);

		// Re-seed for the next window so we always use the most recent interval.
		startQpc = endQpc;
		startTsc = endTsc;
	}
	return 0;
}

static void EnsureCpuSpeedMonitor(void)
{
	if(g_hCpuSpeedThread != NULL) return;
	InterlockedExchange(&g_CpuSpeedStop, 0);
	g_hCpuSpeedThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorCpuSpeed, NULL, 0, NULL);
}

// ----------------------------------------------------------------------------
// WMI-based CPU speed monitor (works on Win10 + AMD Zen where TSC is constant)
// ----------------------------------------------------------------------------
// On AMD Zen CPUs (Ryzen 1000/2000/3000/5000 series) the TSC ticks at the
// *rated* frequency, NOT at the actual core clock, so RDTSC cannot see turbo.
// Win10 task manager reflects turbo because it queries
// `Win32_Processor.CurrentClockSpeed` via WMI, which under Win10's CPPC
// (Collaborative Processor Performance Control) driver returns the *current*
// P-state including boost. We poll it on a background thread every 2 seconds
// (WMI is too slow for per-tick) and prefer the cached result in
// _GetSurrentCpuSpeed.
static volatile LONG  g_WmiCpuMhz = 0;
static volatile LONG  g_WmiCpuReady = 0;
static HANDLE         g_hWmiCpuThread = NULL;
static volatile LONG  g_WmiCpuStop = 0;

static UINT __stdcall Thread_MonitorWmiCpuSpeed(LPVOID lparam)
{
	(void)lparam;

	HRESULT hrCom = CoInitializeEx(0, COINIT_MULTITHREADED);
	BOOL ownCom = SUCCEEDED(hrCom);

	while(g_WmiCpuStop == 0)
	{
		LONG mhz = 0;

		IWbemLocator *pLoc = NULL;
		HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
			IID_IWbemLocator, (LPVOID *)&pLoc);
		if(SUCCEEDED(hr) && pLoc)
		{
			IWbemServices *pSvc = NULL;
			hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0,
				NULL, 0, 0, &pSvc);
			if(SUCCEEDED(hr) && pSvc)
			{
				hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT,
					RPC_C_AUTHZ_NONE, NULL,
					RPC_C_AUTHN_LEVEL_CALL,
					RPC_C_IMP_LEVEL_IMPERSONATE,
					NULL, EOAC_NONE);
				if(SUCCEEDED(hr))
				{
					IEnumWbemClassObject *pEnum = NULL;
					BSTR qLang = SysAllocString(L"WQL");
					BSTR qText = SysAllocString(L"SELECT CurrentClockSpeed FROM Win32_Processor");
					hr = pSvc->ExecQuery(qLang, qText,
						WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
						NULL, &pEnum);
					if(SUCCEEDED(hr) && pEnum)
					{
						IWbemClassObject *pObj = NULL;
						ULONG uRet = 0;
						// Read the first processor entry - we only need one value
						// (CurrentClockSpeed is reported per package and applies
						// to all logical cores on a typical homogeneous setup).
						HRESULT hrNext = pEnum->Next(5000, 1, &pObj, &uRet);
						if(SUCCEEDED(hrNext) && uRet > 0 && pObj)
						{
							VARIANT vt;
							VariantInit(&vt);
							HRESULT hrGet = pObj->Get(L"CurrentClockSpeed", 0, &vt, 0, 0);
							if(SUCCEEDED(hrGet) && V_VT(&vt) == VT_UI4)
							{
								mhz = (LONG)V_UI4(&vt);
							}
							else if(SUCCEEDED(hrGet) && V_VT(&vt) == VT_I4)
							{
								mhz = (LONG)V_I4(&vt);
							}
							else if(SUCCEEDED(hrGet) && V_VT(&vt) == VT_R4)
							{
								mhz = (LONG)(V_R4(&vt) + 0.5f);
							}
							else if(SUCCEEDED(hrGet) && V_VT(&vt) == VT_R8)
							{
								mhz = (LONG)(V_R8(&vt) + 0.5);
							}
							VariantClear(&vt);
							pObj->Release();
						}
						pEnum->Release();
					}
					SysFreeString(qText);
					SysFreeString(qLang);
				}
				pSvc->Release();
			}
			pLoc->Release();
		}

		if(mhz > 50 && mhz < 100000)
		{
			InterlockedExchange(&g_WmiCpuMhz, mhz);
			InterlockedExchange(&g_WmiCpuReady, 1);
		}

		// Sleep ~2 s in small slices so the stop flag is responsive.
		for(int i = 0; i < 20 && g_WmiCpuStop == 0; i++) Sleep(100);
	}

	if(ownCom) CoUninitialize();
	return 0;
}

static void EnsureWmiCpuSpeedMonitor(void)
{
	if(g_hWmiCpuThread != NULL) return;
	InterlockedExchange(&g_WmiCpuStop, 0);
	g_hWmiCpuThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorWmiCpuSpeed, NULL, 0, NULL);
}

// ----------------------------------------------------------------------------
// WMI-based disk metrics monitor (Win7+, no admin required)
// ----------------------------------------------------------------------------
// PDH `\PhysicalDisk(N)\*` counters were tried first but crashed on the
// user's Spanish Win7 VM with 0xc0000417 even with hardened NULL guards
// (likely a PDH/NtQuerySystemInformation buffer issue specific to that
// build). WMI works from a regular user session and does not crash. WMI is
// slow (~50-200 ms per query) so we poll on a background thread every
// 1 second and cache per-disk read bytes/sec, write bytes/sec, and percent
// activity. The UI tick reads from the cache.
#define WMI_DISK_MAX 32
struct WmiDiskMetrics
{
	double ReadBps;
	double WriteBps;
	double Pct;       // 0..100, percent disk time
	LONG   Ready;     // 1 once first poll completes
};
static WmiDiskMetrics g_WmiDisk[WMI_DISK_MAX];
// Aggregate ("_Total") instance from Win32_PerfRawData_PerfDisk_PhysicalDisk.
// This is what Windows Task Manager shows as the disk header %. Using it
// instead of the average of per-disk instances avoids masking activity on a
// single busy disk by an idle one.
static double         g_WmiDiskTotalPct  = 0.0;
static LONG           g_WmiDiskTotalReady = 0;
static HANDLE         g_hWmiDiskThread  = NULL;
static volatile LONG  g_WmiDiskStop     = 0;

// Unpack a PercentDiskTime / PercentDiskReadTime / PercentDiskWriteTime value
// returned by WMI. Win32_PerfRawData_PerfDisk_PhysicalDisk uses counter type
// PERF_100NSEC_TIMER (raw fraction): the WMI refresher returns a uint64 with
// the sample time (denominator) in the high DWORD and the busy time
// (numerator) in the low DWORD. Some WMI implementations / cooked classes
// return the value as a plain 0..100 percentage instead — handle both.
// Returns TRUE and sets *outPct on success; FALSE (with *outPct=0) on garbage.
static BOOL _UnpackDiskPercentVariant(const VARIANT& vt, double* outPct)
{
	*outPct = 0.0;
	if(outPct == NULL) return FALSE;

	VARTYPE vtT = V_VT(&vt);
	if(vtT == VT_UI8 || vtT == VT_I8)
	{
		ULONGLONG raw = (vtT == VT_UI8) ? V_UI8(&vt) : (ULONGLONG)V_I8(&vt);
		ULONG busy  = (ULONG)(raw & 0xFFFFFFFFul);          // low  DWORD = numerator
		ULONG total = (ULONG)((raw >> 32) & 0xFFFFFFFFul); // high DWORD = denominator

		if(total > 0 && busy <= total)
		{
			double p = (double)busy * 100.0 / (double)total;
			if(_finite(p) != 0 && p >= 0.0 && p <= 100.0)
			{
				*outPct = p;
				return TRUE;
			}
		}
		else if(total == 0 && busy <= 100ul)
		{
			// Counter hasn't ticked yet AND some Win7 builds expose the value
			// as a flat percentage 0..100 in this branch.
			*outPct = (double)busy;
			return TRUE;
		}
		// else: garbage / counter not yet initialized -> 0
		return FALSE;
	}
	else if(vtT == VT_R8)
	{
		double d = V_R8(&vt);
		if(_finite(d) != 0 && d >= 0.0 && d <= 100.0)
		{
			*outPct = d;
			return TRUE;
		}
	}
	else if(vtT == VT_R4)
	{
		float f = V_R4(&vt);
		if(_finite(f) != 0 && f >= 0.0f && f <= 100.0f)
		{
			*outPct = (double)f;
			return TRUE;
		}
	}
	else if(vtT == VT_I4 || vtT == VT_UI4)
	{
		ULONG v = (vtT == VT_UI4) ? V_UI4(&vt) : (ULONG)V_I4(&vt);
		if(v <= 100ul)
		{
			*outPct = (double)v;
			return TRUE;
		}
	}
	// VT_NULL / VT_EMPTY / unsupported: counter not available.
	return FALSE;
}

static double _WmiVariantToDouble(const VARIANT& vt)
{
	switch(V_VT(&vt))
	{
		case VT_UI4: return (double)V_UI4(&vt);
		case VT_UI4 | VT_ARRAY: return 0;
		case VT_I4:  return (double)V_I4(&vt);
		case VT_I8:  return (double)V_I8(&vt);
		case VT_UI8: return (double)V_UI8(&vt);
		case VT_R4:  return (double)V_R4(&vt);
		case VT_R8:  return (double)V_R8(&vt);
		default:     return 0;
	}
}

static UINT __stdcall Thread_MonitorWmiDisk(LPVOID lparam)
{
	(void)lparam;

	HRESULT hrCom = CoInitializeEx(0, COINIT_MULTITHREADED);
	BOOL ownCom = SUCCEEDED(hrCom);

	while(g_WmiDiskStop == 0)
	{
		WmiDiskMetrics local[WMI_DISK_MAX];
		memset(local, 0, sizeof(local));
		double localTotalPct    = 0.0;
		LONG   localTotalReady  = 0;

		IWbemLocator *pLoc = NULL;
		HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
			IID_IWbemLocator, (LPVOID *)&pLoc);
		if(SUCCEEDED(hr) && pLoc)
		{
			IWbemServices *pSvc = NULL;
			hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0,
				NULL, 0, 0, &pSvc);
			if(SUCCEEDED(hr) && pSvc)
			{
				hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT,
					RPC_C_AUTHZ_NONE, NULL,
					RPC_C_AUTHN_LEVEL_CALL,
					RPC_C_IMP_LEVEL_IMPERSONATE,
					NULL, EOAC_NONE);
				if(SUCCEEDED(hr))
				{
					IEnumWbemClassObject *pEnum = NULL;
					BSTR qLang = SysAllocString(L"WQL");
					BSTR qText = SysAllocString(
						L"SELECT Name, DiskReadBytesPerSec, DiskWriteBytesPerSec, "
						L"PercentDiskTime FROM Win32_PerfRawData_PerfDisk_PhysicalDisk");
					hr = pSvc->ExecQuery(qLang, qText,
						WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
						NULL, &pEnum);
					if(SUCCEEDED(hr) && pEnum)
					{
						IWbemClassObject *pObj = NULL;
						ULONG uRet = 0;
						// First call on Win7 can be slow while the perf counter
						// refresher warms up; subsequent calls are fast. Give a
						// generous timeout so we don't lose the first sample.
						while(pEnum->Next(15000, 1, &pObj, &uRet) == S_OK && uRet > 0 && pObj)
						{
							VARIANT vtName, vtRead, vtWrite, vtPct;
							VariantInit(&vtName); VariantInit(&vtRead);
							VariantInit(&vtWrite); VariantInit(&vtPct);

							int diskIdx = -1;
							BOOL isTotal = FALSE;
							if(SUCCEEDED(pObj->Get(L"Name", 0, &vtName, 0, 0)) &&
							   V_VT(&vtName) == VT_BSTR && V_BSTR(&vtName) != NULL)
							{
								// Name is one of:
								//   "0 C:" / "1 D:" / ...   -> per-disk instance (numeric prefix)
								//   "_Total"                -> aggregate instance (no digits)
								// Parse leading digits as the disk index. The "_Total"
								// instance is captured separately so we can use the
								// real aggregate for the header % (matches what Windows
								// Task Manager shows).
								LPCWSTR p = V_BSTR(&vtName);
								while(*p == L' ' || *p == L'\t') p++;
								int n = -1;
								while(*p >= L'0' && *p <= L'9')
								{
									if(n < 0) n = 0;
									n = n * 10 + (*p - L'0');
									p++;
								}
								if(n >= 0 && n < WMI_DISK_MAX)
								{
									diskIdx = n;
								}
								else if(p[0] == L'_' &&
										(p[1] == L'T' || p[1] == L't') &&
										(p[2] == L'o' || p[2] == L'O'))
								{
									// "_Total" instance — used for the aggregate header.
									isTotal = TRUE;
								}
								// Anything else (no digits, not _Total): skip.
							}

							if(diskIdx >= 0 || isTotal)
							{
								double pctResult = 0.0;
								if(SUCCEEDED(pObj->Get(L"PercentDiskTime", 0, &vtPct, 0, 0)))
								{
									// _UnpackDiskPercentVariant handles both the raw
									// packed fraction layout (PERF_100NSEC_TIMER) and
									// the cooked 0..100 layout some Win7 builds expose.
									(void)_UnpackDiskPercentVariant(vtPct, &pctResult);
								}

								if(isTotal)
								{
									// Aggregate (_Total) instance — used directly by the
									// header % so it matches what Windows Task Manager
									// shows instead of an average across physical disks.
									localTotalPct    = pctResult;
									localTotalReady  = 1;
								}
								else
								{
									if(SUCCEEDED(pObj->Get(L"DiskReadBytesPerSec",  0, &vtRead,  0, 0)))
										local[diskIdx].ReadBps  = _WmiVariantToDouble(vtRead);
									if(SUCCEEDED(pObj->Get(L"DiskWriteBytesPerSec", 0, &vtWrite, 0, 0)))
										local[diskIdx].WriteBps = _WmiVariantToDouble(vtWrite);
									local[diskIdx].Pct   = pctResult;
									local[diskIdx].Ready = 1;
								}
							}

							VariantClear(&vtName); VariantClear(&vtRead);
							VariantClear(&vtWrite); VariantClear(&vtPct);
							pObj->Release();
						}
						pEnum->Release();
					}
					SysFreeString(qText);
					SysFreeString(qLang);
				}
				pSvc->Release();
			}
			pLoc->Release();
		}

		// Commit to the global cache.
		// IMPORTANT: only overwrite entries that we actually saw this poll.
		// The previous implementation blindly copied every local[] slot into
		// g_WmiDisk[], which meant that any single failed WMI poll would
		// wipe Ready=0 over slots that previously had valid data, leaving
		// the disk stuck at 0% in both the graph and the list. Keep stale
		// entries intact until a fresh sample for that disk arrives.
		for(int i = 0; i < WMI_DISK_MAX; i++)
		{
			if(local[i].Ready)
			{
				g_WmiDisk[i].ReadBps  = local[i].ReadBps;
				g_WmiDisk[i].WriteBps = local[i].WriteBps;
				g_WmiDisk[i].Pct      = local[i].Pct;
				g_WmiDisk[i].Ready    = 1;
			}
		}

		// Commit the aggregate "_Total" instance (system-wide disk usage %).
		// Same staleness policy: only overwrite when we actually saw it this poll.
		if(localTotalReady)
		{
			g_WmiDiskTotalPct   = localTotalPct;
			g_WmiDiskTotalReady = 1;
		}

		// Sleep ~1 s in small slices so the stop flag is responsive.
		for(int i = 0; i < 10 && g_WmiDiskStop == 0; i++) Sleep(100);
	}

	if(ownCom) CoUninitialize();
	return 0;
}

static void EnsureWmiDiskMonitor(void)
{
	if(g_hWmiDiskThread != NULL) return;
	InterlockedExchange(&g_WmiDiskStop, 0);
	memset(g_WmiDisk, 0, sizeof(g_WmiDisk));
	g_hWmiDiskThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorWmiDisk, NULL, 0, NULL);
}

// ----------------------------------------------------------------------------
// PDH-based disk monitor (fallback when WMI's perf counter provider is
// unavailable on Win7, e.g. when the "WMI Performance Adapter" service
// (wmiapsrv) is disabled). PDH reads PhysicalDisk counters directly via
// the same native API the Windows Task Manager uses internally. It does
// NOT require any service, admin rights, or COM — so it works on every
// Win7 build regardless of configuration.
// ----------------------------------------------------------------------------
#define PDH_DISK_MAX 32
struct PdhDiskMetrics
{
	double ReadBps;
	double WriteBps;
	double Pct;       // 0..100, percent disk time
	LONG   Ready;     // 1 once first poll completes
};
static PdhDiskMetrics g_PdhDisk[PDH_DISK_MAX];
static double         g_PdhDiskTotalPct     = 0.0;
static double         g_PdhDiskTotalReadBps = 0.0;
static double         g_PdhDiskTotalWriteBps= 0.0;
static LONG           g_PdhDiskTotalReady   = 0;
static HANDLE         g_hPdhDiskThread      = NULL;
static volatile LONG  g_PdhDiskStop         = 0;

// Try to open a PDH query, enumerate PhysicalDisk instances, and add the
// three counters (% Disk Time, Disk Read Bytes/sec, Disk Write Bytes/sec)
// for every per-disk instance plus the _Total aggregate. Returns TRUE on
// success; *numInst is set to the number of instances seen (including
// _Total). _Total is always added as the last entry when present.
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

	// Try to add the _Total aggregate counter via a HARDCODED path first.
	// This works even when PdhEnumObjectItems returns 0 instances (e.g.
	// the PhysicalDisk perf counter isn't registered, only the default
	// counters are available). On every Win7 build that has the
	// PhysicalDisk perf object, this hardcoded path resolves correctly
	// without needing instance enumeration. This is the same path the
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
		// Only count the slot if % Disk Time was added successfully —
		// the other two always come along with it when the perf counter
		// is registered.
		if(s == ERROR_SUCCESS && pctCtr[slot] != NULL)
			(*numInst)++;
	}

	// Enumerate instances of the "PhysicalDisk" performance object for
	// per-disk metrics. The counter list returned is unused — we
	// hardcode the three counter names we care about (they're English
	// identifiers, not localized display names, so they work on every
	// Win7 language edition).
	//
	// Note: PdhEnumObjectItemsW signature on this SDK is
	//   (szDataSource, szMachineName, szObjectName,
	//    mszCounterList, pcchCounterListLength,
	//    mszInstanceList, pcchInstanceListLength,
	//    dwDetailLevel, dwFlags)
	// i.e. there's no pdwCounterCount parameter; pcchInstanceListLength
	// is the buffer size in characters (not the instance count).
	//
	// Use PERF_DETAIL_STANDARD so PhysicalDisk instances are included even
	// if NOVICE doesn't expose them on this Win7 build. We also try
	// WIZARD as a backup level below if STANDARD comes back empty.
	DWORD cbListSize = 0;
	DWORD detailLevel = PERF_DETAIL_STANDARD;
	PdhEnumObjectItemsW(NULL, NULL, L"PhysicalDisk",
						NULL, &cbListSize,
						NULL, NULL,
						detailLevel, 0);
	if(cbListSize == 0)
	{
		// Retry with WIZARD level in case STANDARD is filtered out.
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
				// Walk the double-null-terminated instance list. Add a
				// counter per non-aggregate instance; the _Total slot
				// was already populated above via the hardcoded path.
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

	// Prime the query with one collection so the first "real" collection
	// already has valid deltas (PDH counters are rate-based).
	PdhCollectQueryData(*hQuery);

	// We need at least the _Total slot to consider this a successful
	// initialization. If even the hardcoded _Total failed, bail out.
	if(*numInst == 0)
	{
		PdhCloseQuery(*hQuery);
		*hQuery = NULL;
		return FALSE;
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
	int        idxMap[PDH_DISK_MAX];  // -2 = _Total, -1 = unmapped, else disk index

	memset(idxMap, -1, sizeof(idxMap));

	// Retry initialization a few times — the perf counter service may not
	// have enumerated all disks on the very first call.
	for(int attempt = 0; attempt < 10 && g_PdhDiskStop == 0; attempt++)
	{
		if(_InitPdhDiskQuery(&hQuery, pctCtr, readCtr, writeCtr, instNames, &numInst))
			break;
		Sleep(500);
	}
	if(!hQuery || numInst == 0)
		return 0;

	// Map each instance name to a disk index by parsing the leading digits.
	// "0 C:" / "1 D:" -> 0 / 1. "_Total" -> -2 sentinel.
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

	while(g_PdhDiskStop == 0)
	{
		// Collect one sample (needs two collects before GetFormattedCounterValue
		// returns a real rate — the priming collect above already covered the
		// first one).
		PdhCollectQueryData(hQuery);

		PdhDiskMetrics local[PDH_DISK_MAX];
		memset(local, 0, sizeof(local));
		double localTotalPct = 0.0, localTotalRead = 0.0, localTotalWrite = 0.0;
		BOOL  gotTotal = FALSE;

		for(int i = 0; i < numInst; i++)
		{
			int diskIdx = idxMap[i];
			if(diskIdx == -2) // _Total
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
					if(v < 0)        v = 0;   // counter underflow (rollover)
					if(v > 100.0)    v = 100; // clamp
					local[diskIdx].Pct = v;
				}
				if(readCtr[i]  && PdhGetFormattedCounterValue(readCtr[i],  PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
					local[diskIdx].ReadBps  = value.doubleValue;
				if(writeCtr[i] && PdhGetFormattedCounterValue(writeCtr[i], PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
					local[diskIdx].WriteBps = value.doubleValue;
				local[diskIdx].Ready = 1;
			}
		}

		// Commit to globals (only overwrite slots we actually saw, so a
		// transient PDH hiccup can't wipe previously valid data).
		for(int i = 0; i < PDH_DISK_MAX; i++)
		{
			if(local[i].Ready)
			{
				g_PdhDisk[i].ReadBps  = local[i].ReadBps;
				g_PdhDisk[i].WriteBps = local[i].WriteBps;
				g_PdhDisk[i].Pct      = local[i].Pct;
				g_PdhDisk[i].Ready    = 1;
			}
		}
		if(gotTotal)
		{
			g_PdhDiskTotalPct      = localTotalPct;
			g_PdhDiskTotalReadBps  = localTotalRead;
			g_PdhDiskTotalWriteBps = localTotalWrite;
			g_PdhDiskTotalReady    = 1;
		}

		// Sleep ~1 s in 100 ms slices so the stop flag stays responsive.
		for(int i = 0; i < 10 && g_PdhDiskStop == 0; i++) Sleep(100);
	}

	PdhCloseQuery(hQuery);
	return 0;
}

static void EnsurePdhDiskMonitor(void)
{
	if(g_hPdhDiskThread != NULL) return;
	InterlockedExchange(&g_PdhDiskStop, 0);
	g_hPdhDiskThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorPdhDisk, NULL, 0, NULL);
}

static void StopPdhDiskMonitor(void)
{
	if(g_hPdhDiskThread == NULL) return;
	InterlockedExchange(&g_PdhDiskStop, 1);
	WaitForSingleObject(g_hPdhDiskThread, 3000);
	CloseHandle(g_hPdhDiskThread);
	g_hPdhDiskThread = NULL;
}

// ----------------------------------------------------------------------------
// NtQuerySystemInformation disk monitor — last-resort fallback that works on
// every Windows version (WinXP through Win11) without any service, perf
// counter registration, or admin rights. It reads the system-wide disk
// counters directly out of the kernel via ntdll.
//
// The kernel returns cumulative read/write byte counts since boot. We poll
// every 100 ms and compute (a) the current throughput in bytes/sec from
// deltas, and (b) an approximation of "% Disk Time" by tracking the fraction
// of polling windows where the disk was busy (i.e. the byte counts changed
// between consecutive samples). The approximation is close to what Windows
// Task Manager shows for the disk header on systems where the perf counter
// subsystem is broken (e.g. Win7 with wmiapsrv disabled, or perf counter
// DLLs unregistered). The throughput is exact.
// ----------------------------------------------------------------------------

// SYSTEM_PERFORMANCE_INFORMATION struct (a subset — the rest is reserved).
typedef struct _DBCTASK_PERF_INFO {
	LARGE_INTEGER IdleTime;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	ULONG ReadOperationCount;
	ULONG WriteOperationCount;
	ULONG OtherOperationCount;
	ULONG AvailablePages;
	ULONG TotalCommittedPages;
	ULONG TotalCommitLimit;
	ULONG PeakCommitment;
	ULONG PageFaultCount;
	ULONG CumulativeFreePages;
	ULONG CommitReuse;
} DBCTASK_PERF_INFO;

static double         g_NtSysReadBps    = 0.0;
static double         g_NtSysWriteBps   = 0.0;
static double         g_NtSysTotalPct   = 0.0;
static LONG           g_NtSysReady      = 0;
static HANDLE         g_hNtSysDiskThread= NULL;
static volatile LONG  g_NtSysDiskStop   = 0;

static unsigned __stdcall Thread_MonitorNtSysDisk(void*)
{
	typedef LONG (WINAPI *PFN_NtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
	HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
	if(!hNtdll) return 0;
	PFN_NtQuerySystemInformation pNtQuerySystemInformation =
		(PFN_NtQuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation");
	if(!pNtQuerySystemInformation) return 0;

	DBCTASK_PERF_INFO prev = {0};
	BOOL havePrev = FALSE;

	// Rolling 100-sample (10 s) busy-time buffer. Each sample is "1" if the
	// disk counters advanced during that 100 ms window, else "0". The header
	// % is the fraction of busy samples — the same definition of "busy time"
	// that the perf counter subsystem exposes via "\% Disk Time".
	BYTE busyBuf[100] = {0};
	int  busyIdx = 0;
	int  busySum = 0;

	// Single first read so we have a baseline before we start computing
	// deltas. We deliberately don't count this as a "busy" sample because
	// we don't know what the previous state was.
	DBCTASK_PERF_INFO info;
	ULONG bytesReturned = 0;
	if(pNtQuerySystemInformation(2, &info, sizeof(info), &bytesReturned) == 0 /* STATUS_SUCCESS */)
	{
		prev = info;
		havePrev = TRUE;
	}

	while(g_NtSysDiskStop == 0)
	{
		Sleep(100);
		if(pNtQuerySystemInformation(2, &info, sizeof(info), &bytesReturned) != 0)
			continue;
		if(!havePrev)
		{
			prev = info;
			havePrev = TRUE;
			continue;
		}

		LONGLONG dr = info.ReadTransferCount.QuadPart  - prev.ReadTransferCount.QuadPart;
		LONGLONG dw = info.WriteTransferCount.QuadPart - prev.WriteTransferCount.QuadPart;
		if(dr < 0) dr = 0;  // counter reset / overflow guard
		if(dw < 0) dw = 0;

		// Update rolling busy-time buffer.
		BOOL busy = (dr > 0 || dw > 0);
		busySum -= busyBuf[busyIdx];
		busyBuf[busyIdx] = busy ? 1 : 0;
		busySum += busyBuf[busyIdx];
		busyIdx = (busyIdx + 1) % 100;
		if(busySum < 0) busySum = 0;

		// Throughput (bytes/sec): scale the 100 ms sample to per-second.
		g_NtSysReadBps  = (double)dr * 10.0;
		g_NtSysWriteBps = (double)dw * 10.0;
		// % busy time over the rolling 10 s window.
		g_NtSysTotalPct  = (double)busySum;        // 0..100 already

		prev = info;
		g_NtSysReady = 1;
	}
	return 0;
}

static void EnsureNtSysDiskMonitor(void)
{
	if(g_hNtSysDiskThread != NULL) return;
	InterlockedExchange(&g_NtSysDiskStop, 0);
	g_hNtSysDiskThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorNtSysDisk, NULL, 0, NULL);
}

static void StopNtSysDiskMonitor(void)
{
	if(g_hNtSysDiskThread == NULL) return;
	InterlockedExchange(&g_NtSysDiskStop, 1);
	WaitForSingleObject(g_hNtSysDiskThread, 3000);
	CloseHandle(g_hNtSysDiskThread);
	g_hNtSysDiskThread = NULL;
}

// ----------------------------------------------------------------------------
// WLAN helpers (Win7+) — connection type (PHY), SSID, signal quality.
// Used to populate the Performance-tab adapter info box, matching the fields
// shown by Windows 10 native Task Manager.
// ----------------------------------------------------------------------------
static HANDLE _WlanOpenOrNull(void)
{
	HANDLE hClient = NULL;
	DWORD dwMaxClient = 2, dwCurVersion = 0;
	if(WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient) != ERROR_SUCCESS)
		return NULL;
	return hClient;
}

// Map a dot11_phy_type enum to a user-friendly Wi-Fi N label. Used as the
// connection-type value in the Performance-tab adapter info box.
static CString _PhyToLabel(DOT11_PHY_TYPE phy)
{
	switch(phy)
	{
		case dot11_phy_type_fhss:       return L"802.11 FHSS";
		case dot11_phy_type_dsss:       return L"802.11 DSSS";
		case dot11_phy_type_irbaseband: return L"802.11 IR";
		case dot11_phy_type_ofdm:       return L"802.11a";
		case dot11_phy_type_hrdsss:     return L"802.11b";
		case dot11_phy_type_erp:        return L"802.11g (Wi-Fi 3)";
		case dot11_phy_type_ht:         return L"802.11n (Wi-Fi 4)";
		case dot11_phy_type_vht:        return L"802.11ac (Wi-Fi 5)";
		case dot11_phy_type_dmg:        return L"802.11ad (Wi-Fi 5)";
		case dot11_phy_type_eht:        return L"802.11be (Wi-Fi 7)";
		default:
			// 802.11ax generic label - older SDKs may not have dot11_phy_type_he
			if((DWORD)phy == (DWORD)dot11_phy_type_he ||
			   (DWORD)phy == /*dot11_phy_type_he*/ 9)
				return L"802.11ax (Wi-Fi 6)";
			return L"802.11";
	}
}

// Returns the PHY type string for a wireless adapter (e.g. "802.11ac (Wi-Fi 5)").
// Falls back to "Wi-Fi (unspecified)" if the adapter is reachable but not
// currently associated, so the line never reads just "Wireless".
static CString _GetWlanPhyType(const GUID* pGuid)
{
	CString s = L"Wi-Fi (unspecified)";
	HANDLE h = _WlanOpenOrNull();
	if(h == NULL) return s;

	WLAN_CONNECTION_ATTRIBUTES* pConn = NULL;
	DWORD dwSize = 0;
	DWORD dw = WlanQueryInterface(h, pGuid, wlan_intf_opcode_current_connection,
								  NULL, &dwSize, (PVOID*)&pConn, NULL);
	if(dw == ERROR_SUCCESS && pConn != NULL)
	{
		if(pConn->isState == wlan_interface_state_connected)
			s = _PhyToLabel(pConn->wlanAssociationAttributes.dot11PhyType);
		else
			s = L"Not connected";
	}
	if(pConn) WlanFreeMemory(pConn);
	WlanCloseHandle(h, NULL);
	return s;
}

// Returns the current SSID for a wireless adapter as a UTF-8 → wide string.
// Returns "Disconnected" when the Wlan API works but the adapter has no
// association, so callers can tell apart "Wlan API failed" from
// "adapter is idle but reachable".
static CString _GetWlanSsid(const GUID* pGuid)
{
	CString s = L"Disconnected";
	HANDLE h = _WlanOpenOrNull();
	if(h == NULL) return L"";        // Wlan API unreachable at OS level

	WLAN_CONNECTION_ATTRIBUTES* pConn = NULL;
	DWORD dwSize = 0;
	DWORD dw = WlanQueryInterface(h, pGuid, wlan_intf_opcode_current_connection,
								  NULL, &dwSize, (PVOID*)&pConn, NULL);
	if(dw == ERROR_SUCCESS && pConn != NULL)
	{
		if(pConn->isState == wlan_interface_state_connected)
		{
			DOT11_SSID* ssid = &pConn->wlanAssociationAttributes.dot11Ssid;
			if(ssid->uSSIDLength > 0 && ssid->uSSIDLength <= DOT11_SSID_MAX_LENGTH)
			{
				s.Empty();
				int needed = MultiByteToWideChar(CP_UTF8, 0,
												 (LPCSTR)ssid->ucSSID,
												 ssid->uSSIDLength, NULL, 0);
				if(needed > 0)
				{
					wchar_t* buf = s.GetBuffer(needed + 1);
					MultiByteToWideChar(CP_UTF8, 0,
										(LPCSTR)ssid->ucSSID,
										ssid->uSSIDLength, buf, needed);
					buf[needed] = 0;
					s.ReleaseBuffer(needed);
				}
				if(s.IsEmpty()) s = L"Connected";
			}
		}
	}
	if(pConn) WlanFreeMemory(pConn);
	WlanCloseHandle(h, NULL);
	return s;
}

// Returns the signal quality in 0..100 for a wireless adapter.
// Returns 0 when not connected / query fails.
static ULONG _GetWlanSignalQuality(const GUID* pGuid)
{
	ULONG q = 0;
	HANDLE h = _WlanOpenOrNull();
	if(h == NULL) return q;

	WLAN_CONNECTION_ATTRIBUTES* pConn = NULL;
	DWORD dwSize = 0;
	DWORD dw = WlanQueryInterface(h, pGuid, wlan_intf_opcode_current_connection,
								  NULL, &dwSize, (PVOID*)&pConn, NULL);
	if(dw == ERROR_SUCCESS && pConn != NULL)
	{
		if(pConn->isState == wlan_interface_state_connected)
			q = pConn->wlanAssociationAttributes.wlanSignalQuality;
		if(q > 100) q = 100;
	}
	if(pConn) WlanFreeMemory(pConn);
	WlanCloseHandle(h, NULL);
	return q;
}

// TRUE if sa is an IPv6 link-local address (fe80::/10).
static BOOL _IsIPv6LinkLocal(const SOCKADDR* sa)
{
	if(sa == NULL || sa->sa_family != AF_INET6) return FALSE;
	const SOCKADDR_IN6* sa6 = (const SOCKADDR_IN6*)sa;
	// Bytes 0..9 must be FE 80 .. BF for fe80::/10. Byte[0]=0xFE, Byte[1] & 0xC0 == 0x80.
	return (sa6->sin6_addr.u.Byte[0] == 0xFE) &&
	       ((sa6->sin6_addr.u.Byte[1] & 0xC0) == 0x80);
}


UINT Thread_GetDiskOtherStaticInfo(LPVOID lparam)
{
	CPerformanceBox * pWnd =  (CPerformanceBox * )lparam;

	if(pWnd !=NULL)
	{
		pWnd->_GetDiskOtherStaticInfo();
	}

		
	
	//MSB (GetDiskVolFailCount);
	if(GetDiskVolFailCount>0)
	{
		pWnd->_GetDiskLetterUseWmi();
	}



	FlagStartDiskMon = TRUE;



	::AfxEndThread(0);

	return 0;
}



UINT Thread_LoadDiskStaticInfo(LPVOID lparam)
{
	PerferListData * pData =  (PerferListData * )lparam;

	if(pData !=NULL)
	{
		pThisBoxView->_LoadDiskStaticInfo(pData);
	}


	

	::AfxEndThread(0);

	return 0;
}






//============================


IMPLEMENT_DYNCREATE(CPerformanceBox, CFormView)

CPerformanceBox::CPerformanceBox()
: CFormView(CPerformanceBox::IDD)
, StrCPUName(_T(""))
, pInfoBoxCpu(NULL)
, pInfoBoxMemory(NULL)
, nLogicalProcessor(0)
, pTotalCpuBox(NULL)
, pCpuBox(NULL)

, nStep(0)
, FlagFirstUpdate(TRUE)
, ShowThisPage(TRUE)
, MaxCPUSpeed(0)
, pViewClass(NULL)
, pHotBox(NULL)
, CurrentDiskCount(0)
{

	 ArraySize= sizeof(float)*60;
}

CPerformanceBox::~CPerformanceBox()
{
	// Signal the CPU-speed monitor thread to stop and wait for it. The thread
	// is process-global so we don't tear it down here; we just stop it cleanly.
	if(g_hCpuSpeedThread != NULL)
	{
		InterlockedExchange(&g_CpuSpeedStop, 1);
		WaitForSingleObject(g_hCpuSpeedThread, 500);
		CloseHandle(g_hCpuSpeedThread);
		g_hCpuSpeedThread = NULL;
	}
	if(g_hWmiCpuThread != NULL)
	{
		InterlockedExchange(&g_WmiCpuStop, 1);
		WaitForSingleObject(g_hWmiCpuThread, 2000);
		CloseHandle(g_hWmiCpuThread);
		g_hWmiCpuThread = NULL;
	}
	// Stop the WMI disk monitor thread so it doesn't keep polling after the
	// view is gone (avoids leaking the thread + the COM apartment it holds).
	if(g_hWmiDiskThread != NULL)
	{
		InterlockedExchange(&g_WmiDiskStop, 1);
		WaitForSingleObject(g_hWmiDiskThread, 2000);
		CloseHandle(g_hWmiDiskThread);
		g_hWmiDiskThread = NULL;
	}
	// Stop the PDH disk monitor (fallback) too.
	StopPdhDiskMonitor();
	// Stop the NtQuerySystemInformation disk monitor (last-resort fallback).
	StopNtSysDiskMonitor();
}

void CPerformanceBox::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_LIST_PERFORMANCEITEM, mPItemList);

}

BEGIN_MESSAGE_MAP(CPerformanceBox, CFormView)
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
	ON_MESSAGE(UM_TIMER,OnUMTimer)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_PERFORMANCEITEM, &CPerformanceBox::OnLvnItemchangedListPerformanceitem)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_CHANGEGRAPHTO_OVERALLUTILIZATION, &CPerformanceBox::OnPop_ChangeGraphToOverallutilization)
	ON_COMMAND(ID_CHANGEGRAPHTO_LOGICALPROCESSORS, &CPerformanceBox::OnPop_ChangeGraphToLogicalprocessors)
	//	ON_WM_CREATE()
	ON_WM_INITMENUPOPUP()
	
	ON_COMMAND(ID_PERFORMANCE_SHOWKERNELTIMES, &CPerformanceBox::OnPerformanceShowkerneltimes)
	ON_UPDATE_COMMAND_UI(ID_PERFORMANCE_SHOWKERNELTIMES, &CPerformanceBox::OnUpdatePerformanceShowkerneltimes)
	
	
	ON_COMMAND(ID_VIEW_CPU, &CPerformanceBox::OnViewCpu)
	ON_COMMAND(ID_VIEW_MEMORY, &CPerformanceBox::OnViewMemory)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_PERFORMANCEITEM, &CPerformanceBox::OnNMRClickListPerformanceitem)
	ON_COMMAND(ID_PERFORMANCETYPE_HIDEGRAPHS, &CPerformanceBox::OnPop_PerformanceListShowHideGraphs)
	ON_UPDATE_COMMAND_UI(ID_PERFORMANCETYPE_HIDEGRAPHS, &CPerformanceBox::OnUpdatePerformancetypeHidegraphs)
	ON_WM_CREATE()
	ON_COMMAND(ID_CHANGEGRAPHTO_NUMANODES, &CPerformanceBox::OnPop_ChangeGraphToNumaNodes)	
	ON_NOTIFY_EX( TTN_NEEDTEXT, 0, SetTipText)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_COMMAND(ID_PERFORMANCE_GRAPHSUMMARYVIEW, &CPerformanceBox::OnPop_GraphSummaryView)
	
	ON_COMMAND(ID_PERFORMANCETYPE_SUMMARYVIEW, &CPerformanceBox::OnPop_PerformanceListSummaryView)

END_MESSAGE_MAP()


// CPerformanceBox diagnostics

#ifdef _DEBUG
void CPerformanceBox::AssertValid() const
{
	CFormView::AssertValid();
}

#ifndef _WIN32_WCE
void CPerformanceBox::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif
#endif //_DEBUG


// CPerformanceBox message handlers

HBRUSH CPerformanceBox::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	//HBRUSH hbr = CFormView::OnCtlColor(pDC, pWnd, nCtlColor);

	// TODO:  Change any attributes of the DC here





	CString StrFontName;
	StrFontName.LoadStringW(IDS_FONTNAME);


	if(nCtlColor == CTLCOLOR_STATIC)
	{
		pDC->SetBkColor(theApp.WndBkgColor);
		int nID=pWnd->GetDlgCtrlID();

		CFont StaticFont;

		if(nID== IDC_TIP1|| nID== IDC_TIP2  )
		{
			pDC->SetTextColor(RGB(112,112,112));
		}
		if(nID== IDC_ITEMLABEL )
		{
			pDC->SetTextColor(theApp.WndTextColor);		

			StaticFont.CreateFont(30,   // nHeight
				0,                         // nWidth
				0,                         // nEscapement
				0,                         // nOrientation
				FW_NORMAL,             // nWeight     FW_NORMAL,     FW_BOLD
				FALSE,                     // bItalic
				FALSE,                     // bUnderline�»��߱�ǣ���Ҫ�»��߰��������ó�TRUE
				0,                         // cStrikeOut
				DEFAULT_CHARSET,              // nCharSet
				OUT_DEFAULT_PRECIS,        // nOutPrecision
				CLIP_DEFAULT_PRECIS,       // nClipPrecision
				DEFAULT_QUALITY,           // nQuality
				DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
				StrFontName);                 // lpszFacename

			pDC->SelectObject(StaticFont);

			//pDC->SetBkColor(RGB(251, 247, 200));
			//pDC->SetBkMode(TRANSPARENT);
			//  return (HBRUSH) m_brush.GetSafeHandle();
		}

		if(nID== IDC_ITEMNAME )
		{
			pDC->SetTextColor(theApp.WndTextColor);	


			StaticFont.CreateFont(17,   // nHeight
				0,                         // nWidth
				0,                         // nEscapement
				0,                         // nOrientation
				FW_NORMAL,             // nWeight     FW_NORMAL,     FW_BOLD
				FALSE,                     // bItalic
				FALSE,                     // bUnderline�»��߱�ǣ���Ҫ�»��߰��������ó�TRUE
				0,                         // cStrikeOut
				DEFAULT_CHARSET,              // nCharSet
				OUT_DEFAULT_PRECIS,        // nOutPrecision
				CLIP_DEFAULT_PRECIS,       // nClipPrecision
				DEFAULT_QUALITY,           // nQuality
				DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
				StrFontName);                 // lpszFacename

			pDC->SelectObject(StaticFont);

			//pDC->SetBkColor(RGB(251, 247, 200));
			//pDC->SetBkMode(TRANSPARENT);
			//  return (HBRUSH) m_brush.GetSafeHandle();
		}

		StaticFont.DeleteObject();

	}



	// TODO:  Return a different brush if the default is not desired
	return theApp.BkgBrush;
}

void CPerformanceBox::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);
	//-----------------------------------------
	PlaceAllCtrl();
	InvalidateIfVisible(this);	 

}

void CPerformanceBox::Init(void)
{





	GetCPUInfo();

	pThisBoxView = this;

	//-------- init  logigal Processor Usage ------------------


	unsigned long bytesreturned;

	// Bug fix Win7: guard against nLogicalProcessor==0 (or negative) so we never
	// allocate zero-sized arrays that lead to wild-pointer writes later.
	int nCpuCount = nLogicalProcessor;
	if (nCpuCount <= 0)
	{
		SYSTEM_INFO si2; memset(&si2,0,sizeof(SYSTEM_INFO));
		GetSystemInfo(&si2);
		nCpuCount = si2.dwNumberOfProcessors;
		if (nCpuCount <= 0) nCpuCount = 1;
		nLogicalProcessor = nCpuCount;
		theApp.PerformanceInfo.nLogicalProcessor = nCpuCount;
	}

	spi_old = new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION [nCpuCount];
	spi = new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION [nCpuCount];

	memset(spi_old,0,sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nCpuCount);
	memset(spi,0,sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nCpuCount);

	MyNtQuerySystemInformation =(API_NtQuerySystemInformation) GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQuerySystemInformation");		

	MyNtQuerySystemInformation(SystemProcessorPerformanceInformation,spi_old, (sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nLogicalProcessor),&bytesreturned);


	

	//-------------------------- Left Item  lsit----------------------------

	//SetWindowTheme(mPItemList.GetSafeHwnd(),L"explorer", NULL);


	CRect rcCtrl;
	mPItemList.GetClientRect(rcCtrl);

	mPItemList.SetExtendedStyle(mPItemList.GetExtendedStyle()|LVS_EX_FULLROWSELECT|LVS_EX_SUBITEMIMAGES | LVS_OWNERDRAWFIXED|LVS_EX_DOUBLEBUFFER);  //| LVS_EX_GRIDLINES |LVS_EX_CHECKBOXES



	mPItemList.SetParent(this->GetParent());

	mPItemList.InsertColumn(0,L"",0,0); //����
	mPItemList.InsertColumn(1,L"",0,0);//�ڶ���������ʾ
	mPItemList.InsertColumn(2,L"",0,0);//�豸����
	mPItemList.InsertColumn(3,L"",0,0);//Tip1
	mPItemList.InsertColumn(4,L"",0,0);//Tip2


	//mPItemList.ShowScrollBar(SB_VERT,TRUE);





	//------------------------------ info box ------------------------------------------------------------------------

	//--------------  CPU---------

	pInfoBoxCpu = new CInfoBox;

	pInfoBoxCpu->Create(L"",WS_CHILD|WS_VISIBLE|SS_CENTER,CRect(0,0,1,1),this);
	pInfoBoxCpu->ShowWindow(SW_SHOW);

	pInfoBoxCpu->Type = PM_CPU ;



	pInfoBoxCpu->Info[0].StrTitle  = STR_CPUINFO_0 ;
	pInfoBoxCpu->Info[1].StrTitle  = STR_CPUINFO_1 ;
	pInfoBoxCpu->Info[2].StrTitle  = STR_CPUINFO_2 ;
	pInfoBoxCpu->Info[3].StrTitle  = STR_CPUINFO_3 ;
	pInfoBoxCpu->Info[4].StrTitle  = STR_CPUINFO_4 ;
	pInfoBoxCpu->Info[5].StrTitle  = STR_CPUINFO_5 ;
	pInfoBoxCpu->Info[6].StrTitle  = STR_CPUINFO_6 ;

	//�޸�Ĭ��λ��
	pInfoBoxCpu->Info[4].rc = pInfoBoxCpu->Info[3].rc;  pInfoBoxCpu->Info[4].rc.OffsetRect(pInfoBoxCpu->Info[3].rc.Width(),0);
	pInfoBoxCpu->Info[5].rc.left = pInfoBoxCpu->Info[0].rc.left;   	pInfoBoxCpu->Info[6].rc.left  = pInfoBoxCpu->Info[6].rc.left+pInfoBoxCpu->Info[5].rc.left+22;



	_UpdateCpuInfoBox(TRUE);

	// --------------Memory---------

	pInfoBoxMemory = NULL;
	pInfoBoxMemory = new CInfoBox;
	pInfoBoxMemory->Create(L"",WS_CHILD|WS_VISIBLE|SS_CENTER|SS_NOTIFY,CRect(0,0,1,1),this); //SS_NOTIFY������в�Ȼ�޷������� ��ɫ ��
	
	pInfoBoxMemory->Type = PM_MEMORY;

	pInfoBoxMemory->Info[0].StrTitle  = STR_MEMINFO_0;
	pInfoBoxMemory->Info[1].StrTitle  = STR_MEMINFO_1 ;
	pInfoBoxMemory->Info[2].StrTitle  = STR_MEMINFO_2 ;
	pInfoBoxMemory->Info[3].StrTitle  = STR_MEMINFO_3 ;
	pInfoBoxMemory->Info[4].StrTitle  = STR_MEMINFO_4 ;
	pInfoBoxMemory->Info[5].StrTitle  = STR_MEMINFO_5 ;
	pInfoBoxMemory->Info[6].StrTitle  = STR_MEMINFO_6 ;

	//�޸�Ĭ��λ��

	pInfoBoxMemory->Info[1].rc.OffsetRect(30,0); 
	pInfoBoxMemory->Info[3].rc.OffsetRect(30,0); 
	pInfoBoxMemory->Info[5].rc.OffsetRect(30,0);


	_UpdateMemoryInfoBox(TRUE);



	//------------------------------------------------------------------------------------------------------------------------------

	//------------------------------------------������ ��  ����ͼ ��----------------------------------------------------------------------

	//-------------------------------------------- �� CPU --------------------------------------


	pTotalCpuBox  = new CWaveBox;
	pTotalCpuBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);   

	pTotalCpuBox->SetLineColumn(1,1);
	pTotalCpuBox->SetColor(theApp.AppSettings.CpuColor);
	pTotalCpuBox->DrawSecondWave =  theApp.AppSettings.ShowKernelTime ;
	mPItemList.NumArray = pTotalCpuBox->Num[0];



	//-------------------------------------------- �߼� CPU  --------------------------------------


	CString StrItemName;
	StrItemName.LoadStringW(IDS_PITEM_CPU);





	mPItemList.InsertItem(0,StrItemName);	
	CString StrTemp=L"";
	double CurrentSpeed = _GetSurrentCpuSpeed();
	StrTemp.Format(L"0.00%%  %0.2f GHz",CurrentSpeed);

	mPItemList.SetItemText(0,1,StrTemp);
	mPItemList.SetItemText(0,2,StrCPUName);
	StrTemp=L"";

	pCpuBox = new CWaveBox;
	PerferListData * pPData =  new PerferListData;	

	pCpuBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);  
	//--------------�������м�����ʾ-------------------
	int nRow = 1, nColum = 1;

	if (theApp.PerformanceInfo.nLogicalProcessor > 0)
	{
		_DetermineRowCol(theApp.PerformanceInfo.nLogicalProcessor,&nRow,&nColum );
	}


	pCpuBox->SetLineColumn(nRow,nColum);
	pCpuBox->SetColor(theApp.AppSettings.CpuColor);
	pCpuBox->DrawSecondWave =  theApp.AppSettings.ShowKernelTime ;

	//--------CPU hot box -------------------
	pHotBox->nRow=nRow; pHotBox->nCol = nColum;
	pHotBox->ArrayPerLogicalCpuUsage = new float[(nLogicalProcessor>0)?nLogicalProcessor:1];




	if(theApp.AppSettings.ProcessorDisplayMode == 1 )
	{
		pPData->pWaveBox = pCpuBox;
		pHotBox->ShowWindow(SW_HIDE);

		CWnd *pWnd=GetDlgItem(IDC_TIP1);
		CString StrTime;
		if((double)theApp.AppSettings.TimerStep == 0.5)
		{
			StrTime.LoadStringW(IDS_STRING_WAVETIME_FAST);
		}
		else if ((double)theApp.AppSettings.TimerStep == 1)
		{
			StrTime.LoadStringW(IDS_STRING_WAVETIME_NORMAL);
		}
		else if ((double)theApp.AppSettings.TimerStep == 4)
		{
			StrTime.LoadStringW(IDS_STRING_WAVETIME_SLOW);
		}
		if(pWnd!=NULL)	
		{ 
			StrTemp.Format(STR_TIP1_CPU_LOGICAL,StrTime);

			mPItemList.SetItemText(0,3,StrTemp);
			pWnd->SetWindowTextW(StrTemp);
		}



	}
	else if(theApp.AppSettings.ProcessorDisplayMode == 0 )
	{
		pPData->pWaveBox = pTotalCpuBox;
		CWnd * pWnd=GetDlgItem(IDC_TIP1);
		if(pWnd!=NULL)	
		{ 
			CString StrTemp = STR_TIP1_CPU_TOTAL;
			mPItemList.SetItemText(0,3,StrTemp);
			pWnd->SetWindowTextW(StrTemp);
		}
	 

	}
	else if(theApp.AppSettings.ProcessorDisplayMode == 2)
	{
		pPData->pWaveBox = pTotalCpuBox; //ҲҪ
		pCpuBox->ShowWindow(SW_HIDE);
		pTotalCpuBox->ShowWindow(SW_HIDE);

		CWnd *pWnd=GetDlgItem(IDC_TIP1);
		if(pWnd!=NULL)	
		{ 
			CString StrTemp = STR_TIP1_CPU_NUMA;
			mPItemList.SetItemText(0,3,StrTemp);
			pWnd->SetWindowTextW(StrTemp);
		}
	 



	}




	pPData->pInfoBox = pInfoBoxCpu;
	pPData->Type = PM_CPU;
	pPData->ID = -1; //��ʾ������Ч
	pPData->pOtherWnd = NULL;



	mPItemList.SetItemData(0,(DWORD_PTR)pPData);
	mPItemList.SetItemText(0,4,L"100%");







	//----------------------------------------------- Memroy --------------------------------------------

	
	StrItemName.LoadStringW(IDS_PITEM_MEM);

	 
	mPItemList.InsertItem(1,StrItemName);
	StrTemp.Format(L"0.0/%0.1f GB (0.00%%)", (double)theApp.PerformanceInfo.TotalPhysMem/1024/1024/1024);
	mPItemList.SetItemText(1,1,StrTemp);	




	StrTemp.Format(L"%.0f GB %s" ,(double) (theApp.PerformanceInfo.InstalledMemKB)/1024/1024,theApp.PerformanceInfo.StrMemoryType); 
	mPItemList.SetItemText(1,2,StrTemp);

	CWaveBox *    pMemoryBox= new CWaveBox;
	CMemCompBox * pMemoryCompBox= new CMemCompBox;
	pPData =  new PerferListData;

	pPData->pWaveBox = pMemoryBox;
	pPData->pOtherWnd = pMemoryCompBox;

	pPData->pInfoBox = pInfoBoxMemory;
	pPData->Type = PM_MEMORY;
	pPData->ID = -1;


	pMemoryBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);
	pMemoryCompBox->Create(NULL,WS_CHILD|SS_NOTIFY,CRect(0,0,1,1),this);
	mPItemList.SetItemData (1,(DWORD_PTR)pPData);

	pMemoryBox->SetLineColumn(1,1);
	pMemoryBox->SetColor(theApp.AppSettings.MemoryColor );

	pMemoryCompBox->SetColor( );

	pMemoryBox->DrawSecondWave = FALSE;

	mPItemList.SetItemText(1,3,STR_TIP3_MEMORY);
	StrTemp.Format(L"%0.1f GB", (double)theApp.PerformanceInfo.TotalPhysMem/1024/1024/1024);
	mPItemList.SetItemText(1,4,StrTemp);




	//-------------------------------------------------------------------------------------
	
 


	//ö�ٴ���
	AddDiskToList();


	//------------------------------------------------------------------------------

	//ö������
	AddEthernetAdapterToList();


	theApp.StartPerformancePageTimer = TRUE;



	//-----------------------------------------------------------------

	// AfxBeginThread(Thread_SetDiskList,this);  //���ƴ��̼�������  ��Ϊ����Ƚ������зŶ����߳�


	//ѡ�е�һ��  �� CPU


	mPItemList.SetItemState(0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);	 
	mPItemList.MoveWindow(0,0,187,300);


	//--------------------------Labels ---------------------


	GetDlgItem(IDC_ITEMLABEL)->SetWindowTextW(L"CPU");
	GetDlgItem(IDC_ITEMNAME)->SetWindowTextW( StrCPUName );

	PlaceAllCtrl();



	EnableToolTips(TRUE);
	mToolTip.Create(this);
	mToolTip.Activate(TRUE);
	mToolTip.AddTool(pMemoryCompBox,LPSTR_TEXTCALLBACK);
	mToolTip.SetMaxTipWidth(200);
	 


	pInfoBoxCpu->Info[1].StrInfo.Format(L"%0.2f GHz",CurrentSpeed);

 





}


void CPerformanceBox::GetCPUInfo(void)
{ 

	CString strPath=_T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0");//ע����Ӽ�·��  
	CRegKey regkey;//����ע��������  
	LONG lResult;//LONG�ͱ�������Ӧ���  
	lResult=regkey.Open(HKEY_LOCAL_MACHINE,LPCTSTR(strPath),KEY_QUERY_VALUE ); //��ע�����   ע�� Ҫ�� �� ��Ȩ�� KEY_ALL_ACCESS ��ĳЩ�û��ᵼ�´���
	if (lResult==ERROR_SUCCESS)  
	{  
		WCHAR chCPUName[50] = {0};  
		DWORD dwSize=50;   

		//��ȡProcessorNameString�ֶ�ֵ  
		if (ERROR_SUCCESS == regkey.QueryStringValue(_T("ProcessorNameString"),chCPUName,&dwSize))  
		{  
			StrCPUName = chCPUName;  
		}  

	}  


	nLogicalProcessor  =1;


	//��ѯCPU��Ƶ  
	DWORD dwValue;  
	if (ERROR_SUCCESS == regkey.QueryDWORDValue(_T("~MHz"),dwValue))  
	{  
		MaxCPUSpeed = dwValue;  

	}  
	regkey.Close();//�ر�ע���  

	//UpdateData(FALSE);  

	//��ȡCPU������Ŀ  
	/*   SYSTEM_INFO si;  
	memset(&si,0,sizeof(SYSTEM_INFO));  
	GetSystemInfo(&si);  
	nLogicalProcessor = si.dwNumberOfProcessors;  
	*/

	PM.GetCpuInfo();

	nLogicalProcessor =  theApp.PerformanceInfo.nLogicalProcessor;

	if(theApp.PerformanceInfo.nLogicalProcessor == 0)
	{
		// ��ȡCPU������Ŀ  
		SYSTEM_INFO si;  
		memset(&si,0,sizeof(SYSTEM_INFO));  
		GetSystemInfo(&si);  
		nLogicalProcessor = si.dwNumberOfProcessors;  
		theApp.PerformanceInfo.nLogicalProcessor = nLogicalProcessor;


	}




}


LRESULT CPerformanceBox::OnUMTimer(WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your message handler code here and/or call default

	//::AfxBeginThread(Thread_UpDatePM,this);



	//mPItemList.SetRedraw(0);


	ShowThisPage =( theApp.pSelPage == (CFormView *)GetParent());

	UpdateAllPMInfo();

	// mPItemList.SetRedraw(1);

	nStep++;
	if(nStep==60)nStep =0;


	FlagFirstUpdate = FALSE; 


	return 0;

}

void CPerformanceBox::OnLvnItemchangedListPerformanceitem(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here

	INT nSel = 0;
	nSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );




	if(nSel== -1) return;


	int n = mPItemList.GetItemCount();



	//	PerferListData  *pPData;

	CWnd *pWnd= NULL;
	CString StrTip;


	for(int i=0;i<n;i++)
	{


		PerferListData * pPData = (PerferListData*)mPItemList.GetItemData(i);	

		if(pPData == NULL) { continue ;  } 
		if(pPData->pWaveBox==NULL) {continue ;  }
		if(pPData->pInfoBox==NULL) {continue ;  }

		if(i==nSel)
		{ 


			pPData->pWaveBox->ShowWindow(SW_SHOW);
			pPData->pInfoBox->ShowWindow(SW_SHOW);
			if(pPData->pOtherWnd != NULL)
			{
				//MSG(nSel)
				pPData->pOtherWnd->ShowWindow(SW_SHOW);
			}



			if(pPData->Type == PM_DISK)
			{

				pWnd=GetDlgItem(IDC_TIP5);
				if(pWnd!=NULL)
				{
					pWnd->SetWindowTextW(STR_TIP5_DISK); 
					pWnd->ShowWindow(SW_SHOW);
				}
				pWnd=GetDlgItem(IDC_TIP6);
				if(pWnd!=NULL)
				{
					pWnd->SetWindowTextW(pPData->StrOther1); 
					pWnd->ShowWindow(SW_SHOW);
				}
				pWnd=GetDlgItem(IDC_TIP7);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
				pWnd=GetDlgItem(IDC_TIP8);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			}
			else if(pPData->Type == PM_MEMORY)
			{
				pWnd=GetDlgItem(IDC_TIP5);
				if(pWnd!=NULL)
				{
					pWnd->SetWindowTextW( STR_TIP5_MEMORY); 
					pWnd->ShowWindow(SW_SHOW);
				}
				pWnd=GetDlgItem(IDC_TIP6);
				if(pWnd!=NULL)
				{
					pWnd->SetWindowTextW(L""); 
					pWnd->ShowWindow(SW_SHOW);
				}
				pWnd=GetDlgItem(IDC_TIP7);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
				pWnd=GetDlgItem(IDC_TIP8);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			}
			else
			{



				pWnd=GetDlgItem(IDC_TIP5);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
				pWnd=GetDlgItem(IDC_TIP6);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
				pWnd=GetDlgItem(IDC_TIP7);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
				pWnd=GetDlgItem(IDC_TIP8);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }

			}


		}
		else  
		{ 


			pPData->pWaveBox->ShowWindow(SW_HIDE);
			pPData->pInfoBox->ShowWindow(SW_HIDE);
			if(pPData->pOtherWnd != NULL)
			{
				pPData->pOtherWnd->ShowWindow(SW_HIDE);
			}
		} 


	}





	if(nSel==0) //CPU
	{


		if(theApp.AppSettings.ProcessorDisplayMode  ==0 ) // ��ʾ��CPU ģʽ
		{
			pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			pHotBox->ShowWindow(SW_HIDE);	
		}
		else if (theApp.AppSettings.ProcessorDisplayMode  ==1   )
		{
			pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			pHotBox->ShowWindow(SW_HIDE);	
		}
		else if (theApp.AppSettings.ProcessorDisplayMode  == 2 )
		{
			pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pCpuBox->ShowWindow(SW_HIDE);
			pTotalCpuBox->ShowWindow(SW_HIDE);	
			pHotBox->ShowWindow(SW_SHOW);	

		}

	}
	else //��������Ŀʱ ��ǩ2 3 4 ����ʾ��
	{
		pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
		pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
		pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
		pHotBox->ShowWindow(SW_HIDE);	
	}









	pWnd=GetDlgItem(IDC_TIP1);
	if(pWnd!=NULL)
	{					
		StrTip = mPItemList.GetItemText(nSel,3);
		pWnd->SetWindowTextW(StrTip); //�������ǿ��͸߲���λ��
	}


	pWnd=GetDlgItem(IDC_TIP2);
	if(pWnd!=NULL)
	{					
		StrTip = mPItemList.GetItemText(nSel,4);
		pWnd->SetWindowTextW(StrTip); //�������ǿ��͸߲���λ��
	}




	PerferListData * pPData = (PerferListData*)mPItemList.GetItemData(nSel);

	if(pPData->Type == PM_ETHERNET)
	{
		SetScrollSizes(MM_TEXT, CSize(BOX_W_MIN, 300)); 
	}
	else
	{
		SetScrollSizes(MM_TEXT, CSize(BOX_W_MIN, 400)); 

	}












	this->PlaceAllCtrl(); 

	GetDlgItem(IDC_ITEMLABEL)->SetWindowTextW(mPItemList.GetItemText(nSel,0));
	GetDlgItem(IDC_ITEMNAME)->SetWindowTextW(mPItemList.GetItemText(nSel,2));

	UpdateInfoBox();

	mPItemList.Invalidate();
	this->Invalidate();






	*pResult = 0;
}

int CPerformanceBox::AddEthernetAdapterToList(void)
{

 




	//---------------------- new ------------------------------------------





	WSADATA WsaData;   
	WSAStartup(MAKEWORD(1,1), &WsaData);


	DWORD dwSize = 0;
	DWORD dwRetVal = 0;

	unsigned int i = 0;

	// Set the flags to pass to GetAdaptersAddresses
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	// default to unspecified address family (both)

	ULONG family = AF_UNSPEC;
	LPVOID lpMsgBuf = NULL;

	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	ULONG OutBufLen = 0;
	ULONG Iterations = 0;

	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
	PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
	PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
	IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
	IP_ADAPTER_PREFIX *pPrefix = NULL;

	PIP_ADAPTER_UNICAST_ADDRESS pCurrentUnicastAddr = NULL;

	// Allocate a 15 KB buffer to start with.

	OutBufLen = 15000;

	do 
	{
		pAddresses = (IP_ADAPTER_ADDRESSES *) HeapAlloc(GetProcessHeap(), 0, (OutBufLen));
		if (pAddresses == NULL) break;		
		dwRetVal =GetAdaptersAddresses(family, flags, NULL, pAddresses, &OutBufLen);
		if (dwRetVal == ERROR_BUFFER_OVERFLOW)
		{
			HeapFree(GetProcessHeap(), 0, (pAddresses));   pAddresses = NULL;
		} 
		else 
		{
			break;
		}

		Iterations++;

	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < 3)); //3��������Դ���

	//�ɹ���ʵ�ʻ�ȡ��Ϣ

	if (dwRetVal == NO_ERROR)
	{

		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses;

			CString StrIPv4,StrIPv6,StrIPv6Link;
		CString StrInfo,StrTemp;
		while (pCurrAddresses)
		{
				if(pCurrAddresses->IfType != IF_TYPE_ETHERNET_CSMACD && pCurrAddresses->IfType != IF_TYPE_IEEE80211 )
				{
				pCurrAddresses = pCurrAddresses->Next; continue;//
				}

				// Filter: hide adapters that are not operationally up OR have no
				// link speed. This matches what Windows 10/11 native Task Manager
				// shows — virtual / disconnected / Bluetooth-PAN-inactive adapters
				// are dropped. Win7-compatible (OperStatus / TransmitLinkSpeed have
				// been in IP_ADAPTER_ADDRESSES since XP/Vista).
				if(pCurrAddresses->OperStatus != IfOperStatusUp)
				{
					pCurrAddresses = pCurrAddresses->Next; continue;
				}
				if(pCurrAddresses->TransmitLinkSpeed == 0 && pCurrAddresses->ReceiveLinkSpeed == 0)
				{
					pCurrAddresses = pCurrAddresses->Next; continue;
				}


				CString StrTypeTitle = L" ";
				StrTypeTitle.LoadStringW(IDS_PITEM_NET);
				if(pCurrAddresses->IfType==IF_TYPE_IEEE80211)
			{
					StrTypeTitle.LoadStringW(IDS_PITEM_WIRELESS);
				}

				CString StrName  ;
				StrName= pCurrAddresses->Description;//��������    AdapterName��������ɵ�����
				PerferListData *pPData = _InsertNetAdapterItem(StrTypeTitle,StrName,pCurrAddresses->IfIndex);

				// ------------------------------------------------------------------
				// Gather: FriendlyName, IPv4, IPv6 (single, prefer global unicast),
				// and for wireless: connection type (PHY), SSID, signal quality.
				// ------------------------------------------------------------------
				StrInfo = L"";
				StrTemp.Format(L"%wS", pCurrAddresses->FriendlyName);
				StrInfo = StrTemp;

				StrIPv4 = L"";
				StrIPv6 = L"";       // preferred (global unicast)
				StrIPv6Link = L"";   // fallback (link-local)

				pUnicast = pCurrAddresses->FirstUnicastAddress;
				if (pUnicast != NULL)
				{
					WCHAR buff[1024];
					DWORD bufflen = 1024;
					for (i = 0; pUnicast != NULL; i++)
					{
						if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
						{
							if(StrIPv4.IsEmpty())
							{
								sockaddr_in *sa_in = (sockaddr_in *)pUnicast->Address.lpSockaddr;
								StrIPv4 = inet_ntoa(sa_in->sin_addr);
							}
						}
						else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6)
						{
							const SOCKADDR* sa = pUnicast->Address.lpSockaddr;
							ZeroMemory(buff, sizeof(buff));
							bufflen = 1024;
							WSAAddressToString((LPSOCKADDR)sa,
							                   pUnicast->Address.iSockaddrLength,
							                   NULL, buff, &bufflen);
							CString s6 = buff;
							if(_IsIPv6LinkLocal(sa))
							{
								if(StrIPv6Link.IsEmpty()) StrIPv6Link = s6;
							}
							else
							{
								if(StrIPv6.IsEmpty()) StrIPv6 = s6;
							}
						}

						pUnicast = pUnicast->Next;

					}
				}
				// If no global unicast was found, fall back to the link-local.
				if(StrIPv6.IsEmpty()) StrIPv6 = StrIPv6Link;

				// Connection type / SSID / signal: only meaningful for wireless.
				CString StrConnType;
				CString StrSSID;
				CString StrSignal;
				BOOL bWireless = (pCurrAddresses->IfType == IF_TYPE_IEEE80211);
				BOOL bWlanOk = FALSE;
				if(bWireless && pCurrAddresses->AdapterName != NULL)
				{
					// GetAdaptersAddresses returns AdapterName as an ANSI string
					// "{XXXXXXXX-...}" but UuidFromStringW (esp. on Win7) rejects
					// the curly braces AND expects a wide string. Strip the
					// braces and convert the ANSI name to wide so we can build
					// the binary GUID WlanQueryInterface needs.
					CString braced;
					if(pCurrAddresses->AdapterName != NULL)
						braced = CA2W(pCurrAddresses->AdapterName);
					braced.Trim();
					if(braced.GetLength() > 0 && braced[0] == L'{')
						braced.Delete(0);
					if(braced.GetLength() > 0 && braced[braced.GetLength()-1] == L'}')
						braced.Delete(braced.GetLength()-1);

					GUID guid;
					ZeroMemory(&guid, sizeof(guid));
					RPC_WSTR rpcStr = (RPC_WSTR)(LPCTSTR)braced;
					if(RPC_S_OK == UuidFromStringW(rpcStr, &guid))
					{
						StrConnType = _GetWlanPhyType(&guid);
						StrSSID     = _GetWlanSsid(&guid);
						ULONG q     = _GetWlanSignalQuality(&guid);
						if(q > 0)
						{
							// 5-bar Unicode approximation of the Win10 signal icon:
							//   ▂  ▃  ▄  ▅  █ (rising heights)
							// Show bars filled up to quality/20 (0..5).
							static const wchar_t* bars = L"\u2582\u2583\u2584\u2585\u2588";
							int nFilled = (int)((q + 10) / 20); // round-half-up
							if(nFilled < 0) nFilled = 0;
							if(nFilled > 5) nFilled = 5;
							CString s;
							for(int k = 0; k < nFilled; k++) s += bars[k];
							StrSignal.Format(L"%lu%%  %s", q, (LPCTSTR)s);
						}
						bWlanOk = TRUE;
					}
				}
				if(StrConnType.IsEmpty())
					StrConnType = bWireless ? L"Wi-Fi (unspecified)" : L"Ethernet";

				// CInfoBox uses wcstok_s to split StrInfo by '\n' to align with
				// the StrTitle labels. wcstok_s COLLAPSES consecutive delimiters,
				// so a single empty value (e.g. missing SSID or signal) would
				// shift every subsequent value up by one slot. To keep alignment
				// we must NEVER emit an empty segment in the middle of the
				// StrInfo string. Use an em-dash as a visible placeholder.
				if(StrSSID.IsEmpty()   && bWireless) StrSSID   = L"\u2014"; // —
				if(StrSignal.IsEmpty() && bWireless) StrSignal = L"\u2014"; // —

				// Build StrInfo + StrTitle in the same line order.
				if(bWireless)
				{
					pPData->pInfoBox->Info[6].StrTitle =
						L"Adapter name:\n"
						L"SSID:\n"
						L"Connection type:\n"
						L"IPv4 address:\n"
						L"IPv6 address:\n"
						L"Signal strength:";
					pPData->pInfoBox->Info[6].StrInfo =
						StrInfo    + L"\n" +
						StrSSID    + L"\n" +
						StrConnType + L"\n" +
						StrIPv4    + L"\n" +
						StrIPv6    + L"\n" +
						StrSignal;
				}
				else
				{
					pPData->pInfoBox->Info[6].StrTitle =
						L"Adapter name:\n"
						L"Connection type:\n"
						L"IPv4 address:\n"
						L"IPv6 address:";
					pPData->pInfoBox->Info[6].StrInfo =
						StrInfo    + L"\n" +
						StrConnType + L"\n" +
						StrIPv4    + L"\n" +
						StrIPv6;
				}

				pPData->pInfoBox->SetColor();




			//if (pCurrAddresses->PhysicalAddressLength != 0)
			//{
			//	Str=Str+L"Physical address: ";
			//	CString StrMAC;

			//	for (i = 0; i < (int) pCurrAddresses->PhysicalAddressLength;i++)
			//	{
			//		if (i == (pCurrAddresses->PhysicalAddressLength - 1))
			//		{
			//			StrTemp.Format(L"%.2X\n",(int) pCurrAddresses->PhysicalAddress[i]);
			//			StrMAC+=StrTemp;
			//		}
			//		else
			//		{
			//			StrTemp.Format(L"%.2X-",(int) pCurrAddresses->PhysicalAddress[i]);
			//			StrMAC+=StrTemp;
			//		}
			//	}
			//	Str+=StrMAC;
			//}


			pCurrAddresses = pCurrAddresses->Next;
		}
	} 
	//else 
	//{		
	//	if (dwRetVal != ERROR_NO_DATA)		
	//	{
	//		if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPTSTR) & lpMsgBuf, 0, NULL)) 
	//		{
	//			//printf("\tError: %s", lpMsgBuf);
	//			LocalFree(lpMsgBuf);
	//			if (pAddresses)HeapFree(GetProcessHeap(), 0, (pAddresses));
	//			return 0;
	//		}
	//	}
	//}
	if (pAddresses)
	{
		HeapFree(GetProcessHeap(), 0, (pAddresses));
	}


	WSACleanup();
	

	return 0;


	//====================����Ϊע�����ȡ��ʽ ��ʱ������������=================================


	/*


	CInfoBox *pInfoBox ;

	HKEY hKey, hSubKey, hNdiIntKey;
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"System\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}",0,KEY_READ,&hKey) != ERROR_SUCCESS)
	return  0;

	int dwIndex = 0;
	DWORD dwBufSize = 256;
	DWORD dwDataType;
	WCHAR szSubKey[256];
	WCHAR szData[256];

	CString StrInfo; 
	CString StrAdapterName =L"" ; 



	CString  StrPdh;

	//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	//  	System\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}ö�ٺ� ��ȡNetCfgInstanceId ���Ի��һ��ID

	//	     HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\services\Tcpip\Parameters\Interfaces\�����ID   ���Զ�ȡ IP��ַ����Ϣ   

	//	  HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Network\{4D36E972-E325-11CE-BFC1-08002BE10318}  �����������Ƶ�


	//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	//


	while(RegEnumKeyEx(hKey, dwIndex++, szSubKey, &dwBufSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
	{

	if(RegOpenKeyEx(hKey, szSubKey, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS)
	{	
	if(RegOpenKeyEx(hSubKey, L"Ndi\\Interfaces", 0, KEY_READ, &hNdiIntKey) == ERROR_SUCCESS)
	{
	dwBufSize = 256;
	memset(szData, 0, sizeof(szData));
	if(RegQueryValueEx(hNdiIntKey, L"LowerRange", 0, &dwDataType, (BYTE*)szData, &dwBufSize) == ERROR_SUCCESS)
	{
	int NetType = 0;

	if(lstrcmp( szData, L"ethernet") == 0 ) NetType = 1;//��ͨ����
	if(lstrcmp( szData, L"wlan,ethernet,vwifi") == 0 ) NetType = 2;//��������

	if( NetType!= 0 )	 //	�ж��ǲ�����̫����
	{

	dwBufSize = 256;
	if(RegQueryValueEx(hSubKey, L"*PhysicalMediaType", 0, &dwDataType, (BYTE*)szData, &dwBufSize) != ERROR_SUCCESS)
	{
	goto	LOOP_01  ;  //��Ҫ�� continue ���� ��һ������ �� ��ֱ������ѭ��
	}

	dwBufSize = 256;	dwDataType = REG_SZ ;memset(szData, 0, sizeof(szData));
	if(RegQueryValueEx(hSubKey, L"NoDisplayClass", 0, &dwDataType, (BYTE*)szData, &dwBufSize) == ERROR_SUCCESS)
	{
	if(lstrcmp( szData, L"1") == 0)	
	{
	goto	LOOP_01  ;//��Ҫ�� continue ���� ��һ������ �� ��ֱ������ѭ��
	}


	}
	------------------------------------------------------------------

	dwBufSize = 256;
	if(RegQueryValueEx(hSubKey, L"DriverDesc", 0, &dwDataType, (BYTE*)szData, &dwBufSize) == ERROR_SUCCESS)
	{
	PerferListData *pPData =  new PerferListData;
	CString StrName = szData;

	// szData �б�����������ϸ����
	int n =mPItemList.GetItemCount();
	CString StrTypeTitle = L"Ethernet";
	if(NetType == 2)
	{
	StrTypeTitle =  L"Wi-Fi";
	}

	mPItemList.InsertItem(n,StrTypeTitle);

	mPItemList.SetItemText(n,1,L"S: 0 Kbps R: 0 Kbps");
	mPItemList.SetItemText(n,2,StrName);  // ע��� DriverDesc ��ȡ�� szData ��������ʾ����
	mPItemList.SetItemData(n,(DWORD_PTR)pPData);



	CWaveBox * pEthernetBox= new CWaveBox;
	pInfoBox = new CInfoBox;	

	pInfoBox->Create(L"",WS_CHILD|WS_VISIBLE|SS_CENTER,CRect(0,0,1,1),this);
	pEthernetBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);

	pPData->Type = PM_ETHERNET;
	pInfoBox->Type  = PM_ETHERNET;
	pPData->ID = dwIndex-1;	

	pPData->pWaveBox = pEthernetBox; //ע��˳�������SetItemDataǰ ���������ⲻ֪Ϊ�Σ�(
	pPData->pInfoBox = pInfoBox;
	pPData->pWaveBox->DrawSecondWave = TRUE;
	pPData->pOtherWnd = NULL;

	pInfoBox->Info[0].StrTitle  = L"Send" ;
	pInfoBox->Info[2].StrTitle  = L"Receive" ;

	pInfoBox->Info[0].StrInfo=L"0 Kbps";
	pInfoBox->Info[2].StrInfo=L"0 Kbps";

	pInfoBox->Info[0].Type=2;  pInfoBox->Info[2].Type=1; //ͼ������
	pInfoBox->Info[6].StrTitle  = L"Adapter name:\nConnection type:\nIPv4 address:\nIPv6 address:";
	pInfoBox->Info[6].rc.left-= 100;


	mPItemList.SetItemText(n,3,L"Throughput");
	mPItemList.SetItemText(n,4,L"100 Kbps");



	pEthernetBox->SetLineColumn(1,1);
	pEthernetBox->SetColor(RGB(167,79,1),RGB(238,222,207));

	pPData->MaxVar = 100*1024/8; //Ĭ�����ֵ100Kbps


	//	---------------  get Connect Name ---------
	CString StrAdapterName;

	dwBufSize = 256;
	if(RegQueryValueEx(hSubKey, L"NetCfgInstanceID", 0, &dwDataType, (BYTE*)szData, &dwBufSize) == ERROR_SUCCESS)
	{

	CString StrPath =  szData;
	StrPath = L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\"+StrPath+L"\\Connection";

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, StrPath, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS)
	{
	dwBufSize = 256;
	if(RegQueryValueEx(hSubKey, L"Name", 0, &dwDataType, (BYTE*)szData, &dwBufSize) == ERROR_SUCCESS)
	{

	StrAdapterName =  szData;
	}
	}

	}

	pInfoBox->Info[6].StrInfo.Format(L"%s\nEthernet\n",StrAdapterName );
	_LoadNetworkStaticInfo (pPData); //ע�⣺�������λ��Ҫ������� ���� pPData���ݲ�ȫ��������


	//	---- Init Pdh  of ethernet Adapter----

	StrName.Replace(L"(",L"[");
	StrName.Replace(L")",L"]");
	StrName.Replace(L"/",L"_");

	if ( PdhOpenQuery(NULL, NULL, &pPData->Query)== ERROR_SUCCESS )  //��
	{

	StrPdh.Format(L"\\Network Interface(%s)\\Bytes Total/sec", StrName );  
	PdhAddCounter(pPData->Query, StrPdh, 0, &pPData->Counter);  
	}

	if ( PdhOpenQuery(NULL, NULL, &pPData->QueryA)== ERROR_SUCCESS )  //Send
	{

	StrPdh.Format(L"\\Network Interface(%s)\\Bytes Received/sec", StrName );  
	PdhAddCounter(pPData->QueryA, StrPdh, 0, &pPData->CounterA);  
	}

	if ( PdhOpenQuery(NULL, NULL, &pPData->QueryB)== ERROR_SUCCESS )  //Receive
	{

	StrPdh.Format(L"\\Network Interface(%s)\\Bytes Sent/sec", StrName );  
	PdhAddCounter(pPData->QueryB, StrPdh, 0, &pPData->CounterB);  
	}


	}
	}
	}
	RegCloseKey(hNdiIntKey);
	}
	RegCloseKey(hSubKey);


	}

	LOOP_01:	dwBufSize = 256;
	}	//end of while 

	RegCloseKey(hKey);


	*/



}

void CPerformanceBox::_UpdateCpuInfoBox(BOOL UpdateAll)
{
	if(!ShowThisPage) return ;


	PERFORMANCE_INFORMATION MyPMInfo;
	ZeroMemory(&MyPMInfo, sizeof(PERFORMANCE_INFORMATION));
	MyPMInfo.cb = sizeof(PERFORMANCE_INFORMATION);
	GetPerformanceInfo(&MyPMInfo,sizeof(PERFORMANCE_INFORMATION));

	theApp.PerformanceInfo.ProcessCount =MyPMInfo.ProcessCount ;

	//MSG(0)

	CString StrUpTime;




	StrUpTime.Format(L"%d:%02d:%02d:%02d",theApp.UpTimeDay,theApp.UpTimeHour,theApp.UpTimeMin,(int)theApp.UpTimeSec);

	double  CurrentSpeed = 0;
	double  nThreads;
	double  nHandles;


	nThreads = MyPMInfo.ThreadCount;//PM.PdhGetInfo(L"\\System\\Threads");
	nHandles = MyPMInfo.HandleCount;//PM.PdhGetInfo(L"\\Process(_Total)\\Handle Count");


	//CurrentSpeed = PM.PdhGetInfo(L"\\Processor Information(0,0)\\Processor Frequency");


	
	PROCESSOR_POWER_INFORMATION  *PPInfo = new PROCESSOR_POWER_INFORMATION [theApp.PerformanceInfo.ProcessCount] ;
	NTSTATUS Ret = CallNtPowerInformation( ProcessorInformation,NULL, 0,PPInfo,sizeof(PROCESSOR_POWER_INFORMATION)*theApp.PerformanceInfo.ProcessCount );

	 

	CurrentSpeed =  PPInfo[0].CurrentMhz;	
	CurrentSpeed=CurrentSpeed/1000;//ע�����cpuƵ���� ����1000����1024;






   /// CPU ��ǰ�ٶȸ��ں���  UpdateAllPMInfo �л�ȡ ���������б� ��ͬ��
 
	//����ʱ��ǰ��ȡһ�η�������ʾ

	pInfoBoxCpu->Info[0].StrInfo.Format(L"%0.0f%%", theApp.PerformanceInfo.CpuUsage) ;
	//pInfoBoxCpu->Info[1].StrInfo.Format(L"%0.2f GHz",CurrentSpeed);
	pInfoBoxCpu->Info[2].StrInfo.Format(L"%d",theApp.PerformanceInfo.ProcessCount);  //Processes
	pInfoBoxCpu->Info[3].StrInfo.Format(L"%d",(int)nThreads);  //Threads
	pInfoBoxCpu->Info[4].StrInfo.Format(L"%d",(int)nHandles);//Handles
	pInfoBoxCpu->Info[5].StrInfo  = StrUpTime ;//Up time
  
 

	 

	if(UpdateAll)
	{
		CString StrCpuInfo;

		CString StrMaxSpeed;

		if(MaxCPUSpeed>1000) //ע���������1024����
		{
			MaxCPUSpeed= MaxCPUSpeed/1000;
			StrMaxSpeed.Format(L"%.2f GHz",MaxCPUSpeed);

		}
		else
		{
			StrMaxSpeed.Format(L"%.2f MHz",MaxCPUSpeed);
		}

		StrCpuInfo.Format(L"%s\n%d\n%d\n%d\n%s\n%s\n%s\n%s",StrMaxSpeed, theApp.PerformanceInfo.nPhysicalProcessorPackages,theApp.PerformanceInfo.nProcessorCores,theApp.PerformanceInfo.nLogicalProcessor,L" ",theApp.PerformanceInfo.StrL1Cache,theApp.PerformanceInfo.StrL2Cache,theApp.PerformanceInfo.StrL3Cache);

		pInfoBoxCpu->Info[6].StrInfo  = StrCpuInfo ;

	} 

	delete []  PPInfo;
}

int CPerformanceBox::AddDiskToList(int LastID)
{

	

	CInfoBox *pInfoBox= NULL;

	HKEY hKeyList,hKeyDisk;
	DWORD dwDataType;
	DWORD dwBufSize = MAX_PATH;

	WCHAR StrRegData[MAX_PATH] ;
	DWORD  RegData;

	if(  ERROR_SUCCESS  != RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\services\\Disk\\Enum",0,KEY_READ,&hKeyList) )
	{
		return 0;
	}

	dwBufSize = sizeof(RegData);
	RegQueryValueEx(hKeyList, L"Count", 0, &dwDataType, (LPBYTE)&RegData, &dwBufSize );

	int i,n;
	n = (int) RegData;

	CString  StrValue;
	CString  StrKey ;

	CString StrID;


	int nItem = 2+CurrentDiskCount;


	CString StrItemName;

	StrItemName.LoadStringW(IDS_PITEM_DISK);

	
	 

	for(i=0;i<n;i++)
	{
		if(i<CurrentDiskCount) continue ;

	

		dwBufSize = MAX_PATH;
		dwDataType = REG_SZ;
		StrValue.Format(L"%d",i);
		RegQueryValueEx(hKeyList, StrValue, 0, &dwDataType, (BYTE*)StrRegData, &dwBufSize );
		StrKey=L"SYSTEM\\CurrentControlSet\\Enum\\";
		StrKey=StrKey+StrRegData;

		

		RegOpenKeyEx(HKEY_LOCAL_MACHINE,StrKey,0,KEY_READ,&hKeyDisk);
		dwBufSize = MAX_PATH;
		RegQueryValueEx(hKeyDisk, L"FriendlyName", 0, &dwDataType, (BYTE*)StrRegData, &dwBufSize );


	



			
		StrID.Format(L"%s %d",StrItemName,i);
		

		mPItemList.InsertItem(nItem,StrID);

		
	 

		mPItemList.SetItemText(nItem,1,L"0.00%");
		mPItemList.SetItemText(nItem,2,StrRegData); //Ӳ������ ���� ADATA �ȵ�


		

		CWaveBox * pDiskBox= new CWaveBox;
		CWaveBox * pDiskBox2= new CWaveBox; //�ڶ��� ����ͼ

		PerferListData *pPData =  new PerferListData;
		pInfoBox = new CInfoBox;


		

		pPData->Type = PM_DISK;


		//����ID �Ƿ���Ч  ��ID �������һ�� ��Ӳ�̱�������  iD��Ϊ��������

		pPData->ID = _TestDiskID(LastID);//�˺����Ὣ��ȷ Ӳ�̴��� pPData->ID ;
		LastID = pPData->ID+1;

		//MSB(LastID)


	

		pPData->pWaveBox = pDiskBox; 
		pPData->pInfoBox = pInfoBox;
		pPData->pOtherWnd = pDiskBox2;

		pPData->MaxVar = 100*1024; //Ĭ�����ֵ100KB/s
		pPData->StrOther1 = L"100 KB/s";

		//pPData->StrOther0 = L" ";//Ԥ�ȴ���26���ո����ڽ��ն�Ӧλ���̷���Ϣ��������������
 



		pDiskBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);
		pDiskBox2->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);


		pDiskBox2->DrawSecondWave = TRUE;

		pInfoBox->Create(L"",WS_CHILD|WS_VISIBLE|SS_CENTER,CRect(0,0,1,1),this);

		pInfoBox->Info[0].StrTitle  = STR_DISKINFO_0 ;
		pInfoBox->Info[1].StrTitle  = STR_DISKINFO_1 ;
		pInfoBox->Info[2].StrTitle  = STR_DISKINFO_2 ;
		pInfoBox->Info[3].StrTitle  = STR_DISKINFO_3 ;		 
		pInfoBox->Info[6].StrTitle  = STR_DISKINFO_6 ;

		pInfoBox->Info[2].Type=1;  pInfoBox->Info[3].Type=2; //ͼ������

		//д���ʼ���� Ϊ���Ӿ�Ч�� ��ͣ��
		pInfoBox->Info[0].StrInfo= L"0%";  pInfoBox->Info[1].StrInfo= L"0.0 ms";
		pInfoBox->Info[2].StrInfo= L"0.0 KB/s";  pInfoBox->Info[3].StrInfo= L"0.0 KB/s"; //ͼ������

		pInfoBox->Type = PM_DISK;

		pInfoBox->Info[3].rc.OffsetRect(30,0);
		pInfoBox->Info[6].rc.OffsetRect(22,0);

		pPData->DataA=pPData->DataB=pPData->DataC = pPData->DataD = 0;
				pPData->QueryD = NULL;
				pPData->CounterD = NULL;
				pPData->CounterDR = NULL;
				pPData->CounterDW = NULL;
				pPData->PdhBaseline = FALSE;

				// Disk activity comes from the WMI background monitor
				// (g_WmiDisk[]) launched by EnsureWmiDiskMonitor() above.
				// The per-tick loop reads from that cache.

		pInfoBox->SetColor();

		//---------------------------------�̶���Ϣ--------------------------------------


		

	   ::AfxBeginThread(Thread_LoadDiskStaticInfo,pPData );


		//--------------------



		mPItemList.SetItemText(nItem,3,STR_DISKINFO_0);
		mPItemList.SetItemText(nItem,4,L"100%");

		 

		mPItemList.SetItemData (nItem,(DWORD_PTR)pPData);

		pDiskBox->SetLineColumn(1,1);
		pDiskBox->SetColor(theApp.AppSettings.DiskColor);

		pDiskBox2->SetLineColumn(1,1);
		pDiskBox2->SetColor(theApp.AppSettings.DiskColor);



		nItem++;


	}

	
		
		
	RegCloseKey(hKeyList);
	RegCloseKey(hKeyDisk);

 
  ::AfxBeginThread(Thread_GetDiskOtherStaticInfo,this);

	 

	return n;
}

void CPerformanceBox::PlaceAllCtrl(void)
{


	CRect rc;
	this->GetClientRect(rc);
	CRect rcWaveList;

	PerferListData * pData = (PerferListData*) mPItemList.GetItemData(0);
	BOOL Ret = FALSE;

	if(pData!=NULL)
	{
		if(pData->pWaveBox != NULL)
		{
			Ret= TRUE;
		}

	}


	if( Ret)     // && rc.Height()>400 
	{
		int WaveBoxBottom = rc.Height()-200;

		if(WaveBoxBottom-72<80)  WaveBoxBottom = 72+80;

		if(rc.Width()<BOX_W_MIN)
		{
			rcWaveList.SetRect(22,72,BOX_W_MIN-20,WaveBoxBottom);	
		}
		else
		{

			rcWaveList.SetRect(22,72,rc.right-20,WaveBoxBottom);	

		}



		int n = mPItemList.GetItemCount();

		INT nSel = 0;
		nSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );




		for(int i=0;i<n;i++)
		{
			PerferListData * pPData = (PerferListData *)mPItemList.GetItemData(i);
			if(pPData==NULL)continue ;
			if(pPData->pInfoBox == NULL)continue ;
			
			if(pPData->pInfoBox->IsWindowVisible()|| i==nSel)  //ʵ����ֻ��һ�����ƶ�  ��Ҫ�õ�ǰѡ�����ж� �ƶ��ĸ� ��Ϊ���ܸı�û��ѡ�������
			{
				//PerferListData * pPData = (PerferListData *)mPItemList.GetItemData(i);
				//if(pPData==NULL)continue ;
				CWnd *pWnd = NULL ;
				CWnd *pBoxWnd = pPData->pWaveBox;
				if(pBoxWnd == NULL)continue ;


				//------------------------------------------------------------------
				pWnd=GetDlgItem(IDC_TIP2);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.right-80,rcWaveList.top-15,80,15);  //�������ǿ��͸߲���λ��	
					InvalidateIfVisible(pWnd);
					
				}

				pWnd=GetDlgItem(IDC_TIP1);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.left,rcWaveList.top-15,rcWaveList.Width()-80,15);  //�������ǿ��͸߲���λ��				
					InvalidateIfVisible(pWnd);
				}




				//--------





				//------------------------------------------------------------------



				//����ͼ�ײ���ʾ��߶�15

				if(pPData->Type == PM_MEMORY || pPData->Type == PM_DISK)
				{

					rcWaveList.bottom-=80;
					if(pPData->Type == PM_DISK) rcWaveList.bottom+=30;
					if(rcWaveList.bottom-72<80)  rcWaveList.bottom  = 72+80;

				}
				if(pPData->Type == PM_ETHERNET  )
				{
					CRect rc1;
					this->GetClientRect(rc1);
					rcWaveList.bottom =rc1.bottom-150;				
					if(rcWaveList.bottom<155)  rcWaveList.bottom  = 155;

				}


				
				//----------------------------------------------

				if(theApp.FlagSummaryView)//SummaryView״̬����ͼ ��������
				{
					CRect rcTemp;
					this->GetClientRect(rcTemp);

					if(pPData->Type == PM_MEMORY || pPData->Type == PM_DISK)
					{
						rcWaveList.bottom = rcTemp.bottom-125;
					}
					else
					{
						rcWaveList.bottom = rcTemp.bottom-40;
					}

					

				}

				pHotBox->MoveWindow(rcWaveList);
				InvalidateIfVisible(pHotBox);
				


				//----------------------------------------------

				pBoxWnd->MoveWindow(rcWaveList);
				InvalidateIfVisible(pBoxWnd);

			

				//------------------------------------------------------------------

				pWnd=GetDlgItem(IDC_TIP3);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.left+2,rcWaveList.bottom,rcWaveList.Width()-80,15);  //�������ǿ��͸߲���λ��
					InvalidateIfVisible(pWnd);
				}
				pWnd=GetDlgItem(IDC_TIP4);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.right-50,rcWaveList.bottom,50,15);
					InvalidateIfVisible(pWnd);
				}
				//-----------

			


				pWnd=GetDlgItem(IDC_TIP5);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.left+2,rcWaveList.bottom+19,rcWaveList.Width()-80,15);  //�������ǿ��͸߲���λ��
					InvalidateIfVisible(pWnd);
				}
				pWnd=GetDlgItem(IDC_TIP6);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.right-80,rcWaveList.bottom+19,80,15);  //�������ǿ��͸߲���λ��				
					InvalidateIfVisible(pWnd);
				}

				

				//------------------------------�ƶ��ڶ�����ͼ------------------------------------

				pBoxWnd = pPData->pOtherWnd;
				if(pBoxWnd != NULL)
				{

					rcWaveList.OffsetRect(0,rcWaveList.Height()+34);
					if(pPData->Type == PM_MEMORY)
					{
						rcWaveList.bottom = rcWaveList.top+50;
						rcWaveList.DeflateRect(2,0);
						rcWaveList.top+=2;

					}
					else
					{
						rcWaveList.bottom = rcWaveList.top+66;
					}
					pBoxWnd->MoveWindow(rcWaveList);
					InvalidateIfVisible(pBoxWnd);
					
				}


				pWnd = pPData->pInfoBox;
				if(pWnd == NULL)continue ;

				if(theApp.FlagSummaryView) //SummaryView״̬�ƶ����������ص�λ�ã�
				{
					pWnd->MoveWindow(rcWaveList.left,rcWaveList.bottom+10000,1,1);	
				}
				else
				{
				pWnd->MoveWindow(rcWaveList.left,rcWaveList.bottom+27,rcWaveList.Width(),160);	
				}

				InvalidateIfVisible(pWnd);

				//---------------------

				pWnd=GetDlgItem(IDC_TIP7);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.left+2,rcWaveList.bottom,rcWaveList.Width()-80,15);  //�������ǿ��͸߲���λ��
					InvalidateIfVisible(pWnd);
				}
				pWnd=GetDlgItem(IDC_TIP8);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.right-80,rcWaveList.bottom,80,15);  //�������ǿ��͸߲���λ��				
					InvalidateIfVisible(pWnd);
				}



			} //if 


		}  //for	 



	}








	//--------------

	CWnd *pWnd= NULL;
	CRect rcTemp;
	CRect rcTemp2;



	//	rcTemp.SetRect(5,12,205,rc.bottom);
	//mPItemList.MoveWindow(rcTemp);






	pWnd=GetDlgItem(IDC_ITEMLABEL);
	if(pWnd!=NULL)
	{

		pWnd->GetWindowRect(rcTemp2);
		this->ScreenToClient(rcTemp2);

	}




	/*pWnd=GetDlgItem(IDC_TIP2);
	if(pWnd!=NULL)
	{

	pWnd->GetWindowRect(rcTemp);
	this->ScreenToClient(rcTemp);
	int W=rcTemp.Width();
	rcTemp.right = rc.right-25;
	rcTemp.left=rcTemp.right-W;
	pWnd->MoveWindow(rcTemp);
	}*/





	pWnd=GetDlgItem(IDC_ITEMNAME);
	if(pWnd!=NULL)
	{

		pWnd->GetWindowRect(rcTemp);
		this->ScreenToClient(rcTemp);
		rcTemp.right = rcWaveList.right;
		rcTemp.left=rcTemp2.right+5;
		pWnd->MoveWindow(rcTemp);
		InvalidateIfVisible(pWnd);
	}





}





//-----------------------------

BOOL CPerformanceBox::_NumberIsPN(int Number) //����
{
	BOOL Ret;

	int i ;
	for( i=2;i<=Number-1;i++)
		if(Number%i==0)
		{  
			Ret = FALSE;
			break;
		}
		if(i>Number/2) Ret =TRUE;

		return Ret;
}

void CPerformanceBox::_GetDiskOtherStaticInfo()
{ 


	// �Է����߳���


	CString StrPagingfilePath;


	CString strPath=_T("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management");//ע����Ӽ�·��  
	CRegKey regkey;//����ע��������  
	LONG lResult;//LONG�ͱ�������Ӧ���  
	lResult=regkey.Open(HKEY_LOCAL_MACHINE,LPCTSTR(strPath),KEY_QUERY_VALUE ); //��ע�����   ע�� Ҫ�� �� ��Ȩ�� KEY_ALL_ACCESS ��ĳЩ�û��ᵼ�´���
	if (lResult==ERROR_SUCCESS)  
	{  
		WCHAR StrData[90] = {0};  
		DWORD dwSize=90;   
		//��ȡProcessorNameString�ֶ�ֵ  
		if (ERROR_SUCCESS == regkey.QueryMultiStringValue(_T("ExistingPageFiles"),StrData,&dwSize))  
		{  
			StrPagingfilePath = StrData;  
		}  

	}  


	regkey.Close();//�ر�ע���  

	//MSB_S(StrPagingfilePath);

	// Bug fix Win7: StrPagingfilePath may be empty (no registry value) in which
	// case Find(L':') returns -1 and -1-1=-2 -- GetAt(-2) is UB and corrupts stack.
	WCHAR CharPagingfile = L' ';
	int nColonIdx = StrPagingfilePath.Find(L':');
	if (nColonIdx > 0 && nColonIdx < StrPagingfilePath.GetLength())
	{
		CharPagingfile = StrPagingfilePath.GetAt(nColonIdx - 1);
	}

	PerferListData * pData = NULL ;


	CString StrPdh;

	int nCount = mPItemList.GetItemCount();
	for(int i=CurrentDiskCount+2;i<nCount;i++)
	{	


		pData = (PerferListData*)mPItemList.GetItemData(i);

		if(pData==NULL) continue ;

		if(pData->Type != PM_DISK)
		{
			break ;

		}


		//-------------------- ϵͳ�������Ϣ -------------------

		CString StrSysPath;
		::GetSystemDirectory(StrSysPath.GetBuffer(MAX_PATH),MAX_PATH);
		StrSysPath.ReleaseBuffer();

		CString StrIsSysDisk=L"No";
		CString StrIsPagingFile = L"No";
		if(pData->StrOther0.Find(StrSysPath.GetAt(0))!=-1) StrIsSysDisk=L"Yes";
		if(pData->StrOther0.Find(CharPagingfile)!=-1) StrIsPagingFile=L"Yes";

		//theApp.m_pMainWnd->SetWindowTextW(pData->StrOther0);
		pData->pInfoBox->Info[6].StrInfo = pData->pInfoBox->Info[6].StrInfo+L"\n"+StrIsSysDisk+L"\n"+StrIsPagingFile;
		pData->pInfoBox->Invalidate();

	}




}



void CPerformanceBox::UpdateInfoBox()
{


	if(!ShowThisPage) return ;


	INT nSel = 0;
	//pNMLV->iItem
	nSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );

	if(nSel<0 ) return;

	PerferListData * pPData = (PerferListData *)mPItemList.GetItemData(nSel);


	if(pPData->Type == PM_CPU)
	{
		this->_UpdateCpuInfoBox();
	}
	if(pPData->Type == PM_MEMORY)
	{
		pPData->pOtherWnd->Invalidate(0);
		this->_UpdateMemoryInfoBox();
	}

	//if(pPData->Type == PM_ETHERNET) ���� ��������ִ��_UpdateNetworkInfoBox(nSel,pPData );

	//if(pPData->Type == PM_DISK)���� ��������ִ��  this->_UpdateDiskInfoBox( );



	pPData->pInfoBox->Invalidate();


}



void CPerformanceBox::_UpdateMemoryInfoBox(BOOL UpdateAll )
{


	if(!ShowThisPage) return ;

	double  Num,Num2;

	CString StrInUse;

	Num = (double)(theApp.PerformanceInfo.InUsePhysMem/1024/1024);
	StrInUse.Format(L"%0.1f MB",Num );

	if(Num>=1024)
	{
		Num= Num/1024;
		StrInUse.Format(L"%0.1f GB",Num );
	}
	//=-------------

	CString StrAvialable;

	//Num =(double)(MyPMInfo.PhysicalAvailable*MyPMInfo.PageSize/1024/1024);
	Num = (double)(( MyPMInfo.PhysicalAvailable*MyPMInfo.PageSize)/1024/1024);
	StrAvialable.Format(L"%0.1f MB",Num );
	if(Num>=1024)
	{
		Num= Num/1024;
		StrAvialable.Format(L"%0.1f GB",Num );
	}

	//---------------Committed--------------

	CString StrCommitCurrent,StrCommit;

	double CommitCurrent = (double)MyPMInfo.CommitPeak*MyPMInfo.PageSize;
	double CommitTotal = (double)MyPMInfo.CommitLimit*MyPMInfo.PageSize;




	Num = (double)(CommitCurrent/1024/1024);
	StrCommitCurrent.Format(L"%0.1f",Num );
	Num2 = (double)(CommitTotal/1024/1024);
	StrCommit.Format(L"%0.1f MB",Num2 );
	if(Num2>=1024)
	{
		Num= Num/1024;
		StrCommitCurrent.Format(L"%0.1f",Num );
		Num2= Num2/1024;
		StrCommit.Format(L"%0.1f GB",Num2 );
	}

	//------------------ Paged Pool ------------------

	CString StrPagedPool;
	//Num = PM.PdhGetInfo(L"\\Memory\\Pool Paged Bytes");
	Num =(double)(MyPMInfo.KernelPaged*MyPMInfo.PageSize);
	Num= Num/1024/1024;
	StrPagedPool.Format(L"%0.1f MB",Num );
	if(Num>=1024)
	{
		Num= Num/1024;
		StrPagedPool.Format(L"%0.1f GB",Num );
	}
	//------------------ Non-Paged Pool ------------------


	CString StrNonPagedPool;
	//Num = PM.PdhGetInfo(L"\\Memory\\Pool Nonpaged Bytes");
	Num =(double)(MyPMInfo.KernelNonpaged*MyPMInfo.PageSize);

	Num= Num/1024/1024;
	StrNonPagedPool.Format(L"%0.1f MB",Num );
	if(Num>=1024)
	{
		Num= Num/1024;
		StrNonPagedPool.Format(L"%0.1f GB",Num );
	}





	//-----------StrSystemCache ------------



	CString StrSystemCache;
	//Num =(double)(MyPMInfo.SystemCache*MyPMInfo.PageSize);
	Num = theApp.PerformanceInfo.Mem_Modified+theApp.PerformanceInfo.Mem_Standby;
	Num= Num/1024/1024;
	StrSystemCache.Format(L"%0.1f MB",Num );
	if(Num>=1024)
	{
		Num= Num/1024;
		StrSystemCache.Format(L"%0.1f GB",Num );
	}











	/*CString TTT;
	TTT.Format(L"%d",MemStatus.ullTotalPhys/1024/1024/1024);
	theApp.GetMainWnd()->SetWindowTextW(TTT);*/



	pInfoBoxMemory->Info[0].StrInfo  = StrInUse ;
	pInfoBoxMemory->Info[1].StrInfo  = StrAvialable ;
	pInfoBoxMemory->Info[2].StrInfo  = StrCommitCurrent+L"/"+StrCommit;  
	pInfoBoxMemory->Info[3].StrInfo  = StrSystemCache ;  
	pInfoBoxMemory->Info[4].StrInfo  = StrPagedPool ; 
	pInfoBoxMemory->Info[5].StrInfo  = StrNonPagedPool ; 


	if(UpdateAll)
	{

		CSMBIOS  MySMBios;

		UINT  Speed;
		int  nSlotUsed;
		int  nSlot;
		WCHAR   StrFormFactor[100];


		theApp.PerformanceInfo.StrMemoryType = MySMBios.GetMemoryInfo(&nSlotUsed,&nSlot,&Speed,StrFormFactor);



		if(theApp.PerformanceInfo.TotalPhysMem ==0)
		{
			MEMORYSTATUSEX  MemStatus;
			MemStatus.dwLength = sizeof(MemStatus);
			BOOL Ret = GlobalMemoryStatusEx(&MemStatus);
			if(Ret)
			{
				theApp.PerformanceInfo.TotalPhysMem = MemStatus.ullTotalPhys;
			}
		}

		//StringCchCat(StrFormFactor,100,L"\n");

		double MemHWReserved = ((double)theApp.PerformanceInfo.InstalledMemKB*1024-theApp.PerformanceInfo.TotalPhysMem)/1024/1024; //MB



		pInfoBoxMemory->Info[6].StrInfo.Format(L"%d MHz \n%d of %d \n%s \n%0.0f MB",Speed,nSlotUsed,nSlot,StrFormFactor,MemHWReserved);  


       



	}



}

void CPerformanceBox::OnContextMenu(CWnd* pWnd, CPoint point)
{
	
	


	// TODO: Add your message handler code here
	CMenu PopMenu;
	CMenu *pMenu;

	PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_BASE ) );
	pMenu = PopMenu.GetSubMenu(1);  //1�� ��� ��Ӧ�� �˵� �ͱ�ǩ˳���Ӧ


 

	 



	//ע��:��Ҫͨ��ѡ������ȷ����ǰ��ʾ�Ǹ��豸���� ��Ϊ���ܸ���û��ѡ�������

	int n= mPItemList.GetItemCount();


	PerferListData *pData = NULL ;
	for(int i=0;i<n;i++)
	{
		pData =(PerferListData *) mPItemList.GetItemData(i);
		if(pData==NULL) continue;
		//������ȡ�Ǹ�����pData �⽫Ӱ��˵���ʾ
		if(pData->pInfoBox->IsWindowVisible())
		{
			break;
		}


	}
	

	if(pData==NULL) return;


	if(pData->Type != PM_CPU) //ȥ�������ڴ���ʾ����
	{
		 
		pMenu->DeleteMenu(0,MF_BYPOSITION);
		pMenu->DeleteMenu(0,MF_BYPOSITION);
		pMenu->DeleteMenu(0,MF_BYPOSITION);

	}

	if(pData->Type != PM_ETHERNET)
	{
		 
		pMenu->DeleteMenu(ID_PERFORMANCE_VIEWNETWORKDETAILS,MF_BYCOMMAND);
	}

	CRect rcBox;

	pData->pWaveBox->GetWindowRect(rcBox);

	

	CPoint CurPos ;
	GetCursorPos(&CurPos);

	
	int L1SubMenuID = 1; //View �Ӳ˵�λ��

	if(pData->Type == PM_CPU)
	{
		L1SubMenuID = 4; //��ʾcpuʱ��λ�ò�ͬ������
		if( ! rcBox.PtInRect(CurPos) )
		{
			L1SubMenuID = 3; //��ʾcpuʱ��λ�ò�ͬ������
			pMenu->DeleteMenu(ID_PERFORMANCE_SHOWKERNELTIMES,MF_BYCOMMAND);
		}
	}





	//--------------------------------
 

	
	int iDiskMenu =0;
	UINT BaseID = ID_DISK_NONE;
	pData = NULL ;

 
	 
	CMenu *pSubMenu1 = pMenu->GetSubMenu(L1SubMenuID);
	CMenu *pSubMenu2 =NULL;
	if(pSubMenu1 )
	{
		pSubMenu2=pSubMenu1->GetSubMenu(2);
	}
 



	if(pSubMenu2)
	{
		for(int i=0;i<n;i++)
		{
			pData =(PerferListData *) mPItemList.GetItemData(i);
			if(pData==NULL) continue;	
			if(pData->Type == PM_DISK)
			{
				CString StrID;
				StrID.Format(L"Disk %d %s",pData->ID,L"");
				pSubMenu2->AppendMenu(MF_STRING|MF_BYCOMMAND,++BaseID ,StrID);
			}
		}

		pSubMenu2->DeleteMenu(0,MF_BYPOSITION);

	}


	//--------------------------------



	
	pMenu->TrackPopupMenu(TPM_LEFTALIGN,CurPos.x,CurPos.y,this);
}

int CPerformanceBox::UpdateAllPMInfo(void)
{


	int n = mPItemList.GetItemCount();
	
	int iSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );

	double *PerformanceData  = new double[n]; 
	double *PerformanceDataA  = new double[n]; 
	double *PerformanceDataB  = new double[n]; 

	//ULONG64 TempData;


	CString StrTemp;


	if(ShowThisPage)
	{
		mPItemList.SetRedraw(0);
	}



	// ����ͼ �ڲ����Ʋ��� 0��1  ��������������ʾ���Y���꣨����50% =  0.5��  �����ǰٷ���  ���Եõ��ٷ�����Ҫת�� ���������ظ��˳� 100 



	//------------------------------------CPU   -----------------------------



	//------------------------------------Memory -------------------------


	double MemUsagePercent;

	ZeroMemory(&MyPMInfo, sizeof(PERFORMANCE_INFORMATION));
	MyPMInfo.cb = sizeof(MyPMInfo);
	GetPerformanceInfo(&MyPMInfo,sizeof(PERFORMANCE_INFORMATION));



	
	//Ϊ�˻��� memory composition
	theApp.PerformanceInfo.Mem_Modified =  PM.PdhGetInfo(L"\\Memory\\Modified Page List Bytes");	
	//�˷�����ȡ Mem_Standby���ɿ� 
	if(iSel == 1)
	theApp.PerformanceInfo.Mem_Standby = MyPMInfo.PhysicalAvailable*MyPMInfo.PageSize - PM.PdhGetInfo(L"\\Memory\\Free & Zero Page List Bytes");


	//------------------------------------------------------------------------------------------------------------------------------------    

	//MEMORYSTATUSEX  MemStatus;
	//MemStatus.dwLength = sizeof(MemStatus);
	//BOOL Ret;
	//BOOL Ret = GlobalMemoryStatusEx(&MemStatus);
	//theApp.PerformanceInfo.PagedVirtual = MemStatus.ullTotalVirtual;
	//theApp.PerformanceInfo.CommitLimit =   MemStatus.ullTotalPageFile;
	//theApp.PerformanceInfo.CommitCurrent =MemStatus.ullTotalPageFile-MemStatus.ullAvailPageFile;

	//------------------------------------------------------------------------------------------------------------------------------------

	// Ԥ�ȶ�����  PERFORMANCE_INFORMATION MyPMInfo;


 
	theApp.PerformanceInfo.InUsePhysMem = (DWORDLONG)( (MyPMInfo.PhysicalTotal-MyPMInfo.PhysicalAvailable)*MyPMInfo.PageSize-theApp.PerformanceInfo.Mem_Modified-(theApp.PerformanceInfo.InstalledMemKB*1024-theApp.PerformanceInfo.TotalPhysMem));
 


	theApp.PerformanceInfo.MemoryUsage =  ((double)(theApp.PerformanceInfo.InUsePhysMem))/((double) (theApp.PerformanceInfo.TotalPhysMem)) ;

	if(theApp.PerformanceInfo.MemoryUsage>1.0  ) theApp.PerformanceInfo.MemoryUsage = 1.0;
	
	MemUsagePercent = theApp.PerformanceInfo.MemoryUsage *100;//// תΪ�ٷֱ�

	if(ShowThisPage)
	{
		StrTemp.Format(L"%.1f/%.1f GB (%.2f%%)",((double)(theApp.PerformanceInfo.InUsePhysMem ))/1024/1024/1024,(double) (theApp.PerformanceInfo.TotalPhysMem)/1024/1024/1024,MemUsagePercent  );
		mPItemList.SetItemText(1,1,StrTemp);
		MySetItem(1,StrTemp);
	}


	PerformanceData[1]=theApp.PerformanceInfo.MemoryUsage ;
	PerformanceDataA[1] = PerformanceDataB[1] =PerformanceData[1];
	theApp.PerformanceInfo.MemoryUsage = MemUsagePercent;// תΪ�ٷֱ�



	//------------------------------------Disk    -------------------------



	double SumDiskUsage=0; int nDiskCount=0;


	PerferListData *pPData = NULL;


	if(! FlagStartDiskMon )  goto SKIPDISK;

		// Start the WMI disk monitor thread once per session, BEFORE iterating
		// the disk items. Previously EnsureWmiDiskMonitor() was called inside
		// the per-disk loop, so if the system reported zero disks (registry
		// enumeration failure, no physical drives yet, etc.) the WMI monitor
		// thread was never spawned and the cache stayed at Ready=0 forever,
		// leaving the disks at 0% even after a disk appeared.
		EnsureWmiDiskMonitor(); // idempotent
		// Also start the PDH fallback monitor. On Win7 systems where the
		// "WMI Performance Adapter" service (wmiapsrv) is disabled (very
		// common on Spanish/non-administrator Win7 installs) the WMI perf
		// counter query returns no data and every disk stays at 0%. PDH
		// reads PhysicalDisk counters directly via the same native API the
		// Windows Task Manager uses, so it works regardless of WMI service
		// state, admin rights, or system locale. The PDH cache is preferred
		// over the WMI cache when it has data (Ready=1).
		EnsurePdhDiskMonitor(); // idempotent
		// Last-resort fallback: NtQuerySystemInformation reads the disk
		// counters directly out of the kernel via ntdll. Unlike PDH/WMI
		// this works on every Win7 build regardless of perf counter
		// registration, WMI service state, or admin rights. The UI uses
		// NtSys data only when both PDH and WMI fail to produce data.
		EnsureNtSysDiskMonitor(); // idempotent

		for(int nItem=2;nItem<n;nItem++)
		{

			pPData = (PerferListData *)mPItemList.GetItemData(nItem);
			if(pPData==NULL) continue ;
			if(pPData->Type == PM_ETHERNET   ) break ;

			// Disk activity: read from the WMI background monitor cache.
			// The cache is populated by Thread_MonitorWmiDisk() every ~1 s.
			double dReadBps = 0, dWriteBps = 0, dActivePct = 0;
			double AvgResponseTime = 0;

			int diskIdx = pPData->ID;
			// Prefer the PDH cache (no WMI dependency, works on every Win7
			// build). Fall back to the WMI cache if PDH hasn't produced data
			// for this disk yet (e.g. on systems where PDH instance names
			// don't match the disk IDs).
			if(diskIdx >= 0 && diskIdx < PDH_DISK_MAX && g_PdhDisk[diskIdx].Ready)
			{
				dReadBps   = g_PdhDisk[diskIdx].ReadBps;
				dWriteBps  = g_PdhDisk[diskIdx].WriteBps;
				dActivePct = g_PdhDisk[diskIdx].Pct;
				if(!_finite(dReadBps)   || dReadBps < 0)   dReadBps = 0;
				if(!_finite(dWriteBps)  || dWriteBps < 0)  dWriteBps = 0;
				if(!_finite(dActivePct) || dActivePct < 0) dActivePct = 0;
			}
			else if(diskIdx >= 0 && diskIdx < WMI_DISK_MAX && g_WmiDisk[diskIdx].Ready)
			{
				dReadBps   = g_WmiDisk[diskIdx].ReadBps;
				dWriteBps  = g_WmiDisk[diskIdx].WriteBps;
				dActivePct = g_WmiDisk[diskIdx].Pct;
				if(!_finite(dReadBps)   || dReadBps < 0)   dReadBps = 0;
				if(!_finite(dWriteBps)  || dWriteBps < 0)  dWriteBps = 0;
				if(!_finite(dActivePct) || dActivePct < 0) dActivePct = 0;
			}

		// AvgResponseTime isn't available via WMI (it's a separate perf
		// counter that requires admin DISK_PERFORMANCE). Leave it at 0.

		nDiskCount++;

		//Read
		PerformanceDataA[nItem] = dReadBps;
		if(PerformanceDataA[nItem]<0.001)PerformanceDataA[nItem]=0.0;

		//Write
		PerformanceDataB[nItem] = dWriteBps;
		if(PerformanceDataB[nItem]<0.001)PerformanceDataB[nItem]=0.0;

		//Active % - PDH % Disk Time is 0..100; DISK_PERFORMANCE was 0..1.
		PerformanceData[nItem] = dActivePct/100.0;
		if(!_finite(PerformanceData[nItem])||PerformanceData[nItem]<0) PerformanceData[nItem]=0;
		if(PerformanceData[nItem]>1) PerformanceData[nItem]=1;

		if(pPData->pInfoBox->IsWindowVisible())
		{
			_UpdateDiskInfoBox(pPData,PerformanceDataA[nItem],PerformanceDataB[nItem],PerformanceData[nItem]*100,AvgResponseTime);
		}


		//---------------------------------------------------------------------------

		if( ((int)theApp.UpTimeSec) % 6 == 0 )
		{
			_TryToChangeDiskMaxVar(PerformanceDataA[nItem],PerformanceDataB[nItem],pPData);
		}


		PerformanceDataA[nItem] = PerformanceDataA[nItem]/(pPData->MaxVar);
		PerformanceDataB[nItem] = PerformanceDataB[nItem]/(pPData->MaxVar);

		if(PerformanceDataA[nItem]<0.001 || PerformanceDataA[nItem]>1)PerformanceDataA[nItem]=0;
		if(PerformanceDataB[nItem]<0.001 || PerformanceDataB[nItem]>1)PerformanceDataB[nItem]=0;

		 



		if(ShowThisPage)		{

			double DiskPct = PerformanceData[nItem]*100;
			// Bug fix Win7 non-admin: clamp NaN/Inf so the inline disk % in the
			// list never prints "-nan%" or "inf%".
			if(_finite(DiskPct) == 0) DiskPct = 0;
			if(DiskPct < 0) DiskPct = 0;
			if(DiskPct > 100) DiskPct = 100;
			StrTemp.Format(L"%0.2f%%",DiskPct); //��ʱΪ�ٷ�֮��
			//mPItemList.SetItemText(nItem,1,StrTemp);
			MySetItem(nItem,StrTemp);
		}

		SumDiskUsage = SumDiskUsage+PerformanceData[nItem];



	}



	// Prefer the PDH "_Total" aggregate instance for the header % — that's
	// what Windows Task Manager uses, and it correctly reflects system-wide
	// disk activity (rather than masking a busy disk by averaging across an
	// idle one). Fall back to the WMI _Total instance if PDH hasn't seen
	// it yet, and finally to the per-disk average only when neither source
	// reported an aggregate.
	if(g_PdhDiskTotalReady)
	{
		double totalPct = g_PdhDiskTotalPct;
		if(_finite(totalPct) == 0) totalPct = 0;
		if(totalPct < 0)   totalPct = 0;
		if(totalPct > 100) totalPct = 100;
		theApp.PerformanceInfo.TotalDiskUsage = totalPct;
	}
		else if(g_WmiDiskTotalReady)
	{
		double totalPct = g_WmiDiskTotalPct;
		if(_finite(totalPct) == 0) totalPct = 0;
		if(totalPct < 0)   totalPct = 0;
		if(totalPct > 100) totalPct = 100;
		theApp.PerformanceInfo.TotalDiskUsage = totalPct;
	}
	else if(g_NtSysReady)
	{
		// Last-resort fallback: % computed by sampling kernel disk
		// counters every 100 ms and tracking the busy fraction over a
		// rolling 10 s window. Available on every Win7 build.
		double totalPct = g_NtSysTotalPct;
		if(_finite(totalPct) == 0) totalPct = 0;
		if(totalPct < 0)   totalPct = 0;
		if(totalPct > 100) totalPct = 100;
		theApp.PerformanceInfo.TotalDiskUsage = totalPct;
	}
	else if(nDiskCount > 0)
	{
		// Bug fix Win7 non-admin: previous code divided by nDiskCount
		// unconditionally, producing NaN (-nan%) in the disk header when no
		// physical disk counters were available (typical on Win7 non-admin).
		// Guard the divide.
		double avgPct = SumDiskUsage/nDiskCount*100;
		if(_finite(avgPct) == 0) avgPct = 0;
		if(avgPct < 0)   avgPct = 0;
		if(avgPct > 100) avgPct = 100;
		theApp.PerformanceInfo.TotalDiskUsage = avgPct;
	}
	else
	{
		theApp.PerformanceInfo.TotalDiskUsage = 0;
	}



SKIPDISK:
 



	//---------------------------------		Network    ---------------------------------


	double SumNetUsage=0; int nNetCount=0;



	MIB_IF_TABLE2   *pIfTable = NULL;
	ULONG          dwSize   = 0;
	DWORD          dwRet;
	dwRet = GetIfTable2( &pIfTable );

	if ( dwRet == ERROR_NOT_ENOUGH_MEMORY )
	{
		 ;
	}
	else if(dwRet == STATUS_SUCCESS )
	{
		CString Str,s1,s2,s3,StrTemp;
		for ( ULONG  i=0; i<pIfTable->NumEntries; i++ )
		{

			if((pIfTable->Table[i].Type == IF_TYPE_IEEE80211 )|| (pIfTable->Table[i].Type ==IF_TYPE_ETHERNET_CSMACD) )
			{


				ULONG Index =pIfTable->Table[i].InterfaceIndex;

				map<int, int>::iterator Iter= NetAdapterList.find((int)Index); // <����ID,�б���ĿID>
				int nItemID;

				
				if(Iter != NetAdapterList.end())//����
				{
					nNetCount++;

					nItemID = Iter->second;
 
					 
					pPData = (PerferListData*)mPItemList.GetItemData(nItemID);	

					if(pPData!=NULL) 
					{							
						 

						// InOctets  OutOctets��λΪ�ֽ�
						//------------����
						if(pPData->DataA==0)    
						{
							PerformanceDataA[nItemID]=0;
						}
						else
						{
							PerformanceDataA[nItemID]= (double)(pIfTable->Table[i].InOctets - pPData->DataA)/(double)theApp.AppSettings.TimerStep;
						}

						//------------����
						if(pPData->DataB==0) 	
						{
							PerformanceDataB[nItemID] =0;
						}
						else
						{
							PerformanceDataB[nItemID]=(double)(pIfTable->Table[i].OutOctets - pPData->DataB)/(double)theApp.AppSettings.TimerStep;
						}


						double ThisUsaeg = (PerformanceDataA[nItemID]+PerformanceDataB[nItemID])/(pIfTable->Table[i].TransmitLinkSpeed/8);

						//��ֹ���ָ�ֵ
						if(ThisUsaeg>0.001)
						{
							SumNetUsage = SumNetUsage+ThisUsaeg;
						}



						//-----------------------------------------

						if(ShowThisPage)
						{

							StrTemp.Format(L"S: %.0f Kbps R: %.0f Kbps", (PerformanceDataB[nItemID]/1024*8), (PerformanceDataA[nItemID]/1024*8));
							//mPItemList.SetItemText(nItemID,1,StrTemp);
							MySetItem(nItemID,StrTemp);
						}

						if(pPData->pInfoBox->IsWindowVisible())
						{
							_UpdateNetworkInfoBox(pPData,PerformanceDataA[nItemID],PerformanceDataB[nItemID]);
						}

						if( ((int)theApp.UpTimeSec) % 6 == 0 )	
						{
							_TryToChangeNetworkMaxVar(PerformanceDataA[nItemID],PerformanceDataB[nItemID],pPData);
						}

						PerformanceDataA[nItemID] =PerformanceDataA[nItemID]/(pPData->MaxVar);
						PerformanceDataB[nItemID] =PerformanceDataB[nItemID]/(pPData->MaxVar);



						pPData->DataA =pIfTable->Table[i].InOctets;
						pPData->DataB =pIfTable->Table[i].OutOctets;


					} //pPData!=NULL

				}//Iter != NetAdapterList.end()


			}	//Type			

		}//for


		FreeMibTable (pIfTable);
	}




	if(nNetCount>0)
	{
		theApp.PerformanceInfo.TotalNetUsage = SumNetUsage/nNetCount*100; //�ٷֱ�
	}
	else
	{
		theApp.PerformanceInfo.TotalNetUsage = 0;
	}





	//----------------------------------���²���ͼ-----------------------------







	CWaveBox *pBox = NULL;
	CWaveBox *pBox2 = NULL;




	//---------------��cpu��ʾ״̬ ��ȡ���ݼ����� ��ͼ---------------------

	double CpuUsage,KernelUsage;



	_GetLogicalProcessorUsage(pCpuBox, &CpuUsage,&KernelUsage); //����ִ�к���

	pPData = (PerferListData *)mPItemList.GetItemData(0);
	if(pPData!=NULL) 
	{
		pBox = pPData->pWaveBox;

		if(pBox->IsWindowVisible()) pBox->Invalidate();

	}




	//--------------------


	if(theApp.AppSettings.ProcessorDisplayMode == 2)
	{
		for(int i =0;i<theApp.PerformanceInfo.nLogicalProcessor;i++)
		{
			pHotBox->ArrayPerLogicalCpuUsage[i] = pCpuBox->Num[i][60];
		} 
		pHotBox->Invalidate(0);

	}


	//----------------------����cpu----------------------

	memmove(&pTotalCpuBox->Num[0][0],&pTotalCpuBox->Num[0][1],ArraySize);
	memmove(&pTotalCpuBox->Num2[0][0],&pTotalCpuBox->Num2[0][1],ArraySize);

	/*for(int i=0;i<61-1;i++)
	{
	pTotalCpuBox->Num[0][i] = pTotalCpuBox->Num[0][i+1];
	pTotalCpuBox->Num2[0][i] = pTotalCpuBox->Num2[0][i+1];
	}*/


	theApp.PerformanceInfo.CpuUsage = CpuUsage*100;

	PerformanceDataA[0] = PerformanceData[0]= CpuUsage;
	PerformanceDataB[0] =KernelUsage;

	pTotalCpuBox->Num[0][60] = (float)PerformanceDataA[0]; //
	pTotalCpuBox->Num2[0][60] =(float)PerformanceDataB[0];

	
	//CPU��ǰ�ٶ� 
	//���ִ���ڸ����Ҳ������Ϣ��֮ǰ  ���� _UpdateCpuInfoBox�в����ٳԻ�ȡ CurrentSpeed

	if(ShowThisPage)
	{
	
		pInfoBoxCpu->Info[1].StrInfo.Format(L"%0.2f GHz",_GetSurrentCpuSpeed());


		StrTemp.Format(L"%.2f%%  %s",theApp.PerformanceInfo.CpuUsage,pInfoBoxCpu->Info[1].StrInfo);
		MySetItem(0,StrTemp);		
	//	mPItemList.SetItemText(0,1,StrTemp);

	}







	//---------------  ���� �� ��ʾ����---------------------




	for(int nItem=1;nItem<n;nItem++)
	{
		pPData = (PerferListData *)mPItemList.GetItemData(nItem);
		if(pPData==NULL) continue ;
		pBox = pPData->pWaveBox;
		// Bug fix Win7: pPData->pOtherWnd is CMemCompBox* for memory items and
		// NULL for net items. Cast unconditionally to CWaveBox* and deref Num/Num2
		// corrupts heap. Only treat as CWaveBox for disk items.
		pBox2 = NULL;
		if(pPData->Type == PM_DISK)
		{
			pBox2 = (CWaveBox *)pPData->pOtherWnd;
		}
		if(pBox==NULL)  continue ;

		//----------------����ǰ��-------------------



		memmove(&pBox->Num[0][0],&pBox->Num[0][1],ArraySize);
		memmove(&pBox->Num2[0][0],&pBox->Num2[0][1],ArraySize);
		/*
		for(int i=0;i<61-1;i++)
		{
		pBox->Num[0][i] = pBox->Num[0][i+1];
		pBox->Num2[0][i] = pBox->Num2[0][i+1];

		}*/

		if(pPData->Type == PM_DISK)   //���� �ڶ���box
		{
			if(pBox2!=NULL)
			{/*
			 for(int i=0;i<61-1;i++)
				{
				pBox2->Num[0][i] = pBox2->Num[0][i+1];
				pBox2->Num2[0][i] = pBox2->Num2[0][i+1];
				}*/

				memmove(&pBox2->Num[0][0], &pBox2->Num[0][1],ArraySize);
				memmove(&pBox2->Num2[0][0],&pBox2->Num2[0][1],ArraySize);


				pBox2->Num[0][60] =(float) PerformanceDataA[nItem];			
				pBox2->Num2[0][60] = (float)PerformanceDataB[nItem];

				if(pBox2->IsWindowVisible())pBox2->Invalidate(); //�����ж� IsWindowVisible �����λ���

			}

			pBox->Num[0][60] = (float)PerformanceData[nItem];

		}
		else  if(pPData->Type == PM_MEMORY)
		{
			pBox->Num[0][60] = (float)PerformanceData[nItem];			
		}
		else
		{
			pBox->Num[0][60] = (float)PerformanceDataA[nItem];
			pBox->Num2[0][60] = (float)PerformanceDataB[nItem];
		}




		if(pBox->IsWindowVisible()) pBox->Invalidate();




	}



	if(ShowThisPage)
	{
		UpdateInfoBox();
		mPItemList.SetRedraw(1);
		mPItemList.InvalidateRect(NULL,0);
	}





	delete [] PerformanceData ;PerformanceData=NULL;
	delete [] PerformanceDataA ;PerformanceDataA=NULL;
	delete [] PerformanceDataB ;PerformanceDataB=NULL;





	return 0;
}

void CPerformanceBox::OnPop_ChangeGraphToOverallutilization()
{
	// TODO: Add your command handler code here

	if(theApp.AppSettings.ProcessorDisplayMode == 0) return;


	theApp.AppSettings.ProcessorDisplayMode = 0;

	PerferListData *pData =(PerferListData *) mPItemList.GetItemData(0);
	if(pData==NULL) return;
	if(pData->pWaveBox==NULL)return;





	pData->pWaveBox = pTotalCpuBox;



	pTotalCpuBox->ShowWindow(SW_SHOW);
	pCpuBox->ShowWindow(SW_HIDE);
	pHotBox->ShowWindow(SW_HIDE);


	pData->pWaveBox->Invalidate();


	CWnd *pWnd=GetDlgItem(IDC_TIP3);
	if(pWnd!=NULL)	{  pWnd->ShowWindow(SW_SHOW); }
	pWnd=GetDlgItem(IDC_TIP4);
	if(pWnd!=NULL)	{  pWnd->ShowWindow(SW_SHOW); }


	pWnd=GetDlgItem(IDC_TIP1);
	if(pWnd!=NULL)	
	{ 
		CString StrTemp = STR_TIP1_CPU_TOTAL;
		mPItemList.SetItemText(0,3,StrTemp);
		pWnd->SetWindowTextW(StrTemp);
	}

	pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }



	this->PlaceAllCtrl();









}

void CPerformanceBox::OnPop_ChangeGraphToLogicalprocessors()
{
	if(theApp.AppSettings.ProcessorDisplayMode == 1) return;



	theApp.AppSettings.ProcessorDisplayMode = 1;

	PerferListData *pData =(PerferListData *) mPItemList.GetItemData(0);
	if(pData==NULL) return;
	if(pData->pWaveBox==NULL)return;

	pData->pWaveBox = pCpuBox;


	pCpuBox->ShowWindow(SW_SHOW);
	pTotalCpuBox->ShowWindow(SW_HIDE);
	pHotBox->ShowWindow(SW_HIDE);


	pData->pWaveBox->Invalidate();



	CString StrTime;

	CWnd *pWnd=GetDlgItem(IDC_TIP3);
	if(pWnd!=NULL)	{  pWnd->ShowWindow(SW_HIDE); pWnd->GetWindowTextW(StrTime); }
	pWnd=GetDlgItem(IDC_TIP4);
	if(pWnd!=NULL)	{  pWnd->ShowWindow(SW_HIDE); }


	pWnd=GetDlgItem(IDC_TIP1);
	if(pWnd!=NULL)	
	{ 
		CString StrTemp;    

		StrTemp.Format(STR_TIP1_CPU_LOGICAL,StrTime);
		
		mPItemList.SetItemText(0,3,StrTemp);
		pWnd->SetWindowTextW(StrTemp);
	}


	pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }



	this->PlaceAllCtrl();



}

void CPerformanceBox::_UpdateNetworkInfoBox(PerferListData* pData,double Received,double Sent,BOOL UpdateAll)
{

	if(!ShowThisPage) return ;


	double SentSpeed = Sent/1024*8;
	if(SentSpeed>1024)
	{
		pData->pInfoBox->Info[0].StrInfo.Format(L"%.2f Mbps",(SentSpeed/1024));
	}
	else
	{
		pData->pInfoBox->Info[0].StrInfo.Format(L"%.2f Kbps",(SentSpeed));
	}


	double ReceivedSpeed = Received/1024*8;

	if(ReceivedSpeed>1024)
	{
		pData->pInfoBox->Info[2].StrInfo.Format(L"%.2f Mbps",(ReceivedSpeed/1024));
	}
	else
	{
		pData->pInfoBox->Info[2].StrInfo.Format(L"%.2f Kbps",(ReceivedSpeed));
	}


	






	//---------get IP v4  -------
	/*CString StrKey = pData->StrOther0;
	HKEY hKey  ;
	DWORD dwIndex = 0;
	DWORD dwBufSize = 256;
	DWORD dwDataType;
	WCHAR szData[256];

	CString StrType ;
	if(pData->Type == PM_ETHERNET){StrType = L"Ethernet" ;}


	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,StrKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
	dwBufSize = 256;
	if(RegQueryValueEx(hKey, L"IPAddress", 0, &dwDataType, (BYTE*)szData, &dwBufSize) == ERROR_SUCCESS)
	{

	pData->pInfoBox->Info[6].StrInfo.Format(L"%s\n \n%s",StrType,szData);
	}
	}*/

	//--------- IPV6--------

	//PIP_ADAPTER_ADDRESSES pAddresses = NULL;

	//GetAdaptersAddresses(AF_UNSPEC,GAA_FLAG_INCLUDE_PREFIX,NULL ,)





}

void CPerformanceBox::_UpdateDiskInfoBox(PerferListData* pData,double Read,double Write,double ActiveTime,double Other,BOOL UpdateAll)
{


	if(!ShowThisPage) return ;


	// Bug fix Win7 non-admin: clamp NaN/Inf/negative on ActiveTime so the disk
	// percentage display never prints "-nan%" or "<huge>%".
	if(_finite(ActiveTime) == 0) ActiveTime = 0;
	if(ActiveTime < 0) ActiveTime = 0;
	if(ActiveTime > 100) ActiveTime = 100;

	pData->pInfoBox->Info[0].StrInfo.Format(L"%.0f%%",ActiveTime );


	double Num = Read;

	Num = Num/1024;

	pData->pInfoBox->Info[2].StrInfo.Format(L"%0.1f KB/s",Num);

	if(Num>1024)
	{
		Num = Num/1024;
		pData->pInfoBox->Info[2].StrInfo.Format(L"%0.1f MB/s",Num);
	}

	//--------------------------


	Num = Write;

	Num = Num/1024;

	pData->pInfoBox->Info[3].StrInfo.Format(L"%0.1f KB/s",Num);

	if(Num>1024)
	{
		Num = Num/1024;
		pData->pInfoBox->Info[3].StrInfo.Format(L"%0.1f MB/s",Num);
	}




	//-------------------------

	//CString StrPdh;

	//StrPdh.Format(L"\\PhysicalDisk(%d%s)\\Avg. Disk Queue Length",pData->ID,pData->StrOther0);

	//theApp.m_pMainWnd->SetWindowTextW(StrPdh);

	//double AvgRespondseTime = PM.PdhGetInfo(StrPdh);

	pData->pInfoBox->Info[1].StrInfo.Format(L"%.1f ms", Other);







}




BOOL CPerformanceBox::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class

	mToolTip.RelayEvent(pMsg);



	int n =mPItemList.GetItemCount();
	int DiskCount = 0;
	PerferListData *pPData =NULL;	
	for(int i=2;i<n;i++)
	{		
		if(pPData == NULL) continue;
		pPData=(PerferListData*)mPItemList.GetItemData(i);
		if(pPData->Type==PM_DISK) DiskCount++;
	}




	if(pMsg->message == WM_COMMAND)
	{
		UINT CmdMenuID = (UINT) pMsg->wParam;
		if( (CmdMenuID > ID_DISK_NONE) &&  (CmdMenuID <= CmdMenuID+DiskCount) )
		{

			 
			_FakeSelPItem(CmdMenuID-ID_DISK_NONE+1);
		}

	}

	return CFormView::PreTranslateMessage(pMsg);
}

void CPerformanceBox::_GetLogicalProcessorUsage(CWaveBox *pBox,double * pTotalUsage,double * pTotalKernelUsage )
{



	memset(spi,0,sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nLogicalProcessor);

	unsigned long bytesreturned;
	MyNtQuerySystemInformation(SystemProcessorPerformanceInformation,spi, (sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nLogicalProcessor),&bytesreturned);


	double TotalCpuUsage = 0;
	double TotalKernelCpuUsage = 0;


	CString Str0,Str1,StrTemp;

	for(int cpuloopcount = 0; cpuloopcount < nLogicalProcessor; cpuloopcount++) 
	{
		//-----------����ǰ��---------------
		/*for(int i=0;i<61-1;i++)
		{
		pBox->Num[cpuloopcount][i] = pBox->Num[cpuloopcount][i+1];
		pBox->Num2[cpuloopcount][i] = pBox->Num2[cpuloopcount][i+1];
		}*/

		memmove(&pBox->Num[cpuloopcount][0],&pBox->Num[cpuloopcount][1],ArraySize);
		memmove(&pBox->Num2[cpuloopcount][0],&pBox->Num2[cpuloopcount][1],ArraySize);




		LONGLONG TimeUserAndKernel=  (spi[cpuloopcount].KernelTime.QuadPart + spi[cpuloopcount].UserTime.QuadPart) - (spi_old[cpuloopcount].KernelTime.QuadPart + spi_old[cpuloopcount].UserTime.QuadPart);

		LONGLONG TimeIdle = spi[cpuloopcount].IdleTime.QuadPart - spi_old[cpuloopcount].IdleTime.QuadPart ;

		//------------------------
		double CpuUsage =0;
		if(TimeUserAndKernel>0)
		{
			CpuUsage = 100 - (double)( TimeIdle * 100  / TimeUserAndKernel);
		}


		LONGLONG TimeUser = spi[cpuloopcount].UserTime.QuadPart-spi_old[cpuloopcount].UserTime.QuadPart;

		LONGLONG TimeKernel =  spi[cpuloopcount].KernelTime.QuadPart  - spi_old[cpuloopcount].KernelTime.QuadPart ;


		//  double CpuUsag_Kernel = (double)( TimeKernel*100/TimeUserAndKernel);	  

		double CpuUsag_Kernel = 0;
		if(TimeUserAndKernel>0)
		{
			CpuUsag_Kernel = 100  - (double)(  (TimeIdle+TimeUser)* 100  /TimeUserAndKernel);
		}



		pBox->Num[cpuloopcount][60] = ( float)  (CpuUsage/100 ); //
		pBox->Num2[cpuloopcount][60] =( float)  (CpuUsag_Kernel/100  );

		spi_old[cpuloopcount] = spi[cpuloopcount];

		TotalCpuUsage = TotalCpuUsage + pBox->Num[cpuloopcount][60];
		TotalKernelCpuUsage = TotalKernelCpuUsage + pBox->Num2[cpuloopcount][60];
	}

	TotalCpuUsage = TotalCpuUsage/nLogicalProcessor;
	TotalKernelCpuUsage = TotalKernelCpuUsage/nLogicalProcessor;

	* pTotalUsage = TotalCpuUsage;
	*pTotalKernelUsage = TotalKernelCpuUsage;



}

void CPerformanceBox::_DetermineRowCol(int nLogicalProcessor , int * pNRow, int * pNCol)//ȷ����cpu����ͼ��Ϊ���м�����ʾ
{
	int nRow =  (int)sqrt( (float) nLogicalProcessor );



	while(1)
	{
		if(	nLogicalProcessor%nRow == 0) //�����������������Ϊ���� �����һ ֱ��������
		{
			break;
		}
		else
		{
			nRow--;
		}

	}

	* pNRow = nRow;
	* pNCol = nLogicalProcessor/nRow;




}

void CPerformanceBox::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	CFormView::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);


	switch(theApp.AppSettings.ProcessorDisplayMode)
	{
	case 0:
		pPopupMenu->CheckMenuRadioItem(ID_CHANGEGRAPHTO_OVERALLUTILIZATION,ID_CHANGEGRAPHTO_NUMANODES,ID_CHANGEGRAPHTO_OVERALLUTILIZATION,MF_BYCOMMAND);
		break;
	case 1:
		pPopupMenu->CheckMenuRadioItem(ID_CHANGEGRAPHTO_OVERALLUTILIZATION,ID_CHANGEGRAPHTO_NUMANODES,ID_CHANGEGRAPHTO_LOGICALPROCESSORS,MF_BYCOMMAND);
		break;
	case 2: 
		pPopupMenu->CheckMenuRadioItem(ID_CHANGEGRAPHTO_OVERALLUTILIZATION,ID_CHANGEGRAPHTO_NUMANODES,ID_CHANGEGRAPHTO_NUMANODES,MF_BYCOMMAND);
		break;
	}



	UINT Flag = (theApp.FlagSummaryView) ? MF_CHECKED:MF_UNCHECKED;
	pPopupMenu->CheckMenuItem(ID_PERFORMANCE_GRAPHSUMMARYVIEW,MF_BYCOMMAND|Flag);



	//-------------------------
	int iSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );

	switch(iSel)
	{
	case 0:
		pPopupMenu->CheckMenuRadioItem(ID_VIEW_CPU,ID_VIEW_MEMORY,ID_VIEW_CPU,MF_BYCOMMAND);
		break;
	case 1:
		pPopupMenu->CheckMenuRadioItem(ID_VIEW_CPU,ID_VIEW_MEMORY,ID_VIEW_MEMORY,MF_BYCOMMAND);
		break;
	
	}
	



	if(pPopupMenu->GetMenuItemID(0) ==ID_VIEW_CPU)
	{
		

		


		int ItemCount =mPItemList.GetItemCount();
		PerferListData *pPData = NULL ;		
		int DiskCount = 0;
		int SelectedDiskID = -1;

	    int FirstDiskItemID = 2; //��һ��������Ŀ�������Ŀ�б��е�ID

			 
		for(int i = FirstDiskItemID;  i<ItemCount ;i++ ) 
		{		
			pPData=(PerferListData*)mPItemList.GetItemData(i);
			if(pPData == NULL)continue;		
			if(pPData->Type == PM_DISK)
			{
				DiskCount++;
				if(i == iSel){ SelectedDiskID = iSel-FirstDiskItemID ;}
				
			}
		}

		Flag = (DiskCount>0)?MF_ENABLED:MF_GRAYED;  //�����Ƿ��д��̾���
		pPopupMenu->EnableMenuItem(2,MF_BYPOSITION|Flag  );  
		if(SelectedDiskID>=0)
		{
			CMenu *pSubMenu = pPopupMenu->GetSubMenu(2);
			if(pSubMenu)pSubMenu->CheckMenuRadioItem(0, DiskCount-1, SelectedDiskID, MF_BYPOSITION);
		}

		
	}




	//ASSERT(pPopupMenu != NULL);

	//CCmdUI state;
	//state.m_pMenu = pPopupMenu;
	//ASSERT(state.m_pOther == NULL);
	//ASSERT(state.m_pParentMenu == NULL);

	//HMENU hParentMenu;
	//if (AfxGetThreadState()->m_hTrackingMenu == pPopupMenu->m_hMenu)
	//	state.m_pParentMenu = pPopupMenu;    // Parent == child for tracking popup.
	//else if ((hParentMenu = ::GetMenu(m_hWnd)) != NULL)
	//{
	//	CWnd* pParent = this;

	//	if (pParent != NULL &&	(hParentMenu = ::GetMenu(pParent->m_hWnd)) != NULL)
	//	{
	//		int nIndexMax = ::GetMenuItemCount(hParentMenu);
	//		for (int nIndex = 0; nIndex < nIndexMax; nIndex++)
	//		{
	//			if (::GetSubMenu(hParentMenu, nIndex) == pPopupMenu->m_hMenu)
	//			{

	//				state.m_pParentMenu = CMenu::FromHandle(hParentMenu);
	//				break;
	//			}
	//		}
	//	}
	//}

	//state.m_nIndexMax = pPopupMenu->GetMenuItemCount();
	//for (state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax;state.m_nIndex++)
	//{
	//	state.m_nID = pPopupMenu->GetMenuItemID(state.m_nIndex);
	//	if (state.m_nID == 0)
	//		continue; // Menu separator or invalid cmd - ignore it.

	//	ASSERT(state.m_pOther == NULL);
	//	ASSERT(state.m_pMenu != NULL);
	//	if (state.m_nID == (UINT)-1)
	//	{
	//		// Possibly a popup menu, route to first item of that popup.
	//		state.m_pSubMenu = pPopupMenu->GetSubMenu(state.m_nIndex);
	//		if (state.m_pSubMenu == NULL ||	(state.m_nID = state.m_pSubMenu->GetMenuItemID(0)) == 0 ||state.m_nID == (UINT)-1)
	//		{
	//			continue;       // First item of popup can't be routed to.
	//		}
	//		state.DoUpdate(this, TRUE);   // Popups are never auto disabled.
	//	}
	//	else
	//	{

	//		state.m_pSubMenu = NULL;
	//		state.DoUpdate(this, FALSE);
	//	}

	//	// Adjust for menu deletions and additions.
	//	UINT nCount = pPopupMenu->GetMenuItemCount();
	//	if (nCount < state.m_nIndexMax)
	//	{
	//		state.m_nIndex -= (state.m_nIndexMax - nCount);
	//		while (state.m_nIndex < nCount &&	pPopupMenu->GetMenuItemID(state.m_nIndex) == state.m_nID)
	//		{
	//			state.m_nIndex++;
	//		}
	//	}
	//	state.m_nIndexMax = nCount;
	//}



}




void CPerformanceBox::OnPerformanceShowkerneltimes()
{

	theApp.AppSettings.ShowKernelTime = !theApp.AppSettings.ShowKernelTime;

	pTotalCpuBox->DrawSecondWave = theApp.AppSettings.ShowKernelTime ;
	pCpuBox->DrawSecondWave = theApp.AppSettings.ShowKernelTime ;

	pTotalCpuBox->Invalidate();
	pCpuBox->Invalidate();


}

void CPerformanceBox::OnUpdatePerformanceShowkerneltimes(CCmdUI *pCmdUI)
{

	if(theApp.AppSettings.ShowKernelTime)
	{
		pCmdUI->SetCheck(1);
	}
	else
	{
		pCmdUI->SetCheck(0);
	}
}





void CPerformanceBox::OnViewCpu()
{
	mPItemList.SetItemState(0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);	 
}

void CPerformanceBox::OnViewMemory()
{
	mPItemList.SetItemState(1,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);	 
}

double CPerformanceBox::_TryToChangeDiskMaxVar( double DataA,double DataB,PerferListData *pPData )
{


	double RetMaxVar;
	double Data = (DataA>DataB)?DataA:DataB;

	int nK,nM;


	CString Str ;

	nK = ((int)Data)/1024;  nM = ((int)Data)/1024/1024;

	RetMaxVar = 100*1024;

	if(nM == 0) //KB ����
	{
		if(nK<200 )
		{
			RetMaxVar = 100*1024;
			Str =L"100 KB/s";
		}
		else if(nK<500 )
		{
			RetMaxVar = 500*1024;
			Str =L"500 KB/s";
		}
		else if(nK<1024)
		{
			RetMaxVar = 1024*1024;
			Str =L"1 MB/s";
		}

	}
	else
	{

		if(nM<5)
		{
			RetMaxVar = 5*1024*1024;//5MB
			Str =L"5 MB/s";
		}
		else if(nM<10) 
		{

			RetMaxVar =10*1024*1024 ; //10MB
			Str =L"10 MB/s";
		}
		else if(nM<50) 
		{

			RetMaxVar =50*1024*1024 ; //50
			Str =L"50 MB/s";
		}
		else if(nM<100) 
		{

			RetMaxVar =100*1024*1024 ; //100
			Str =L"100 MB/s";
		}
		else if(nM<250) 
		{

			RetMaxVar =250*1024*1024 ; //500
			Str =L"250 MB/s";
		}
		else if(nM<500) 
		{

			RetMaxVar =500*1024*1024 ; //500
			Str =L"500 MB/s";
		}
		else //if(nM<1000) 
		{

			RetMaxVar =1000*1024*1024 ; //1000
			Str =L"1000 MB/s";
		}

	}

	if(RetMaxVar!=pPData->MaxVar) //��Ҫ���
	{
		double Per = RetMaxVar/pPData->MaxVar ;//�任����
		CWaveBox *pBox2= (CWaveBox *)pPData->pOtherWnd;	

		for(int i=0;i<61;i++) //��61 �� ����
		{
			pBox2->Num[0][i] =(float)(pBox2->Num[0][i]/Per) ;//�任Ϊ�±���
			pBox2->Num2[0][i] =(float)(pBox2->Num2[0][i]/Per) ;//�任Ϊ�±���
		}

		pPData->StrOther1 = Str;
		pPData->MaxVar =	RetMaxVar;


		CWnd *pWnd=GetDlgItem(IDC_TIP6);
		if(pWnd!=NULL)
		{
			pWnd->SetWindowTextW(Str); 
			pWnd->Invalidate();
		}
		//pBox2->LineVar = (DataA+DataB)/2/RetMaxVar;


	}



	return RetMaxVar;





}

double CPerformanceBox::_TryToChangeNetworkMaxVar(double DataA,double DataB,PerferListData *pPData )
{
	double RetMaxVar;//   = pPData->MaxVar;     //   Byte/sec
	double Data = (DataA>DataB)?DataA:DataB;    // Byte/sec

	Data=Data*8; //bps

	int nK,nM;

	CString Str ;

	nK = ((int)Data)/1024;  nM = ((int)Data)/1024/1024;

	RetMaxVar = 100*1024/8;


	if(nM == 0) //KB ����
	{
		if(nK<200 )
		{
			RetMaxVar = 100*1024/8;
			Str =L"100 Kbps";
		}
		else if(nK<500 )
		{
			RetMaxVar = 500*1024/8;
			Str =L"500 Kbps";
		}
		else if(nK<1024)
		{
			RetMaxVar = 1024*1024/8;
			Str =L"1 Mbps";
		}


	}
	else
	{

		if(nM<5)
		{
			RetMaxVar = 5*1024*1024/8;//5MB
			Str =L"5 Mbps";
		}
		else if(nM<10) 
		{

			RetMaxVar =10*1024*1024/8 ; //10MB
			Str =L"10 Mbps";
		}
		else if(nM<50) 
		{

			RetMaxVar =50*1024*1024/8 ; //50
			Str =L"50 Mbps";
		}
		else if(nM<100) 
		{

			RetMaxVar =100*1024*1024/8 ; //100
			Str =L"100 Mbps";
		}
		else if(nM<250) 
		{

			RetMaxVar =250*1024*1024/8 ; //500
			Str =L"250 Mbps";
		}
		else if(nM<500) 
		{

			RetMaxVar =500*1024*1024/8 ; //500
			Str =L"500 Mbps";
		}
		else //if(nM<1000) 
		{

			RetMaxVar =1000*1024*1024/8 ; //1000
			Str =L"1000 Mbps";
		}




	}





	if( RetMaxVar !=pPData->MaxVar) //��Ҫ���
	{

		double Per = RetMaxVar/pPData->MaxVar ;//�任����
		CWaveBox *pBox2= (CWaveBox *)pPData->pWaveBox;	

		for(int i=0;i<61;i++) //��61 �� ����
		{
			pBox2->Num[0][i] =(float)(pBox2->Num[0][i]/Per) ;//�任Ϊ�±���
			pBox2->Num2[0][i] =(float)(pBox2->Num2[0][i]/Per) ;//�任Ϊ�±���
		}

		pPData->StrOther1 = Str;
		pPData->MaxVar = RetMaxVar;


		CWnd *pWnd=GetDlgItem(IDC_TIP2);
		if(pWnd!=NULL)
		{
			pWnd->SetWindowTextW(Str); 
			pWnd->Invalidate();
		}
		//pBox2->LineVar = (DataA+DataB)/2/RetMaxVar;


	}



	return RetMaxVar;

}

void CPerformanceBox::_LoadDiskStaticInfo(PerferListData * pData)
{

	pData->StrOther0 = GetDrivelettersFormDiskID(pData->ID);

 
	// -------------------- ���� -----------------------

	CString StrDev,Str ;
	StrDev.Format(L"\\\\.\\PhysicalDrive%d",pData->ID);

	HANDLE hDevice=CreateFile(StrDev,0,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);

	if(hDevice ==INVALID_HANDLE_VALUE) return;


	

	DISK_GEOMETRY_EX geoStruct;
	memset(&geoStruct,0,sizeof(geoStruct));
	DWORD bytesReturned;
	if(DeviceIoControl(hDevice,IOCTL_DISK_GET_DRIVE_GEOMETRY_EX ,NULL,0,&geoStruct,sizeof(geoStruct),&bytesReturned,NULL)!=0)
	{
		//MSB(88)
		double DiskSpace =  (double)(geoStruct.DiskSize.QuadPart/1024/1024); //MB;
		if(DiskSpace>1024)
		{
			DiskSpace = DiskSpace/1024;//GB

			Str.Format(L"%0.1f GB",(double)DiskSpace);
		}
		else
		{
			Str.Format(L"%0.1f MB",(double)DiskSpace);
		}

		pData->pInfoBox->Info[6].StrInfo = Str+L"\n";		

	}
	else
	{
		pData->pInfoBox->Info[6].StrInfo =  L"-\n";
	}
	CloseHandle(hDevice);


	// -------------------- ��ʽ������ -----------------------

	LONGLONG DiskFmtSize=0;

	WCHAR strRootPath[]={L"c:\\"};//����Ŀ¼��ǵĴ��̷���
	DWORD dwSectorsPerCluster=0;//ÿ����������
	DWORD dwBytesPerSector=0;//ÿ�������ֽ���
	DWORD dwFreeClusters=0;//ʣ�����
	DWORD dwTotalClusters=0;//�ܴ���
	int n =	0;	


	while(1)
	{

		n =	pData->StrOther0.Find(L":",n+1);
		if(n == -1) break;

		strRootPath[0] = pData->StrOther0.GetAt(n-1);

		//MSB_S(strRootPath)


		if (GetDiskFreeSpace(strRootPath,&dwSectorsPerCluster,&dwBytesPerSector,&dwFreeClusters,&dwTotalClusters))
		{
			//m_dwVolSize=dwTotalClusters*dwSectorsPerCluster*dwBytesPerSector;//��������������Խ��
			double VolSize=dwSectorsPerCluster*dwBytesPerSector/(1024.*1024.);	 
			//m_dVolSize=dwTotalClusters*dd;//�ô����ܴ�С
			DiskFmtSize = (LONGLONG) (DiskFmtSize+VolSize*dwTotalClusters);


		}

	}


	if(DiskFmtSize>1024)
	{

		Str.Format(L"%0.1f GB\n",(double)DiskFmtSize/(double)1024);
	}
	else
	{
		Str.Format(L"%0.1f MB\n",(double)DiskFmtSize );
	}

	if(DiskFmtSize<=0)
	{
		Str=L"-\n";
	}


	pData->pInfoBox->Info[6].StrInfo = pData->pInfoBox->Info[6].StrInfo+Str;


	


}



void CPerformanceBox::OnNMRClickListPerformanceitem(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: Add your control notification handler code here

	CMenu PopMenu;
	CMenu *pMenu= NULL;
	PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_BASE) );
	pMenu = PopMenu.GetSubMenu(5);  //5�� ��� ��Ӧ�� �˵� �ͱ�ǩ˳���Ӧ

	CPoint CurPos ;
	GetCursorPos(&CurPos); 

	if(theApp.FlagSummaryView)
	{
		pMenu->CheckMenuItem(ID_PERFORMANCETYPE_SUMMARYVIEW, MF_BYCOMMAND|MF_CHECKED);
	}

	pMenu->TrackPopupMenu(TPM_LEFTALIGN,CurPos.x,CurPos.y,this);

	//----------

	*pResult = 0;
}

void CPerformanceBox::OnPop_PerformanceListShowHideGraphs()
{
	//theApp.AppSettings.PerformanceListShowGraph =   (theApp.AppSettings.PerformanceListShowGraph==0)?1:0 ;

	theApp.AppSettings.PerformanceListShowGraph =   (theApp.AppSettings.PerformanceListShowGraph==0)?1:0 ;
	mPItemList.SetRowHeight(20);
	//this->GetParent()->PostMessageW(WM_COMMAND,ID_PERFORMANCETYPE_HIDEGRAPHS);//UM_PERFORMANCELIST_SYTLECHANGED
	

}





void CPerformanceBox::OnUpdatePerformancetypeHidegraphs(CCmdUI *pCmdUI)
{
	CString Str;

	if(theApp.AppSettings.PerformanceListShowGraph!=0)
	{

		Str.LoadStringW(IDS_STRING_HIDEGRAPH);
	}
	else
	{
		Str.LoadStringW(IDS_STRING_SHOWGRAPH);
	}

	pCmdUI->SetText(Str);



}

PerferListData * CPerformanceBox::_InsertNetAdapterItem(CString StrType, CString StrDescription,int IfIndex)
{

	int n =mPItemList.GetItemCount();
	PerferListData *pPData =  new PerferListData;

	mPItemList.InsertItem(n,StrType);

	mPItemList.SetItemText(n,1,L"S: 0 Kbps R: 0 Kbps");
	mPItemList.SetItemText(n,2,StrDescription);  // ע��� DriverDesc ��ȡ�� szData ��������ʾ����
	mPItemList.SetItemData(n,(DWORD_PTR)pPData);

	CInfoBox *pInfoBox = NULL;
	CString StrInfo; 
	CWaveBox * pEthernetBox= new CWaveBox;
	pInfoBox = new CInfoBox;	

	pInfoBox->Create(L"",WS_CHILD|WS_VISIBLE|SS_CENTER,CRect(0,0,1,1),this);
	pEthernetBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);



	pPData->Type = PM_ETHERNET;
	pInfoBox->Type  = PM_ETHERNET;
	pPData->ID = IfIndex;	

	pPData->pWaveBox = pEthernetBox; 
	pPData->pInfoBox = pInfoBox;
	pPData->pWaveBox->DrawSecondWave = TRUE;
	pPData->pOtherWnd = NULL;
	pPData->DataA = pPData->DataB = 0;

	pInfoBox->Info[0].StrTitle  = STR_NETWORKINFO_0  ;
	pInfoBox->Info[2].StrTitle  = STR_NETWORKINFO_2  ;

	pInfoBox->Info[0].StrInfo=L"0 Kbps";
	pInfoBox->Info[2].StrInfo=L"0 Kbps";

	pInfoBox->Info[0].Type=2;  pInfoBox->Info[2].Type=1; //ͼ������
	pInfoBox->Info[6].StrTitle  = STR_NETWORKINFO_6;
	pInfoBox->Info[6].rc.left-= 100;


	mPItemList.SetItemText(n,3,STR_TIP3_NETWORK);
	mPItemList.SetItemText(n,4,L"100 Kbps");

	pEthernetBox->SetLineColumn(1,1);
	pEthernetBox->SetColor(theApp.AppSettings.NetworkColor);

	pPData->MaxVar = 100*1024/8; //Ĭ�����ֵ100Kbps	
	NetAdapterList[IfIndex] = n;
 
	return pPData;
}

int CPerformanceBox::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFormView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here

	CCreateContext * pContextBox; 


	CWnd * pWnd = NULL; 

	//-----------------------------------------------------------------------

	pViewClass = RUNTIME_CLASS(CHotboxView);

	pContextBox= new CCreateContext; 
	pContextBox->m_pCurrentDoc = NULL; 
	pContextBox->m_pCurrentFrame = NULL; 
	pContextBox->m_pLastView = NULL; 
	pContextBox->m_pNewDocTemplate =NULL; 

	pContextBox->m_pNewViewClass = pViewClass; 

	pWnd = NULL; 
	pWnd = DYNAMIC_DOWNCAST(CWnd, pViewClass->CreateObject());
	pWnd->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, IDD_HOTBOXVIEW, pContextBox); 

	pHotBox= DYNAMIC_DOWNCAST(CHotboxView, pWnd); 
	pHotBox->SetScrollSizes(MM_TEXT, CSize(1, 1)); //BOX_W_MIN,180
	pHotBox->ModifyStyleEx(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE,0);

	delete pContextBox;
	pContextBox=NULL;


	return 0;
}

void CPerformanceBox::OnPop_ChangeGraphToNumaNodes()
{

	if(theApp.AppSettings.ProcessorDisplayMode == 2) return;

	theApp.AppSettings.ProcessorDisplayMode = 2;



	pTotalCpuBox->ShowWindow(SW_HIDE);
	pCpuBox->ShowWindow(SW_HIDE);

	pHotBox->ShowWindow(SW_SHOW);



	CWnd *pWnd= NULL;
	pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
	pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }


	pWnd=GetDlgItem(IDC_TIP1);
	if(pWnd!=NULL)	
	{ 
		CString StrTemp = STR_TIP1_CPU_NUMA;
		mPItemList.SetItemText(0,3,StrTemp);
		pWnd->SetWindowTextW(StrTemp);
	}

	pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
	 


	this->PlaceAllCtrl();
}



int CPerformanceBox::_TestDiskID(int TestID)
{

	int DiskID = TestID;
	CString StrDev ;
	while(1)
	{
		StrDev.Format(L"\\\\.\\PhysicalDrive%d",DiskID);
		HANDLE hDevice=CreateFile(StrDev,0,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
		if(( hDevice!=INVALID_HANDLE_VALUE)||(DiskID>1000))
		{	

			 CloseHandle(hDevice);
			return DiskID;
		}

		DiskID++;
	}
	return DiskID;

}

void CPerformanceBox::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	//google 

	theApp.m_pMainWnd->PostMessage(UM_SUMMARYVIEW,(WPARAM)this);

	

	CFormView::OnLButtonDblClk(nFlags, point);
}

void CPerformanceBox::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	CFormView::OnLButtonDown(nFlags, point);

	if(theApp.FlagSummaryView)
	{
		theApp.m_pMainWnd->PostMessage(WM_NCLBUTTONDOWN,    HTCAPTION,    MAKELPARAM(point.x, point.y)); 
	}
}


void CPerformanceBox::OnPop_GraphSummaryView()
{
	theApp.m_pMainWnd->PostMessage(UM_SUMMARYVIEW,(WPARAM)this);
	

}



void CPerformanceBox::OnPop_PerformanceListSummaryView()
{
	theApp.m_pMainWnd->PostMessage(UM_SUMMARYVIEW,(WPARAM)(&mPItemList));

}

BOOL CPerformanceBox::SetTipText( UINT ID, NMHDR * pTTTStruct, LRESULT * pResult )
{
	TOOLTIPTEXT *pTooltipText = (TOOLTIPTEXT *)pTTTStruct;
	
	HWND hWnd = (HWND)(pTTTStruct->idFrom); //�õ���Ӧ����ID���п�����HWND 

 
	if (pTooltipText->uFlags & TTF_IDISHWND) //����nID�Ƿ�ΪHWND 
	{    
		PerferListData *pPData = (PerferListData *) mPItemList.GetItemData(1);
		if(pPData!=NULL)
		{
			CMemCompBox *pMemCompBox =(CMemCompBox *)( pPData->pOtherWnd);
			if(pMemCompBox->m_hWnd == hWnd)//��HWND�õ�IDֵ����Ȼ��Ҳ����ͨ��HWNDֵ���ж�
			{		

				//pTooltipText->lpszText = L"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
			
				pTooltipText->lpszText=(LPWSTR)(LPCTSTR) pMemCompBox->GetMemCompType() ;
			//	StringCchCopy(pTooltipText->lpszText,MAX_PATH,pMemCompBox->GetMemCompType());

				//theApp.m_pMainWnd->SetWindowTextW(pMemCompBox->GetMemCompType());
				//pTooltipText->lpszText =(LPWSTR) pMemCompBox->GetMemCompType() ;
			}		
		}
	}
     
	return TRUE; 
}


void CPerformanceBox::_GetDiskLetterUseWmi(void)
{
	
    IEnumWbemClassObject* pEnumerator = NULL;
	pEnumerator = GetWmiObject(L"Win32_LogicalDiskToPartition");

	if(pEnumerator == NULL) return;

 
    IWbemClassObject *pclsObj;
    ULONG uReturn = 0;   

	CString StrDisk,StrVolume,StrPartitionID ;

	int DiskID;	
	//int PartitionID ;
	PerferListData * pData ;

	while (pEnumerator)
	{
		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if(0 == uReturn || FAILED(hr))  {break;}

		// Bug fix Win7 heap-corruption: vtProp must be VariantInit'd and each
		// Get() must be checked for success before reading bstrVal. The previous
		// code reused vtProp between two Get() calls without VariantClear between
		// them, and never initialised the variant - reading vtProp.bstrVal on a
		// failed Get() returned random stack/heap data, which CString::operator=
		// would then try to SysFreeString on at destruction. That was corrupting
		// the heap, surfacing later as STATUS_HEAP_CORRUPTION in ntdll.dll.
		VARIANT vtProp;
		VariantInit(&vtProp);
		hr = pclsObj->Get(L"Dependent", 0, &vtProp, 0, 0);
		if(SUCCEEDED(hr) && V_VT(&vtProp) == VT_BSTR && vtProp.bstrVal != NULL)
		{
			StrVolume = vtProp.bstrVal;
		}
		else
		{
			StrVolume.Empty();
		}
		VariantClear(&vtProp);

		VariantInit(&vtProp);
		hr = pclsObj->Get(L"Antecedent", 0, &vtProp, 0, 0);
		if(SUCCEEDED(hr) && V_VT(&vtProp) == VT_BSTR && vtProp.bstrVal != NULL)
		{
			StrDisk = vtProp.bstrVal;
		}
		else
		{
			StrDisk.Empty();
		}
		VariantClear(&vtProp);
		StrPartitionID = StrDisk;

		if(StrVolume.IsEmpty())
		{
			pclsObj->Release();
			continue;
		}

		StrVolume= StrVolume.Right(4);
		StrVolume.Remove(L'\"');

		if(StrDisk.IsEmpty())
		{
			pclsObj->Release();
			continue;
		}

		int n=StrDisk.Find(L'#');
		if(n < 0)
		{
			pclsObj->Release();
			continue;
		}
		StrDisk.Delete(0,n+1);
		n=StrDisk.Find(L',');  //ע�ⲻҪֱ��ȡ��һ����Ϊ����Ӳ��ID  ��Ϊ���ܳ���9�� ����ֹһλ���֣�
		if(n < 0)
		{
			pclsObj->Release();
			continue;
		}
		StrDisk =StrDisk.Left(n);

		StrPartitionID = StrPartitionID.Right(2); //�����λ�����Ƿ�����   ����������ʽ rtition.DeviceID="Disk #4, Partition #0"
		//if(StrPartitionID.Left(1)==L"#")//ֻ��һλ��
		//{
		//	//StrPartitionID = StrPartitionID.Right(1);
		//}

		//PartitionID = _wtoi(StrPartitionID);

		DiskID = _wtoi(StrDisk);

		int i=2;

		while(1)
		{

			pData = (PerferListData*) mPItemList.GetItemData(i);
			if(pData==NULL)break;
			if(pData->Type==PM_ETHERNET)break;
			if(pData->ID == DiskID && (pData->StrOther0.Find(StrVolume.GetAt(0))<0))
			{
				// �˴����̷�д�������Ӧ��λ�� �����0�����̷���A�� ���ַ�����0���ַ�ΪA

				if(pData->StrOther0  != L" ")StrVolume =L" "+StrVolume;
				pData->StrOther0 = pData->StrOther0+StrVolume;
			}
			i++;
		}



		pclsObj->Release();

		//break;
	}

	pEnumerator->Release();

}


void CPerformanceBox::MySetItem(int iItem, CString Strtext)
{
	LVITEM Item;	 
	Item.iItem = iItem;
	Item.iSubItem = 1;
	Item.mask = LVIF_TEXT;
	Item.cchTextMax=MAX_PATH;
	Item.pszText= (LPTSTR)(LPCTSTR)(Strtext);
	mPItemList.SetItem(&Item);

}


 
void CPerformanceBox::_FakeSelPItem(int ID)
{
	 

	   mPItemList.SetItemState(ID,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
}

double CPerformanceBox::_GetSurrentCpuSpeed(void)  //��λ�� GHz
{
	double  CurrentSpeed = 0.0;
	// Bug fix Win7: previous code used a single PROCESSOR_POWER_INFORMATION on stack
	// but told CallNtPowerInformation the buffer was nLogicalProcessor structs wide.
	// When nLogicalProcessor > 1 the kernel writes past the single struct and
	// corrupts the stack of the caller (eventually corrupting UpdateAllPMInfo GS cookie).
	int nLP = theApp.PerformanceInfo.nLogicalProcessor;
	if(nLP < 1) nLP = 1;
	PROCESSOR_POWER_INFORMATION  *PPInfo = new PROCESSOR_POWER_INFORMATION[nLP];
	memset(PPInfo, 0, sizeof(PROCESSOR_POWER_INFORMATION)*nLP);
	CallNtPowerInformation( ProcessorInformation,NULL, 0,PPInfo, sizeof(PROCESSOR_POWER_INFORMATION)*nLP );

	ULONG CurMhz = PPInfo[0].CurrentMhz;
	ULONG MaxMhz = PPInfo[0].MaxMhz;

	delete [] PPInfo;

	// Defensive clamp: garbage from API (or 0/NaN when API fails) must not reach display.
	if(_finite((double)CurMhz) == 0) CurMhz = 0;
	if(CurMhz > 100000) CurMhz = 100000;
	if(_finite((double)MaxMhz) == 0) MaxMhz = 0;
	if(MaxMhz > 100000) MaxMhz = 100000;

	// Preferred: the WMI-based current-clock-speed monitor. On Windows 10 +
	// modern CPUs (AMD Zen CPPC, Intel Speed Shift) this reflects the actual
	// current P-state, including turbo / boost - the same source Win10's
	// native task manager uses. Polled every 2 s on a background thread.
	EnsureWmiCpuSpeedMonitor();
	if(InterlockedCompareExchange(&g_WmiCpuReady, 0, 0) != 0)
	{
		LONG wmi = InterlockedCompareExchange(&g_WmiCpuMhz, 0, 0);
		if(wmi > 50 && wmi < 100000)
		{
			CurrentSpeed = (double)wmi / 1000.0;
			return CurrentSpeed;
		}
	}

	// Fallback: the OS-reported CurrentMhz when it is *less* than MaxMhz
	// (i.e. the OS actually has live P-state info). Works on older CPUs and
	// on Win7 + Intel with proper ACPI.
	if(CurMhz > 0 && MaxMhz > 0 && CurMhz < MaxMhz)
	{
		CurrentSpeed = (double)CurMhz / 1000.0;
		return CurrentSpeed;
	}

	// Fallback: live RDTSC measurement. Works on Intel CPUs where TSC ticks
	// at the core clock, NOT on AMD Zen (TSC is rated-frequency there).
	EnsureCpuSpeedMonitor();
	if(InterlockedCompareExchange(&g_CpuSpeedReady, 0, 0) != 0)
	{
		LONG measured = InterlockedCompareExchange(&g_CpuSpeedMhz, 0, 0);
		if(measured > 0 && measured < 100000)
		{
			CurrentSpeed = (double)measured / 1000.0;
			return CurrentSpeed;
		}
	}

	// Last resort: report MaxMhz / 1000 so the field is at least populated.
	if(MaxMhz > 0)
	{
		CurrentSpeed = (double)MaxMhz / 1000.0;
	}
	else if(CurMhz > 0)
	{
		CurrentSpeed = (double)CurMhz / 1000.0;
	}

	return CurrentSpeed;
}
