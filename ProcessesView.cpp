// ProcessesView.cpp : implementation file


// ProcessesView.cpp : implementation file
//

#include "stdafx.h"
#include "DBCTaskman.h"
#include "ProcessesView.h"
#include "DetailsView.h"







//  ==========================================    ��������б� ===================================

static UINT Thread_FillAllItemsData(LPVOID pParam);
static UINT Thread_GenProcesses(LPVOID pParam)
{

	CPageProcesses  *Dlg=(CPageProcesses *)pParam;
	if(Dlg == NULL) return -1; //

	Dlg->PreListItems();	 	 

	AfxEndThread(0,TRUE);
	return 0;

}




///---------------- ������������   -------------

UINT Thread_FillAllItemsData(LPVOID pParam)
{

	// FIX T3: este worker thread ya NO llama Dlg->FillAllItemData()
	// directamente porque esa funcion itera mTaskList (UI control MFC)
	// desde un thread secundario, lo cual es ilegal en MFC.
	// En su lugar, encolamos UM_REFRESH y el UI thread ejecuta la
	// recarga real en PreTranslateMessage -> FillAllItemData().
	// PostMessage SI es cross-thread-safe (MFC lo enruta internamente
	// a la cola de mensajes del thread que creo la ventana).

	CPageProcesses  *Dlg=(CPageProcesses *)pParam;
	if(Dlg == NULL) return -1; //

	Dlg->PostMessageW(UM_REFRESH, 0, 0);
	AfxEndThread(0,TRUE);
	return 0;

}



static UINT Thread_GetItemName(LPVOID pParam)
{

	PROCLISTDATA  *Data=(PROCLISTDATA *)pParam;
	if(Data== NULL) return -1; //

	//Dlg->PreListItems();	 	 
	//Dlg->mTaskList.Invalidate();
	CProcess mProcInfo;
	CString StrDescription=mProcInfo.GetVerInfoString(mProcInfo.GetPathName(Data->hProcess),L"FileDescription");
	Data->Description=StrDescription;

	if(StrDescription == L"")
		Data->Description =Data->Name;

	
	AfxEndThread(0,TRUE);

	return 0;

}














//-------------------------------------------------------------------------

typedef struct tagWNDINFO
{
	DWORD dwProcessId;
	HWND hWnd;
	HWND Dll_hwnd;
} WNDINFO, *LPWNDINFO;





//  ==========================================     ����  ===================================







//����   ���� �� �� ���� SortType  ��������
int CALLBACK Sort_Processes(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)  
{
	CCoolListCtrl* pList= &( ((CPageProcesses*)theApp.pSelPage)->mTaskList);

	int iCol =(int) lParamSort;
	int result = 0;     //����ֵ   


	APPLISTDATA * pData1 = NULL;
	APPLISTDATA * pData2 = NULL;


	pData1 = (APPLISTDATA * )lParam1;
	pData2 = (APPLISTDATA * )lParam2;

	// DIAG: trace opt-in del callback de sort. Throttle 500ms para no inundar el log.
	// Registra: iCol, SubTypes, PIDs, Str1/Str2 (texto comparado), y case del switch que se ejecuta.
	{
		static DWORD _lastDumpTick = 0;
		DWORD _now = GetTickCount();
		if (_ListDiagEnabled() && (_now - _lastDumpTick) > 500) {
			DWORD p1 = (pData1 && pData1->pPData) ? (unsigned long)((PROCLISTDATA*)pData1->pPData)->PID : 0;
			DWORD p2 = (pData2 && pData2->pPData) ? (unsigned long)((PROCLISTDATA*)pData2->pPData)->PID : 0;
			_ListDiagLog("Sort iCol=%d Sub1=%d Sub2=%d PID1=%lu PID2=%lu",
				(int)iCol,
				pData1 ? pData1->SubType : -99,
				pData2 ? pData2->SubType : -99,
				(unsigned long)p1, (unsigned long)p2);
			_ListDiagDump("Sort", 0, pList);
			_lastDumpTick = _now;
		}
	}

	//	int Data1,Data2;
	if((pData1==NULL) || (pData2 == NULL) )return result;


	CString Str1,Str2;

	Str1= pList->GetItemText(pData1->SortID,iCol);
	Str2= pList->GetItemText(pData2->SortID,iCol);









	switch(iCol)
	{
	case PROCLIST_TYPE:
	case PROCLIST_STATUS:
	case PROCLIST_PUB:
	case PROCLIST_PNAME:
	case PROCLIST_CMDLINE:

		//�����ж�Ҫ�Ȱ� pid���� ��������չ�������������븸���п��ܴ�����������


		result =lstrcmp(Str1,Str2);
		if(result==0)
		{
				int PID1,PID2;
				if(pData1->pPData == NULL ) PID1 =  0 ; else PID1 = ((PROCLISTDATA *) pData1->pPData)->PID;
				if(pData2->pPData == NULL ) PID2 =  0 ; else PID2 = ((PROCLISTDATA *) pData2->pPData)->PID;
				result=PID1-PID2;
		}		
		break;
	case PROCLIST_NAME: //��һ�����⴦��

		if(theApp.AppSettings.GroupByType) //������ʾ
			result = pData1->ItemType - pData2->ItemType;
		else//��������ʾʱ����
			result = 0; 

		//------------------------------

		if(result != 0)
		{
			goto SORTOK;
		}
		else //��ͬʱ ���������� //���������ڵ� ���� ������ PID����ͬ�����Ȱ�PID ����Ȼ�� �ڰ� SUBType ����չ���� ����subtype ��1 ��������0���ڻ�������ȷ������
		{
			CString  StrDescription1=L"";
			CString  StrDescription2=L"";


			if(pData1->pPData != NULL )
			{


				StrDescription1 = ((PROCLISTDATA *) pData1->pPData)->Description;//Xxxxxxxxx
				if(StrDescription1==L"") StrDescription1=  ((PROCLISTDATA *) pData1->pPData)->Name;

			}

			if(pData2->pPData != NULL )
			{	

				 
				StrDescription2 = ((PROCLISTDATA *) pData2->pPData)->Description;
				if(StrDescription2==L"") StrDescription2=  ((PROCLISTDATA *) pData2->pPData)->Name;

			}



			result=lstrcmp(StrDescription1,StrDescription2);
			if(result != 0)
			{
				goto SORTOK;
			}
			else //��� ��������ͬ���Ȱ�PID����
			{
				int PID1,PID2;
				if(pData1->pPData == NULL ) PID1 =  0 ; else PID1 = ((PROCLISTDATA *) pData1->pPData)->PID;
				if(pData2->pPData == NULL ) PID2 =  0 ; else PID2 = ((PROCLISTDATA *) pData2->pPData)->PID;
				result=PID1-PID2; 				
				goto SORTOK;
			}	 
		}


		break;

	default:


		



		CString StrL1,StrL2;
		StrL1.Format(L"%16s",Str1);//תΪͬ�������ַ���ǰ�油�ո�
		StrL2.Format(L"%16s",Str2);//תΪͬ�������ַ���ǰ�油�ո�

		result =lstrcmp(StrL1,StrL2);
		if(result==0)//�����ж�Ҫ�� pid���� ��������չ�������������븸���п��ܴ�����������
		{
			int PID1,PID2;
			if(pData1->pPData == NULL ) PID1 =  0 ; else PID1 = ((PROCLISTDATA *) pData1->pPData)->PID;
			if(pData2->pPData == NULL ) PID2 =  0 ; else PID2 = ((PROCLISTDATA *) pData2->pPData)->PID;
			result=PID1-PID2;
		}	


	}




	// DIAG: registrar el resultado del switch ANTES del tiebreak final SORTOK.
	// Asi sabemos que case del switch se ejecuto y los Str1/Str2 que se compararon.
	// Throttle: solo loguea cuando estamos viendo comparaciones con sub-items
	// (Sub1==3 o Sub2==3) o cuando los PIDs empatan (comparacion padre vs sub).
	if (_ListDiagEnabled() && pData1 && pData2 && pData1->pPData && pData2->pPData)
	{
		static DWORD _lastTieTick = 0;
		DWORD _tNow = GetTickCount();
		DWORD p1 = ((PROCLISTDATA*)pData1->pPData)->PID;
		DWORD p2 = ((PROCLISTDATA*)pData2->pPData)->PID;
		BOOL bRelevant = (pData1->SubType == SUB_ITEM || pData2->SubType == SUB_ITEM) || (p1 == p2);
		if (bRelevant && (_tNow - _lastTieTick) > 100)
		{
			const char* szCase = "?";
			if (iCol == PROCLIST_NAME) szCase = "NAME";
			else if (iCol == PROCLIST_PID) szCase = "PID";
			else if (iCol == PROCLIST_CPU) szCase = "CPU";
			else if (iCol == PROCLIST_MEMORY) szCase = "MEM";
			else if (iCol == PROCLIST_DISK) szCase = "DISK";
			else if (iCol == PROCLIST_NETWORK) szCase = "NET";
			else if (iCol == PROCLIST_TYPE) szCase = "TYPE";
			else if (iCol == PROCLIST_STATUS) szCase = "STATUS";
			else if (iCol == PROCLIST_PUB) szCase = "PUB";
			else if (iCol == PROCLIST_PNAME) szCase = "PNAME";
			else if (iCol == PROCLIST_CMDLINE) szCase = "CMD";
			else szCase = "?";
			_ListDiagLog("  cmp case=%s iCol=%d Sub1=%d Sub2=%d PID1=%lu PID2=%lu Str1=\"%s\" Str2=\"%s\" preResult=%d",
				szCase, (int)iCol,
				pData1->SubType, pData2->SubType,
				(unsigned long)p1, (unsigned long)p2,
				(LPCSTR)(CStringA)Str1.Left(20), (LPCSTR)(CStringA)Str2.Left(20),
				result);
			_lastTieTick = _tNow;
		}
	}


SORTOK:
	if(result == 0)
	{							
		result = (pData1->SubType  - pData2->SubType);  //����SubType ֵ���

		if(result != 0)
		{
			return result;   //��Ҫ�ٴη�ת��
		}
		else  //��� �����ڱ������� ����ڱ���Ϊ��
		{
			result =  lstrcmp(pData1->StrWnd,pData2->StrWnd);
			//�ߵ�������
			if( pList->FlagSortUp == FALSE)
			{
				result = -result;
			}

			return result; 
		}
	}




	//�ߵ�������
	if( pList->FlagSortUp == FALSE)
	{
		result = -result;

	}



	return result;  


}




//----------------------------------------------------------------------------------------------------














//======================================================================================================================
















//======================================================================================================================



// CPageProcesses

IMPLEMENT_DYNCREATE(CPageProcesses, CFormView)

CPageProcesses::CPageProcesses()
: CFormView(CPageProcesses::IDD)
,nApp(0)
,nBkg(0)
,nWin(0)
, StrWndList(_T(""))
, FlagEnableRefresh(FALSE)
, pPageDetails(NULL)
, pPerformanceMon(NULL)
, CoolheaderCtrl(NULL)
, SvchostIconIndex(0)
{

	nApp = nBkg = nWin =0;
}

