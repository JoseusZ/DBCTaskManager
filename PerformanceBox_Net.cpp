// PerformanceBox_Net.cpp

#include "stdafx.h"
#include "DBCTaskman.h"
#include "PerformanceBox.h"

// inet_ntop / INET_ADDRSTRLEN replacement for deprecated inet_ntoa()
#include <ws2tcpip.h>
#include "PerfNetHelpers.h"
#include "PerfWlan.h"

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
								char ipv4buf[INET_ADDRSTRLEN] = {0};
								if(inet_ntop(AF_INET, &sa_in->sin_addr, ipv4buf, sizeof(ipv4buf)) != NULL)
									StrIPv4 = ipv4buf;
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
							if(PerfNet_IsIPv6LinkLocal(sa))
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
						StrConnType = PerfWlan_GetPhyType(&guid);
						StrSSID     = PerfWlan_GetSsid(&guid);
						ULONG q     = PerfWlan_GetSignalQuality(&guid);
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
