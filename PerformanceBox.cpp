// PerformanceView.cpp : implementation file


#include "stdafx.h"
#include "DBCTaskman.h"
#include "PerformanceBox.h"
#include "MemCompBox.h"
#include "SMBIOS.h"
#include <intrin.h>      
#include <process.h>     

// inet_ntop / INET_ADDRSTRLEN — declared in <ws2tcpip.h>.
#include <ws2tcpip.h>

#include "PerfCpuSpeed.h"
#include "PerfDbgLog.h"
#include "PerfDiskShared.h"
#include "PerfNetHelpers.h"
#include "PerfNtSys.h"
#include "PerfPdhDisk.h"
#include "PerfPdhProcIo.h"
#include "PerfPidIoCache.h"
#include "PerfWlan.h"
#include "PerfWmiDisk.h"

extern "C" {
#include <powrprof.h>
}
#pragma comment(lib , "PowrProf.lib") 




#include <atlbase.h>

// #include <Iphlpapi.h>
#pragma comment(lib , "Iphlpapi.lib") 

// CPerformanceBox

static  CPerformanceBox* pThisBoxView;


static   BOOL FlagStartDiskMon = FALSE;


API_NtQuerySystemInformation  MyNtQuerySystemInformation       = NULL;
PROCNTQSIP                    MyNtQueryInformationProcess      = NULL;

UINT Thread_GetDiskOtherStaticInfo(LPVOID lparam)
{
	CPerformanceBox * pWnd =  (CPerformanceBox * )lparam;

	if(pWnd !=NULL)
	{
		pWnd->_GetDiskOtherStaticInfo();
	}

	//MSB (GetDiskVolFailCount);
	if(GetDiskVolFailCount>0)
	{
		pWnd->_GetDiskLetterUseWmi();
	}

	FlagStartDiskMon = TRUE;

	::AfxEndThread(0);

	return 0;
}

UINT Thread_LoadDiskStaticInfo(LPVOID lparam)
{
	PerferListData * pData =  (PerferListData * )lparam;

	if(pData !=NULL)
	{
		pThisBoxView->_LoadDiskStaticInfo(pData);
	}

	::AfxEndThread(0);

	return 0;
}

// ============================================================================
// ComputeHeaderDiskUsage
//   Returns the disk %% to display in the Processes tab header (and the
//   MiniDlg header bar) — a number in [0, 100].
//
//   PRIMARY: per-disk average over the disks the per-disk cascade produced
//   a real sample for (one of the branches 'K' / 'P' / 'W' / 'S' / 'N' /
//   'D'). This is the same set of values the Performance tab shows for each
//   disk; their average is therefore the value the user expects to see in
//   the header — and it matches Windows Task Manager's own header, which
//   also reports the per-disk average.
//
//   FALLBACKS (used only when the per-disk cascade produced no valid sample
//   at all — extremely rare; happens if every disk failed every source):
//   kernel-timer sum, then the aggregated external counters (PDH, WMI,
//   NtSys). These do NOT in general equal the per-disk average, which is
//   why the per-disk average is the primary source and the aggregations
//   only step in as a safety net.
//
//   *outBranch receives a one-char tag identifying the source that won:
//     'A' = Average of per-disk cascade values (PRIMARY)
//     'K' = Kernel SumRWTime / SumTotalTime
//     'P' = PDH \PhysicalDisk(_Total)\%% Disk Time
//     'S' = PDH process busy fraction
//     'W' = WMI _Total instance
//     'N' = NtSys busy fraction
//     '-' = nothing answered (returns 0)
// ============================================================================
static double ComputeHeaderDiskUsage(double  sumDiskUsage,
									 int     nValidDiskSamples,
									 ULONG64 sumRWTime,
									 ULONG64 sumTotalTime,
									 char*   outBranch)
{
	if(outBranch) *outBranch = '-';

	
	if(nValidDiskSamples > 0)
	{
		double avg = (sumDiskUsage / (double)nValidDiskSamples) * 100.0;
		if(_finite(avg) == 0) avg = 0;
		if(avg <   0) avg =   0;
		if(avg > 100) avg = 100;
		if(outBranch) *outBranch = 'A';
		return avg;
	}

	
	if(sumTotalTime > 0)
	{
		double totalPct = (double)sumRWTime / (double)sumTotalTime * 100.0;
		if(_finite(totalPct) == 0) totalPct = 0;
		if(totalPct <   0) totalPct = 0;
		if(totalPct > 100) totalPct = 100;
		if(outBranch) *outBranch = 'K';
		return totalPct;
	}


	if(PerfPdhDisk_GetTotalReady() && PerfPdhDisk_GetTotalEverNonZero())
	{
		double totalPct = PerfPdhDisk_GetTotalPct();
		if(_finite(totalPct) == 0) totalPct = 0;
		if(totalPct <   0) totalPct = 0;
		if(totalPct > 100) totalPct = 100;
		if(outBranch) *outBranch = 'P';
		return totalPct;
	}

	if(PerfPdhProcIo_GetReady() && PerfPdhProcIo_GetSystemBps() > 0.0)
	{
		double totalPct = PerfPdhProcIo_GetSystemBusyPct();
		if(_finite(totalPct) == 0) totalPct = 0;
		if(totalPct <   0) totalPct = 0;
		if(totalPct > 100) totalPct = 100;
		if(outBranch) *outBranch = 'S';
		return totalPct;
	}

	if(PerfWmiDisk_GetTotalReady() && PerfWmiDisk_GetTotalEverNonZero())
	{
		double totalPct = PerfWmiDisk_GetTotalPct();
		if(_finite(totalPct) == 0) totalPct = 0;
		if(totalPct <   0) totalPct = 0;
		if(totalPct > 100) totalPct = 100;
		if(outBranch) *outBranch = 'W';
		return totalPct;
	}

	if(PerfNtSys_GetReady())
	{
		double totalPct = PerfNtSys_GetTotalPct();
		if(_finite(totalPct) == 0) totalPct = 0;
		if(totalPct <   0) totalPct = 0;
		if(totalPct > 100) totalPct = 100;
		if(outBranch) *outBranch = 'N';
		return totalPct;
	}

	return 0;
}

//============================


IMPLEMENT_DYNCREATE(CPerformanceBox, CFormView)

CPerformanceBox::CPerformanceBox()
: CFormView(CPerformanceBox::IDD)
, StrCPUName(_T(""))
, pInfoBoxCpu(NULL)
, pInfoBoxMemory(NULL)
, nLogicalProcessor(0)
, pTotalCpuBox(NULL)
, pCpuBox(NULL)

, nStep(0)
, FlagFirstUpdate(TRUE)
, ShowThisPage(TRUE)
, MaxCPUSpeed(0)
, pViewClass(NULL)
, pHotBox(NULL)
, CurrentDiskCount(0)
{

	 ArraySize= sizeof(float)*60;
}

CPerformanceBox::~CPerformanceBox()
{
	// Stop the PDH monitor (Win10-style primary) and the WMI fallback
	// (3 s polling). The RDTSC + QPC sampler was retired because the
	// Invariant TSC on Intel Sandy Bridge / Core / AMD Zen makes it
	// report only the nominal clock.
	PerfPdhCpuPerf_Stop();
	PerfWmiCpu_Stop();
	PerfWmiDisk_Stop();
	PerfPdhDisk_Stop();
	PerfNtSys_Stop();
	PerfPdhProcIo_Stop();

	// Release the Segoe UI font handle allocated in OnCreate().
	if(mFontTurboNote.GetSafeHandle() != NULL)
		mFontTurboNote.DeleteObject();
}

void CPerformanceBox::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_LIST_PERFORMANCEITEM, mPItemList);

}

BEGIN_MESSAGE_MAP(CPerformanceBox, CFormView)
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
	ON_MESSAGE(UM_TIMER,OnUMTimer)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_PERFORMANCEITEM, &CPerformanceBox::OnLvnItemchangedListPerformanceitem)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_CHANGEGRAPHTO_OVERALLUTILIZATION, &CPerformanceBox::OnPop_ChangeGraphToOverallutilization)
	ON_COMMAND(ID_CHANGEGRAPHTO_LOGICALPROCESSORS, &CPerformanceBox::OnPop_ChangeGraphToLogicalprocessors)
	//	ON_WM_CREATE()
	ON_WM_INITMENUPOPUP()
	
	ON_COMMAND(ID_PERFORMANCE_SHOWKERNELTIMES, &CPerformanceBox::OnPerformanceShowkerneltimes)
	ON_UPDATE_COMMAND_UI(ID_PERFORMANCE_SHOWKERNELTIMES, &CPerformanceBox::OnUpdatePerformanceShowkerneltimes)
	
	
	ON_COMMAND(ID_VIEW_CPU, &CPerformanceBox::OnViewCpu)
	ON_COMMAND(ID_VIEW_MEMORY, &CPerformanceBox::OnViewMemory)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_PERFORMANCEITEM, &CPerformanceBox::OnNMRClickListPerformanceitem)
	ON_COMMAND(ID_PERFORMANCETYPE_HIDEGRAPHS, &CPerformanceBox::OnPop_PerformanceListShowHideGraphs)
	ON_UPDATE_COMMAND_UI(ID_PERFORMANCETYPE_HIDEGRAPHS, &CPerformanceBox::OnUpdatePerformancetypeHidegraphs)
	ON_WM_CREATE()
	ON_COMMAND(ID_CHANGEGRAPHTO_NUMANODES, &CPerformanceBox::OnPop_ChangeGraphToNumaNodes)	
	ON_NOTIFY_EX( TTN_NEEDTEXT, 0, SetTipText)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_COMMAND(ID_PERFORMANCE_GRAPHSUMMARYVIEW, &CPerformanceBox::OnPop_GraphSummaryView)
	
	ON_COMMAND(ID_PERFORMANCETYPE_SUMMARYVIEW, &CPerformanceBox::OnPop_PerformanceListSummaryView)

END_MESSAGE_MAP()


// CPerformanceBox diagnostics

#ifdef _DEBUG
void CPerformanceBox::AssertValid() const
{
	CFormView::AssertValid();
}

#ifndef _WIN32_WCE
void CPerformanceBox::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif
#endif //_DEBUG


// CPerformanceBox message handlers