CPageProcesses::~CPageProcesses()
{
}

void CPageProcesses::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROCESSLIST, mTaskList);
}

BEGIN_MESSAGE_MAP(CPageProcesses, CFormView)

	ON_WM_SIZE()
	ON_MESSAGE(UM_TIMER,OnUMTimer)
	ON_WM_CTLCOLOR()
	ON_WM_INITMENUPOPUP()
	ON_NOTIFY(NM_RCLICK, IDC_PROCESSLIST, &CPageProcesses::OnNMRClickProcesslist)
	ON_COMMAND(ID_PROCESSESLIST_EXPAND, &CPageProcesses::OnPop_ProcesseslistExpand)
	ON_COMMAND(ID_PROCESSESLIST_ENDTASK, &CPageProcesses::OnPop_ProcesseslistEndtask)
	ON_COMMAND(ID_PROCESSESLIST_GOTODETAILS, &CPageProcesses::OnPop_ProcesseslistGotoDetails)
	ON_BN_CLICKED(IDC_BTN_ENDTASK, &CPageProcesses::OnBnClickedBtnEndTask)

	ON_NOTIFY(LVN_KEYDOWN, IDC_PROCESSLIST, &CPageProcesses::OnLvnKeydownProcesslist)

	ON_COMMAND(ID_PROCESSESLIST_OPENFILELOCATION, &CPageProcesses::OnPop_OpenFileLocation)
	ON_COMMAND(ID_PROCESSESLIST_PROPERTIES, &CPageProcesses::OnPop_Properties)
	ON_COMMAND(ID_PROCLISTTASKITEM_SWITCHTO, &CPageProcesses::OnPop_SwitchTo)
	ON_COMMAND(ID_PROCLISTTASKITEM_BRINGTOFRONT, &CPageProcesses::OnPop_Bringtofront)
	ON_COMMAND(ID_PROCLISTTASKITEM_MAXIMIZE, &CPageProcesses::OnPop_Maximize)
	ON_COMMAND(ID_PROCLISTTASKITEM_MINIMIZE, &CPageProcesses::OnPop_Minimize)
	ON_COMMAND(ID_PROCLISTTASKITEM_ENDTASK, &CPageProcesses::OnPop_Endtask)
	ON_COMMAND(ID_MEMORY_PERCENTS_PLIST, &CPageProcesses::OnPop_Memory_ShowAsPercents)
	ON_COMMAND(ID_MEMORY_VALUES_PLIST, &CPageProcesses::OnPop_Memory_ShowAsValues)
	ON_COMMAND(ID_PROCESSESLIST_SEARCHONLINE, &CPageProcesses::OnPop_Searchonline)
END_MESSAGE_MAP()


// CPageProcesses diagnostics

#ifdef _DEBUG
void CPageProcesses::AssertValid() const
{
	CFormView::AssertValid();
}

#ifndef _WIN32_WCE
void CPageProcesses::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif
#endif //_DEBUG


// CPageProcesses message handlers

void CPageProcesses::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here


	PlaceAllCtrl();

	mTaskList._GetRedrawColumn();////��Ҫ�����������ϸ��·�����ʾ������



}

void CPageProcesses::InitList(void)
{ 
	// ������Ի����Ч�� �Ͳ����������� ������
	//SetWindowTheme(mTaskList.GetSafeHwnd(),L"explorer", NULL);


	mTaskList.SetExtendedStyle(mTaskList.GetExtendedStyle()|LVS_EX_FULLROWSELECT| LVS_OWNERDRAWFIXED|LVS_EX_DOUBLEBUFFER |LVS_EX_HEADERDRAGDROP );  //| LVS_EX_GRIDLINES |LVS_EX_CHECKBOXES

	mTaskList.ModifyStyle (0, LVS_SHAREIMAGELISTS, 0);
	mTaskList.FullColumnCount= 11; //�����ȫ������ ��

	if(!theApp.FlagThemeActive)
	{
		mTaskList.ModifyStyleEx(WS_EX_CLIENTEDGE,0);
		mTaskList.ModifyStyle(0,WS_BORDER);
	}

	//---------------------------------------------------------------------

	mTaskList.pColStatusArray = COL_SAT_PROC;//��¼�����ֵ�����


	mTaskList.InitAllColumn(COL_SAT_PROC,STR_COLUMN_PROCESS,COL_COUNT_PROC);//11 ��ȫ��������



	//-------------------------------------------------------------------

	SvchostIconIndex = theApp.mSvchostIconIndex;





	mTaskList.SetImageList(theApp.mImagelist.m_hImageList);  //�����˿����Ի��ͷ �������InsertColumn֮��!!!!!!!!!


	//--------------------------------------------------------


	//ListProcesses(); //�˺���ת������߳��� ����������ٶȣ�����





}

void CPageProcesses::OnLvnGetdispinfoProcesslist(NMHDR *pNMHDR, LRESULT *pResult)
{
}
 
