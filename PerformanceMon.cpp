#include "StdAfx.h"
#include "PerformanceMon.h"
#include "DBCTaskman.h"
#include <stdio.h>


#define SystemBasicInformation    0
#define SystemPerformanceInformation    2
#define SystemTimeInformation    3
#define Li2Double(x) ((double)((x).HighPart) * 4.294967296E9 + (double)((x).LowPart))

// ----------------------------------------------------------------------------
// Local log helper — mirrors the _DbgIoLog() in PerformanceBox.cpp so the
// per-IOCTL diagnostic lines from TryDiskIoctl land in the same dbc_dbg_io.log
// file the cascade log uses. The opt-in switch (DBC_DBG_IO_LOG compile flag
// OR an empty dbc_dbg_io.log file next to the exe) is honored.
// ----------------------------------------------------------------------------
static BOOL _MonLogEnabled(void)
{
#ifdef DBC_DBG_IO_LOG
	return TRUE;
#else
	static LONG cached = -1;
	LONG v = InterlockedCompareExchange(&cached, 0, 0);
	if(v >= 0) return (BOOL)v;
	BOOL enabled = (GetFileAttributesA("dbc_dbg_io.log") != INVALID_FILE_ATTRIBUTES);
	InterlockedExchange(&cached, enabled ? 1 : 0);
	return enabled;
#endif
}

