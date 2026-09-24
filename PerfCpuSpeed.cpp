// PerfCpuSpeed.cpp
#include "stdafx.h"
#include "PerfCpuSpeed.h"

#include <pdh.h>
#pragma comment(lib, "pdh.lib")

#include <process.h>
#include <comdef.h>

// ============================================================================
//  PDH-based "% Processor Performance" monitor (Win10-style PRIMARY source)
//
//  Windows 10/11 task manager computes the live CPU speed as:
//      fCurrentGhz = fBaseGhz * (% Processor Performance / 100.0)
//
//  On Intel Core (any generation, Nehalem -> current) and AMD Zen
//  (Ryzen / Threadripper / EPYC), % Processor Performance exceeds 100 %
//  when the cores are in Turbo / Boost state. On Windows 7 and on CPUs
//  that don't expose CPPC the counter is either missing or pinned to 100 %.
//
//  The counter path is opened with PdhAddEnglishCounterW so it resolves
//  against the English counter names regardless of the user locale
//  (es-ES, es-MX, fr-FR, de-DE, ja-JP, ...).
// ============================================================================

static HQUERY          s_hPdhQuery   = NULL;
static HCOUNTER        s_hPdhCounter = NULL;
static volatile double s_PdhPct      = 0.0;   // latest % Processor Performance
static volatile LONG   s_PdhReady    = 0;
static HANDLE          s_hPdhThread  = NULL;
static volatile LONG   s_PdhStop     = 0;

static volatile LONG   s_WmiCpuMhz      = 0;
static volatile LONG   s_WmiCpuReady    = 0;
static HANDLE          s_hWmiCpuThread  = NULL;
static volatile LONG   s_WmiCpuStop     = 0;

// Some older Windows SDKs ship pdh.h without PdhAddEnglishCounterW; forward
// declare it so the binary still links on a Vista-or-newer runtime.
#if !defined(PdhAddEnglishCounterW) && (_WIN32_WINNT < 0x0600)
PDH_STATUS WINAPI PdhAddEnglishCounterW(PDH_HQUERY hQuery,
									   LPCWSTR    szFullCounterPath,
									   DWORD_PTR  dwUserData,
									   PDH_HCOUNTER *phCounter);
#endif

static UINT __stdcall Thread_MonitorPdhCpuPerformance(LPVOID lparam)
{
	(void)lparam;

	for(int attempt = 0; attempt < 10 && s_PdhStop == 0; attempt++)
	{
		if(PdhOpenQuery(NULL, 0, &s_hPdhQuery) == ERROR_SUCCESS && s_hPdhQuery)
			break;
		s_hPdhQuery = NULL;
		Sleep(500);
	}
	if(!s_hPdhQuery) return 0;

	PDH_STATUS s = PdhAddEnglishCounterW(s_hPdhQuery,
		L"\\Processor Information(_Total)\\% Processor Performance",
		0, &s_hPdhCounter);
	if(s != ERROR_SUCCESS || !s_hPdhCounter)
	{
		PdhCloseQuery(s_hPdhQuery);
		s_hPdhQuery   = NULL;
		s_hPdhCounter = NULL;
		return 0;
	}

	// "% Processor Performance" is a rate-based counter - it needs two
	// collects before a meaningful value can be read. Prime it here.
	PdhCollectQueryData(s_hPdhQuery);
	Sleep(1000);

	while(s_PdhStop == 0)
	{
		PdhCollectQueryData(s_hPdhQuery);

		PDH_FMT_COUNTERVALUE v;
		PDH_STATUS g = PdhGetFormattedCounterValue(s_hPdhCounter,
			PDH_FMT_DOUBLE, NULL, &v);
		if(g == ERROR_SUCCESS)
		{
			double pct = v.doubleValue;
			if(!_finite(pct)) pct = 0.0;
			if(pct < 0.0)    pct = 0.0;
			// Note: pct can legitimately exceed 100.0 for Turbo/Boost -
			// do not clamp here.
			s_PdhPct = pct;
			InterlockedExchange(&s_PdhReady, 1);
		}

		Sleep(1000);
	}

	PdhRemoveCounter(s_hPdhCounter);
	PdhCloseQuery(s_hPdhQuery);
	s_hPdhQuery   = NULL;
	s_hPdhCounter = NULL;
	return 0;
}