LRESULT CPageProcesses::OnUMTimer( WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your message handler code here and/or call default



	if(!FlagEnableRefresh) return 0; 

	//mTaskList.XXX=0;



	int nCount = mTaskList.GetItemCount();
	APPLISTDATA * pData = (APPLISTDATA *)mTaskList.GetItemData(nCount-1);
	if(pData->SubType == -1)
	{



		mTaskList.CurrentSortColumn = PROCLIST_NAME ;
		Sort(0,FALSE);
	}


	BOOL  UpdateWndList = CheckWndChange();

	CRect rcItem,rc,rcTemp;

	rc = mTaskList._GetRedrawColumn();


	CString StrItem;


	int i= 0;


	int NewAppCount,NewBkgPrcCount,NewWinPrcCount;

	NewAppCount = NewBkgPrcCount = NewWinPrcCount = 0;





	while(1)
	{
		if(mTaskList.GetItemCount() == i) break ;

		mTaskList.GetItemRect(i,rcItem,LVIR_BOUNDS);
		BOOL RedrawItem =  IntersectRect (rcTemp,rcItem,rc);


		APPLISTDATA *pListData = (APPLISTDATA *)mTaskList.GetItemData(i);

		if( (pListData == NULL) || (pListData->pPData == NULL ))  //ע���ų��������
		{
			i++; continue ;
		}

		if(  pListData->SubType == SUB_ITEM)    //ע���ų�����  ���� ֻ����´��ڱ��� //��״̬
		{


			if( RedrawItem && COL_SAT_PROC[PROCLIST_NAME].Redraw   )
			{

				APPLISTDATA *pAboveData = (APPLISTDATA *)mTaskList.GetItemData(i-1);

				if(pAboveData!=NULL)
				{
					pListData->CoolUsageArray[PROCLIST_CPU] = pAboveData->CoolUsageArray[PROCLIST_CPU];
					pListData->CoolUsageArray[PROCLIST_MEMORY] = pAboveData->CoolUsageArray[PROCLIST_MEMORY];
					pListData->CoolUsageArray[PROCLIST_DISK] = pAboveData->CoolUsageArray[PROCLIST_DISK];
					pListData->CoolUsageArray[PROCLIST_NETWORK] = pAboveData->CoolUsageArray[PROCLIST_NETWORK];
				}


				::GetWindowText(pListData->hMainWnd,pListData->StrWnd.GetBuffer(MAX_PATH),MAX_PATH);
				pListData->StrWnd.ReleaseBuffer();

				//CString StrTemp;

				//StrTemp=mTaskList.GetItemText(i,PROCLIST_NAME);
				//if(strcmp(StrTemp,pListData->StrWnd
				mTaskList.MySetItemText(i,PROCLIST_NAME,pListData->StrWnd);
			}

			i++;
			continue ;

		}



		//---------------- ǰ��ļ������ ƽ̨� ��С �ǰ  ��----------------





		if( pListData->ItemType == APP  )
		{
			NewAppCount ++ ;
		}
		else if( pListData->ItemType == BKGPROC  )
		{
			NewBkgPrcCount++;
		}
		else if( pListData->ItemType == WINPROC  )
		{
			NewWinPrcCount++;
		}




		if( (pListData->ItemType != WINPROC)  &&  UpdateWndList)
		{

			ListItemWindows(i,pListData);	
		} 

		//����Ϊ�����Ƿ���ƶ���������

		if( !RedrawItem ) {i++;continue ; } 



		//---------------------CPU---------------------------------


		if(COL_SAT_PROC[PROCLIST_CPU].Redraw)
		{			



			mProcInfo.GetCpuUsage( (PROCLISTDATA*)pListData->pPData );
			double Usage = ( (PROCLISTDATA*)pListData->pPData )->CPU_Usage;

			if(Usage>=0 && Usage<=100) //��ֹ��ʾ���ҵ� ����
			{
				// User format: idle shows "0" (no decimals), non-idle uses one decimal ("0.1%").
				if(Usage == 0.0)
					StrItem = L"0%";
				else
					StrItem.Format(L"%0.1f%%",Usage );
				pListData->CoolUsageArray[PROCLIST_CPU]= Usage/100; //���ڱ�ɫ��ʾ������
				mTaskList.MySetItemText(i,PROCLIST_CPU,StrItem);
			}


		}


		//---------------------Memory---------------------------------

		if(COL_SAT_PROC[PROCLIST_MEMORY].Redraw)
		{			 

			double OldWorkingSet = ((PROCLISTDATA*)pListData->pPData )->Mem_WorkingSet;
			mProcInfo.GetMemUsageInfo((PROCLISTDATA*)pListData->pPData);
			double WSDelta = ((PROCLISTDATA*)pListData->pPData )->Mem_WorkingSet - OldWorkingSet;

			if(WSDelta!=0)
			{
				_GetMemDataAndSetItemText(i,(APPLISTDATA *)pListData  );
			}

		}

		//---------------------disk---------------------------------



		double OtherBytePerSec = 0;
		double DiskUsageBytePerSec = 0;
		double DiskUsageMBPerSec = 0;
		// Bug fix Win7 non-admin: NewOtherIO was previously uninitialised when
		// the underlying call couldn't open the process (hProcess==NULL on system
		// processes under non-admin). That produced the huge "phantom" KB/s
		// values shown on the disk/network columns of idle processes.
		ULONGLONG NewOtherIO = 0;
		ULONGLONG NewIO = 0;
		// Cambio F: GetDiskIO es una syscall NT que con ~200 procesos suma
		// ~200 syscalls/seg innecesarias si ni Disk ni Network son visibles
		// (sus resultados solo se usan dentro de los bloques protegidos por
		// COL_SAT_PROC[PROCLIST_DISK].Redraw y [...NETWORK].Redraw). Cuando
		// ambas columnas estan ocultas dejamos NewIO=0 y NewOtherIO=0, asi
		// DiskUsageBytePerSec/OtherBytePerSec son 0 y no se pinta nada raro.
		// Edge case: si luego el usuario activa una columna, el primer tick
		// tendra un delta obsoleto, pero los clamps ya existentes en este
		// archivo (lineas ~674 y ~728) lo dejaran en "0 MB/s" / "0 KB/s"
		// durante un tick antes de estabilizarse.
		if(COL_SAT_PROC[PROCLIST_DISK].Redraw || COL_SAT_PROC[PROCLIST_NETWORK].Redraw)
		{
			NewIO = mProcInfo.GetDiskIO(((PROCLISTDATA*)pListData->pPData)->hProcess,
									   ((PROCLISTDATA*)pListData->pPData)->PID,
									   &NewOtherIO);
		}
		DiskUsageBytePerSec =  ((double) (NewIO - ((PROCLISTDATA*)pListData->pPData)->DiskIO ))/theApp.AppSettings.TimerStep;
		OtherBytePerSec  =   ((double) (NewOtherIO - ((PROCLISTDATA*)pListData->pPData)->OtherIO ))/theApp.AppSettings.TimerStep;
		((PROCLISTDATA*)pListData->pPData)->OtherIO = NewOtherIO;


		if(COL_SAT_PROC[PROCLIST_DISK].Redraw)
		{
			DiskUsageMBPerSec = ((double)(NewIO-((PROCLISTDATA*)pListData->pPData)->DiskIO))/1048576/theApp.AppSettings.TimerStep ;

			if(_finite(DiskUsageMBPerSec) == 0) DiskUsageMBPerSec = 0;
			if(DiskUsageMBPerSec>=0 && DiskUsageMBPerSec < 1024.0*1024.0)//��ֹ��ʾ���ҵ� ����
			{

				// User format: idle shows "0 MB/s", non-idle uses one decimal ("0.1 MB/s").
				if(DiskUsageMBPerSec == 0.0)
					StrItem = L"0 MB/s";
				else
					StrItem.Format(L"%0.1f MB/s",DiskUsageMBPerSec);    //  ����1024/1024/0.5
				mTaskList.MySetItemText(i,PROCLIST_DISK,StrItem);
				((PROCLISTDATA*)pListData->pPData)->DiskIO = NewIO;

			}
			else
			{
				// Out-of-range (NaN, Inf, or ridiculous counter) - clamp to 0.
				mTaskList.MySetItemText(i,PROCLIST_DISK,L"0 MB/s");
				((PROCLISTDATA*)pListData->pPData)->DiskIO = NewIO;
			}

		}



		//-------------- Network-----------------

		if(COL_SAT_PROC[PROCLIST_NETWORK].Redraw)
		{
			double NetUsage = 0;

			//double IOUsage = mProcInfo.GetIOUsage(NULL,((PROCLISTDATA*)pListData->pPData)->hQueryIO,((PROCLISTDATA*)pListData->pPData)->hCounterIO);
			//NetUsage = IOUsage-DiskUsageBytePerSec-OtherBytePerSec;


			IO_COUNTERS  IOCounter;
			// Bug fix Win7 non-admin: GetProcessIoCounters returns FALSE without
			// zeroing IOCounter when hProcess is NULL/invalid. Zero first so we
			// never read uninitialised stack memory into the display.
			memset(&IOCounter, 0, sizeof(IOCounter));
			BOOL bIoOk = GetProcessIoCounters(((PROCLISTDATA*)pListData->pPData)->hProcess,&IOCounter);
			if(bIoOk)
			{
				ULONGLONG CurrentIO = IOCounter.ReadTransferCount+IOCounter.WriteTransferCount - pListData->IOLast;
				NetUsage =(double)((CurrentIO/theApp.AppSettings.TimerStep )-DiskUsageBytePerSec)*0.9;
				pListData->IOLast = IOCounter.ReadTransferCount+IOCounter.WriteTransferCount;
			}
			else
			{
				pListData->IOLast = 0;
			}



			if(_finite(NetUsage) == 0) NetUsage = 0;        // NaN / Inf guard
			if(NetUsage < 0) NetUsage = 0;
			if(NetUsage > 1024.0*1024.0*1024.0) NetUsage = 0; // clamp insane values

			if(NetUsage<0.001) NetUsage = 0;  //��ֹ��ʾ���ҵ� ����


			CString StrOut;
						// User format: idle shows "0 KB/s", non-idle uses one decimal ("0.1 KB/s").
						if(NetUsage == 0.0)
							StrItem = L"0 KB/s";
						else
							StrItem.Format(L"%0.1f KB/s",  NetUsage/1024 );

						mTaskList.MySetItemText(i,PROCLIST_NETWORK,StrItem);



		}


		//-------------------���и������----------------------


		i++; //ע�ⲻ��ɾ��������


	}

 
	

	if(UpdateWndList)ReSort();  //timer update





	CString StrColheaderTemp;
	BOOL FlagRedrawHeacerCtrl = FALSE;
	if(COL_SAT_PROC[PROCLIST_CPU].Redraw)
	{
		float NewCpuPct = (float)theApp.PerformanceInfo.CpuUsage;
		StrColheaderTemp.Format(L"%.0f%%", NewCpuPct);
		if ((wcscmp(COL_SAT_PROC[PROCLIST_CPU].StrItem, StrColheaderTemp) != 0) || (COL_SAT_PROC[PROCLIST_CPU].Percents != NewCpuPct))
		{
			StringCchCopy(COL_SAT_PROC[PROCLIST_CPU].StrItem,5,StrColheaderTemp);
			COL_SAT_PROC[PROCLIST_CPU].Percents = NewCpuPct;
			FlagRedrawHeacerCtrl = TRUE;
		}
	}

	if(COL_SAT_PROC[PROCLIST_MEMORY].Redraw)
	{
		float NewMemPct = (float)theApp.PerformanceInfo.MemoryUsage;
		StrColheaderTemp.Format(L"%d%%",(int)NewMemPct);
		if ((wcscmp(COL_SAT_PROC[PROCLIST_MEMORY].StrItem, StrColheaderTemp) != 0) || (COL_SAT_PROC[PROCLIST_MEMORY].Percents != NewMemPct))
		{
			StringCchCopy(COL_SAT_PROC[PROCLIST_MEMORY].StrItem,5,StrColheaderTemp);
			COL_SAT_PROC[PROCLIST_MEMORY].Percents = NewMemPct;
			FlagRedrawHeacerCtrl = TRUE;
		}
	}

	if(COL_SAT_PROC[PROCLIST_DISK].Redraw)
	{
		double DiskPct = theApp.PerformanceInfo.TotalDiskUsage;
		if(_finite(DiskPct) == 0) DiskPct = 0;
		if(DiskPct < 0) DiskPct = 0;
		if(DiskPct > 100) DiskPct = 100;
		float NewDiskPct = (float)DiskPct;
		StrColheaderTemp.Format(L"%.0f%%", NewDiskPct);
		if ((wcscmp(COL_SAT_PROC[PROCLIST_DISK].StrItem, StrColheaderTemp) != 0) || (COL_SAT_PROC[PROCLIST_DISK].Percents != NewDiskPct))
		{
			StringCchCopy(COL_SAT_PROC[PROCLIST_DISK].StrItem,5,StrColheaderTemp);
			COL_SAT_PROC[PROCLIST_DISK].Percents = NewDiskPct;
			FlagRedrawHeacerCtrl = TRUE;
		}
	}

	if(COL_SAT_PROC[PROCLIST_NETWORK].Redraw)
	{
		float NewNetPct = (float)theApp.PerformanceInfo.TotalNetUsage;
		StrColheaderTemp.Format(L"%.0f%%", NewNetPct);
		if ((wcscmp(COL_SAT_PROC[PROCLIST_NETWORK].StrItem, StrColheaderTemp) != 0) || (COL_SAT_PROC[PROCLIST_NETWORK].Percents != NewNetPct))
		{
			StringCchCopy(COL_SAT_PROC[PROCLIST_NETWORK].StrItem,5,StrColheaderTemp);
			COL_SAT_PROC[PROCLIST_NETWORK].Percents = NewNetPct;
			FlagRedrawHeacerCtrl = TRUE;
		}
	}

	if(FlagRedrawHeacerCtrl)
		this->mTaskList.CoolheaderCtrl.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_NOERASE);





	int OldAppCount = nApp;
	int OldBkgCount = nBkg;
	int OldWinCount = nWin;

	nApp = NewAppCount;
	nBkg = NewBkgPrcCount;
	nWin = NewWinPrcCount;

	if(OldAppCount != nApp || OldBkgCount != nBkg || OldWinCount != nWin)
	{
		UpdateGroupText();
	}






//	mTaskList.XXX=1;
	//	mTaskList.Invalidate();


	return 0;
	

}

HBRUSH CPageProcesses::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{


	return theApp.BkgBrush;
}





void CPageProcesses::_AddGroupItem(void)
{
	mTaskList.SetRedraw(0);


	APPLISTDATA *pNewAppListData = NULL;

	CString StrGroupTitle;

	StrGroupTitle.Format(L"%s (%d)",STR_GROUP_APP,nApp);
		
	mTaskList.InsertItem(0, StrGroupTitle);

	pNewAppListData   =    new  APPLISTDATA  ;
	mTaskList.SetItemData(0,(DWORD_PTR)pNewAppListData);





	pNewAppListData->iImage =-1;
	pNewAppListData->pPData = NULL ;
	pNewAppListData->ItemType = 0;
	pNewAppListData->SubType = -1;
	pNewAppListData->pParent = NULL;  // FIX T6: defensivo (estos items nunca son sub-items)

	//i++;


	StrGroupTitle.Format(L"%s(%d)",STR_GROUP_BKG,nBkg);
	mTaskList.InsertItem(0, StrGroupTitle);

	pNewAppListData   =    new  APPLISTDATA  ;
	mTaskList.SetItemData(0,(DWORD_PTR)pNewAppListData);

	pNewAppListData->iImage =-1;
	pNewAppListData->pPData = NULL ;
	pNewAppListData->ItemType  = 2;
	pNewAppListData->SubType = -1;
	pNewAppListData->pParent = NULL;  // FIX T6: defensivo


	//i++;


	StrGroupTitle.Format(L"%s (%d)",STR_GROUP_WIN,nWin);
	mTaskList.InsertItem(0, StrGroupTitle);

	pNewAppListData   =    new  APPLISTDATA  ;
	mTaskList.SetItemData(0,(DWORD_PTR)pNewAppListData);

	pNewAppListData->iImage =-1;
	pNewAppListData->pPData = NULL ;
	pNewAppListData->ItemType   =  4;
	pNewAppListData->SubType = -1;
	pNewAppListData->pParent = NULL;  // FIX T6: defensivo

	mTaskList.SetRedraw(1);

}