static void _MonLog(const char* fmt, ...)
{
	if(!_MonLogEnabled()) return;
	FILE* f = NULL;
	if(fopen_s(&f, "dbc_dbg_io.log", "a") != 0 || !f) return;
	va_list ap; va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

// ----------------------------------------------------------------------------
// EnableVolumePrivilege: enable SeManageVolumePrivilege in the current process
// token so CreateFile on \\.\PhysicalDriveN with GENERIC_READ succeeds.
//
// The privilege exists in admin tokens but is DISABLED by default — Windows
// grants it but the program must explicitly turn it on. Without this, the
// IOCTL_DISK_PERFORMANCE call on internal physical disks returns
// ERROR_ACCESS_DENIED (err=5) even when the user is running as Administrator.
//
// We attempt this exactly once per process. The static sPrivState encodes:
//   0 = not attempted yet, 1 = enabled successfully, 2 = enable failed
// so we don't pay the cost on every IOCTL call.
// ----------------------------------------------------------------------------
static LONG sPrivState = 0;
static BOOL EnableVolumePrivilege(void)
{
	LONG prev = InterlockedCompareExchange(&sPrivState, 1, 0);
	if(prev == 1) return TRUE;        // already enabled
	if(prev == 2) return FALSE;       // already failed, don't retry

	HANDLE hToken = NULL;
	if(!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		_MonLog("EnableVolumePrivilege: OpenProcessToken failed err=%lu", GetLastError());
		InterlockedExchange(&sPrivState, 2);
		return FALSE;
	}

	LUID luid;
	if(!LookupPrivilegeValueW(NULL, L"SeManageVolumePrivilege", &luid))
	{
		_MonLog("EnableVolumePrivilege: LookupPrivilegeValue failed err=%lu", GetLastError());
		CloseHandle(hToken);
		InterlockedExchange(&sPrivState, 2);
		return FALSE;
	}

	TOKEN_PRIVILEGES tp;
	memset(&tp, 0, sizeof(tp));
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
	DWORD err = GetLastError();
	CloseHandle(hToken);

	if(!ok || err == ERROR_NOT_ALL_ASSIGNED)
	{
		_MonLog("EnableVolumePrivilege: AdjustTokenPrivileges err=%lu (token likely non-admin)", err);
		InterlockedExchange(&sPrivState, 2);
		return FALSE;
	}

	_MonLog("EnableVolumePrivilege: SeManageVolumePrivilege ENABLED");
	InterlockedExchange(&sPrivState, 1);
	return TRUE;
}

// ----------------------------------------------------------------------------
// TryOpenDiskHandle: open a disk / volume device with multiple access masks.
// On internal disks the storage driver may refuse GENERIC_READ even for an
// admin process unless SeManageVolumePrivilege is held. FILE_READ_ATTRIBUTES
// is a much smaller mask that the IOCTL subsystem will accept in many cases
// where GENERIC_READ is denied. We try the masks in order and return the
// first handle that opens. Logging is throttled.
// ----------------------------------------------------------------------------
static HANDLE TryOpenDiskHandle(LPCWSTR DevicePath, DWORD Flags)
{
	struct Mode { DWORD access; LPCSTR name; };
	static const Mode modes[] = {
		{ GENERIC_READ,                          "GENERIC_READ"                  },
		{ FILE_READ_ATTRIBUTES | SYNCHRONIZE,    "FILE_READ_ATTRIBUTES|SYNC"     },
		{ FILE_READ_ATTRIBUTES,                  "FILE_READ_ATTRIBUTES"          },
		{ 0,                                     "0"                             },
	};
	const int nModes = sizeof(modes)/sizeof(modes[0]);

	static DWORD sLogged[8] = {0};

	for(int i = 0; i < nModes; i++)
	{
		HANDLE h = CreateFileW(DevicePath, modes[i].access,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			Flags, NULL);
		if(h != INVALID_HANDLE_VALUE)
		{
			if(i > 0)
			{
				DWORD now = GetTickCount();
				int slot = -1;
				for(int k = 0; k < 8; k++) if(!sLogged[k] || (now - sLogged[k]) > 30000) { slot = k; break; }
				if(slot >= 0) {
					sLogged[slot] = now;
					char pathUtf8[MAX_PATH] = {0};
					WideCharToMultiByte(CP_ACP, 0, DevicePath, -1, pathUtf8, MAX_PATH, NULL, NULL);
					_MonLog("TryOpenDiskHandle %S: GENERIC_READ denied, fell back to %s",
						pathUtf8, modes[i].name);
				}
			}
			return h;
		}
	}
	return INVALID_HANDLE_VALUE;
}

CPerformanceMon::CPerformanceMon(void)
 
{
}

CPerformanceMon::~CPerformanceMon(void)
{
}

void CPerformanceMon::Init(void)
{
	 
	MEMORYSTATUSEX  MemStatus;
	MemStatus.dwLength = sizeof(MemStatus);
	BOOL Ret = GlobalMemoryStatusEx(&MemStatus);

	if(Ret)
	{
		theApp.PerformanceInfo.TotalPhysMem = MemStatus.ullTotalPhys;
	}

	

}


int CPerformanceMon::StopMon(void)
{




	//    Close the query.

 


	return 0;
}

 

double CPerformanceMon::GetDiskUsage(int nDisk)
{
	return 0;
}


DWORD CPerformanceMon::_CountSetBits(ULONG_PTR bitMask)
{
	DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;    
    DWORD i;
    
    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
	
	 
}

void CPerformanceMon::GetCpuInfo(void)
{
	
    BOOL done = FALSE;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
    DWORD returnLength = 0;
    DWORD logicalProcessorCount = 0;
    DWORD numaNodeCount = 0;
    DWORD processorCoreCount = 0;
    DWORD processorL1CacheCount = 0;
    DWORD processorL2CacheCount = 0;
    DWORD processorL3CacheCount = 0;
    DWORD processorPackageCount = 0;
    DWORD byteOffset = 0;
    PCACHE_DESCRIPTOR Cache;


   
    while (!done)
    {
        DWORD rc = GetLogicalProcessorInformation(buffer, &returnLength);

        if (FALSE == rc) 
        {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
            {
                if (buffer)free(buffer);

                buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);

                if (NULL == buffer) 
                {
                   // _tprintf(TEXT("\nError: Allocation failure\n"));
                    return  ;
                }
            } 
            else 
            {
                //_tprintf(TEXT("\nError %d\n"), GetLastError());
                return  ;
            }
        } 
        else
        {
            done = TRUE;
        }
    }

    ptr = buffer;

	DWORD SizeCacheL1 = 0,SizeCacheL2 = 0,SizeCacheL3 = 0;

    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) 
    {
        switch (ptr->Relationship) 
        {
        case RelationNumaNode:
            // Non-NUMA systems report a single record of this type.
            numaNodeCount++;
            break;

        case RelationProcessorCore:
            processorCoreCount++;

            // A hyperthreaded core supplies more than one logical processor.
            logicalProcessorCount += _CountSetBits(ptr->ProcessorMask);
            break;

        case RelationCache:
            // Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
            Cache = &ptr->Cache;
			
            if (Cache->Level == 1)
            {
				SizeCacheL1 = Cache->Size;
                processorL1CacheCount++;
            }
            else if (Cache->Level == 2)
            {
				SizeCacheL2 = Cache->Size;
                processorL2CacheCount++;
            }
            else if (Cache->Level == 3)
            {
				SizeCacheL3 = Cache->Size;
                processorL3CacheCount++;
            }
            break;

        case RelationProcessorPackage:
            // Logical processors share a physical package.
            processorPackageCount++;
            break;

        default:
           // _tprintf(TEXT("\nError: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n"));
            break;
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }

	

	/*CString Str;
	CString StrTemp;

	StrTemp = L"\nGetLogicalProcessorInformation results:\n" ;

	Str=Str+StrTemp;

	StrTemp.Format(L"Number of NUMA nodes: %d\n",  numaNodeCount);
	Str=Str+StrTemp;

    StrTemp.Format(L"Number of physical processor packages: %d\n" ,processorPackageCount);
	Str=Str+StrTemp;

    StrTemp.Format(L"Number of processor cores: %d\n" ,processorCoreCount);
	Str=Str+StrTemp;

    StrTemp.Format(L"Number of logical processors: %d\n",logicalProcessorCount);
	Str=Str+StrTemp;

    StrTemp.Format(L"Number of processor L1/L2/L3 caches: %d/%d/%d\n",processorL1CacheCount,processorL2CacheCount,processorL3CacheCount);    
	Str=Str+StrTemp;*/


	CString StrUnit1=L"KB";
	CString StrUnit2=L"KB";
	CString StrUnit3=L"KB";

	SizeCacheL1 = SizeCacheL1/1024*processorL1CacheCount;
	
	SizeCacheL2 = SizeCacheL2/1024*processorL2CacheCount;
	if(SizeCacheL2>1024) 
	{
		StrUnit2 = L"MB";
		SizeCacheL2=SizeCacheL2/1024;
	}

	SizeCacheL3 = SizeCacheL3/1024*processorL3CacheCount;
	if(SizeCacheL3>1024) 
	{
		StrUnit3 = L"MB";
		SizeCacheL3=SizeCacheL3/1024;
	}


	theApp.PerformanceInfo.nNUMA_node = 	numaNodeCount;
	theApp.PerformanceInfo.nPhysicalProcessorPackages = processorPackageCount;
	theApp.PerformanceInfo.nProcessorCores = processorCoreCount;
	theApp.PerformanceInfo.nLogicalProcessor = logicalProcessorCount;
	theApp.PerformanceInfo.StrL1Cache.Format(L"%d%s",SizeCacheL1,StrUnit1);
	theApp.PerformanceInfo.StrL2Cache.Format(L"%d%s",SizeCacheL2,StrUnit2);
	theApp.PerformanceInfo.StrL3Cache.Format(L"%d%s",SizeCacheL3,StrUnit3);








	//StrTemp.Format(L"%d%s/%d%s/%d%s\n",SizeCacheL1,StrUnit1,SizeCacheL2,StrUnit2,SizeCacheL3,StrUnit3);    
	//Str=Str+StrTemp;
 
    free(buffer);
}