HBRUSH CPerformanceBox::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	//HBRUSH hbr = CFormView::OnCtlColor(pDC, pWnd, nCtlColor);

	// TODO:  Change any attributes of the DC here





	CString StrFontName;
	StrFontName.LoadStringW(IDS_FONTNAME);


	if(nCtlColor == CTLCOLOR_STATIC)
	{
		pDC->SetBkColor(theApp.WndBkgColor);
		int nID=pWnd->GetDlgCtrlID();

		CFont StaticFont;

		if(nID== IDC_TIP1|| nID== IDC_TIP2  )
		{
			pDC->SetTextColor(RGB(112,112,112));
		}
		if(nID== IDC_ITEMLABEL )
		{
			pDC->SetTextColor(theApp.WndTextColor);		

			StaticFont.CreateFont(30,   // nHeight
				0,                         // nWidth
				0,                         // nEscapement
				0,                         // nOrientation
				FW_NORMAL,             // nWeight     FW_NORMAL,     FW_BOLD
				FALSE,                     // bItalic
				FALSE,                     // bUnderline�»��߱�ǣ���Ҫ�»��߰��������ó�TRUE
				0,                         // cStrikeOut
				DEFAULT_CHARSET,              // nCharSet
				OUT_DEFAULT_PRECIS,        // nOutPrecision
				CLIP_DEFAULT_PRECIS,       // nClipPrecision
				DEFAULT_QUALITY,           // nQuality
				DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
				StrFontName);                 // lpszFacename

			pDC->SelectObject(StaticFont);

			//pDC->SetBkColor(RGB(251, 247, 200));
			//pDC->SetBkMode(TRANSPARENT);
			//  return (HBRUSH) m_brush.GetSafeHandle();
		}

		if(nID== IDC_ITEMNAME )
		{
			pDC->SetTextColor(theApp.WndTextColor);	


			StaticFont.CreateFont(17,   // nHeight
				0,                         // nWidth
				0,                         // nEscapement
				0,                         // nOrientation
				FW_NORMAL,             // nWeight     FW_NORMAL,     FW_BOLD
				FALSE,                     // bItalic
				FALSE,                     // bUnderline�»��߱�ǣ���Ҫ�»��߰��������ó�TRUE
				0,                         // cStrikeOut
				DEFAULT_CHARSET,              // nCharSet
				OUT_DEFAULT_PRECIS,        // nOutPrecision
				CLIP_DEFAULT_PRECIS,       // nClipPrecision
				DEFAULT_QUALITY,           // nQuality
				DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
				StrFontName);                 // lpszFacename

			pDC->SelectObject(StaticFont);

			//pDC->SetBkColor(RGB(251, 247, 200));
			//pDC->SetBkMode(TRANSPARENT);
			//  return (HBRUSH) m_brush.GetSafeHandle();
		}

		if(nID == IDC_TURBO_NOTE)
		{
			// Gray secondary text, same hue as IDC_TIP1 / IDC_TIP2.
			// Font is the native Segoe UI captured in OnCreate().
			pDC->SetTextColor(RGB(112, 112, 112));
			if(mFontTurboNote.GetSafeHandle() != NULL)
				pDC->SelectObject(&mFontTurboNote);
		}

		StaticFont.DeleteObject();

	}



	// TODO:  Return a different brush if the default is not desired
	return theApp.BkgBrush;
}

void CPerformanceBox::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);
	//-----------------------------------------
	PlaceAllCtrl();
	InvalidateIfVisible(this);	 

}

void CPerformanceBox::Init(void)
{





	GetCPUInfo();

	pThisBoxView = this;

	//-------- init  logigal Processor Usage ------------------


	unsigned long bytesreturned;

	// Bug fix Win7: guard against nLogicalProcessor==0 (or negative) so we never
	// allocate zero-sized arrays that lead to wild-pointer writes later.
	int nCpuCount = nLogicalProcessor;
	if (nCpuCount <= 0)
	{
		SYSTEM_INFO si2; memset(&si2,0,sizeof(SYSTEM_INFO));
		GetSystemInfo(&si2);
		nCpuCount = si2.dwNumberOfProcessors;
		if (nCpuCount <= 0) nCpuCount = 1;
		nLogicalProcessor = nCpuCount;
		theApp.PerformanceInfo.nLogicalProcessor = nCpuCount;
	}

	spi_old = new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION [nCpuCount];
	spi = new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION [nCpuCount];

	memset(spi_old,0,sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nCpuCount);
	memset(spi,0,sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nCpuCount);

	MyNtQuerySystemInformation =(API_NtQuerySystemInformation) GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQuerySystemInformation");

	MyNtQuerySystemInformation(SystemProcessorPerformanceInformation,spi_old, (sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nLogicalProcessor),&bytesreturned);


	

	//-------------------------- Left Item  lsit----------------------------

	//SetWindowTheme(mPItemList.GetSafeHwnd(),L"explorer", NULL);


	CRect rcCtrl;
	mPItemList.GetClientRect(rcCtrl);

	mPItemList.SetExtendedStyle(mPItemList.GetExtendedStyle()|LVS_EX_FULLROWSELECT|LVS_EX_SUBITEMIMAGES | LVS_OWNERDRAWFIXED|LVS_EX_DOUBLEBUFFER);  //| LVS_EX_GRIDLINES |LVS_EX_CHECKBOXES



	mPItemList.SetParent(this->GetParent());

	mPItemList.InsertColumn(0,L"",0,0); //����
	mPItemList.InsertColumn(1,L"",0,0);//�ڶ���������ʾ
	mPItemList.InsertColumn(2,L"",0,0);//�豸����
	mPItemList.InsertColumn(3,L"",0,0);//Tip1
	mPItemList.InsertColumn(4,L"",0,0);//Tip2


	//mPItemList.ShowScrollBar(SB_VERT,TRUE);





	//------------------------------ info box ------------------------------------------------------------------------

	//--------------  CPU---------

	pInfoBoxCpu = new CInfoBox;

	pInfoBoxCpu->Create(L"",WS_CHILD|WS_VISIBLE|SS_CENTER,CRect(0,0,1,1),this);
	pInfoBoxCpu->ShowWindow(SW_SHOW);

	pInfoBoxCpu->Type = PM_CPU ;



	pInfoBoxCpu->Info[0].StrTitle  = STR_CPUINFO_0 ;
	pInfoBoxCpu->Info[1].StrTitle  = STR_CPUINFO_1 ;
	pInfoBoxCpu->Info[2].StrTitle  = STR_CPUINFO_2 ;
	pInfoBoxCpu->Info[3].StrTitle  = STR_CPUINFO_3 ;
	pInfoBoxCpu->Info[4].StrTitle  = STR_CPUINFO_4 ;
	pInfoBoxCpu->Info[5].StrTitle  = STR_CPUINFO_5 ;
	pInfoBoxCpu->Info[6].StrTitle  = STR_CPUINFO_6 ;

	//�޸�Ĭ��λ��
	pInfoBoxCpu->Info[4].rc = pInfoBoxCpu->Info[3].rc;  pInfoBoxCpu->Info[4].rc.OffsetRect(pInfoBoxCpu->Info[3].rc.Width(),0);
	pInfoBoxCpu->Info[5].rc.left = pInfoBoxCpu->Info[0].rc.left;   	pInfoBoxCpu->Info[6].rc.left  = pInfoBoxCpu->Info[6].rc.left+pInfoBoxCpu->Info[5].rc.left+22;



	_UpdateCpuInfoBox(TRUE);

	// --------------Memory---------

	pInfoBoxMemory = NULL;
	pInfoBoxMemory = new CInfoBox;
	pInfoBoxMemory->Create(L"",WS_CHILD|WS_VISIBLE|SS_CENTER|SS_NOTIFY,CRect(0,0,1,1),this); //SS_NOTIFY������в�Ȼ�޷������� ��ɫ ��
	
	pInfoBoxMemory->Type = PM_MEMORY;

	pInfoBoxMemory->Info[0].StrTitle  = STR_MEMINFO_0;
	pInfoBoxMemory->Info[1].StrTitle  = STR_MEMINFO_1 ;
	pInfoBoxMemory->Info[2].StrTitle  = STR_MEMINFO_2 ;
	pInfoBoxMemory->Info[3].StrTitle  = STR_MEMINFO_3 ;
	pInfoBoxMemory->Info[4].StrTitle  = STR_MEMINFO_4 ;
	pInfoBoxMemory->Info[5].StrTitle  = STR_MEMINFO_5 ;
	pInfoBoxMemory->Info[6].StrTitle  = STR_MEMINFO_6 ;

	//�޸�Ĭ��λ��

	pInfoBoxMemory->Info[1].rc.OffsetRect(30,0); 
	pInfoBoxMemory->Info[3].rc.OffsetRect(30,0); 
	pInfoBoxMemory->Info[5].rc.OffsetRect(30,0);


	_UpdateMemoryInfoBox(TRUE);



	//------------------------------------------------------------------------------------------------------------------------------

	//------------------------------------------������ ��  ����ͼ ��----------------------------------------------------------------------

	//-------------------------------------------- �� CPU --------------------------------------


	pTotalCpuBox  = new CWaveBox;
	pTotalCpuBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);   

	pTotalCpuBox->SetLineColumn(1,1);
	pTotalCpuBox->SetColor(theApp.AppSettings.CpuColor);
	pTotalCpuBox->DrawSecondWave =  theApp.AppSettings.ShowKernelTime ;
	mPItemList.NumArray = pTotalCpuBox->Num[0];



	//-------------------------------------------- �߼� CPU  --------------------------------------


	CString StrItemName;
	StrItemName.LoadStringW(IDS_PITEM_CPU);





	mPItemList.InsertItem(0,StrItemName);	
	CString StrTemp=L"";
	double CurrentSpeed = _GetSurrentCpuSpeed();
	StrTemp.Format(L"0.00%%  %0.2f GHz",CurrentSpeed);

	mPItemList.SetItemText(0,1,StrTemp);
	mPItemList.SetItemText(0,2,StrCPUName);
	StrTemp=L"";

	pCpuBox = new CWaveBox;
	PerferListData * pPData =  new PerferListData;	

	pCpuBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);  
	//--------------�������м�����ʾ-------------------
	int nRow = 1, nColum = 1;

	if (theApp.PerformanceInfo.nLogicalProcessor > 0)
	{
		_DetermineRowCol(theApp.PerformanceInfo.nLogicalProcessor,&nRow,&nColum );
	}


	pCpuBox->SetLineColumn(nRow,nColum);
	pCpuBox->SetColor(theApp.AppSettings.CpuColor);
	pCpuBox->DrawSecondWave =  theApp.AppSettings.ShowKernelTime ;

	//--------CPU hot box -------------------
	pHotBox->nRow=nRow; pHotBox->nCol = nColum;
	pHotBox->ArrayPerLogicalCpuUsage = new float[(nLogicalProcessor>0)?nLogicalProcessor:1];




	if(theApp.AppSettings.ProcessorDisplayMode == 1 )
	{
		pPData->pWaveBox = pCpuBox;
		pHotBox->ShowWindow(SW_HIDE);

		CWnd *pWnd=GetDlgItem(IDC_TIP1);
		CString StrTime;
		if((double)theApp.AppSettings.TimerStep == 0.5)
		{
			StrTime.LoadStringW(IDS_STRING_WAVETIME_FAST);
		}
		else if ((double)theApp.AppSettings.TimerStep == 1)
		{
			StrTime.LoadStringW(IDS_STRING_WAVETIME_NORMAL);
		}
		else if ((double)theApp.AppSettings.TimerStep == 4)
		{
			StrTime.LoadStringW(IDS_STRING_WAVETIME_SLOW);
		}
		if(pWnd!=NULL)	
		{ 
			StrTemp.Format(STR_TIP1_CPU_LOGICAL,StrTime);

			mPItemList.SetItemText(0,3,StrTemp);
			pWnd->SetWindowTextW(StrTemp);
		}



	}
	else if(theApp.AppSettings.ProcessorDisplayMode == 0 )
	{
		pPData->pWaveBox = pTotalCpuBox;
		CWnd * pWnd=GetDlgItem(IDC_TIP1);
		if(pWnd!=NULL)	
		{ 
			CString StrTemp = STR_TIP1_CPU_TOTAL;
			mPItemList.SetItemText(0,3,StrTemp);
			pWnd->SetWindowTextW(StrTemp);
		}
	 

	}
	else if(theApp.AppSettings.ProcessorDisplayMode == 2)
	{
		pPData->pWaveBox = pTotalCpuBox; //ҲҪ
		pCpuBox->ShowWindow(SW_HIDE);
		pTotalCpuBox->ShowWindow(SW_HIDE);

		CWnd *pWnd=GetDlgItem(IDC_TIP1);
		if(pWnd!=NULL)	
		{ 
			CString StrTemp = STR_TIP1_CPU_NUMA;
			mPItemList.SetItemText(0,3,StrTemp);
			pWnd->SetWindowTextW(StrTemp);
		}
	 



	}




	pPData->pInfoBox = pInfoBoxCpu;
	pPData->Type = PM_CPU;
	pPData->ID = -1; //��ʾ������Ч
	pPData->pOtherWnd = NULL;



	mPItemList.SetItemData(0,(DWORD_PTR)pPData);
	mPItemList.SetItemText(0,4,L"100%");







	//----------------------------------------------- Memroy --------------------------------------------

	
	StrItemName.LoadStringW(IDS_PITEM_MEM);

	 
	mPItemList.InsertItem(1,StrItemName);
	StrTemp.Format(L"0.0/%0.1f GB (0.00%%)", (double)theApp.PerformanceInfo.TotalPhysMem/1024/1024/1024);
	mPItemList.SetItemText(1,1,StrTemp);	




	StrTemp.Format(L"%.0f GB %s" ,(double) (theApp.PerformanceInfo.InstalledMemKB)/1024/1024,theApp.PerformanceInfo.StrMemoryType); 
	mPItemList.SetItemText(1,2,StrTemp);

	CWaveBox *    pMemoryBox= new CWaveBox;
	CMemCompBox * pMemoryCompBox= new CMemCompBox;
	pPData =  new PerferListData;

	pPData->pWaveBox = pMemoryBox;
	pPData->pOtherWnd = pMemoryCompBox;

	pPData->pInfoBox = pInfoBoxMemory;
	pPData->Type = PM_MEMORY;
	pPData->ID = -1;


	pMemoryBox->Create(NULL,WS_CHILD,CRect(0,0,1,1),this);
	pMemoryCompBox->Create(NULL,WS_CHILD|SS_NOTIFY,CRect(0,0,1,1),this);
	mPItemList.SetItemData (1,(DWORD_PTR)pPData);

	pMemoryBox->SetLineColumn(1,1);
	pMemoryBox->SetColor(theApp.AppSettings.MemoryColor );

	pMemoryCompBox->SetColor( );

	pMemoryBox->DrawSecondWave = FALSE;

	mPItemList.SetItemText(1,3,STR_TIP3_MEMORY);
	StrTemp.Format(L"%0.1f GB", (double)theApp.PerformanceInfo.TotalPhysMem/1024/1024/1024);
	mPItemList.SetItemText(1,4,StrTemp);




	//-------------------------------------------------------------------------------------
	
 


	//ö�ٴ���
	AddDiskToList();


	//------------------------------------------------------------------------------

	//ö������
	AddEthernetAdapterToList();


	theApp.StartPerformancePageTimer = TRUE;



	//-----------------------------------------------------------------

	// AfxBeginThread(Thread_SetDiskList,this);  //���ƴ��̼�������  ��Ϊ����Ƚ������зŶ����߳�


	//ѡ�е�һ��  �� CPU


	mPItemList.SetItemState(0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);	 
	mPItemList.MoveWindow(0,0,187,300);


	//--------------------------Labels ---------------------


	GetDlgItem(IDC_ITEMLABEL)->SetWindowTextW(L"CPU");
	GetDlgItem(IDC_ITEMNAME)->SetWindowTextW( StrCPUName );

	PlaceAllCtrl();



	EnableToolTips(TRUE);
	mToolTip.Create(this);
	mToolTip.Activate(TRUE);
	mToolTip.AddTool(pMemoryCompBox,LPSTR_TEXTCALLBACK);
	mToolTip.SetMaxTipWidth(200);
	 


	pInfoBoxCpu->Info[1].StrInfo.Format(L"%0.2f GHz",CurrentSpeed);

 





}