BOOL CPageProcesses::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class

	//	if(pMsg->message == UM_PROCEXIT) ��һ�����崦����������ɾ��ʱ��������ɴ���


	if(pMsg->message == UM_PROCSTART)
	{
		mTaskList.SetRedraw(0);


		PROCLISTDATA * pData = (PROCLISTDATA *) pMsg->lParam;

		AddNewItem( pData);
		
		if( pData->Type  == APP )
		{
		
			nApp++;


		}
		else  if( pData->Type == WINPROC )
		{


			nWin++;
		}
		else
		{

			nBkg++;		

		}


		//--------------------��ʱ���� ����������Ϣ -------------------
		UpdateGroupText();




		ReSort(FALSE);

		

		mTaskList.SetRedraw(1);

	}



	//---------------------------------------------------

	if(pMsg->message == UM_BASELISTOK) //��ϸ�б� ������Ϣ �о����  ���Կ�ʼ���ɵ�һҳ�� �߼��б�
	{
		AfxBeginThread(Thread_GenProcesses,this);

	}

	//---------------------------------------------------

	if(pMsg->message == UM_ALLINFO_OK) //��ϸ�б�����������Ϣ  ���Կ�ʼ���� ��һҳ�� �߼��б�
	{
		AfxBeginThread(Thread_FillAllItemsData,this);

		

	}

	// FIX T3: handler de UM_REFRESH. Thread_FillAllItemsData envia este
	// mensaje y aqui, en el UI thread, llamamos FillAllItemData (que
	// itera mTaskList y otras APIs MFC). Antes esto se hacia directamente
	// desde el worker thread, lo cual es ilegal en MFC.
	if(pMsg->message == UM_REFRESH)
	{
		FillAllItemData();
		FlagEnableRefresh = TRUE;
		return TRUE;
	}
	if(pMsg->message ==  UM_DBCLICK_LSIT)
	{
		int nItem = mTaskList.GetNextItem(-1,LVNI_SELECTED);
		APPLISTDATA *pData = NULL;
		pData=(APPLISTDATA * )mTaskList.GetItemData(nItem);
		if(pData!=NULL)
		{
			if(pData->SubType == PARENT_ITEM_CLOSE )
			{

				_OpenSubList(nItem);

			}
			else  if(pData->SubType == PARENT_ITEM_OPEN )
			{
				_CloseSubList(nItem);
			}
		}

	}




	if(pMsg->message == UM_HEADER_LCLICK)
	{
		int n=(int)pMsg->wParam;
		if(FlagEnableRefresh)//�������ǰ�������������
		{
			Sort(n);
		}

	}

	if(pMsg->message == UM_HEADER_RCLICK)
	{
		CPoint pt;  
		GetCursorPos(&pt);  
		//mTaskList.ScreenToClient(&pt)
		CMenu PopMenu;
		CMenu *pMenu = NULL;
		PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_COLUMN) );
		pMenu = PopMenu.GetSubMenu(0);  //0�� ��� ��Ӧ�� �˵� �ͱ�ǩ˳���Ӧ


		//MSB_S (L"DDDDDD")
		if(pMenu->GetMenuItemID(0) == ID_PROCESSES_TYPE )
		{
			int i=0;

			int nItem = pMenu->GetMenuItemCount();

			if(nItem>3) //��ֻ֤�Ե�һ���˵���Ч
			{
				for(i=1;i<11;i++)
				{
					if(! (COL_SAT_PROC[i].IsHiddenColumn)  ) 
					{
						pMenu->CheckMenuItem(i-1,MF_BYPOSITION|MF_CHECKED);
					}
				}

			}

		}
		 
	
		
		pMenu->TrackPopupMenu(TPM_LEFTALIGN,pt.x,pt.y, &(mTaskList.CoolheaderCtrl));
		
		//MSB_S (L"XXXXXX")

	}

	//-----------------
	if(pMsg->message == UM_ITEMCHANGED_COOLLIST)
	{
		_OnListItemChaged();
	}

 




	return CFormView::PreTranslateMessage(pMsg);
}

APPLISTDATA* CPageProcesses::AddNewItem(PROCLISTDATA * pDetailData, int ID,BOOL SetBaseInfoOnly)
{

	//������ �жϡ��趨 ÿ� ���� ֻ�谴���� ���鼴�ɣ���������

	//������������������������������������������������������������������������������������������������������



	APPLISTDATA * pNewAppListData;
	pNewAppListData   =    new  APPLISTDATA  ;

	CString  StrWndCaption;
	CString StrItem;


	//һ��Ҫ����ǰ�棡 ����InsertItemʱ��Ч���Ѿ���ʾ
	pNewAppListData->CoolUsageArray[PROCLIST_CPU]=pNewAppListData->CoolUsageArray[PROCLIST_MEMORY]=pNewAppListData->CoolUsageArray[PROCLIST_DISK]=pNewAppListData->CoolUsageArray[PROCLIST_NETWORK]=0;


	StrItem=pDetailData->Description;

	StrItem.Remove(L' ');

	if(StrItem.Compare(L"")==0)//����Ϊ���ý���������
	{
		pNewAppListData->StrTitle = pDetailData->Name.Left(pDetailData->Name.GetLength()-4);
	}
	else
	{
		pNewAppListData->StrTitle = pDetailData->Description;
	}


	mTaskList.SetRedraw(0);

	mTaskList.InsertItem(ID,StrItem);



	pNewAppListData->pPData = pDetailData ;
	pNewAppListData->iImage = pDetailData->IconIndex ;
	mTaskList.SetItemData(ID,(DWORD_PTR)pNewAppListData);	


	IO_COUNTERS  IOCounter;
	GetProcessIoCounters(pDetailData->hProcess,&IOCounter);	
	pNewAppListData->IOLast = IOCounter.ReadTransferCount+IOCounter.WriteTransferCount;




	pNewAppListData->ItemType = pDetailData->Type;
	pNewAppListData->SubType = PARENT_ITEM_NOSUB;    //PARENT_ITEM_NOSUB
	pNewAppListData->StrWnd  =L"";
	pNewAppListData->nSubItem = 0;
	pNewAppListData->pParent = NULL;  // FIX T5: padres no tienen padre


	pNewAppListData->CoolUsageArray[PROCLIST_MEMORY] = 0.001;



	if( pDetailData->Type  == APP )
	{
		//::GetWindowTextW(pDetailData->hMainWnd ,StrWndCaption.GetBuffer(MAX_PATH),MAX_PATH);						
		//StrWndCaption.ReleaseBuffer();
		//pDetailData->WindowCaption = StrWndCaption;
	

		mTaskList.SetItemText(ID,PROCLIST_TYPE,L"App");
		pNewAppListData->SubType = PARENT_ITEM_CLOSE; 

	    // nApp++;


	}
	else  if( pDetailData->Type == WINPROC )
	{
		
		mTaskList.SetItemText(ID,PROCLIST_TYPE,STR_GROUP_WIN);
		//nWin++;
	}

	else
	{
		 
		mTaskList.SetItemText(ID,PROCLIST_TYPE,STR_GROUP_BKG);

		// nBkg++;

		//if(pNewAppListData->SubType = PARENT_ITEM_CLOSE; )
		//pNewAppListData->SubType = PARENT_ITEM_CLOSE; 


	}
	CString StrDescription=mProcInfo.GetVerInfoString(mProcInfo.GetPathName(pDetailData->hProcess),L"FileDescription");
	pDetailData->Description=StrDescription;
	if(StrDescription==L"") StrDescription = pDetailData->Name;
	mTaskList.SetItemText(ID,PROCLIST_NAME, StrDescription );





	//--------------------------------  Process name ---------------------------


	mTaskList.SetItemText(ID,PROCLIST_PNAME, pDetailData->Name );
	//------------------------------------   PID   --------------------------------------


	StrItem.Format(L"%d",pDetailData->PID);
	mTaskList.SetItemText(ID,PROCLIST_PID, StrItem );


	if(SetBaseInfoOnly)
	{
		//д���ʼ���� Ϊ���Ӿ�Ч�� ��ͣ��
		mTaskList.SetItemText(ID,PROCLIST_CPU,L"0%");
		if(theApp.AppSettings.ProcList_MemPercents)
		{
			mTaskList.SetItemText(ID,PROCLIST_MEMORY,L"0%");
		}
		else
		{
			mTaskList.SetItemText(ID,PROCLIST_MEMORY,L"0 MB");
		}
		mTaskList.SetItemText(ID,PROCLIST_DISK,L"0 MB/s");
		mTaskList.SetItemText(ID,PROCLIST_NETWORK,L"0 KB/s");

		return pNewAppListData;
	}




	//---------------------------------    Publisher   ------------------------------
	StrItem = mProcInfo.GetVerInfoString(mProcInfo.GetPathName(pDetailData->hProcess),L"CompanyName") ;
	mTaskList.SetItemText(ID,PROCLIST_PUB,StrItem );

	//--------------------------------    command line    ----------------------------------

	mTaskList.SetItemText(ID,PROCLIST_CMDLINE, mProcInfo.GetProcCommandLine(pDetailData->PID) );

	//-----------------------------------CPU-----------------------------------

	mTaskList.SetItemText(ID,PROCLIST_CPU,L"0%");



	//---------------------------------�ڴ�------------------------

	_GetMemDataAndSetItemText(ID,(APPLISTDATA*)pNewAppListData );

	// NOTE: the line below used to be `mTaskList.SetItemText(ID, PROCLIST_MEMORY, StrItem);`
	// but StrItem is the outer-scope buffer that was filled with the publisher
	// (CompanyName, e.g. "Microsoft Corporation") at line ~1359, so that call
	// was overwriting the just-set MEM column with the publisher text whenever
	// the per-row pass hit this function. _GetMemDataAndSetItemText owns the
	// MEM column now, and writes "0 MB" / "0%" itself on read failure, so we
	// must not write to MEM here at all.

	//---------------------------------------Disk----------------------------------


	mTaskList.SetItemText(ID,PROCLIST_DISK,L"0 MB/s");

	//-----------------------------------Network----------------------------------


	mTaskList.SetItemText(ID,PROCLIST_NETWORK,L"0 KB/s");




	mTaskList.SetRedraw(1);


	ReSort();

	return pNewAppListData;

}


int CPageProcesses::_OpenSubList(int ID,BOOL LockDraw)
{

	int n=0;
	CWnd *pDeskTop=CWnd::GetDesktopWindow();
	CWnd  *pAppWnd = pDeskTop->GetWindow(GW_CHILD);

	CString  StrWndCaption;

	DWORD WindowPID ;
	DWORD ItemPID ;
	APPLISTDATA *pData = NULL;
	APPLISTDATA *pDataNew = NULL;
	pData=(APPLISTDATA * )mTaskList.GetItemData(ID);
	ItemPID = ((PROCLISTDATA * )pData->pPData)->PID ;

	int Pos=ID+1;

	if(LockDraw) mTaskList.SetRedraw(0);

	while( pAppWnd!=NULL)
	{



		GetWindowThreadProcessId(pAppWnd->m_hWnd,&WindowPID);
		if(ItemPID == WindowPID)
		{
			if( ((pData->ItemType== APP) && (pAppWnd->IsWindowVisible()))  || (pData->ItemType==BKGPROC) )
			{
				if((!(pAppWnd->GetExStyle()&WS_EX_TOOLWINDOW))&&(pAppWnd->GetParent()==NULL))
				{
					pAppWnd->GetWindowTextW(StrWndCaption);
					if(StrWndCaption!= L"")
					{
						pDataNew =  AddNewItem((PROCLISTDATA * )pData->pPData,Pos);

						pDataNew->CoolUsageArray[ PROCLIST_CPU]=pData->CoolUsageArray[ PROCLIST_CPU];
						pDataNew->CoolUsageArray[ PROCLIST_MEMORY]=pData->CoolUsageArray[ PROCLIST_MEMORY];
						pDataNew->CoolUsageArray[ PROCLIST_DISK]=pData->CoolUsageArray[ PROCLIST_DISK];
						pDataNew->CoolUsageArray[ PROCLIST_NETWORK]=pData->CoolUsageArray[ PROCLIST_NETWORK];




						pDataNew->StrWnd = StrWndCaption;
						pDataNew->hMainWnd = pAppWnd->m_hWnd;
						mTaskList.SetItemText(Pos,0,StrWndCaption);
						pDataNew->SubType = SUB_ITEM;
						pDataNew->ItemType =pData->ItemType;
						pDataNew->pParent = pData;  // enlazado al padre real (CoolListCtrl::_DelItem / DBCTaskman)

						Pos++;
						n++;
					}
				}
			}


		}



		pAppWnd=pAppWnd->GetWindow(GW_HWNDNEXT);

	}


	pData->SubType = PARENT_ITEM_OPEN ;

	pData->nSubItem = n;

	ReSort(FALSE);

	if(LockDraw)mTaskList.SetRedraw(1);
	mTaskList.Invalidate();


	return n;
}