double CPerformanceMon::PdhGetInfo(CString Path)
{

	//

	 HQUERY QueryTemp = NULL;

	Status = PdhOpenQuery(NULL, NULL, &QueryTemp);

	if (Status != ERROR_SUCCESS)   { return 0;	}


	HCOUNTER  Counter ;
	
	Status = PdhAddCounter(QueryTemp, Path, 0, &Counter);
 
	if (Status != ERROR_SUCCESS) {PdhCloseQuery ( QueryTemp ) ;	return 0 ;	}
 
	//-----------------------


	PDH_FMT_COUNTERVALUE DisplayValue;
	DWORD CounterType;
 
	Status = PdhCollectQueryData( QueryTemp);	
	if ( Status != ERROR_SUCCESS)	{PdhRemoveCounter(Counter);	PdhCloseQuery ( QueryTemp ) ;		return 0;	}
 

	Status = PdhGetFormattedCounterValue(Counter,  PDH_FMT_DOUBLE,  &CounterType,   &DisplayValue);

	if (Status != ERROR_SUCCESS) 	{PdhRemoveCounter(Counter);	PdhCloseQuery ( QueryTemp ) ;		return 0;	}


	
	double  Ret=DisplayValue.doubleValue;

   PdhRemoveCounter(Counter);
	Status = PdhCloseQuery ( QueryTemp ) ;
	if ( Status != ERROR_SUCCESS ) {	 ;	}

	 


	return Ret;
}


double CPerformanceMon::GetTotalDiskIO(void)
{
	

	
	return 0;
}

double CPerformanceMon::GetTotalNetworkIO(void)
{

	
	return 0;
}

