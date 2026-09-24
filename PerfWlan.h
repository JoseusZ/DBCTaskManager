// PerfWlan.h
#pragma once

#include "stdafx.h"
#include <wlanapi.h>

// WLAN helpers (Win7+) — connection type (PHY), SSID, signal quality.
// Used to populate the Performance-tab adapter info box, matching the fields
// shown by Windows 10 native Task Manager.

CString PerfWlan_GetPhyType(const GUID* pGuid);
CString PerfWlan_GetSsid(const GUID* pGuid);
ULONG   PerfWlan_GetSignalQuality(const GUID* pGuid);