int CPageProcesses::_CloseSubList(int ID,BOOL LockDraw)
{
	APPLISTDATA *pData = NULL;
	APPLISTDATA *pDelData = NULL;

	pData=(APPLISTDATA * )mTaskList.GetItemData(ID);


	int Pos = ID+1;
	if(LockDraw)mTaskList.SetRedraw(0);
	while(1)
	{
		pDelData = (APPLISTDATA * )mTaskList.GetItemData(Pos);
		if(pDelData->SubType == SUB_ITEM)
		{
			// Defensa: nulificar pParent antes de DeleteItem para
			// que CoolListCtrl::_DelItem / DBCTaskman no vean un
			// puntero colgando tras emitirse LVN_DELETEITEM.
			pDelData->pParent = NULL;
			mTaskList.DeleteItem(Pos);

		}
		else
		{
			break ;
		}

	}

	pData->SubType = PARENT_ITEM_CLOSE ;
	if(LockDraw)mTaskList.SetRedraw(1);
	mTaskList.Invalidate();



	return 0;
}

int CPageProcesses::ListItemWindows(int nItem,APPLISTDATA * pListData )
{


	APPLISTDATA *  pData = NULL ;
	if(pListData == NULL)
	{
		pData = (APPLISTDATA *) mTaskList.GetItemData(nItem);
	}
	else
	{
		pData = pListData;
	}

	if(pData == NULL) return -1;



	//----------------------------------


	DWORD PID = ((PROCLISTDATA *)pData->pPData)->PID;

	CWnd *pDeskTop=CWnd::GetDesktopWindow();

	CWnd  *pAppWnd = pDeskTop->GetWindow(GW_CHILD);

	CString  StrWndCaptionF;
	CString  StrWndCaptionB;
	CString Strtemp;
	DWORD WindowPID ;


	int nAppWnd=0;
	int nBkgWnd=0;



	while( pAppWnd!=NULL)
	{

		GetWindowThreadProcessId(pAppWnd->m_hWnd, &WindowPID);

		if(WindowPID ==PID)
		{
			DWORD  WndExStyle = pAppWnd->GetExStyle();
			if(pAppWnd->IsWindowVisible()&& (!(WndExStyle&WS_EX_TOOLWINDOW))&&(pAppWnd->GetParent()==NULL)&&(!(WndExStyle&0x200000)))
			{

				nAppWnd ++;
			}
			else  if(pAppWnd->GetParent()==NULL)
			{
				pAppWnd->GetWindowText(Strtemp);
				if(Strtemp!=L"")nBkgWnd ++ ;
			}

		}

		pAppWnd=pAppWnd->GetWindow(GW_HWNDNEXT);

	}

	CString StrItem;

	if(nAppWnd>0 )
	{
		if(pData->ItemType!=APP) //���ı�ɾ��չ����
		{
			_CloseSubList(nItem,FALSE);
			mTaskList.SetItemText(nItem,PROCLIST_TYPE,L"App");
			pData->ItemType = APP;
		}

		if((pData->nSubItem != nAppWnd) &&(pData->SubType == PARENT_ITEM_OPEN) )
		{		
			_CloseSubList(nItem,FALSE);
			_OpenSubList(nItem,FALSE);
		}

		pData->nSubItem = nAppWnd;


	}
	else 
	{

		if(pData->ItemType!=BKGPROC) //���ı�ɾ��չ����
		{
			_CloseSubList(nItem,FALSE);
			pData->ItemType = BKGPROC;
			mTaskList.MySetItemText(nItem,PROCLIST_TYPE,L"Background Process");

		}


		if( (pData->nSubItem != nBkgWnd ) &&(pData->SubType == PARENT_ITEM_OPEN) )
		{
			_CloseSubList(nItem,FALSE);
			_OpenSubList(nItem,FALSE);			
		}

		pData->nSubItem = nBkgWnd; 




	}

	if(pData->ItemType == BKGPROC && pData->nSubItem == 0)
	{
		pData->SubType = PARENT_ITEM_NOSUB;
	}
	else if(pData->SubType != PARENT_ITEM_OPEN)
	{
		pData->SubType = PARENT_ITEM_CLOSE;
	}


	((PROCLISTDATA *)pData->pPData)->Type = pData->ItemType;// һ��Ҫ������� ���� ������������

	//��������

 



	if(pData->nSubItem>1)
	{
		
			StrItem.Format(L"%s (%d)",pData->StrTitle, pData->nSubItem);
			mTaskList.MySetItemText(nItem,PROCLIST_NAME,StrItem);
		
	}
	else
	{
	
	 
			mTaskList.MySetItemText(nItem,PROCLIST_NAME,pData->StrTitle);
		
	}



	return 0;
}



BOOL CPageProcesses::CheckWndChange(void)
{


	int n=0;
	CWnd *pDeskTop=CWnd::GetDesktopWindow();
	CWnd  *pAppWnd = pDeskTop->GetWindow(GW_CHILD);

	CString  StrWndCaption;

	CString StrNewWndList;

	CString StrTemp ;

	DWORD   hWndNum;

	while( pAppWnd!=NULL)
	{

		DWORD  WndExStyle =  pAppWnd->GetExStyle();
		if( !( WndExStyle & WS_EX_TOOLWINDOW)  )
		{

			hWndNum = (DWORD) pAppWnd->GetSafeHwnd();
			pAppWnd->GetWindowTextW(StrWndCaption);
			StrTemp.Format(L"%x",hWndNum);
			StrTemp=StrTemp+StrWndCaption;

			if(pAppWnd->IsWindowVisible()&&(pAppWnd->GetParent()==NULL)&& (!(WndExStyle & 0x200000)) )
			{
				StrTemp = StrTemp+L"{APP}";
			}
			else if(pAppWnd->GetParent()==NULL)
			{
				StrTemp = StrTemp+L"{BKG}";
			}
			StrNewWndList= StrNewWndList+StrTemp;


		}

		pAppWnd=pAppWnd->GetWindow(GW_HWNDNEXT);

	}

	BOOL Ret = TRUE;

	if(	wcscmp(StrNewWndList,StrWndList)==0)
	{
		Ret =FALSE;
	}

	StrWndList= StrNewWndList;




	return Ret;
}

//BOOL CPageProcesses::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
//{
//	// TODO: Add your specialized code here and/or call the base class
//
//	if ((((LPNMHDR)lParam)->code == NM_RCLICK))  
//	{  
//		CPoint pt, pt2;  
//		GetCursorPos(&pt);  
//		pt2 = pt;  
//		mTaskList.ScreenToClient(&pt);  
//		CWnd* pWnd = mTaskList.ChildWindowFromPoint(pt);  
//		CHeaderCtrl* pHeader = mTaskList.GetHeaderCtrl();  
//		if(pWnd && (pWnd->GetSafeHwnd() == pHeader->GetSafeHwnd()))  
//		{  
//			//----------ע�ʹ��� �ɼ�� �����а��Ҽ�
//			//HDHITTESTINFO info = {0};  
//			//info.pt = pt;  
//			//pHeader->SendMessage(HDM_HITTEST, 0, (LPARAM)&info);  
//			//-------------------------------------------------------------------
//			
//
//		}  
//	}  
//	return CFormView::OnNotify(wParam, lParam, pResult);
//}

void CPageProcesses::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	CFormView::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

	if(pPopupMenu == NULL) return;





	pPopupMenu->SetDefaultItem(ID_PROCESSESLIST_EXPAND);

	//----------------------
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	APPLISTDATA *pListData = NULL;
	if(nSel >= 0) pListData = (APPLISTDATA *)mTaskList.GetItemData(nSel);
	CString Str;
	if(pListData!=NULL && pListData->SubType == PARENT_ITEM_CLOSE)
	{			
		Str.LoadStringW(IDS_STRING_POPMENU_EXP);

	}

	else
	{
		Str.LoadStringW(IDS_STRING_POPMENU_COLL);
	}

	pPopupMenu->ModifyMenuW(ID_PROCESSESLIST_EXPAND,MF_BYCOMMAND,ID_PROCESSESLIST_EXPAND,Str);

	if(pListData != NULL && pListData->SubType == PARENT_ITEM_NOSUB)
	{
		pPopupMenu->DeleteMenu(ID_PROCESSESLIST_EXPAND,MF_BYCOMMAND);
	}

	 


	//----------------------

	//Menu status : percents Or Values   

	if(theApp.AppSettings.ProcList_MemPercents)
	{		 
		pPopupMenu->CheckMenuRadioItem(ID_MEMORY_PERCENTS_PLIST,ID_MEMORY_VALUES_PLIST,ID_MEMORY_PERCENTS_PLIST,MF_BYCOMMAND);
	}
	else
	{
		pPopupMenu->CheckMenuRadioItem(ID_MEMORY_PERCENTS_PLIST,ID_MEMORY_VALUES_PLIST,ID_MEMORY_VALUES_PLIST,MF_BYCOMMAND);
	}
		






}

void CPageProcesses::PlaceAllCtrl(void)
{

	CRect rc;	
	CRect rcHeader;
	CRect rcParent;
	GetClientRect(rc);



	if(rc.Height()<110)
	{
		if(mTaskList.CoolheaderCtrl.IsWindowVisible())
			mTaskList.CoolheaderCtrl.ShowWindow(SW_HIDE);
	}
	else
	{
		if(!mTaskList.CoolheaderCtrl.IsWindowVisible())
			mTaskList.CoolheaderCtrl.ShowWindow(SW_SHOW);
	}




	rc.InflateRect(1,1);


	//���ù�headerCtrl


	rc.bottom-=50;

	rc.top=mTaskList.CoolheaderCtrl.Height;	
	mTaskList.MoveWindow(rc);


	CWnd *pBtn= GetDlgItem(IDC_BTN_ENDTASK);

	if(pBtn!=NULL)
	{
		CRect rcBtn(rc.right-95-15,rc.bottom+25-12,rc.right-20,rc.bottom+25+12);
		pBtn->MoveWindow(rcBtn);


	}

	if(theApp.pButtonFOM!=NULL)
	{
		if(theApp.AppSettings.TaskManMode == 0)
		{
			rc.bottom+=theApp.MenuAndTabHeight;
		}

		theApp.pButtonFOM->MoveWindow(rc.left+15,rc.bottom+25-12,95,25);

	}





}