// ---------------------------------------------------------------------
// IOCTL_DISK_PERFORMANCE path-finder
// ---------------------------------------------------------------------
// On Win7 the standard per-disk data source is IOCTL_DISK_PERFORMANCE on
// \\.\PhysicalDriveN. That works for admin sessions on real disks.
// On stripped-down / virtualised setups (VirtualBox, locked-down ACLs,
// certain Win10 builds) the physical-drive path silently returns zero
// bytes because the storage driver declines the IOCTL. We now try FOUR
// candidate paths per disk and return the first one that produces a
// non-zero DISK_PERFORMANCE buffer:
//
//   1. Physical drive ASYNC  (\\.\PhysicalDriveN,  FILE_FLAG_OVERLAPPED)
//   2. Physical drive SYNC   (\\.\PhysicalDriveN,  no OVERLAPPED) — some
//      virtual disk drivers only accept the synchronous form.
//   3. Volume handle ASYNC   (\\.\X:) — partition-level IOCTL_DISK_PERFORMANCE
//      is implemented by the FSD stack above the disk class driver and
//      works on some VMs (especially VirtualBox) where the raw disk path
//      is denied. The first drive letter from VolumeLetter is used.
//   4. Volume handle SYNC    (\\.\X:,  no OVERLAPPED)
//
// We log via the local _MonLog helper which writes to the same
// dbc_dbg_io.log file used by PerformanceBox.cpp's _DbgIoLog; this lets
// the cascade log show exactly which path produced the data on the current
// OS so we can decide whether to retire the slow paths.
// ---------------------------------------------------------------------
BOOL CPerformanceMon::TryDiskIoctl(LPCWSTR DevicePath, BOOL bAsync, DISK_PERFORMANCE& OutBuffer, DWORD& OutErr)
{
	OutErr = 0;
	memset(&OutBuffer, 0, sizeof(OutBuffer));

	DWORD nOutBufferSize = sizeof(DISK_PERFORMANCE);
	DWORD BytesReturned = 0;

	HANDLE hEvent = NULL;
	OVERLAPPED Overlapped;
	memset(&Overlapped, 0, sizeof(Overlapped));

	DWORD dwCreateFlags = 0;
	HANDLE hDevice = INVALID_HANDLE_VALUE;

	// Make sure SeManageVolumePrivilege is on in our token — without this,
	// internal physical disks (\\.\PhysicalDriveN) refuse CreateFile with
	// GENERIC_READ even for an admin process. The privilege exists in admin
	// tokens but is DISABLED by default.
	EnableVolumePrivilege();

	if(bAsync)
	{
		// Async path: open with FILE_FLAG_OVERLAPPED and use a real event.
		hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
		if(hEvent == NULL) { OutErr = GetLastError(); return FALSE; }

		dwCreateFlags = FILE_FLAG_OVERLAPPED;
		hDevice = TryOpenDiskHandle(DevicePath, dwCreateFlags);
		if(hDevice == INVALID_HANDLE_VALUE)
		{
			OutErr = GetLastError();
			CloseHandle(hEvent);
			return FALSE;
		}
		Overlapped.hEvent = hEvent;
	}
	else
	{
		// Sync path: open without FILE_FLAG_OVERLAPPED and pass NULL
		// OVERLAPPED to DeviceIoControl. Some virtual disk drivers
		// (VirtualBox, certain SAN miniports) refuse the async form even
		// when the OS allows it; the sync form works on those.
		dwCreateFlags = 0;
		hDevice = TryOpenDiskHandle(DevicePath, dwCreateFlags);
		if(hDevice == INVALID_HANDLE_VALUE)
		{
			OutErr = GetLastError();
			return FALSE;
		}
	}

	BOOL Rer = DeviceIoControl(hDevice, IOCTL_DISK_PERFORMANCE,
		NULL, 0, &OutBuffer, nOutBufferSize,
		(LPDWORD)&BytesReturned,
		bAsync ? &Overlapped : NULL);

	if(!Rer && bAsync)
	{
		DWORD err = GetLastError();
		if(err == ERROR_IO_PENDING)
		{
			if(WaitForSingleObject(hEvent, 500) == WAIT_OBJECT_0)
			{
				if(GetOverlappedResult(hDevice, &Overlapped, &BytesReturned, FALSE))
					Rer = TRUE;
				else
					err = GetLastError();
			}
			else
			{
				// Timed out — cancel and report failure
				CancelIo(hDevice);
				WaitForSingleObject(hEvent, 100);
				err = WAIT_TIMEOUT;
			}
		}
		OutErr = err;
	}
	else if(!Rer)
	{
		OutErr = GetLastError();
	}

	CloseHandle(hDevice);
	if(hEvent) CloseHandle(hEvent);

	if(!Rer || BytesReturned != nOutBufferSize)
	{
		memset(&OutBuffer, 0, sizeof(OutBuffer));
		return FALSE;
	}
	return TRUE;
}

