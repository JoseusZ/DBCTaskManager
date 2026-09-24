// PerfDiskShared.h
#pragma once
#include "stdafx.h"
#define PERF_DISK_MAX 32

extern volatile LONG g_KernelDiskEverLive[PERF_DISK_MAX];
extern LONG g_AnyPerDiskSourceEverLive;

void PerfDiskShared_Init(void);