void CPageProcesses::OnNMRClickProcesslist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: Add your control notification handler code here

	int  nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );

	APPLISTDATA *pData =(APPLISTDATA *)mTaskList.GetItemData(nSel);

	if(pData == NULL) return ;

	if(pData->pPData == NULL )return ; //������ⲻ�����˵�������


	CMenu PopMenu;
	CMenu *pMenu = NULL;

	PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_BASE) );

	if(pData->SubType==SUB_ITEM)
	{
		pMenu = PopMenu.GetSubMenu(6);  //6�� �����Ҽ� ��Ӧ�� �˵�  
	}
	else
	{
		pMenu = PopMenu.GetSubMenu(0);  //0�� ��� ��Ӧ�� �˵�  
	}


	CPoint CurPos ;
	GetCursorPos(&CurPos); 

	pMenu->TrackPopupMenu(TPM_LEFTALIGN,CurPos.x,CurPos.y,this);



	//---------------



	*pResult = 0;
}



void CPageProcesses::PreListItems(void)
{




	nApp=nBkg=nWin=0;
	/*

	Tips !  ����1    /

	���е������������Ȱ���Ŀ �������� ��������� ���� ���� ������ APP/Background Processes /Windows Processes ���飩

	��Ŀ�����������£����ݶ���

	APP����ı���           0, 6               APP������Ŀ          1  
	BGKProc ����ı���       2 , 4              BGKProc������Ŀ       3
	WinProc ����ı���       4  ,2                WinProc������Ŀ      5



	���� ����ı���        iImage =-1; 



	*/



	PROCLISTDATA *pDetailListData = NULL;


	mTaskList.SetRedraw(0); //����� ��Ϊ���߳���

	for(int  i= 0;i<pPageDetails->mDetailsList.GetItemCount() ;i++)
	{	 
		pDetailListData =( PROCLISTDATA *) ( pPageDetails->mDetailsList.GetItemData(i));

		if(pDetailListData!=NULL)
		{
			if(pDetailListData->PID == 0) //���������� sis idle
			{

				continue  ;

			}
			AddNewItem(pDetailListData,mTaskList.GetItemCount(),TRUE);

			if( pDetailListData->Type  == APP )
			{
				nApp++;
			}
			else  if( pDetailListData->Type == WINPROC )
			{
				nWin++;
			}
			else
			{
				nBkg++;		
			}


			AfxBeginThread(Thread_GetItemName,pDetailListData);



		}

	}




	//-------------������� -------------

	_AddGroupItem();

	//AfxBeginThread(Thread_AddGroupItem,this);


	//------------------------------------------------------------------------

	//����

	mTaskList.FlagSortUp = TRUE;
	mTaskList.CurrentSortColumn = PROCLIST_NAME;

	Sort(PROCLIST_NAME,FALSE);

	//-------------------------------------------------------------------------

	mTaskList.SetRedraw(1);

	mTaskList._GetRedrawColumn();
	mTaskList.Invalidate();
	mTaskList.CoolheaderCtrl.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_NOERASE);

	//MSB(0)
	

	




}




void CPageProcesses::FillAllItemData(BOOL LoadAllTrueData)
{

	APPLISTDATA *pData = NULL;
	PROCLISTDATA *pDetailListData = NULL;

	//mTaskList.SetRedraw(0);

	CString StrItem;


	// �����ж� ��ֹ����������������
	for( int i =0;i<mTaskList.GetItemCount() ;i++)
	{	 
		pData = ( APPLISTDATA *) mTaskList.GetItemData(i);

		if(pData==NULL) continue ;

		pDetailListData = (( PROCLISTDATA *)pData->pPData) ;

		if(pDetailListData==NULL) continue ;



		//pData->npDetailListData->Name   ;

		
		

		

		if(( pDetailListData->Name.CompareNoCase(L"svchost.exe") == 0) && (pDetailListData->SessionID == 0))
		{
			pData->iImage = SvchostIconIndex;
		}
		else
		{
			pData->iImage = pDetailListData->IconIndex;
		}



		// FIX: usar pData->iImage (que contiene SvchostIconIndex para svchost)
		// en vez de pDetailListData->IconIndex. Antes el svchost quedaba con
		// el icono del shell catalog aunque pData->iImage ya tuviese el
		// SVCHOST.ico correcto -> por eso veias el icono equivocado en
		// svchost en Win7.
		mTaskList.SetItem(i,0, LVIF_IMAGE, NULL, pData->iImage, 0, 0, 0); 
		

		//--------------------------------  ��һ�� Title  ---------------------------
		if(	pData->SubType != SUB_ITEM )
		{
			CString StrTemp=pDetailListData->Description;

			StrTemp.Remove(L' ');

			if(StrTemp.Compare(L"")==0)//����Ϊ���ý���������
			{
				pData->StrTitle = pDetailListData->Name.Left(pDetailListData->Name.GetLength()-4);
				mTaskList.SetItemText(i,PROCLIST_NAME, pData->StrTitle);
			}
			else
			{
				mTaskList.SetItemText(i,PROCLIST_NAME, pDetailListData->Description);
				pData->StrTitle = pDetailListData->Description;
			}


			

			
		}

		//--------------------------------  Process name ---------------------------

		mTaskList.SetItemText(i,PROCLIST_PNAME, pDetailListData->Name );

		//--------------------------------  Process Status  ---------------------------

		//StrItem.Format(L"%d",pDetailListData->IconIndex);
		//mTaskList.SetItemText(i,PROCLIST_STATUS, StrItem );

		//------------------------------------   PID   --------------------------------------

		StrItem.Format(L"%d",pDetailListData->PID);
		mTaskList.SetItemText(i,PROCLIST_PID, StrItem );


		//---------------------------------    Publisher   ------------------------------
		StrItem = mProcInfo.GetVerInfoString(mProcInfo.GetPathName(pDetailListData->hProcess),L"CompanyName") ;
		mTaskList.SetItemText(i,PROCLIST_PUB,StrItem );


		//--------------------------------    command line    ----------------------------------
		mTaskList.SetItemText(i,PROCLIST_CMDLINE, mProcInfo.GetProcCommandLine(pDetailListData->PID) );


		//-----------------------------------CPU-----------------------------------

		mTaskList.SetItemText(i,PROCLIST_CPU,L"0%");
		pDetailListData->CPU_Usage = 0; //��ֹ����
		pData->CoolUsageArray[PROCLIST_CPU] = 0;

		//---------------------------------�ڴ�------------------------


		_GetMemDataAndSetItemText(i,pData);

		pData->CoolUsageArray[PROCLIST_MEMORY] = 0.001;



		//---------------------------------------Disk----------------------------------


		mTaskList.SetItemText(i,PROCLIST_DISK,L"0 MB/s");

		//-----------------------------------Network----------------------------------


		mTaskList.SetItemText(i,PROCLIST_NETWORK,L"0 KB/s");

	

	}







	//����

	ReSort(FALSE);

	// DIAG: trace opt-in tras recargar la lista
	if (_ListDiagEnabled()) {
		_ListDiagLog("FillAllItemData complete loadAll=%d", (int)LoadAllTrueData);
		_ListDiagDump("post-FillAll", 0, &mTaskList);
	}

	//-------------------------------------------------------------------------



	//	mTaskList.SetRedraw(1);



}

void CPageProcesses::DeleteProcessItem(PROCLISTDATA * pDetailsListData)
{
	int n = mTaskList.GetItemCount();
	APPLISTDATA *pListData = NULL;

	for(int i = 0;i<n;i++)
	{
		pListData =(APPLISTDATA *) mTaskList.GetItemData(i);

		if(pListData == NULL) continue;

		if( pListData->SubType != -1) //���Ƿ������
		{
			if( (pListData->pPData ==  pDetailsListData)  || ( (pListData->pPData==NULL) && (pListData->SubType<SUB_ITEM) ) ) //SubType<SUB_ITEM) ���ų�������Ϊ�����ö������� ���Զ��游��ɾ��  || ( (pListData->pPData==NULL) && (pListData->SubType<SUB_ITEM) )
			{

				switch(pListData->ItemType)
				{
				case  APP:
					nApp--;
					break;
				case  BKGPROC:
					nBkg--;
					break;
				case  WINPROC:
					nWin--;
					break;
				}

				mTaskList.DeleteItemAndSub(i);
				break ;
			}
		}


	}//for


	ReSort();
	
	UpdateGroupText();
}

void CPageProcesses::SetProcessStatusInfo(DWORD PID, int Status)
{

	APPLISTDATA *pData = NULL;
	CString  StrStatus ;

	switch(Status)
	{
	case PS_RUNNING:StrStatus = L"";break;
	case PS_NOTRESPONDING:StrStatus = L"Not Responding";break;
		//case 2:StrStatus = L"Running";break;
	}


	for(int i=0;i<mTaskList.GetItemCount();i++)
	{
		pData = (APPLISTDATA *)mTaskList.GetItemData(i);

		if(pData ==NULL) continue ;

		if(pData->pPData ==NULL) continue ;

		if( ((PROCLISTDATA *)(pData->pPData))->PID == PID)
		{
			if( (pData->SubType !=  SUB_ITEM)&&(pData->pPData!=NULL))
			{
				mTaskList.SetItemText(i,PROCLIST_STATUS,StrStatus);
			}
			break;
		}
	}


}


//void CPageProcesses::OnSysCommand(UINT nID, LPARAM lParam)
//{
//	// TODO: Add your message handler code here and/or call default
//
//
//	CFormView::OnSysCommand(nID, lParam);
//}

BOOL CPageProcesses::OnCommand(WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your specialized code here and/or call the base class

 


	UINT ID= (UINT)wParam;
	switch(ID)
	{
	case ID_VIEW_EXPANDALL:
		
		break;
	case ID_PROCESSES_TYPE:
	case ID_PROCESSES_STATUS:
	case ID_PROCESSES_PUBLISHER:
	case ID_PROCESSES_PID:
	case ID_PROCESSES_PROCESSNAME:
	case ID_PROCESSES_COMMANDLINE:
	case ID_PROCESSES_CPU:
	case ID_PROCESSES_MEMORY:
	case ID_PROCESSES_DISK:
	case ID_PROCESSES_NETWORK:

		CMenu PopMenu;
		CMenu *pMenu;
		PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_COLUMN) );
		pMenu = PopMenu.GetSubMenu(0);  //0�� ��� ��Ӧ�� �˵� �ͱ�ǩ˳���Ӧ

		//		if((HMENU)(pMsg->wParam)  == pMenu->m_hMenu) MSBOX(999)

		int nItem = pMenu->GetMenuItemCount();
		int iClick = -1;
		for(int i= 0;i<nItem;i++)
		{
			if(pMenu->GetMenuItemID(i)==ID)
			{
				iClick= i;
				break;
			}
		}

		int iCol = iClick+1;

		
		BOOL IsTurnColShow;
		IsTurnColShow =mTaskList.ShowOrHideColumn(iCol);
		if(IsTurnColShow)RefreshList();

		 
		break;

	}





	return CFormView::OnCommand(wParam, lParam);
}

