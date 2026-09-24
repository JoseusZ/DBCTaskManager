// PerformanceBox_Disk.cpp

#include "stdafx.h"
#include "DBCTaskman.h"
#include "PerformanceBox.h"
#include "PerfDiskShared.h"

// Forward decls for file-level thread functions in PerformanceBox.cpp
UINT Thread_LoadDiskStaticInfo(LPVOID lparam);
UINT Thread_GetDiskOtherStaticInfo(LPVOID lparam);

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

				// Disk activity comes from the WMI / PDH / NtSys background
				// monitors launched by PerfWmiDisk_Start() / PerfPdhDisk_Start()
				// / PerfNtSys_Start() above. The per-tick loop reads from those
				// caches via the Perf*_Get* accessors.

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


