// PerfWlan.cpp
#include "stdafx.h"
#include "PerfWlan.h"

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "rpcrt4.lib")

static HANDLE WlanOpenOrNull(void)
{
	HANDLE hClient = NULL;
	DWORD dwMaxClient = 2, dwCurVersion = 0;
	if(WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient) != ERROR_SUCCESS)
		return NULL;
	return hClient;
}

static CString PhyToLabel(DOT11_PHY_TYPE phy)
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
			if((DWORD)phy == (DWORD)dot11_phy_type_he ||
			   (DWORD)phy == /*dot11_phy_type_he*/ 9)
				return L"802.11ax (Wi-Fi 6)";
			return L"802.11";
	}
}

CString PerfWlan_GetPhyType(const GUID* pGuid)
{
	CString s = L"Wi-Fi (unspecified)";
	HANDLE h = WlanOpenOrNull();
	if(h == NULL) return s;

	WLAN_CONNECTION_ATTRIBUTES* pConn = NULL;
	DWORD dwSize = 0;
	DWORD dw = WlanQueryInterface(h, pGuid, wlan_intf_opcode_current_connection,
								  NULL, &dwSize, (PVOID*)&pConn, NULL);
	if(dw == ERROR_SUCCESS && pConn != NULL)
	{
		if(pConn->isState == wlan_interface_state_connected)
			s = PhyToLabel(pConn->wlanAssociationAttributes.dot11PhyType);
		else
			s = L"Not connected";
	}
	if(pConn) WlanFreeMemory(pConn);
	WlanCloseHandle(h, NULL);
	return s;
}

CString PerfWlan_GetSsid(const GUID* pGuid)
{
	CString s = L"Disconnected";
	HANDLE h = WlanOpenOrNull();
	if(h == NULL) return L"";

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

ULONG PerfWlan_GetSignalQuality(const GUID* pGuid)
{
	ULONG q = 0;
	HANDLE h = WlanOpenOrNull();
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