// Back-compat wrapper kept for callers that don't have a volume letter.
DISK_PERFORMANCE CPerformanceMon::GetDiskPerformance(int DiskID)
{
	return GetDiskPerformance(DiskID, CString());
}

// Real implementation: try up to four IOCTL paths, return the first one that
// produces a non-zero buffer.
DISK_PERFORMANCE CPerformanceMon::GetDiskPerformance(int DiskID, const CString& VolumeLetter)
{
	DISK_PERFORMANCE OutBuffer;
	memset(&OutBuffer, 0, sizeof(OutBuffer));

	// Build candidate list
	struct Candidate {
		CString  path;
		BOOL     async;
		LPCSTR   tag;
	};
	Candidate candidates[4];
	int nCand = 0;

	candidates[nCand].path.Format(L"\\\\.\\PhysicalDrive%d", DiskID);
	candidates[nCand].async = TRUE;  candidates[nCand].tag = "PhysAsync"; nCand++;

	candidates[nCand].path.Format(L"\\\\.\\PhysicalDrive%d", DiskID);
	candidates[nCand].async = FALSE; candidates[nCand].tag = "PhysSync";  nCand++;

	// Volume handle: extract first drive letter from the comma-separated
	// string in pPData->StrOther0 (format is "C:,D:,").
	if(!VolumeLetter.IsEmpty())
	{
		int colonPos = VolumeLetter.Find(L':');
		if(colonPos >= 1)
		{
			WCHAR letter = VolumeLetter.GetAt(colonPos - 1);
			if(letter >= L'A' && letter <= L'Z')
			{
				candidates[nCand].path.Format(L"\\\\.\\%c:", letter);
				candidates[nCand].async = TRUE;  candidates[nCand].tag = "VolAsync";  nCand++;

				candidates[nCand].path.Format(L"\\\\.\\%c:", letter);
				candidates[nCand].async = FALSE; candidates[nCand].tag = "VolSync";   nCand++;
			}
		}
	}

	// Throttle diagnostic logging: only print the first failure of each
	// (disk, tag) pair, plus the first success.
	static DWORD sLoggedFail[8][4] = {0};
	static DWORD sLoggedWin[8]    = {0};

	DWORD nowTick = GetTickCount();

	for(int i = 0; i < nCand; i++)
	{
		DWORD err = 0;
		DISK_PERFORMANCE trial;
		BOOL ok = TryDiskIoctl(candidates[i].path, candidates[i].async, trial, err);
		BOOL nonZero =
			(trial.BytesRead.QuadPart | trial.BytesWritten.QuadPart |
			 trial.ReadTime.QuadPart   | trial.WriteTime.QuadPart   |
			 trial.IdleTime.QuadPart   | trial.ReadCount           |
			 trial.WriteCount          | trial.QueueDepth) != 0;

		if(ok && nonZero)
		{
				if(sLoggedWin[DiskID] == 0 || (nowTick - sLoggedWin[DiskID]) > 30000)
				{
					sLoggedWin[DiskID] = nowTick;
					char pathUtf8[MAX_PATH] = {0};
					WideCharToMultiByte(CP_UTF8, 0, candidates[i].path, -1, pathUtf8, MAX_PATH, NULL, NULL);
					_MonLog("IOCTL win disk=%d tag=%s path=%s (took %lu ms)",
						DiskID, candidates[i].tag, pathUtf8, GetTickCount() - nowTick);
				}
				return trial;
			}

				// Log the first failure of each path so we know what failed.
				int tagIdx = 0;
				if(strcmp(candidates[i].tag, "PhysAsync") == 0) tagIdx = 0;
				else if(strcmp(candidates[i].tag, "PhysSync") == 0) tagIdx = 1;
				else if(strcmp(candidates[i].tag, "VolAsync") == 0) tagIdx = 2;
				else if(strcmp(candidates[i].tag, "VolSync") == 0) tagIdx = 3;

				if(sLoggedFail[DiskID][tagIdx] == 0)
				{
					sLoggedFail[DiskID][tagIdx] = nowTick;
					char pathUtf8[MAX_PATH] = {0};
					WideCharToMultiByte(CP_UTF8, 0, candidates[i].path, -1, pathUtf8, MAX_PATH, NULL, NULL);
					_MonLog("IOCTL fail disk=%d tag=%s path=%s err=%lu ok=%d nonZero=%d",
						DiskID, candidates[i].tag, pathUtf8, err, ok, nonZero);
				}
	}

	// All paths exhausted — return the zero buffer.
	return OutBuffer;
}