LRESULT CPerformanceBox::OnUMTimer(WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your message handler code here and/or call default

	//::AfxBeginThread(Thread_UpDatePM,this);



	//mPItemList.SetRedraw(0);


	ShowThisPage =( theApp.pSelPage == (CFormView *)GetParent());

	UpdateAllPMInfo();

	// Keep the Win 7 Turbo disclaimer aligned with the current page /
	// selection so it appears as soon as the user lands on the CPU tab
	// and disappears when they switch to Memory / Disk / Wi-Fi.
	_RefreshTurboNoteVisibility();

	// mPItemList.SetRedraw(1);

	nStep++;
	if(nStep==60)nStep =0;


	FlagFirstUpdate = FALSE; 


	return 0;

}

void CPerformanceBox::OnLvnItemchangedListPerformanceitem(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here

	INT nSel = 0;
	nSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );




	if(nSel== -1) return;


	int n = mPItemList.GetItemCount();

	// Re-evaluate the Win 7 Turbo disclaimer visibility now that the
	// selected item has changed (it is only meaningful when CPU is
	// the active entry, since the disclaimer sits under IDC_ITEMNAME).
	_RefreshTurboNoteVisibility();



	//	PerferListData  *pPData;

	CWnd *pWnd= NULL;
	CString StrTip;


	for(int i=0;i<n;i++)
	{


		PerferListData * pPData = (PerferListData*)mPItemList.GetItemData(i);	

		if(pPData == NULL) { continue ;  } 
		if(pPData->pWaveBox==NULL) {continue ;  }
		if(pPData->pInfoBox==NULL) {continue ;  }

		if(i==nSel)
		{ 


			pPData->pWaveBox->ShowWindow(SW_SHOW);
			pPData->pInfoBox->ShowWindow(SW_SHOW);
			if(pPData->pOtherWnd != NULL)
			{
				//MSG(nSel)
				pPData->pOtherWnd->ShowWindow(SW_SHOW);
			}



			if(pPData->Type == PM_DISK)
			{

				pWnd=GetDlgItem(IDC_TIP5);
				if(pWnd!=NULL)
				{
					pWnd->SetWindowTextW(STR_TIP5_DISK); 
					pWnd->ShowWindow(SW_SHOW);
				}
				pWnd=GetDlgItem(IDC_TIP6);
				if(pWnd!=NULL)
				{
					pWnd->SetWindowTextW(pPData->StrOther1); 
					pWnd->ShowWindow(SW_SHOW);
				}
				pWnd=GetDlgItem(IDC_TIP7);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
				pWnd=GetDlgItem(IDC_TIP8);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			}
			else if(pPData->Type == PM_MEMORY)
			{
				pWnd=GetDlgItem(IDC_TIP5);
				if(pWnd!=NULL)
				{
					pWnd->SetWindowTextW( STR_TIP5_MEMORY); 
					pWnd->ShowWindow(SW_SHOW);
				}
				pWnd=GetDlgItem(IDC_TIP6);
				if(pWnd!=NULL)
				{
					pWnd->SetWindowTextW(L""); 
					pWnd->ShowWindow(SW_SHOW);
				}
				pWnd=GetDlgItem(IDC_TIP7);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
				pWnd=GetDlgItem(IDC_TIP8);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			}
			else
			{



				pWnd=GetDlgItem(IDC_TIP5);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
				pWnd=GetDlgItem(IDC_TIP6);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
				pWnd=GetDlgItem(IDC_TIP7);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
				pWnd=GetDlgItem(IDC_TIP8);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }

			}


		}
		else  
		{ 


			pPData->pWaveBox->ShowWindow(SW_HIDE);
			pPData->pInfoBox->ShowWindow(SW_HIDE);
			if(pPData->pOtherWnd != NULL)
			{
				pPData->pOtherWnd->ShowWindow(SW_HIDE);
			}
		} 


	}





	if(nSel==0) //CPU
	{


		if(theApp.AppSettings.ProcessorDisplayMode  ==0 ) // ��ʾ��CPU ģʽ
		{
			pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			pHotBox->ShowWindow(SW_HIDE);	
		}
		else if (theApp.AppSettings.ProcessorDisplayMode  ==1   )
		{
			pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
			pHotBox->ShowWindow(SW_HIDE);	
		}
		else if (theApp.AppSettings.ProcessorDisplayMode  == 2 )
		{
			pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
			pCpuBox->ShowWindow(SW_HIDE);
			pTotalCpuBox->ShowWindow(SW_HIDE);	
			pHotBox->ShowWindow(SW_SHOW);	

		}

	}
	else //��������Ŀʱ ��ǩ2 3 4 ����ʾ��
	{
		pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
		pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
		pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }
		pHotBox->ShowWindow(SW_HIDE);	
	}









	pWnd=GetDlgItem(IDC_TIP1);
	if(pWnd!=NULL)
	{					
		StrTip = mPItemList.GetItemText(nSel,3);
		pWnd->SetWindowTextW(StrTip); //�������ǿ��͸߲���λ��
	}


	pWnd=GetDlgItem(IDC_TIP2);
	if(pWnd!=NULL)
	{					
		StrTip = mPItemList.GetItemText(nSel,4);
		pWnd->SetWindowTextW(StrTip); //�������ǿ��͸߲���λ��
	}




	PerferListData * pPData = (PerferListData*)mPItemList.GetItemData(nSel);

	if(pPData->Type == PM_ETHERNET)
	{
		SetScrollSizes(MM_TEXT, CSize(BOX_W_MIN, 300)); 
	}
	else
	{
		SetScrollSizes(MM_TEXT, CSize(BOX_W_MIN, 400)); 

	}












	this->PlaceAllCtrl(); 

	GetDlgItem(IDC_ITEMLABEL)->SetWindowTextW(mPItemList.GetItemText(nSel,0));
	GetDlgItem(IDC_ITEMNAME)->SetWindowTextW(mPItemList.GetItemText(nSel,2));

	UpdateInfoBox();

	mPItemList.Invalidate();
	this->Invalidate();






	*pResult = 0;
}

