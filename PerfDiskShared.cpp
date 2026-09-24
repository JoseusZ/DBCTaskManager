// PerfDiskShared.cpp
#include "stdafx.h"
#include "PerfDiskShared.h"

volatile LONG g_KernelDiskEverLive[PERF_DISK_MAX];
LONG           g_AnyPerDiskSourceEverLive = 0;

void PerfDiskShared_Init(void)
{
	static LONG inited = 0;
	if(InterlockedCompareExchange(&inited, 1, 0) != 0) return;
	memset((void*)g_KernelDiskEverLive, 0, sizeof(g_KernelDiskEverLive));
	InterlockedExchange(&g_AnyPerDiskSourceEverLive, 0);
}
