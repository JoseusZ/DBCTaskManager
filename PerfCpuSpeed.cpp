// PerfCpuSpeed.cpp
#include "stdafx.h"
#include "PerfCpuSpeed.h"

#include <intrin.h>
#include <process.h>
#include <comdef.h>

static volatile LONG  s_CpuSpeedMhz   = 0;
static volatile LONG  s_CpuSpeedReady = 0;
static HANDLE         s_hCpuSpeedThread = NULL;
static volatile LONG  s_CpuSpeedStop    = 0;

static volatile LONG  s_WmiCpuMhz   = 0;
static volatile LONG  s_WmiCpuReady = 0;
static HANDLE         s_hWmiCpuThread = NULL;
static volatile LONG  s_WmiCpuStop    = 0;

static UINT __stdcall Thread_MonitorCpuSpeed(LPVOID lparam)
{
	(void)lparam;

	HANDLE hThread = GetCurrentThread();
	SetThreadAffinityMask(hThread, 1);

	LARGE_INTEGER qpcFreq;
	if(!QueryPerformanceFrequency(&qpcFreq) || qpcFreq.QuadPart <= 0)
		return 0;

	LARGE_INTEGER startQpc; QueryPerformanceCounter(&startQpc);
	unsigned __int64 startTsc = __rdtsc();

	const DWORD SampleMs = 250;
	while(s_CpuSpeedStop == 0)
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

		if(_finite(mhz) == 0) continue;
		if(mhz < 100.0) continue;
		if(mhz > 20000.0) continue;

		LONG mhzInt = (LONG)(mhz + 0.5);
		InterlockedExchange(&s_CpuSpeedMhz, mhzInt);
		InterlockedExchange(&s_CpuSpeedReady, 1);

		startQpc = endQpc;
		startTsc = endTsc;
	}
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

		for(int i = 0; i < 20 && s_WmiCpuStop == 0; i++) Sleep(100);
	}

	if(ownCom) CoUninitialize();
	return 0;
}

void PerfCpuSpeed_Start(void)
{
	if(s_hCpuSpeedThread != NULL) return;
	InterlockedExchange(&s_CpuSpeedStop, 0);
	s_hCpuSpeedThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorCpuSpeed, NULL, 0, NULL);
}

void PerfCpuSpeed_Stop(void)
{
	if(s_hCpuSpeedThread == NULL) return;
	InterlockedExchange(&s_CpuSpeedStop, 1);
	WaitForSingleObject(s_hCpuSpeedThread, 3000);
	CloseHandle(s_hCpuSpeedThread);
	s_hCpuSpeedThread = NULL;
}

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

LONG PerfCpuSpeed_GetMhz(void)   { return InterlockedCompareExchange(&s_CpuSpeedMhz,   0, 0); }
LONG PerfCpuSpeed_GetReady(void) { return InterlockedCompareExchange(&s_CpuSpeedReady, 0, 0); }
LONG PerfWmiCpu_GetMhz(void)     { return InterlockedCompareExchange(&s_WmiCpuMhz,     0, 0); }
LONG PerfWmiCpu_GetReady(void)   { return InterlockedCompareExchange(&s_WmiCpuReady,   0, 0); }