void CPageProcesses::Sort(int nCol,BOOL InvertSort)
{

	APPLISTDATA *pListData = NULL;
	int nCount = mTaskList.GetItemCount();






	if(nCol == PROCLIST_NAME )//����� PROCLIST_NAME ��
	{
		if(mTaskList.CurrentSortColumn  == PROCLIST_NAME  ) //ĿǰҲ�ǰ���������
		{
			BYTE  ResetType ;
			if(InvertSort)
			{
				nCount = mTaskList.GetItemCount();

				if(mTaskList.FlagSortUp)//Ŀǰ���������� ����type�ֱ�Ϊ 0,2,4תΪ����2,4,6
				{
					ResetType = 2;
					for(int i=0;i<nCount;i++)
					{
						pListData = (APPLISTDATA *)mTaskList.GetItemData(i);
						if(pListData==NULL) continue ;
						if(pListData->pPData == NULL)
						{
							pListData->ItemType =  ResetType;	
							ResetType+=2;
						}
					}
				}
				else//Ŀǰ�ǽ������� ����type�ֱ�Ϊ 2,4,6תΪ���� 0,2,4 //ע���ʱ��������棡����
				{
					ResetType = 4;
					for(int i=0;i<nCount;i++) //
					{
						pListData = (APPLISTDATA *)mTaskList.GetItemData(i);
						if(pListData==NULL) continue ;
						if(pListData->pPData == NULL)
						{
							pListData->ItemType =  ResetType;	
							ResetType-=2;
						}
					}
				}


			}





		}
		else //Ŀǰ�����ǰ���0������
		{

			//-------------���� ������� -------------
			if(theApp.AppSettings.GroupByType)
			_AddGroupItem();


		}

	}
	else //������0������ �Ƴ����������
	{
		if(theApp.AppSettings.GroupByType)
		_RemoveGroupTitle();

	}



	//-------------------------------------------------
	//   �������   �Ƴ�/���� �������     �����б���֮��





	if(InvertSort)
	{
		if(mTaskList.CurrentSortColumn == nCol)
		{
			mTaskList.FlagSortUp = !mTaskList.FlagSortUp;
		}
		else
		{
			mTaskList.FlagSortUp = TRUE;
		}
	}


	mTaskList.CurrentSortColumn =  nCol;


	//���ûص������Ĳ�������ڵ�ַ   

	int LastParentItemID = -1 ;
	nCount = mTaskList.GetItemCount();
	for(int i= 0;i<nCount;i++)
	{
		APPLISTDATA *pListData = (APPLISTDATA *) mTaskList.GetItemData(i);
		if(pListData == NULL) continue;
		if(pListData->SubType == PARENT_ITEM_OPEN)
		{
			LastParentItemID = i;
		}
		if(pListData->SubType == SUB_ITEM && (nCol != PROCLIST_NAME))
		{
			CString StrItemText;

			// FIX T5: usar el puntero al APPLISTDATA padre guardado en el
			// sub-item (pListData->pParent) en lugar del LastParentItemID
			// del recorrido. El bug raiz era: si hay varios padres
			// expandidos y el sort pone un sub-item DESPUES del padre de
			// OTRO proceso (o antes del primer POpen), LastParentItemID
			// apunta al padre equivocado (o a -1), y el sub-item acaba
			// con texto "" en columnas numericas. Sort_Processes entonces
			// compara "" vs "" en lstrcmp de strings pad-eados a 16 chars
			// -> empate -> tiebreak por PID -> QuickSort no estable -> el
			// sub-item se separa del padre que le corresponde.
			//
			// Con pParent tenemos el padre real, asi que su SortID es
			// siempre el correcto (fue asignado en el ciclo previo o en
			// este mismo antes de llegar aqui).
			int parentSortID = -1;
			if (pListData->pParent != NULL)
				parentSortID = pListData->pParent->SortID;

			// Fallback defensivo: si por alguna razon pParent es NULL
			// (sub-item huérfano de una version vieja del binario,
			// por ejemplo), recurrimos al LastParentItemID legacy.
			if (parentSortID < 0)
				parentSortID = LastParentItemID;

			if (parentSortID >= 0)
				StrItemText = mTaskList.GetItemText(parentSortID, nCol);

			// Si el padre tampoco tiene texto (caso raro, primer tick
			// tras expandir antes de que llegue datos reales), usamos
			// defaults para no dejar "".
			if (StrItemText.IsEmpty())
			{
				switch (nCol)
				{
				case PROCLIST_CPU:     StrItemText = L"0%"; break;
				case PROCLIST_MEMORY:  StrItemText = theApp.AppSettings.ProcList_MemPercents ? L"0%" : L"0 MB"; break;
				case PROCLIST_DISK:    StrItemText = L"0 MB/s"; break;
				case PROCLIST_NETWORK: StrItemText = L"0 KB/s"; break;
				default: break;
				}
			}
			mTaskList.SetItemText(i, nCol, StrItemText);
		}
		pListData->SortID = i;
	}
	mTaskList.SortItems(Sort_Processes, nCol);




}

void CPageProcesses::ReSort(BOOL SkipStaticColumn)
{
	//ע����������ʱ��Ҫ���ϵ���һ�α����� SkipStaticColumn Ҫ��Ϊ FALSE����̬�в����Զ����򣡣���

	if(mTaskList)
	{
		switch(mTaskList.CurrentSortColumn)
		{//������ ���� ����ڽ��� �̶� ����Ҫÿ��ˢ����������

		case PROCLIST_TYPE:
		case PROCLIST_PUB:
		case PROCLIST_PID:
		case PROCLIST_PNAME:
		case PROCLIST_CMDLINE:
			return;

		}
	}



	Sort(mTaskList.CurrentSortColumn,FALSE);

}

void CPageProcesses::OnPop_ProcesseslistExpand()
{

	this->PostMessageW(UM_DBCLICK_LSIT);


}


void CPageProcesses::OnPop_ProcesseslistEndtask()
{
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	APPLISTDATA *pListData = NULL;
	if(nSel >= 0) pListData = (APPLISTDATA *)mTaskList.GetItemData(nSel);
	CString Str;
	if(pListData != NULL && pListData->SubType != SUB_ITEM)
	{			
	
		if(pListData->pPData == NULL) return;
		if(theApp.Global_ShowOperateTip(((PROCLISTDATA*)(pListData->pPData))->Name,STR_ENDPROC_MAINTIP,STR_ENDPROC_CONTENT,STR_ENDPROC_BTN) ==IDOK)
		{
			TerminateProcess(((PROCLISTDATA*)(pListData->pPData))->hProcess, 4);
		}


	}

}

void CPageProcesses::OnPop_ProcesseslistGotoDetails()
{

	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);


	//����������ѡ ����Ҫ�����֮ǰѡ�е� 

	pPageDetails->ClearSelecet();





	if(pData!=NULL)
	{
		if(pData->pPData ==NULL )return;
		DWORD PID = ((PROCLISTDATA *)(pData->pPData))->PID;
		for(int i=0;i<pPageDetails->mDetailsList.GetItemCount();i++)
		{
			PROCLISTDATA * pDetailsData =(PROCLISTDATA *) pPageDetails->mDetailsList.GetItemData(i);
			if(pDetailsData!=NULL)
			{
				if(pDetailsData->PID == PID)
				{
					pPageDetails->mDetailsList.EnsureVisible(i,FALSE);
					pPageDetails->mDetailsList.SetItemState(i,LVIS_SELECTED,LVIS_SELECTED);
					break;
				}
			}

		}
	}



	int PageID=3;
	theApp.pMainTab->SetCurSel(PageID);
	//--------------����ʵ�ʶ���---------- 
	NMHDR nmhdr; 
	nmhdr.code = TCN_SELCHANGE;  
	nmhdr.hwndFrom = theApp.pMainTab->GetSafeHwnd();  
	nmhdr.idFrom= theApp.pMainTab->GetDlgCtrlID();  
	::SendMessage(theApp.pMainTab->GetSafeHwnd(), WM_NOTIFY,MAKELONG(TCN_SELCHANGE,PageID), (LPARAM)(&nmhdr));
}


void CPageProcesses::OnBnClickedBtnEndTask()
{
	OnPop_ProcesseslistEndtask();
}

void CPageProcesses::_OnListItemChaged(void)
{
	CWnd * pButton=	this->GetDlgItem(IDC_BTN_ENDTASK);
	if(pButton==NULL) return;

	if(mTaskList.GetSelectedCount()>0)
	{	
		
		int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED ); 
		APPLISTDATA * pData =(APPLISTDATA *) mTaskList.GetItemData(nSel); 
		if(pData!=NULL)
		{
			if(pData->SubType != -1)
			{			
				pButton->EnableWindow(TRUE);
			}	
			else
			{
				pButton->EnableWindow(FALSE);		
			}
		}

	}
	else
	{	
		pButton->EnableWindow(FALSE);		
	}

	
}

int CPageProcesses::GetIndentLevel(DWORD pid)
{
	// FIX B2: walk recursivo por ParentPID para calcular el nivel de
	// indentacion estilo Win10 (0 = raiz, 1 = hijo de raiz, etc.).
	// Limita la profundidad a 32 y usa std::set<DWORD> visited para
	// protegerse contra ciclos transitorios en el arbol (mismo patron
	// anti-ciclo que el FIX T4 aplicado a _EndProcessTree).
	//
	// Devuelve:
	//   - 0  si el PID es root (ParentPID es 0, 4 o invalido)
	//   - N  si el PID tiene N generaciones por encima antes de un root
	//   - -1 si no se encuentra el PROCLISTDATA del PID (recien creado)
	//
	// Map_PidToData esta protegido por g_MapDataLock (FIX T2), asi que
	// hacemos una copia local del PROCLISTDATA necesario bajo lock para
	// no retenerlo durante la iteracion.
	if (pid == 0 || pid == 4) return 0; // System / Idle = raiz

	const int kMaxDepth = 32;
	std::set<DWORD> visited;
	DWORD currentPid = pid;
	int  level = 0;

	while (level < kMaxDepth) {
		// Anti-ciclo
		if (visited.count(currentPid)) return level;
		visited.insert(currentPid);

		// Buscar el PROCLISTDATA de currentPid bajo lock
		PROCLISTDATA* pData = NULL;
		EnterCriticalSection(&g_MapDataLock);
		{
			map<DWORD,PVOID>::iterator it = Map_PidToData.find(currentPid);
			if (it != Map_PidToData.end())
				pData = (PROCLISTDATA*)it->second;
		}
		LeaveCriticalSection(&g_MapDataLock);

		if (pData == NULL) return level; // No encontrado: tratamos como root

		DWORD parentPid = pData->ParentPID;

		// Root del arbol (System / Idle / invalido)
		if (parentPid == 0 || parentPid == 4 || parentPid == (DWORD)-1)
			return level;

		// Subir un nivel
		currentPid = parentPid;
		level++;
	}

	return level; // Cap a kMaxDepth
}