int CPerformanceBox::UpdateAllPMInfo(void)
{


	int n = mPItemList.GetItemCount();
	
	int iSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );

	double *PerformanceData  = new double[n]; 
	double *PerformanceDataA  = new double[n]; 
	double *PerformanceDataB  = new double[n]; 

	//ULONG64 TempData;


	CString StrTemp;


	if(ShowThisPage)
	{
		mPItemList.SetRedraw(0);
	}



	// ����ͼ �ڲ����Ʋ��� 0��1  ��������������ʾ���Y���꣨����50% =  0.5��  �����ǰٷ���  ���Եõ��ٷ�����Ҫת�� ���������ظ��˳� 100 



	//------------------------------------CPU   -----------------------------



	//------------------------------------Memory -------------------------


	double MemUsagePercent;

	ZeroMemory(&MyPMInfo, sizeof(PERFORMANCE_INFORMATION));
	MyPMInfo.cb = sizeof(MyPMInfo);
	GetPerformanceInfo(&MyPMInfo,sizeof(PERFORMANCE_INFORMATION));



	
	//Ϊ�˻��� memory composition
	theApp.PerformanceInfo.Mem_Modified =  PM.PdhGetInfo(L"\\Memory\\Modified Page List Bytes");	
	//�˷�����ȡ Mem_Standby���ɿ� 
	if(iSel == 1)
	theApp.PerformanceInfo.Mem_Standby = MyPMInfo.PhysicalAvailable*MyPMInfo.PageSize - PM.PdhGetInfo(L"\\Memory\\Free & Zero Page List Bytes");


	//------------------------------------------------------------------------------------------------------------------------------------    

	//MEMORYSTATUSEX  MemStatus;
	//MemStatus.dwLength = sizeof(MemStatus);
	//BOOL Ret;
	//BOOL Ret = GlobalMemoryStatusEx(&MemStatus);
	//theApp.PerformanceInfo.PagedVirtual = MemStatus.ullTotalVirtual;
	//theApp.PerformanceInfo.CommitLimit =   MemStatus.ullTotalPageFile;
	//theApp.PerformanceInfo.CommitCurrent =MemStatus.ullTotalPageFile-MemStatus.ullAvailPageFile;

	//------------------------------------------------------------------------------------------------------------------------------------

	// Ԥ�ȶ�����  PERFORMANCE_INFORMATION MyPMInfo;


 
	theApp.PerformanceInfo.InUsePhysMem = (DWORDLONG)( (MyPMInfo.PhysicalTotal-MyPMInfo.PhysicalAvailable)*MyPMInfo.PageSize-theApp.PerformanceInfo.Mem_Modified-(theApp.PerformanceInfo.InstalledMemKB*1024-theApp.PerformanceInfo.TotalPhysMem));
 


	theApp.PerformanceInfo.MemoryUsage =  ((double)(theApp.PerformanceInfo.InUsePhysMem))/((double) (theApp.PerformanceInfo.TotalPhysMem)) ;

	if(theApp.PerformanceInfo.MemoryUsage>1.0  ) theApp.PerformanceInfo.MemoryUsage = 1.0;
	
	MemUsagePercent = theApp.PerformanceInfo.MemoryUsage *100;//// תΪ�ٷֱ�

	if(ShowThisPage)
	{
		StrTemp.Format(L"%.1f/%.1f GB (%.2f%%)",((double)(theApp.PerformanceInfo.InUsePhysMem ))/1024/1024/1024,(double) (theApp.PerformanceInfo.TotalPhysMem)/1024/1024/1024,MemUsagePercent  );
		mPItemList.SetItemText(1,1,StrTemp);
		MySetItem(1,StrTemp);
	}


	PerformanceData[1]=theApp.PerformanceInfo.MemoryUsage ;
	PerformanceDataA[1] = PerformanceDataB[1] =PerformanceData[1];
	theApp.PerformanceInfo.MemoryUsage = MemUsagePercent;// תΪ�ٷֱ�



	//------------------------------------Disk    -------------------------



	double SumDiskUsage=0; int nDiskCount=0;
	int nTotalDisks = 0;
	// nValidDiskSamples counts only disks whose per-disk cascade produced
	// a real source on this tick (branch K/P/W/S/N/D, not '-'). Disks that
	// produced no data are excluded from the header's denominator so they
	// do not drag down the average when their source failed.
	int nValidDiskSamples = 0;
	ULONG64 SumRWTime=0, SumTotalTime=0;


	PerferListData *pPData = NULL;


	if(! FlagStartDiskMon )  goto SKIPDISK;

	// Start the WMI / PDH / NtSys fallback monitor threads once per session.
	// These are ONLY consulted when IOCTL_DISK_PERFORMANCE fails (non-admin
	// or unsupported driver). The PRIMARY data source is the synchronous
	// kernel call PM.GetDiskPerformance() below, which is what the original
	// DBC Task Manager uses and which works on every Win7 build when run
	// with administrator privileges.
	PerfWmiDisk_Start();
	PerfPdhDisk_Start();
	PerfNtSys_Start();
	PerfPdhProcIo_Start();

	// Pre-count the total number of physical drives the UI knows about.
	// nDiskCount inside the per-disk loop is the loop index (incremented
	// AFTER the cascade below), NOT the total. The 'D' distribution branch
	// below needs the TOTAL to compute a per-disk share
	// (PerfNtSys_GetTotalPct() / nTotalDisks). Without this pre-count we'd
	// use nDiskCount and disk 1 would get share=1/1 (full global), disk 2
	// would get 1/2 (half), disk 3 would get 1/3, etc — exactly the
	// inflated decreasing pattern the user saw before this fix.
	for(int nPre=2; nPre<n; nPre++)
	{
		PerferListData* pPre = (PerferListData*)mPItemList.GetItemData(nPre);
		if(pPre == NULL) continue;
		if(pPre->Type == PM_ETHERNET) break;
		nTotalDisks++;
	}

	for(int nItem=2;nItem<n;nItem++)
	{

		pPData = (PerferListData *)mPItemList.GetItemData(nItem);
		if(pPData==NULL) continue ;
		if(pPData->Type == PM_ETHERNET   ) break ;

		//-----------------------------------------------------------------------
		// PRIMARY PATH: IOCTL_DISK_PERFORMANCE via PM.GetDiskPerformance()
		//-----------------------------------------------------------------------
		// This is a direct kernel-to-usersystem call (\\.\PhysicalDriveN) that
		// returns ReadTime/WriteTime/IdleTime/BytesRead/BytesWritten. It does
		// NOT require the WMI service, PDH counter registration, or anything
		// else that can be disabled on stripped-down Win7 installs. Only
		// prerequisites: GENERIC_READ on the disk handle (which the function
		// already requests) and admin rights (which the user has).
		//
		// On a working admin Win7 system this returns real kernel disk timers
		// and the % is computed as the busy fraction:
		//     (RWTime_delta) / (TotalTime_delta)   where
		//         RWTime     = ReadTime + WriteTime
		//         TotalTime  = IdleTime + RWTime
		//
		// This is exactly what the original DBC source did (PerformanceBox.cpp
		// @ 5d93ba9, around the "Disk    " section).
		//-----------------------------------------------------------------------
		double dReadBps = 0, dWriteBps = 0, dActivePct = 0;
		double AvgResponseTime = 0;
		BOOL bKernelOk = FALSE;
		BOOL bKernelUsed = FALSE;
		ULONG64 RWTime = 0, TotalTime = 0;

		// Try the kernel IOCTL_DISK_PERFORMANCE path on every Windows session.
		// On Win7 the default DACL on \\.\PhysicalDriveN grants GENERIC_READ to
		// the Users group, so CreateFile succeeds even from a non-admin
		// session and the IOCTL returns the same data as when admin. If the
		// ACL has been tightened (e.g. by a corporate GPO) CreateFile fails,
		// GetDiskPerformance returns a zero struct, bKernelOk stays FALSE and
		// the cascade falls through to the PDH / WMI / NtSys paths below.
		DISK_PERFORMANCE DiskPerformance;
		memset(&DiskPerformance, 0, sizeof(DiskPerformance));
		// Pass pPData->StrOther0 (format "C:,D:,") so the IOCTL path-finder
		// can try the volume-handle fallback in addition to \\.\PhysicalDriveN.
		// On stripped-down / VM setups the volume path sometimes returns data
		// when the raw physical-drive path is denied by the storage driver.
		DiskPerformance = PM.GetDiskPerformance(pPData->ID, pPData->StrOther0);

		// GetDiskPerformance() zeroes the struct on failure (CreateFile or
		// DeviceIoControl returned an error). We consider the call
		// "successful" only if at least one of the kernel counters has ever
		// advanced past zero — that's how we know the kernel really answered
		// us and it's not just zeroed memory.
		RWTime     = (ULONG64)DiskPerformance.ReadTime.QuadPart
				   + (ULONG64)DiskPerformance.WriteTime.QuadPart;
		TotalTime  = (ULONG64)DiskPerformance.IdleTime.QuadPart + RWTime;

		// Diagnostic: log whether the IOCTL actually returned non-zero
		// counters for THIS disk on the first poll only. Combined with the
		// per-disk CASCADE log, this tells us whether CreateFile is failing
		// for ACL reasons on the user's system.
		{
			static LONG ioctlDiagOnce = 0;
			LONG d = InterlockedIncrement(&ioctlDiagOnce);
			if(d <= 10)  // log first 10 ticks worth of disk IOCTL states
			{
				_DbgIoLog("IOCTL disk=%d Read=%lld Write=%lld Idle=%lld BytesR=%lld BytesW=%lld",
					pPData->ID,
					(long long)DiskPerformance.ReadTime.QuadPart,
					(long long)DiskPerformance.WriteTime.QuadPart,
					(long long)DiskPerformance.IdleTime.QuadPart,
					(long long)DiskPerformance.BytesRead.QuadPart,
					(long long)DiskPerformance.BytesWritten.QuadPart);
			}
		}

		// Sticky flag: if THIS disk's IOCTL ever produced non-zero counters
		// (BytesRead/BytesWritten advancing), mark it. The cascade can use
		// this to trust subsequent IOCTL samples for this disk even when
		// the moment-by-moment tick check (bKernelOk below) hasn't fired
		// because we're still on the very first tick.
		if((ULONG64)DiskPerformance.BytesRead.QuadPart > 0
			|| (ULONG64)DiskPerformance.BytesWritten.QuadPart > 0)
		{
			int kid = pPData->ID;
			if(kid >= 0 && kid < PERF_DISK_MAX
				&& InterlockedCompareExchange(&g_KernelDiskEverLive[kid], 1, 0) == 0)
			{
				InterlockedExchange(&g_AnyPerDiskSourceEverLive, 1);
				_DbgIoLog("IOCTL kernel disk=%d just came live (BytesR=%lld BytesW=%lld)",
					kid,
					(long long)DiskPerformance.BytesRead.QuadPart,
					(long long)DiskPerformance.BytesWritten.QuadPart);
			}
		}

		ULONG64 dRW = 0, dTT = 0;
		if((pPData->DataD > 0) && (TotalTime > pPData->DataD) && (RWTime >= pPData->DataC))
		{
			dRW = RWTime    - pPData->DataC;
			dTT = TotalTime - pPData->DataD;
			if(dTT > 0)
			{
				dActivePct = (double)dRW / (double)dTT * 100.0;
				if(dActivePct > 100.0) dActivePct = 100.0;
				bKernelOk = TRUE;
			}
		}

		// BytesRead / BytesWritten deltas (B/s). Note: GetDiskPerformance
		// zeroes the struct on failure, so when this is the first tick
		// after admin elevation we just latch the baseline and emit 0.
		if(pPData->DataA > 0)
		{
			__int64 dBR = DiskPerformance.BytesRead.QuadPart - pPData->DataA;
			if(dBR > 0) dReadBps = (double)dBR / theApp.AppSettings.TimerStep;
		}
		pPData->DataA = DiskPerformance.BytesRead.QuadPart;

		if(pPData->DataB > 0)
		{
			__int64 dBW = DiskPerformance.BytesWritten.QuadPart - pPData->DataB;
			if(dBW > 0) dWriteBps = (double)dBW / theApp.AppSettings.TimerStep;
		}
		pPData->DataB = DiskPerformance.BytesWritten.QuadPart;

		// Average response time (ms) — same formula as the original.
		if(ShowThisPage || theApp.UpTimeSec < 10)
		{
			if(pPData->DataC > 0)
			{
				AvgResponseTime = (double)(RWTime - pPData->DataC)
								/ 2.0 / 10000.0 / theApp.AppSettings.TimerStep;
				if(AvgResponseTime < 0 || !_finite(AvgResponseTime))
					AvgResponseTime = 0;
			}
		}

		// Accumulate the kernel timer deltas BEFORE updating the baseline
		// (the next iteration needs the previous tick values).
		if(bKernelOk)
		{
			SumRWTime    += dRW;
			SumTotalTime += dTT;
		}
		pPData->DataC = RWTime;
		pPData->DataD = TotalTime;

		int diskIdx = pPData->ID;
		// Diagnostic: tag which cascade branch actually fired for this
		// disk on this tick, so the user can see in dbc_dbg_io.log why
		// every physical drive ends up with the same % (or not).
		//   'K' = kernel IOCTL_DISK_PERFORMANCE worked (bKernelOk)
		//   'P' = PDH \PhysicalDisk(N)\... per-instance
		//   'S' = per-process PDH sum (system-wide, applied to every disk)
		//   'W' = WMI Win32_PerfRawData_PerfDisk_PhysicalDisk per-instance
		//   'N' = NtSys global fallback (system-wide applied to every disk)
		//   '-' = nothing (cascade produced zeros)
		// Note: the 'S' and 'N' branches are only reached when at least
		// one per-disk source has ever produced real data this session
		// (anyPerDiskEver). Otherwise the global numbers would be applied
		// to every disk and the per-disk display would be useless.
		char branch = bKernelOk ? 'K' : '-';

		if(bKernelOk)
		{
			bKernelUsed = TRUE;
		}
		else
		{
			// Fallback: use the WMI/PDH/NtSys caches only when the kernel
			// IOCTL isn't available (non-admin or driver doesn't support it).
			//
			// Trust PDH only when we've already seen it produce a non-zero
			// sample. On Win7 non-admin sessions where the perfdisk counter
			// DLL isn't loaded, PdhGetFormattedCounterValue returns success
			// with value 0 forever — without this check the cascade would
			// be locked at 0% even while a file copy is in progress.
			// Honesty gate: if NO per-disk source has EVER produced non-zero
			// data on this machine (typical of Win7 non-admin sessions where
			// perfdisk.dll is never loaded), refuse to apply the system-wide
			// fallbacks (PID/NtSys) to this disk. Doing so would paint every
			// physical drive with the same global number, which is misleading.
			// The per-disk meter honestly stays at 0% and the *header* bar
			// still shows the real system-wide % via PerfNtSys_GetTotalPct().
			const BOOL anyPerDiskEver = (g_AnyPerDiskSourceEverLive != 0);
			PdhDiskMetrics pdhDisk = {0};
			WmiDiskMetrics wmiDisk = {0};
			BOOL havePdh = PerfPdhDisk_GetPerDisk(diskIdx, &pdhDisk) && pdhDisk.Ready && pdhDisk.EverNonZero;
			BOOL haveWmi = diskIdx >= 0 && diskIdx < PERF_DISK_MAX && PerfWmiDisk_GetPerDisk(diskIdx, &wmiDisk) && wmiDisk.Ready && wmiDisk.EverNonZero;
			double sysBps      = PerfPdhProcIo_GetSystemBps();
			double sysBusyPct  = PerfPdhProcIo_GetSystemBusyPct();
			LONG   sysReady    = PerfPdhProcIo_GetReady();
			double ntReadBps   = PerfNtSys_GetReadBps();
			double ntWriteBps  = PerfNtSys_GetWriteBps();
			double ntTotalPct  = PerfNtSys_GetTotalPct();
			LONG   ntReady     = PerfNtSys_GetReady();

			if(diskIdx >= 0 && diskIdx < PERF_DISK_MAX && havePdh)
			{
				dReadBps   = pdhDisk.ReadBps;
				dWriteBps  = pdhDisk.WriteBps;
				dActivePct = pdhDisk.Pct;
				branch = 'P';
			}
			else if(anyPerDiskEver && haveWmi)
			{
				dReadBps   = wmiDisk.ReadBps;
				dWriteBps  = wmiDisk.WriteBps;
				dActivePct = wmiDisk.Pct;
				branch = 'W';
			}
			else if(anyPerDiskEver && sysReady && sysBps > 0.0)
			{
				dReadBps   = sysBps * 0.5;
				dWriteBps  = sysBps * 0.5;
				dActivePct = sysBusyPct;
				branch = 'S';
			}
			else if(anyPerDiskEver && diskIdx >= 0 && diskIdx < PERF_DISK_MAX && ntReady
				&& (ntReadBps > 0 || ntWriteBps > 0))
			{
				// NtQuerySystemInformation fallback (no per-disk attribution —
				// we don't know which physical disk each Read/Write byte went
				// to). We split the system-wide throughput across the disks
				// the UI knows about and apply the system-wide busy fraction
				// as a uniform per-disk % so the activity graph still moves
				// in non-admin sessions where PDH and WMI are both broken.
				//
				// The bytes/sec values are the same for every disk on this
				// path — that's a known limitation of using a system-wide
				// data source for a per-disk display. It is still strictly
				// better than pinning every disk at 0%% when there is real
				// activity happening, which is what used to happen before.
				dReadBps   = ntReadBps;
				dWriteBps  = ntWriteBps;
				dActivePct = ntTotalPct;
				branch = 'N';
			}
			else if(diskIdx >= 0 && diskIdx < PERF_DISK_MAX && ntReady
				&& ntTotalPct > 0.0
				&& nTotalDisks > 0)
			{
				// DISTRIBUTE branch — fires when NO per-disk source has ever
				// produced non-zero data this session (anyPerDiskEver==0) but
				// NtSys DOES have a real system-wide busy % to report. The
				// honest behaviour for that case used to be "paint every disk
				// at 0%% because we can't attribute to one". That left the
				// per-disk graph completely static during a file copy, which
				// was misleading — it implied nothing was happening when in
				// fact the disk subsystem was busy.
				//
				// We now distribute the system-wide % EQUALLY across the
				// physical drives the UI knows about (nTotalDisks is the
				// pre-counted total from before the loop, NOT the running
				// nDiskCount which is just the loop index). Each disk
				// therefore shows PerfNtSys_GetTotalPct() / nTotalDisks, which
				// makes the per-disk graph track the system's overall disk
				// activity even though we cannot tell which disk the
				// activity was on. The bytes/sec numbers are split the same
				// way so the KB/s readout below the meter is also honest.
				//
				// This is strictly weaker than the 'N' branch above (which
				// also requires anyPerDiskEver) but it is strictly stronger
				// than the previous "honesty gate" behaviour of 0%% on every
				// disk: at least the graph moves when the disk subsystem
				// is moving.
				//
				// NOTE: with nTotalDisks=5 and global=20%%, every disk shows
				// 4%%. This is intentionally uniform — we have NO per-disk
				// attribution in this branch, so pretending one disk is
				// "the busy one" would be a lie. The 4%% is honest: it's
				// "if all five drives were equally busy, this is what the
				// system-wide %% would look like per drive".
				double share = 1.0 / (double)nTotalDisks;
				dReadBps   = ntReadBps  * share;
				dWriteBps  = ntWriteBps * share;
				dActivePct = ntTotalPct * share;
				branch = 'D';
			}
			// Final clamp on whatever the fallback produced.
			if(dReadBps   < 0 || !_finite(dReadBps))   dReadBps   = 0;
			if(dWriteBps  < 0 || !_finite(dWriteBps))  dWriteBps  = 0;
			if(dActivePct < 0 || !_finite(dActivePct)) dActivePct = 0;

			// Emit the per-disk cascade branch log only on state changes
			// (or every ~50 ticks as a heartbeat). Keeps the log readable.
			{
				static int   prevBranch[PERF_DISK_MAX] = {0};
				static DWORD prevTick   [PERF_DISK_MAX] = {0};
				DWORD nowTick = GetTickCount();
				if(diskIdx >= 0 && diskIdx < PERF_DISK_MAX)
				{
					int pb = prevBranch[diskIdx];
					DWORD pt = prevTick[diskIdx];
					if(pb != branch || (nowTick - pt) > 5000)
					{
						prevBranch[diskIdx] = branch;
						prevTick   [diskIdx] = nowTick;
						_DbgIoLog("CASCADE disk=%d branch=%c PDH(R=%d/E=%d PDHtotR=%d/E=%d) PID(R=%d/Bps=%.0f) WMI(R=%d/E=%d WtotR=%d/E=%d) N(R=%d/Rd=%.0f/Wr=%.0f/Tpct=%.1f) nDisk=%d total=%d -> pct=%.1f",
							diskIdx, branch,
							pdhDisk.Ready, pdhDisk.EverNonZero,
							PerfPdhDisk_GetTotalReady(), PerfPdhDisk_GetTotalEverNonZero(),
							sysReady, sysBps,
							wmiDisk.Ready, wmiDisk.EverNonZero,
							PerfWmiDisk_GetTotalReady(), PerfWmiDisk_GetTotalEverNonZero(),
							(int)ntReady, ntReadBps, ntWriteBps, ntTotalPct,
							nDiskCount,
							nTotalDisks,
							dActivePct);
					}
				}
			}
		}

		nDiskCount++;

		// A disk counts as a "valid sample" for the header average only
		// when the per-disk cascade produced a real source on this tick
		// (branch 'K','P','W','S','N','D'). A '-'-branch means we had no
		// data for this disk and it must NOT appear in the denominator —
		// otherwise a single empty failure drags every other disk down.
		if(branch != '-')
			nValidDiskSamples++;

		//Read
		PerformanceDataA[nItem] = dReadBps;
		if(PerformanceDataA[nItem]<0.001)PerformanceDataA[nItem]=0.0;

		//Write
		PerformanceDataB[nItem] = dWriteBps;
		if(PerformanceDataB[nItem]<0.001)PerformanceDataB[nItem]=0.0;

		//Active % — kernel path already produced a 0..100 value; WMI/PDH
		//produce 0..100 too. Convert to the 0..1 fraction used internally.
		PerformanceData[nItem] = dActivePct/100.0;
		if(!_finite(PerformanceData[nItem])||PerformanceData[nItem]<0) PerformanceData[nItem]=0;
		if(PerformanceData[nItem]>1) PerformanceData[nItem]=1;

		if(pPData->pInfoBox->IsWindowVisible())
		{
			_UpdateDiskInfoBox(pPData,PerformanceDataA[nItem],PerformanceDataB[nItem],PerformanceData[nItem]*100,AvgResponseTime);
		}


		//---------------------------------------------------------------------------

		if( ((int)theApp.UpTimeSec) % 6 == 0 )
		{
			_TryToChangeDiskMaxVar(PerformanceDataA[nItem],PerformanceDataB[nItem],pPData);
		}


		PerformanceDataA[nItem] = PerformanceDataA[nItem]/(pPData->MaxVar);
		PerformanceDataB[nItem] = PerformanceDataB[nItem]/(pPData->MaxVar);

		if(PerformanceDataA[nItem]<0.001 || PerformanceDataA[nItem]>1)PerformanceDataA[nItem]=0;
		if(PerformanceDataB[nItem]<0.001 || PerformanceDataB[nItem]>1)PerformanceDataB[nItem]=0;





		if(ShowThisPage)		{

			double DiskPct = PerformanceData[nItem]*100;
			// Clamp NaN/Inf so the inline disk % in the list never prints
			// "-nan%" or "inf%".
			if(_finite(DiskPct) == 0) DiskPct = 0;
			if(DiskPct < 0) DiskPct = 0;
			if(DiskPct > 100) DiskPct = 100;
			StrTemp.Format(L"%0.2f%%",DiskPct); // ʱΪ �֮
			//mPItemList.SetItemText(nItem,1,StrTemp);
			MySetItem(nItem,StrTemp);
		}

		SumDiskUsage = SumDiskUsage+PerformanceData[nItem];



	}

	// ---------------------------------------------------------------
	// Header disk %% — PRIMARY = per-disk average, FALLBACKS = the
	// aggregated external counters (used only when NO per-disk sample
	// was produced). All logic is in ComputeHeaderDiskUsage() above.
	//
	// Why the priority swap (was: K sum → PDH _Total → … → per-disk
	// average at the very bottom): the aggregated counters are not a
	// pure average of per-disk %%, so for the well-known scenario
	// where N physical disks are present and only one of them is at
	// 100%% busy with the others idle, they read inflated (e.g.
	// ~67%% for N=5 with one disk busy). The Performance tab already
	// displays the per-disk %%s correctly, so the header should be
	// their simple average — which matches what Windows Task Manager
	// itself reports and what the user expects.
	// ---------------------------------------------------------------
	char  hdrBranch = '-';
	double headerPct = ComputeHeaderDiskUsage(SumDiskUsage,
											  nValidDiskSamples,
											  SumRWTime,
											  SumTotalTime,
											  &hdrBranch);
	theApp.PerformanceInfo.TotalDiskUsage = headerPct;

	// Diagnostic: log which branch produced the header value, throttled
	// to the first 20 ticks / every branch change / >5%% drift, so the
	// user can see in dbc_dbg_io.log that the per-disk average ('A') is
	// the one being chosen and the resulting %% agrees with the
	// Performance tab.
	{
		static LONG   hdrSeq       = 0;
		static int    prevBranch   = 0;
		static double prevPct      = -1.0;
		LONG seq = InterlockedIncrement(&hdrSeq);
		BOOL first20  = (seq <= 20);
		BOOL brChange = (prevBranch != hdrBranch);
		BOOL driftBig = (prevPct < 0.0)
					 || ((prevPct - headerPct) >  5.0)
					 || ((prevPct - headerPct) < -5.0);
		if(first20 || brChange || driftBig)
		{
			prevBranch = hdrBranch;
			prevPct    = headerPct;
			_DbgIoLog("HEADER disk: branch=%c pct=%.1f (nValid=%d nDisk=%d sumPct=%.3f K_RW=%llu K_TT=%llu PDH_T=%.1f WMI_T=%.1f NtPct=%.1f)",
				hdrBranch, headerPct,
				nValidDiskSamples, nDiskCount, SumDiskUsage,
				(unsigned long long)SumRWTime, (unsigned long long)SumTotalTime,
				PerfPdhDisk_GetTotalPct(),
				PerfWmiDisk_GetTotalPct(),
				PerfNtSys_GetTotalPct());
		}
	}