static UINT __stdcall Thread_MonitorWmiCpuSpeed(LPVOID lparam)
{
	(void)lparam;

	HRESULT hrCom = CoInitializeEx(0, COINIT_MULTITHREADED);
	BOOL ownCom = SUCCEEDED(hrCom);

	while(s_WmiCpuStop == 0)
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
			InterlockedExchange(&s_WmiCpuMhz, mhz);
			InterlockedExchange(&s_WmiCpuReady, 1);
		}

		// 3000 ms sampling interval - reduce WMI overhead when this is
		// only acting as a fallback for the PDH primary source.
		for(int i = 0; i < 30 && s_WmiCpuStop == 0; i++) Sleep(100);
	}

	if(ownCom) CoUninitialize();
	return 0;
}

void PerfPdhCpuPerf_Start(void)
{
	if(s_hPdhThread != NULL) return;
	InterlockedExchange(&s_PdhStop, 0);
	s_hPdhThread = (HANDLE)_beginthreadex(NULL, 0,
		Thread_MonitorPdhCpuPerformance, NULL, 0, NULL);
}

void PerfPdhCpuPerf_Stop(void)
{
	if(s_hPdhThread == NULL) return;
	InterlockedExchange(&s_PdhStop, 1);
	WaitForSingleObject(s_hPdhThread, 3000);
	CloseHandle(s_hPdhThread);
	s_hPdhThread = NULL;
}

double PerfPdhCpuPerf_GetPct(void)   { return s_PdhPct; }
LONG   PerfPdhCpuPerf_GetReady(void) { return InterlockedCompareExchange(&s_PdhReady, 0, 0); }

void PerfWmiCpu_Start(void)
{
	if(s_hWmiCpuThread != NULL) return;
	InterlockedExchange(&s_WmiCpuStop, 0);
	s_hWmiCpuThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorWmiCpuSpeed, NULL, 0, NULL);
}

void PerfWmiCpu_Stop(void)
{
	if(s_hWmiCpuThread == NULL) return;
	InterlockedExchange(&s_WmiCpuStop, 1);
	WaitForSingleObject(s_hWmiCpuThread, 3000);
	CloseHandle(s_hWmiCpuThread);
	s_hWmiCpuThread = NULL;
}

LONG PerfWmiCpu_GetMhz(void)     { return InterlockedCompareExchange(&s_WmiCpuMhz,     0, 0); }
LONG PerfWmiCpu_GetReady(void)   { return InterlockedCompareExchange(&s_WmiCpuReady,   0, 0); }

// ---------------------------------------------------------------------------
//  PerfIsWindows7: returns TRUE when the running OS is Windows 7 (NT 6.1).
//
//  Implemented via VerifyVersionInfoW so it stays correct even under a
//  manifest that lies to GetVersion(). The helper is referenced from both
//  the cascade (to gate the software Turbo estimator) and the Performance
//  view UI (to show the disclaimer).
// ---------------------------------------------------------------------------
BOOL PerfIsWindows7(void)
{
	OSVERSIONINFOEXW osvi;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	osvi.dwMajorVersion = 6;
	osvi.dwMinorVersion = 1;

	ULONGLONG mask = 0;
	mask = VerSetConditionMask(mask, VER_MAJORVERSION, VER_EQUAL);
	mask = VerSetConditionMask(mask, VER_MINORVERSION, VER_EQUAL);

	return VerifyVersionInfoW(&osvi,
		VER_MAJORVERSION | VER_MINORVERSION, mask) != FALSE;
}
