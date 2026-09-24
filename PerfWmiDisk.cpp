// PerfWmiDisk.cpp
#include "stdafx.h"
#include "PerfWmiDisk.h"
#include "PerfDbgLog.h"

#include <comdef.h>

#define WMI_DISK_MAX PERF_DISK_MAX

static WmiDiskMetrics s_WmiDisk[WMI_DISK_MAX];
static double         s_TotalPct        = 0.0;
static LONG           s_TotalReady      = 0;
static LONG           s_TotalEverNonZero= 0;
static HANDLE         s_hThread         = NULL;
static volatile LONG  s_Stop            = 0;

static BOOL _UnpackDiskPercentVariant(const VARIANT& vt, double* outPct)
{
	*outPct = 0.0;
	if(outPct == NULL) return FALSE;

	VARTYPE vtT = V_VT(&vt);
	if(vtT == VT_UI8 || vtT == VT_I8)
	{
		ULONGLONG raw = (vtT == VT_UI8) ? V_UI8(&vt) : (ULONGLONG)V_I8(&vt);
		ULONG busy  = (ULONG)(raw & 0xFFFFFFFFul);
		ULONG total = (ULONG)((raw >> 32) & 0xFFFFFFFFul);

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
			*outPct = (double)busy;
			return TRUE;
		}
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

	while(s_Stop == 0)
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
									isTotal = TRUE;
								}
							}

							if(diskIdx >= 0 || isTotal)
							{
								double pctResult = 0.0;
								if(SUCCEEDED(pObj->Get(L"PercentDiskTime", 0, &vtPct, 0, 0)))
								{
									(void)_UnpackDiskPercentVariant(vtPct, &pctResult);
								}

								if(isTotal)
								{
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

		{
			static LONG wmiSummaryTick = 0;
			LONG wt = InterlockedIncrement(&wmiSummaryTick);
			if((wt % 10) == 1)
			{
				char buf[1024] = "WMI-DISK poll: ";
				char num[16];
				int first = 1;
				for(int i = 0; i < WMI_DISK_MAX; i++)
				{
					if(local[i].Ready)
					{
						if(!first) strcat_s(buf, sizeof(buf), ",");
						first = 0;
						sprintf_s(num, sizeof(num), "%d:%.1f%%/R%.0f/W%.0f",
							i, local[i].Pct, local[i].ReadBps, local[i].WriteBps);
						strcat_s(buf, sizeof(buf), num);
					}
				}
				if(localTotalReady) {
					char tb[64];
					sprintf_s(tb, sizeof(tb), " _Total=%.1f", localTotalPct);
					strcat_s(buf, sizeof(buf), tb);
				}
				_DbgIoLog("%s", buf);
			}
		}
		for(int i = 0; i < WMI_DISK_MAX; i++)
		{
			if(local[i].Ready)
			{
				s_WmiDisk[i].ReadBps  = local[i].ReadBps;
				s_WmiDisk[i].WriteBps = local[i].WriteBps;
				s_WmiDisk[i].Pct      = local[i].Pct;
				s_WmiDisk[i].Ready    = 1;
				if(local[i].Pct > 0.0 || local[i].ReadBps > 0.0 || local[i].WriteBps > 0.0)
								{
									s_WmiDisk[i].EverNonZero = 1;
									InterlockedExchange(&g_AnyPerDiskSourceEverLive, 1);
								}
							}
						}

		if(localTotalReady)
		{
			s_TotalPct        = localTotalPct;
			s_TotalReady      = 1;
			if(localTotalPct > 0.0)
				s_TotalEverNonZero = 1;
		}

		for(int i = 0; i < 10 && s_Stop == 0; i++) Sleep(100);
	}

	if(ownCom) CoUninitialize();
	return 0;
}

void PerfWmiDisk_Start(void)
{
	if(s_hThread != NULL) return;
	PerfDiskShared_Init();
	InterlockedExchange(&s_Stop, 0);
	memset(s_WmiDisk, 0, sizeof(s_WmiDisk));
	s_hThread = (HANDLE)_beginthreadex(NULL, 0, Thread_MonitorWmiDisk, NULL, 0, NULL);
}

void PerfWmiDisk_Stop(void)
{
	if(s_hThread == NULL) return;
	InterlockedExchange(&s_Stop, 1);
	WaitForSingleObject(s_hThread, 3000);
	CloseHandle(s_hThread);
	s_hThread = NULL;
}

BOOL PerfWmiDisk_GetPerDisk(int idx, WmiDiskMetrics* out)
{
	if(idx < 0 || idx >= WMI_DISK_MAX || out == NULL) return FALSE;
	*out = s_WmiDisk[idx];
	return TRUE;
}

double PerfWmiDisk_GetTotalPct(void)         { return s_TotalPct;         }
LONG   PerfWmiDisk_GetTotalReady(void)       { return s_TotalReady;       }
LONG   PerfWmiDisk_GetTotalEverNonZero(void) { return s_TotalEverNonZero; }