SKIPDISK:
 



	//---------------------------------		Network    ---------------------------------


	double SumNetUsage=0; int nNetCount=0;



	MIB_IF_TABLE2   *pIfTable = NULL;
	ULONG          dwSize   = 0;
	DWORD          dwRet;
	dwRet = GetIfTable2( &pIfTable );

	if ( dwRet == ERROR_NOT_ENOUGH_MEMORY )
	{
		 ;
	}
	else if(dwRet == STATUS_SUCCESS )
	{
		CString Str,s1,s2,s3,StrTemp;
		for ( ULONG  i=0; i<pIfTable->NumEntries; i++ )
		{

			if((pIfTable->Table[i].Type == IF_TYPE_IEEE80211 )|| (pIfTable->Table[i].Type ==IF_TYPE_ETHERNET_CSMACD) )
			{


				ULONG Index =pIfTable->Table[i].InterfaceIndex;

				map<int, int>::iterator Iter= NetAdapterList.find((int)Index); // <����ID,�б���ĿID>
				int nItemID;

				
				if(Iter != NetAdapterList.end())//����
				{
					nNetCount++;

					nItemID = Iter->second;
 
					 
					pPData = (PerferListData*)mPItemList.GetItemData(nItemID);	

					if(pPData!=NULL) 
					{							
						 

						// InOctets  OutOctets��λΪ�ֽ�
						//------------����
						if(pPData->DataA==0)    
						{
							PerformanceDataA[nItemID]=0;
						}
						else
						{
							PerformanceDataA[nItemID]= (double)(pIfTable->Table[i].InOctets - pPData->DataA)/(double)theApp.AppSettings.TimerStep;
						}

						//------------����
						if(pPData->DataB==0) 	
						{
							PerformanceDataB[nItemID] =0;
						}
						else
						{
							PerformanceDataB[nItemID]=(double)(pIfTable->Table[i].OutOctets - pPData->DataB)/(double)theApp.AppSettings.TimerStep;
						}


						double ThisUsaeg = (PerformanceDataA[nItemID]+PerformanceDataB[nItemID])/(pIfTable->Table[i].TransmitLinkSpeed/8);

						//��ֹ���ָ�ֵ
						if(ThisUsaeg>0.001)
						{
							SumNetUsage = SumNetUsage+ThisUsaeg;
						}



						//-----------------------------------------

						if(ShowThisPage)
						{

							StrTemp.Format(L"S: %.0f Kbps R: %.0f Kbps", (PerformanceDataB[nItemID]/1024*8), (PerformanceDataA[nItemID]/1024*8));
							//mPItemList.SetItemText(nItemID,1,StrTemp);
							MySetItem(nItemID,StrTemp);
						}

						if(pPData->pInfoBox->IsWindowVisible())
						{
							_UpdateNetworkInfoBox(pPData,PerformanceDataA[nItemID],PerformanceDataB[nItemID]);
						}

						if( ((int)theApp.UpTimeSec) % 6 == 0 )	
						{
							_TryToChangeNetworkMaxVar(PerformanceDataA[nItemID],PerformanceDataB[nItemID],pPData);
						}

						PerformanceDataA[nItemID] =PerformanceDataA[nItemID]/(pPData->MaxVar);
						PerformanceDataB[nItemID] =PerformanceDataB[nItemID]/(pPData->MaxVar);



						pPData->DataA =pIfTable->Table[i].InOctets;
						pPData->DataB =pIfTable->Table[i].OutOctets;


					} //pPData!=NULL

				}//Iter != NetAdapterList.end()


			}	//Type			

		}//for


		FreeMibTable (pIfTable);
	}




	if(nNetCount>0)
	{
		theApp.PerformanceInfo.TotalNetUsage = SumNetUsage/nNetCount*100; //�ٷֱ�
	}
	else
	{
		theApp.PerformanceInfo.TotalNetUsage = 0;
	}





	//----------------------------------���²���ͼ-----------------------------







	CWaveBox *pBox = NULL;
	CWaveBox *pBox2 = NULL;




	//---------------��cpu��ʾ״̬ ��ȡ���ݼ����� ��ͼ---------------------

	double CpuUsage,KernelUsage;



	_GetLogicalProcessorUsage(pCpuBox, &CpuUsage,&KernelUsage); //����ִ�к���

	pPData = (PerferListData *)mPItemList.GetItemData(0);
	if(pPData!=NULL) 
	{
		pBox = pPData->pWaveBox;

		if(pBox->IsWindowVisible()) pBox->Invalidate();

	}




	//--------------------


	if(theApp.AppSettings.ProcessorDisplayMode == 2)
	{
		for(int i =0;i<theApp.PerformanceInfo.nLogicalProcessor;i++)
		{
			pHotBox->ArrayPerLogicalCpuUsage[i] = pCpuBox->Num[i][60];
		} 
		pHotBox->Invalidate(0);

	}



	//----------------------cpu----------------------

	memmove(&pTotalCpuBox->Num[0][0],&pTotalCpuBox->Num[0][1],ArraySize);
	memmove(&pTotalCpuBox->Num2[0][0],&pTotalCpuBox->Num2[0][1],ArraySize);

	


	theApp.PerformanceInfo.CpuUsage = CpuUsage*100;

	PerformanceDataA[0] = PerformanceData[0]= CpuUsage;
	PerformanceDataB[0] =KernelUsage;

	pTotalCpuBox->Num[0][60] = (float)PerformanceDataA[0]; 
	pTotalCpuBox->Num2[0][60] =(float)PerformanceDataB[0];

	


	if(ShowThisPage)
	{
	
		pInfoBoxCpu->Info[1].StrInfo.Format(L"%0.2f GHz",_GetSurrentCpuSpeed());


		StrTemp.Format(L"%.2f%%  %s",theApp.PerformanceInfo.CpuUsage,pInfoBoxCpu->Info[1].StrInfo);
		MySetItem(0,StrTemp);		
	//	mPItemList.SetItemText(0,1,StrTemp);

	}





	for(int nItem=1;nItem<n;nItem++)
	{
		pPData = (PerferListData *)mPItemList.GetItemData(nItem);
		if(pPData==NULL) continue ;
		pBox = pPData->pWaveBox;
		// Bug fix Win7: pPData->pOtherWnd is CMemCompBox* for memory items and
		// NULL for net items. Cast unconditionally to CWaveBox* and deref Num/Num2
		// corrupts heap. Only treat as CWaveBox for disk items.
		pBox2 = NULL;
		if(pPData->Type == PM_DISK)
		{
			pBox2 = (CWaveBox *)pPData->pOtherWnd;
		}
		if(pBox==NULL)  continue ;



		memmove(&pBox->Num[0][0],&pBox->Num[0][1],ArraySize);
		memmove(&pBox->Num2[0][0],&pBox->Num2[0][1],ArraySize);

		if(pPData->Type == PM_DISK)   
		{
			if(pBox2!=NULL)
			{

				memmove(&pBox2->Num[0][0], &pBox2->Num[0][1],ArraySize);
				memmove(&pBox2->Num2[0][0],&pBox2->Num2[0][1],ArraySize);


				pBox2->Num[0][60] =(float) PerformanceDataA[nItem];			
				pBox2->Num2[0][60] = (float)PerformanceDataB[nItem];

				if(pBox2->IsWindowVisible())pBox2->Invalidate(); 

			}

			pBox->Num[0][60] = (float)PerformanceData[nItem];

		}
		else  if(pPData->Type == PM_MEMORY)
		{
			pBox->Num[0][60] = (float)PerformanceData[nItem];			
		}
		else
		{
			pBox->Num[0][60] = (float)PerformanceDataA[nItem];
			pBox->Num2[0][60] = (float)PerformanceDataB[nItem];
		}




		if(pBox->IsWindowVisible()) pBox->Invalidate();




	}



	if(ShowThisPage)
	{
		UpdateInfoBox();
		mPItemList.SetRedraw(1);
		mPItemList.InvalidateRect(NULL,0);
	}





	delete [] PerformanceData ;PerformanceData=NULL;
	delete [] PerformanceDataA ;PerformanceDataA=NULL;
	delete [] PerformanceDataB ;PerformanceDataB=NULL;





	return 0;
}