void CPageProcesses::RecalcIndentForAll()
{
	// FIX B3: recalcula IndentLevel para TODOS los items de mTaskList.
	// Por ahora es un stub: solo calcula el valor. La aplicacion
	// visual (prefijo de espacios/tabs en PROCLIST_NAME) se hace en
	// Phase C (CPhase C2/D2 render).
	int nCount = mTaskList.GetItemCount();
	for (int i = 0; i < nCount; i++) {
		APPLISTDATA* pData = (APPLISTDATA*)mTaskList.GetItemData(i);
		if (pData == NULL || pData->pPData == NULL) continue;
		if (pData->SubType == SUB_ITEM) continue; // sub-items siempre de 0

		PROCLISTDATA* pProc = (PROCLISTDATA*)pData->pPData;
		pData->IndentLevel = GetIndentLevel(pProc->PID);
	}
}

void CPageProcesses::RecalcIndentForPID(DWORD pid)
{
	// FIX B4: stub inicial. Recalcula IndentLevel solo para los items
	// cuyo IndentLevel puede haber cambiado. Por ahora recalcula TODOS
	// (es O(N) y solo se ejecuta cuando se anade/elimina un proceso,
	// que es raro). Se optimizara en Phase E si el rendimiento lo exige.
	RecalcIndentForAll();
}

void CPageProcesses::_RemoveGroupTitle(void)
{

	mTaskList.SetRedraw(0);

	int nCount = mTaskList.GetItemCount();
		BYTE  ResetType = 0;
		for(int i=nCount-1;i>=0;i--)  //�Ӻ�ɾ����ֹ�кű仯��ɵ��鷳
		{
			APPLISTDATA *pListData = (APPLISTDATA *)mTaskList.GetItemData(i);
			if(pListData && pListData->pPData ==NULL)
			{
				// NO hacer delete aqui: OnLvnDeleteitem en
				// CoolListCtrl.cpp YA libera el APPLISTDATA cuando
				// CListCtrl emite LVN_DELETEITEM. Hacer delete
				// aqui seria double-free (STATUS_HEAP_CORRUPTION
				// 0xc0000374).
				mTaskList.DeleteItem(i);  //�����Զ�ɾ��

			}
		}
	mTaskList.SetRedraw(1);
}

void CPageProcesses::OnLvnKeydownProcesslist(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
	// TODO: Add your control notification handler code here

	if(pLVKeyDow->wVKey == VK_DELETE)
	{
		//AfxBeginThread(Thread_FillAllItemsData,this);
		CWnd * pCtrl= NULL;
		pCtrl = this->GetDlgItem(IDC_BTN_ENDTASK);
		if(pCtrl)
		{
			if(pCtrl->IsWindowEnabled())
			{
				OnPop_ProcesseslistEndtask();
			}
		}
	}

	*pResult = 0;
}


void CPageProcesses::RefreshList(void)
{

	// FIX T3: en lugar de lanzar un thread que llama FillAllItemData
	// directamente (UI MFC desde worker, ilegal), encolamos UM_REFRESH
	// y el UI thread ejecuta la recarga desde el handler en
	// PreTranslateMessage. El thread es ahora opcional: si lo lanzamos
	// seguimos teniendo el patron PostMessage -> handler, asi que el
	// comportamiento es seguro sea quien sea quien llame.
	AfxBeginThread(Thread_FillAllItemsData,this);

}

void CPageProcesses::OnPop_OpenFileLocation()
{
	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);

	if(pData!=NULL)
	{
		if(pData->pPData ==NULL )return;

		CString StrTargetPath = mProcInfo.GetPathName(((PROCLISTDATA *)(pData->pPData))->hProcess);
		OpenFileLocation(StrTargetPath);
	}



}

void CPageProcesses::OnPop_Properties()
{
	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);

	if(pData!=NULL)
	{
		if(pData->pPData ==NULL )return;

		CString StrTargetPath = mProcInfo.GetPathName(((PROCLISTDATA *)(pData->pPData))->hProcess);
		OpenPropertiesDlg(StrTargetPath);
	}
}

void CPageProcesses::OnPop_SwitchTo()
{
	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);
	if(pData!=NULL)
	{
		if(pData->hMainWnd!=NULL)
		{
			::BringWindowToTop(pData->hMainWnd);	
			::ShowWindow(pData->hMainWnd,SW_RESTORE);
			::SetActiveWindow(pData->hMainWnd);
			if(theApp.AppSettings.MiniOnUse)
			{
				theApp.m_pMainWnd->ShowWindow(SW_MINIMIZE);
			}
		}
	}

}

void CPageProcesses::OnPop_Bringtofront()
{
	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);
	if(pData!=NULL)
	{
		if(pData->hMainWnd!=NULL)
		{
		
			::BringWindowToTop(pData->hMainWnd);	
			::ShowWindow(pData->hMainWnd,SW_RESTORE);
			::SetForegroundWindow(pData->hMainWnd);
		}
	}
}

void CPageProcesses::OnPop_Maximize()
{
	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);
	if(pData!=NULL)
	{
		if(pData->hMainWnd!=NULL)
		{		
				
			::ShowWindow(pData->hMainWnd,SW_MAXIMIZE);
			
		}
	}
}

void CPageProcesses::OnPop_Minimize()
{
	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);
	if(pData!=NULL)
	{
		if(pData->hMainWnd!=NULL)
		{		
				
			::ShowWindow(pData->hMainWnd,SW_MINIMIZE);
			
		}
	}
}

void CPageProcesses::OnPop_Endtask() //����ĳ��չ������Խ��̴��ڵĲ���
{
	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);
	if(pData!=NULL)
	{
		if(pData->hMainWnd!=NULL)
		{		
			::PostMessage(pData->hMainWnd, WM_DESTROY,0,0);
		
		}
	}
}

void CPageProcesses::SetWndItem(void)
{

	//int n=0; 
	//CWnd *pDeskTop=CWnd::GetDesktopWindow();
	//CWnd  *pAppWnd = pDeskTop->GetWindow(GW_CHILD);

	//CString  StrWndCaption;
	//CString StrNewWndList;

	//CString StrTemp ;

	//DWORD   hWndNum;

	// 
	//APPLISTDATA *pData;

	//while( pAppWnd!=NULL)
	//{

	//	if(!(pAppWnd->GetExStyle()&WS_EX_TOOLWINDOW) )  //      WS_CAPTION   pAppWnd->IsWindowVisible() && 
	//	{

	//		hWndNum = (DWORD) pAppWnd->GetSafeHwnd();
	//		pAppWnd->GetWindowTextW(StrWndCaption);
	//		StrTemp.Format(L"%x",hWndNum);
	//		StrTemp=StrTemp+StrWndCaption;
	//		DWORD PID;
	//		::GetWindowThreadProcessId(hWndNum,&PID);

	//		for(int i=0;i<mTaskList.GetItemCount();i++)
	//		{
	//			pData = (APPLISTDATA *)mTaskList.GetItemData(i);
	//			if(pData==NULL) continue;
	//			if(pData->pPData == NULL) continue;	
	//			if(pData->ItemType == WINPROC) continue;		
	//			if(((PROCLISTDATA*)pData->pPData)->PID ==PID )
	//			{
	//				pData->ItemType = = BKGPROC;

	//				if(pAppWnd->IsWindowVisible())
	//				{
	//					pData->ItemType = APP;
	//					
	//				}
	//				if()

	//				
	//			}


	//		}


	//	}

	//	pAppWnd=pAppWnd->GetWindow(GW_HWNDNEXT);

	//}



}

void CPageProcesses::OnPop_Memory_ShowAsPercents()
{
	if(theApp.AppSettings.ProcList_MemPercents==0)
	{
		theApp.AppSettings.ProcList_MemPercents = 1;
		RefreshList();
	}
}

void CPageProcesses::OnPop_Memory_ShowAsValues()
{
	if(theApp.AppSettings.ProcList_MemPercents!=0)
	{
		theApp.AppSettings.ProcList_MemPercents = 0;
		RefreshList();
	}
}

void CPageProcesses::_GetMemDataAndSetItemText(int iItem, APPLISTDATA * PData)
{

	double  MemUsage;
	MemUsage = mProcInfo.GetWsPrivate_PDH( (PROCLISTDATA*)PData->pPData );

	CString StrItem =L"";



	if(MemUsage<= (double)theApp.PerformanceInfo.TotalPhysMem   &&   MemUsage>0 && theApp.PerformanceInfo.InUsePhysMem>0)//��ֹ��ʾ���ҵ� ����
	{

		((PROCLISTDATA*)PData->pPData)->Mem_PrivateWS = MemUsage;

		double Percents =  (double)( MemUsage/(theApp.PerformanceInfo.InUsePhysMem )*100);
		if(Percents<0.1&&Percents>0)Percents=0.1;

		if(theApp.AppSettings.ProcList_MemPercents)//���ٷֱ���ʾ
		{

			StrItem.Format(L"%.1f%%",  Percents );
			mTaskList.MySetItemText(iItem,PROCLIST_MEMORY,StrItem);


		}
		else
		{
			StrItem.Format(L"%.1f",  MemUsage/1024/1024);
			mTaskList.MySetItemText(iItem,PROCLIST_MEMORY,StrItem+L" MB");

		}

		if(Percents>1&&Percents<10)Percents=20;//����ܶ඼����ʾ��ɫ
		PData->CoolUsageArray[PROCLIST_MEMORY]= Percents/100; //���ڱ�ɫ��ʾ������
	}

}

void CPageProcesses::OnPop_Searchonline()
{
	APPLISTDATA *pData = NULL;
	int nSel = mTaskList.GetNextItem( -1, LVNI_SELECTED );
	pData = (APPLISTDATA *)mTaskList.GetItemData(nSel);

	if(pData!=NULL)
	{
		if(pData->pPData ==NULL )return;		 
		SearchOnline(((PROCLISTDATA *)(pData->pPData))->Description);
	}


	
}

void CPageProcesses::UpdateGroupText(void)
{
	int i= 0;
	while(1)
	{
		if(mTaskList.GetItemCount() == i) break ;

		APPLISTDATA *pListData = (APPLISTDATA *)mTaskList.GetItemData(i);	 

		if( (pListData == NULL) || (pListData->pPData == NULL ) )  //ע���ų��������
		{ 	

			CString StrGroupTitle  = mTaskList.GetItemText(i, 0);
			CString StrNewGroupTitle = NULL  ;

			if(StrGroupTitle.Find(STR_GROUP_APP)>=0)
			{
				StrNewGroupTitle.Format(L"%s (%d)",STR_GROUP_APP,nApp);						 

			}
			else if(StrGroupTitle.Find(STR_GROUP_BKG)>=0)
			{
				StrNewGroupTitle.Format(L"%s (%d)",STR_GROUP_BKG,nBkg);			

			}
			else if(StrGroupTitle.Find(STR_GROUP_WIN)>=0)
			{
				StrNewGroupTitle.Format(L"%s (%d)",STR_GROUP_WIN,nWin);		
			}

			mTaskList.SetItemText(i,0,StrNewGroupTitle);

		} 

		i++;  

	}
}