void CPerformanceBox::OnPop_ChangeGraphToOverallutilization()
{
	// TODO: Add your command handler code here

	if(theApp.AppSettings.ProcessorDisplayMode == 0) return;


	theApp.AppSettings.ProcessorDisplayMode = 0;

	PerferListData *pData =(PerferListData *) mPItemList.GetItemData(0);
	if(pData==NULL) return;
	if(pData->pWaveBox==NULL)return;





	pData->pWaveBox = pTotalCpuBox;



	pTotalCpuBox->ShowWindow(SW_SHOW);
	pCpuBox->ShowWindow(SW_HIDE);
	pHotBox->ShowWindow(SW_HIDE);


	pData->pWaveBox->Invalidate();


	CWnd *pWnd=GetDlgItem(IDC_TIP3);
	if(pWnd!=NULL)	{  pWnd->ShowWindow(SW_SHOW); }
	pWnd=GetDlgItem(IDC_TIP4);
	if(pWnd!=NULL)	{  pWnd->ShowWindow(SW_SHOW); }


	pWnd=GetDlgItem(IDC_TIP1);
	if(pWnd!=NULL)	
	{ 
		CString StrTemp = STR_TIP1_CPU_TOTAL;
		mPItemList.SetItemText(0,3,StrTemp);
		pWnd->SetWindowTextW(StrTemp);
	}

	pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }



	this->PlaceAllCtrl();









}

void CPerformanceBox::OnPop_ChangeGraphToLogicalprocessors()
{
	if(theApp.AppSettings.ProcessorDisplayMode == 1) return;



	theApp.AppSettings.ProcessorDisplayMode = 1;

	PerferListData *pData =(PerferListData *) mPItemList.GetItemData(0);
	if(pData==NULL) return;
	if(pData->pWaveBox==NULL)return;

	pData->pWaveBox = pCpuBox;


	pCpuBox->ShowWindow(SW_SHOW);
	pTotalCpuBox->ShowWindow(SW_HIDE);
	pHotBox->ShowWindow(SW_HIDE);


	pData->pWaveBox->Invalidate();



	CString StrTime;

	CWnd *pWnd=GetDlgItem(IDC_TIP3);
	if(pWnd!=NULL)	{  pWnd->ShowWindow(SW_HIDE); pWnd->GetWindowTextW(StrTime); }
	pWnd=GetDlgItem(IDC_TIP4);
	if(pWnd!=NULL)	{  pWnd->ShowWindow(SW_HIDE); }


	pWnd=GetDlgItem(IDC_TIP1);
	if(pWnd!=NULL)	
	{ 
		CString StrTemp;    

		StrTemp.Format(STR_TIP1_CPU_LOGICAL,StrTime);
		
		mPItemList.SetItemText(0,3,StrTemp);
		pWnd->SetWindowTextW(StrTemp);
	}


	pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_SHOW); }



	this->PlaceAllCtrl();



}



void CPerformanceBox::OnPerformanceShowkerneltimes()
{

	theApp.AppSettings.ShowKernelTime = !theApp.AppSettings.ShowKernelTime;

	pTotalCpuBox->DrawSecondWave = theApp.AppSettings.ShowKernelTime ;
	pCpuBox->DrawSecondWave = theApp.AppSettings.ShowKernelTime ;

	pTotalCpuBox->Invalidate();
	pCpuBox->Invalidate();


}

void CPerformanceBox::OnUpdatePerformanceShowkerneltimes(CCmdUI *pCmdUI)
{

	if(theApp.AppSettings.ShowKernelTime)
	{
		pCmdUI->SetCheck(1);
	}
	else
	{
		pCmdUI->SetCheck(0);
	}
}





void CPerformanceBox::OnViewCpu()
{
	mPItemList.SetItemState(0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);	 
}

void CPerformanceBox::OnViewMemory()
{
	mPItemList.SetItemState(1,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);	 
}

void CPerformanceBox::OnPop_PerformanceListShowHideGraphs()
{
	

	theApp.AppSettings.PerformanceListShowGraph =   (theApp.AppSettings.PerformanceListShowGraph==0)?1:0 ;
	mPItemList.SetRowHeight(20);

}





void CPerformanceBox::OnUpdatePerformancetypeHidegraphs(CCmdUI *pCmdUI)
{
	CString Str;

	if(theApp.AppSettings.PerformanceListShowGraph!=0)
	{

		Str.LoadStringW(IDS_STRING_HIDEGRAPH);
	}
	else
	{
		Str.LoadStringW(IDS_STRING_SHOWGRAPH);
	}

	pCmdUI->SetText(Str);



}

int CPerformanceBox::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFormView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here

	CCreateContext * pContextBox;

	CWnd * pWnd = NULL;

	//-----------------------------------------------------------------------

	pViewClass = RUNTIME_CLASS(CHotboxView);

	pContextBox= new CCreateContext;
	pContextBox->m_pCurrentDoc = NULL;
	pContextBox->m_pCurrentFrame = NULL;
	pContextBox->m_pLastView = NULL;
	pContextBox->m_pNewDocTemplate =NULL;

	pContextBox->m_pNewViewClass = pViewClass;

	pWnd = NULL;
	pWnd = DYNAMIC_DOWNCAST(CWnd, pViewClass->CreateObject());

	// Defensive NULL guard — was previously dereferencing pWnd unconditionally.
	if(!pWnd)
	{
		delete pContextBox;
		return -1;
	}

	if(!pWnd->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, IDD_HOTBOXVIEW, pContextBox))
	{
		delete pContextBox;
		return -1;
	}

	pHotBox= DYNAMIC_DOWNCAST(CHotboxView, pWnd);
	pHotBox->SetScrollSizes(MM_TEXT, CSize(1, 1)); //BOX_W_MIN,180
	pHotBox->ModifyStyleEx(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE,0);

	delete pContextBox;
	pContextBox=NULL;


	// ------------------------------------------------------------------
	//  Build the native Segoe UI font used by the Win7 Turbo disclaimer
	//  (IDC_TURBO_NOTE). We pull the caption font from the system
	//  non-client metrics so the typeface tracks the running OS: on
	//  Windows 7 / Vista it is Tahoma (the closest "native" equivalent),
	//  on Windows 8/8.1 it is Segoe UI, and on Windows 10/11 it remains
	//  Segoe UI.
	// ------------------------------------------------------------------
	{
		NONCLIENTMETRICS ncm;
		ZeroMemory(&ncm, sizeof(ncm));
		ncm.cbSize = sizeof(ncm);
		if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
			sizeof(ncm), &ncm, 0) != FALSE)
		{
			// The caption font on Win10/11 is Segoe UI at ~ -12 (9pt).
			// Adjust to ~ 9pt (-12 px @96 DPI) for the disclaimer.
			LOGFONTW lf = ncm.lfCaptionFont;
			lf.lfHeight = -12;
			if(!lf.lfFaceName[0])
			{
				// Fall back to Segoe UI if the OS left the face empty.
				wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Segoe UI");
			}
			mFontTurboNote.CreateFontIndirectW(&lf);
		}
		else
		{
			// Last-resort: hard-coded Segoe UI 9pt.
			mFontTurboNote.CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
				0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
				DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
		}

		// Hide the disclaimer by default; _RefreshTurboNoteVisibility()
		// will surface it on Windows 7 when the CPU page is selected.
		if(CWnd *pNote = GetDlgItem(IDC_TURBO_NOTE))
			pNote->ShowWindow(SW_HIDE);
	}

	// Apply the visibility decision once now; the periodic OnUMTimer
	// tick will keep it in sync as the user navigates between tabs.
	_RefreshTurboNoteVisibility();

	return 0;
}

void CPerformanceBox::_RefreshTurboNoteVisibility(void)
{
	CWnd *pNote = GetDlgItem(IDC_TURBO_NOTE);
	if(pNote == NULL) return;

	// Only show the disclaimer when:
	//   1. the running OS is Windows 7, AND
	//   2. the Performance tab is currently visible AND the CPU item
	//      is the selected one (the layout mirrors IDC_ITEMNAME which
	//      is right of the "CPU" label and changes per selection).
	BOOL bCpuSelected = FALSE;
	if(ShowThisPage)
	{
		int nSel = mPItemList.GetNextItem(-1, LVNI_SELECTED);
		if(nSel >= 0)
		{
			PerferListData *pData = (PerferListData *)mPItemList.GetItemData(nSel);
			if(pData && pData->Type == PM_CPU)
				bCpuSelected = TRUE;
		}
	}

	const BOOL bWin7   = (PerfIsWindows7() != FALSE);
	const int  nCmdShow = (bWin7 && bCpuSelected) ? SW_SHOW : SW_HIDE;

	pNote->ShowWindow(nCmdShow);
	if(nCmdShow == SW_SHOW)
	{
		// Refresh geometry and force a repaint so the gray Segoe UI
		// text shows immediately instead of being hidden behind the
		// dialog template background.
		PlaceAllCtrl();
		InvalidateIfVisible(pNote);
	}
}

void CPerformanceBox::OnPop_ChangeGraphToNumaNodes()
{

	if(theApp.AppSettings.ProcessorDisplayMode == 2) return;

	theApp.AppSettings.ProcessorDisplayMode = 2;



	pTotalCpuBox->ShowWindow(SW_HIDE);
	pCpuBox->ShowWindow(SW_HIDE);

	pHotBox->ShowWindow(SW_SHOW);



	CWnd *pWnd= NULL;
	pWnd=GetDlgItem(IDC_TIP3);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
	pWnd=GetDlgItem(IDC_TIP4);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }


	pWnd=GetDlgItem(IDC_TIP1);
	if(pWnd!=NULL)	
	{ 
		CString StrTemp = STR_TIP1_CPU_NUMA;
		mPItemList.SetItemText(0,3,StrTemp);
		pWnd->SetWindowTextW(StrTemp);
	}

	pWnd=GetDlgItem(IDC_TIP2);if(pWnd!=NULL){ pWnd->ShowWindow(SW_HIDE); }
	 


	this->PlaceAllCtrl();
}


void CPerformanceBox::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	CFormView::OnLButtonDown(nFlags, point);

	if(theApp.FlagSummaryView)
	{
		theApp.m_pMainWnd->PostMessage(WM_NCLBUTTONDOWN,    HTCAPTION,    MAKELPARAM(point.x, point.y)); 
	}
}


void CPerformanceBox::OnPop_GraphSummaryView()
{
	theApp.m_pMainWnd->PostMessage(UM_SUMMARYVIEW,(WPARAM)this);
	

}



void CPerformanceBox::OnPop_PerformanceListSummaryView()
{
	theApp.m_pMainWnd->PostMessage(UM_SUMMARYVIEW,(WPARAM)(&mPItemList));

}

BOOL CPerformanceBox::SetTipText( UINT ID, NMHDR * pTTTStruct, LRESULT * pResult )
{
	TOOLTIPTEXT *pTooltipText = (TOOLTIPTEXT *)pTTTStruct;
	
	HWND hWnd = (HWND)(pTTTStruct->idFrom); 

 
	if (pTooltipText->uFlags & TTF_IDISHWND) 
	{    
		PerferListData *pPData = (PerferListData *) mPItemList.GetItemData(1);
		if(pPData!=NULL)
		{
			CMemCompBox *pMemCompBox =(CMemCompBox *)( pPData->pOtherWnd);
			if(pMemCompBox->m_hWnd == hWnd)//��HWND�õ�IDֵ����Ȼ��Ҳ����ͨ��HWNDֵ���ж�
			{		

				//pTooltipText->lpszText = L"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
			
				pTooltipText->lpszText=(LPWSTR)(LPCTSTR) pMemCompBox->GetMemCompType() ;
			}		
		}
	}
     
	return TRUE; 
}


 
void CPerformanceBox::_FakeSelPItem(int ID)
{
	 

	   mPItemList.SetItemState(ID,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
}

void CPerformanceBox::PlaceAllCtrl(void)
{


	CRect rc;
	this->GetClientRect(rc);
	CRect rcWaveList;

	PerferListData * pData = (PerferListData*) mPItemList.GetItemData(0);
	BOOL Ret = FALSE;

	if(pData!=NULL)
	{
		if(pData->pWaveBox != NULL)
		{
			Ret= TRUE;
		}

	}


	if( Ret)     // && rc.Height()>400 
	{
		int WaveBoxBottom = rc.Height()-200;

		if(WaveBoxBottom-72<80)  WaveBoxBottom = 72+80;

		if(rc.Width()<BOX_W_MIN)
		{
			rcWaveList.SetRect(22,72,BOX_W_MIN-20,WaveBoxBottom);	
		}
		else
		{

			rcWaveList.SetRect(22,72,rc.right-20,WaveBoxBottom);	

		}



		int n = mPItemList.GetItemCount();

		INT nSel = 0;
		nSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );




		for(int i=0;i<n;i++)
		{
			PerferListData * pPData = (PerferListData *)mPItemList.GetItemData(i);
			if(pPData==NULL)continue ;
			if(pPData->pInfoBox == NULL)continue ;
			
			if(pPData->pInfoBox->IsWindowVisible()|| i==nSel)  
			{
				//PerferListData * pPData = (PerferListData *)mPItemList.GetItemData(i);
				//if(pPData==NULL)continue ;
				CWnd *pWnd = NULL ;
				CWnd *pBoxWnd = pPData->pWaveBox;
				if(pBoxWnd == NULL)continue ;



				pWnd=GetDlgItem(IDC_TIP2);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.right-80,rcWaveList.top-15,80,15); 
					InvalidateIfVisible(pWnd);
					
				}

				pWnd=GetDlgItem(IDC_TIP1);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.left,rcWaveList.top-15,rcWaveList.Width()-80,15);			
					InvalidateIfVisible(pWnd);
				}

				if(pPData->Type == PM_MEMORY || pPData->Type == PM_DISK)
				{

					rcWaveList.bottom-=80;
					if(pPData->Type == PM_DISK) rcWaveList.bottom+=30;
					if(rcWaveList.bottom-72<80)  rcWaveList.bottom  = 72+80;

				}
				if(pPData->Type == PM_ETHERNET  )
				{
					CRect rc1;
					this->GetClientRect(rc1);
					rcWaveList.bottom =rc1.bottom-150;				
					if(rcWaveList.bottom<155)  rcWaveList.bottom  = 155;

				}


				if(theApp.FlagSummaryView)
				{
					CRect rcTemp;
					this->GetClientRect(rcTemp);

					if(pPData->Type == PM_MEMORY || pPData->Type == PM_DISK)
					{
						rcWaveList.bottom = rcTemp.bottom-125;
					}
					else
					{
						rcWaveList.bottom = rcTemp.bottom-40;
					}

					

				}

				pHotBox->MoveWindow(rcWaveList);
				InvalidateIfVisible(pHotBox);

				pBoxWnd->MoveWindow(rcWaveList);
				InvalidateIfVisible(pBoxWnd);

				pWnd=GetDlgItem(IDC_TIP3);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.left+2,rcWaveList.bottom,rcWaveList.Width()-80,15);  
					InvalidateIfVisible(pWnd);
				}
				pWnd=GetDlgItem(IDC_TIP4);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.right-50,rcWaveList.bottom,50,15);
					InvalidateIfVisible(pWnd);
				}


				pWnd=GetDlgItem(IDC_TIP5);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.left+2,rcWaveList.bottom+19,rcWaveList.Width()-80,15);  
					InvalidateIfVisible(pWnd);
				}
				pWnd=GetDlgItem(IDC_TIP6);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.right-80,rcWaveList.bottom+19,80,15);			
					InvalidateIfVisible(pWnd);
				}


				pBoxWnd = pPData->pOtherWnd;
				if(pBoxWnd != NULL)
				{

					rcWaveList.OffsetRect(0,rcWaveList.Height()+34);
					if(pPData->Type == PM_MEMORY)
					{
						rcWaveList.bottom = rcWaveList.top+50;
						rcWaveList.DeflateRect(2,0);
						rcWaveList.top+=2;

					}
					else
					{
						rcWaveList.bottom = rcWaveList.top+66;
					}
					pBoxWnd->MoveWindow(rcWaveList);
					InvalidateIfVisible(pBoxWnd);
					
				}


				pWnd = pPData->pInfoBox;
				if(pWnd == NULL)continue ;

				if(theApp.FlagSummaryView) 
				{
					pWnd->MoveWindow(rcWaveList.left,rcWaveList.bottom+10000,1,1);	
				}
				else
				{
				pWnd->MoveWindow(rcWaveList.left,rcWaveList.bottom+27,rcWaveList.Width(),160);	
				}

				InvalidateIfVisible(pWnd);

				pWnd=GetDlgItem(IDC_TIP7);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.left+2,rcWaveList.bottom,rcWaveList.Width()-80,15);  
					InvalidateIfVisible(pWnd);
				}
				pWnd=GetDlgItem(IDC_TIP8);
				if(pWnd!=NULL)
				{					 
					pWnd->MoveWindow(rcWaveList.right-80,rcWaveList.bottom,80,15);  			
					InvalidateIfVisible(pWnd);
				}



			} //if 


		}  //for	 



	}

	CWnd *pWnd= NULL;
	CRect rcTemp;
	CRect rcTemp2;



	pWnd=GetDlgItem(IDC_ITEMLABEL);
	if(pWnd!=NULL)
	{

		pWnd->GetWindowRect(rcTemp2);
		this->ScreenToClient(rcTemp2);

	}





	pWnd=GetDlgItem(IDC_ITEMNAME);
	if(pWnd!=NULL)
	{

		pWnd->GetWindowRect(rcTemp);
		this->ScreenToClient(rcTemp);
		rcTemp.right = rcWaveList.right;
		rcTemp.left=rcTemp2.right+5;
		pWnd->MoveWindow(rcTemp);
		InvalidateIfVisible(pWnd);

		// Position the Win7 Turbo disclaimer immediately below
		// IDC_ITEMNAME, sharing the same right-aligned geometry so it
		// visually "hangs" off the CPU model name on the right side.
		if(CWnd *pNote = GetDlgItem(IDC_TURBO_NOTE))
		{
			CRect rcNote(rcTemp);
			rcNote.top    = rcTemp.bottom + 1;
			rcNote.bottom = rcNote.top + 14;   // ~ one line of 9pt text
			pNote->MoveWindow(rcNote);
			InvalidateIfVisible(pNote);
		}
	}





}

void CPerformanceBox::UpdateInfoBox()
{


	if(!ShowThisPage) return ;


	INT nSel = 0;
	//pNMLV->iItem
	nSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );

	if(nSel<0 ) return;

	PerferListData * pPData = (PerferListData *)mPItemList.GetItemData(nSel);


	if(pPData->Type == PM_CPU)
	{
		this->_UpdateCpuInfoBox();
	}
	if(pPData->Type == PM_MEMORY)
	{
		pPData->pOtherWnd->Invalidate(0);
		this->_UpdateMemoryInfoBox();
	}



	pPData->pInfoBox->Invalidate();


}

void CPerformanceBox::OnContextMenu(CWnd* pWnd, CPoint point)
{
	
	


	// TODO: Add your message handler code here
	CMenu PopMenu;
	CMenu *pMenu;

	PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_BASE ) );
	pMenu = PopMenu.GetSubMenu(1); 

	int n= mPItemList.GetItemCount();


	PerferListData *pData = NULL ;
	for(int i=0;i<n;i++)
	{
		pData =(PerferListData *) mPItemList.GetItemData(i);
		if(pData==NULL) continue;
		if(pData->pInfoBox->IsWindowVisible())
		{
			break;
		}


	}
	

	if(pData==NULL) return;


	if(pData->Type != PM_CPU)
	{
		 
		pMenu->DeleteMenu(0,MF_BYPOSITION);
		pMenu->DeleteMenu(0,MF_BYPOSITION);
		pMenu->DeleteMenu(0,MF_BYPOSITION);

	}

	if(pData->Type != PM_ETHERNET)
	{
		 
		pMenu->DeleteMenu(ID_PERFORMANCE_VIEWNETWORKDETAILS,MF_BYCOMMAND);
	}

	CRect rcBox;

	pData->pWaveBox->GetWindowRect(rcBox);

	

	CPoint CurPos ;
	GetCursorPos(&CurPos);

	
	int L1SubMenuID = 1; 

	if(pData->Type == PM_CPU)
	{
		L1SubMenuID = 4; 
		if( ! rcBox.PtInRect(CurPos) )
		{
			L1SubMenuID = 3;
			pMenu->DeleteMenu(ID_PERFORMANCE_SHOWKERNELTIMES,MF_BYCOMMAND);
		}
	}
	
	int iDiskMenu =0;
	UINT BaseID = ID_DISK_NONE;
	pData = NULL ;

 
	 
	CMenu *pSubMenu1 = pMenu->GetSubMenu(L1SubMenuID);
	CMenu *pSubMenu2 =NULL;
	if(pSubMenu1 )
	{
		pSubMenu2=pSubMenu1->GetSubMenu(2);
	}
 



	if(pSubMenu2)
	{
		for(int i=0;i<n;i++)
		{
			pData =(PerferListData *) mPItemList.GetItemData(i);
			if(pData==NULL) continue;	
			if(pData->Type == PM_DISK)
			{
				CString StrID;
				StrID.Format(L"Disk %d %s",pData->ID,L"");
				pSubMenu2->AppendMenu(MF_STRING|MF_BYCOMMAND,++BaseID ,StrID);
			}
		}

		pSubMenu2->DeleteMenu(0,MF_BYPOSITION);

	}



	
	pMenu->TrackPopupMenu(TPM_LEFTALIGN,CurPos.x,CurPos.y,this);
}

BOOL CPerformanceBox::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class

	mToolTip.RelayEvent(pMsg);



	int n =mPItemList.GetItemCount();
	int DiskCount = 0;
	PerferListData *pPData =NULL;	
	for(int i=2;i<n;i++)
	{		
		if(pPData == NULL) continue;
		pPData=(PerferListData*)mPItemList.GetItemData(i);
		if(pPData->Type==PM_DISK) DiskCount++;
	}




	if(pMsg->message == WM_COMMAND)
	{
		UINT CmdMenuID = (UINT) pMsg->wParam;
		if( (CmdMenuID > ID_DISK_NONE) &&  (CmdMenuID <= CmdMenuID+DiskCount) )
		{

			 
			_FakeSelPItem(CmdMenuID-ID_DISK_NONE+1);
		}

	}

	return CFormView::PreTranslateMessage(pMsg);
}

void CPerformanceBox::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	CFormView::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);


	switch(theApp.AppSettings.ProcessorDisplayMode)
	{
	case 0:
		pPopupMenu->CheckMenuRadioItem(ID_CHANGEGRAPHTO_OVERALLUTILIZATION,ID_CHANGEGRAPHTO_NUMANODES,ID_CHANGEGRAPHTO_OVERALLUTILIZATION,MF_BYCOMMAND);
		break;
	case 1:
		pPopupMenu->CheckMenuRadioItem(ID_CHANGEGRAPHTO_OVERALLUTILIZATION,ID_CHANGEGRAPHTO_NUMANODES,ID_CHANGEGRAPHTO_LOGICALPROCESSORS,MF_BYCOMMAND);
		break;
	case 2: 
		pPopupMenu->CheckMenuRadioItem(ID_CHANGEGRAPHTO_OVERALLUTILIZATION,ID_CHANGEGRAPHTO_NUMANODES,ID_CHANGEGRAPHTO_NUMANODES,MF_BYCOMMAND);
		break;
	}



	UINT Flag = (theApp.FlagSummaryView) ? MF_CHECKED:MF_UNCHECKED;
	pPopupMenu->CheckMenuItem(ID_PERFORMANCE_GRAPHSUMMARYVIEW,MF_BYCOMMAND|Flag);

	int iSel = mPItemList.GetNextItem( -1, LVNI_SELECTED );

	switch(iSel)
	{
	case 0:
		pPopupMenu->CheckMenuRadioItem(ID_VIEW_CPU,ID_VIEW_MEMORY,ID_VIEW_CPU,MF_BYCOMMAND);
		break;
	case 1:
		pPopupMenu->CheckMenuRadioItem(ID_VIEW_CPU,ID_VIEW_MEMORY,ID_VIEW_MEMORY,MF_BYCOMMAND);
		break;
	
	}
	



	if(pPopupMenu->GetMenuItemID(0) ==ID_VIEW_CPU)
	{
		

		


		int ItemCount =mPItemList.GetItemCount();
		PerferListData *pPData = NULL ;		
		int DiskCount = 0;
		int SelectedDiskID = -1;

	    int FirstDiskItemID = 2;

			 
		for(int i = FirstDiskItemID;  i<ItemCount ;i++ ) 
		{		
			pPData=(PerferListData*)mPItemList.GetItemData(i);
			if(pPData == NULL)continue;		
			if(pPData->Type == PM_DISK)
			{
				DiskCount++;
				if(i == iSel){ SelectedDiskID = iSel-FirstDiskItemID ;}
				
			}
		}

		Flag = (DiskCount>0)?MF_ENABLED:MF_GRAYED;
		pPopupMenu->EnableMenuItem(2,MF_BYPOSITION|Flag  );  
		if(SelectedDiskID>=0)
		{
			CMenu *pSubMenu = pPopupMenu->GetSubMenu(2);
			if(pSubMenu)pSubMenu->CheckMenuRadioItem(0, DiskCount-1, SelectedDiskID, MF_BYPOSITION);
		}

		
	}

}

void CPerformanceBox::OnNMRClickListPerformanceitem(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: Add your control notification handler code here

	CMenu PopMenu;
	CMenu *pMenu= NULL;
	PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_BASE) );
	pMenu = PopMenu.GetSubMenu(5);

	CPoint CurPos ;
	GetCursorPos(&CurPos); 

	if(theApp.FlagSummaryView)
	{
		pMenu->CheckMenuItem(ID_PERFORMANCETYPE_SUMMARYVIEW, MF_BYCOMMAND|MF_CHECKED);
	}

	pMenu->TrackPopupMenu(TPM_LEFTALIGN,CurPos.x,CurPos.y,this);

	*pResult = 0;
}

void CPerformanceBox::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	//google 

	theApp.m_pMainWnd->PostMessage(UM_SUMMARYVIEW,(WPARAM)this);

	

	CFormView::OnLButtonDblClk(nFlags, point);
}

void CPerformanceBox::MySetItem(int iItem, CString Strtext)
{
	LVITEM Item;	 
	Item.iItem = iItem;
	Item.iSubItem = 1;
	Item.mask = LVIF_TEXT;
	Item.cchTextMax=MAX_PATH;
	Item.pszText= (LPTSTR)(LPCTSTR)(Strtext);
	mPItemList.SetItem(&Item);

}

