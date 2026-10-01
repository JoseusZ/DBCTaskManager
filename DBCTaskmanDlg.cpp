// DBCTaskmanDlg.cpp : implementation file
//




#include "stdafx.h"
#include "DBCTaskman.h"
#include "DBCTaskmanDlg.h"
#include "Wtsapi32.h"
#include "Dbt.h"


#include "AboutDlg.h"
#include "MiniPerfermanceDlg.h"


#include "MemCompBox.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#endif






//=================================================================================================





//Simple list ����Ļص�����   
int CALLBACK SortFuncSimpleList(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)  
{  
	int result;     //����ֵ   

	PROCLISTDATA* pAnalysis1= NULL;
	PROCLISTDATA* pAnalysis2 =NULL;  

	//���еĲ��������ڱȽ�   
	pAnalysis1 = (PROCLISTDATA*)lParam1;  
	pAnalysis2 = (PROCLISTDATA*)lParam2;  

	//����   
	if(lParamSort==0)
	{
		result = wcscmp(pAnalysis1->Description, pAnalysis2->Description);  

	}



	return result;  
}  






//=================================================================================================



static UINT Thread_PageTimerPerformance(LPVOID pParam)
{

	CDBCTaskmanDlg  *Dlg=(CDBCTaskmanDlg *)pParam;

	if(Dlg->m_hWnd == NULL) return -1; //
	while(1)
	{
		Dlg->pPagePerformance->pViewBox->OnUMTimer(0,0);
		Sleep( (UINT) (theApp.AppSettings.TimerStep*1000) );
	}
	//Dlg->FlagEnableRefresh = TRUE;
	AfxEndThread(0,TRUE);
	return 0;


}





static UINT Thread_DeleteData(LPVOID pParam)
{

	PROCLISTDATA  *pData=(PROCLISTDATA *)pParam;

	if(pData == NULL) return -1; //

	Sleep(0); //���ݹ������κι�������߳���Ϣ�������,Ȼ������ɾ��(���ⳤʱ������ָ��)
	

	delete  pData ;
	pData = NULL;

	AfxEndThread(0,TRUE);
	return 0;
}









static UINT Thread_InitPerformanceMon(LPVOID pParam)
{



	CDBCTaskmanDlg  *Dlg=(CDBCTaskmanDlg *)pParam;
	if(Dlg->m_hWnd == NULL) return -1; //

	Dlg->mPerformanceMon.Init();	
	//Dlg->FlagEnableRefresh = TRUE;

	AfxEndThread(0,TRUE);
	return 0;


}
//=================================================================================================



static UINT Thread_ListTask(LPVOID pParam)
{



	CDBCTaskmanDlg  *Dlg=(CDBCTaskmanDlg *)pParam;
	if(Dlg->m_hWnd == NULL) return -1; //

	Dlg->mSimpleTaskList.SetRedraw(0);
	Dlg->ListTask();	

	Sleep(150);
	Dlg->SortSimpleList();
	Dlg->mSimpleTaskList.SetRedraw(1);
	//Dlg->FlagEnableRefresh = TRUE;	
	AfxEndThread(0,TRUE);
	return 0;


}







//=================================================================================================





// CDBCTaskmanDlg dialog




CDBCTaskmanDlg::CDBCTaskmanDlg(CWnd* pParent /*=NULL*/)
: CDialog(CDBCTaskmanDlg::IDD, pParent)
,pPageProcesses(NULL)
,pPagePerformance(NULL)
,pPageDetails(NULL)
,pPageServices(NULL)
,pPageUsers(NULL)
, pMainMenu(NULL)
, FirstShow(TRUE)
, SimpleListSortEnable(TRUE)
, pPageStartup(NULL)
, mLastTrayCpuLevel(-1)
, mLastTipCpu(-1)
, mLastTipMem(-1)
, mLastTipDisk(-1)
, mLastTipNet(-1)
{

	CString   ExeFullPath;
	DWORD BuffSize=MAX_PATH;
	GetModuleFileName(NULL,ExeFullPath.GetBuffer(BuffSize),BuffSize);  
	ExeFullPath.ReleaseBuffer();

	::ExtractIconEx(ExeFullPath,0,NULL,&m_hIcon,1);	



	//BkgBrush.CreateSolidBrush( );//for simple mode 

}

void CDBCTaskmanDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PAGETAB, mTab);

	DDX_Control(pDX, IDC_BTN_FEWERORMORE, mBtnFewerOrMore);
	DDX_Control(pDX, IDC_SIMPLETASK, mSimpleTaskList);
}

BEGIN_MESSAGE_MAP(CDBCTaskmanDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//ON_WM_DEVICECHANGE()
	ON_WM_SIZE()
	ON_WM_TIMER()
	
	ON_NOTIFY(TCN_SELCHANGE, IDC_PAGETAB, &CDBCTaskmanDlg::OnTcnSelchangePageTab)
	ON_WM_CREATE()
	ON_WM_WTSSESSION_CHANGE()
	
	

	ON_WM_INITMENUPOPUP()	
	ON_WM_CLOSE()	 
	ON_WM_SHOWWINDOW()
	ON_WM_CTLCOLOR()

	
	ON_WM_MEASUREITEM()
	ON_NOTIFY(NM_RCLICK, IDC_SIMPLETASK, &CDBCTaskmanDlg::OnNMRClickSimpletaskList)
	
	ON_WM_EXITSIZEMOVE()
	ON_WM_ACTIVATE()
	ON_MESSAGE(UM_NOTIFYICON,&CDBCTaskmanDlg::OnTrayIcon)
	ON_WM_NCDESTROY()

	ON_COMMAND(ID_FILE_RUNNEWTASK, &CDBCTaskmanDlg::OnMM_FileRunNewTask)
	ON_COMMAND(ID_SIMPLELIST_SWITCHTO, &CDBCTaskmanDlg::OnPop_SimplelistSwitchTo)
	ON_COMMAND(ID_SIMPLELIST_ENDTASK, &CDBCTaskmanDlg::OnPop_SimplelistEndTask)

	ON_COMMAND(ID_NOTIFY_RESTORE, &CDBCTaskmanDlg::OnPop_NotifyRestore)	
	ON_COMMAND(ID_VIEW_GROUPBYTYPE, &CDBCTaskmanDlg::OnMM_GroupByType)
	ON_COMMAND(ID_SIMPLELIST_OPENFILELOCATION, &CDBCTaskmanDlg::OnPop_SimplelistOpenfilelocation)
	ON_COMMAND(ID_SIMPLELIST_PROPERTIES, &CDBCTaskmanDlg::OnPop_SimplelistProperties)	
	ON_COMMAND(ID_FILE_SAVELIST, &CDBCTaskmanDlg::OnFileSaveDetailsList)
	ON_COMMAND(ID_FILE_SAVESCREENSHOT, &CDBCTaskmanDlg::OnFileSaveScreenshot)	

	ON_COMMAND(ID_UPDATESPEED_HIGH, &CDBCTaskmanDlg::OnMM_UpdatespeedHigh)
	ON_COMMAND(ID_UPDATESPEED_NORMAL, &CDBCTaskmanDlg::OnMM_UpdateSpeedNormal)
	ON_COMMAND(ID_UPDATESPEED_LOW, &CDBCTaskmanDlg::OnMM_UpdateSpeedLow)
	ON_COMMAND(ID_UPDATESPEED_PAUSED, &CDBCTaskmanDlg::OnMM_UpdateSpeedPaused)
		
	ON_WM_THEMECHANGED()	
	ON_NOTIFY(LVN_KEYDOWN, IDC_SIMPLETASK, &CDBCTaskmanDlg::OnLvnKeydownSimpletaskList)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CDBCTaskmanDlg message handlers

BOOL CDBCTaskmanDlg::OnInitDialog()
{

	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here



	this->SetWindowTextW(MAIN_CAPTION);

	InitAll();

	


 



	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CDBCTaskmanDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout; 
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CDBCTaskmanDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		if(FirstShow)
		{

			FirstShow = FALSE;
			
			if(theApp.AppSettings.TaskManMode==0)
			{
				CRect rcWnd;
				this->GetWindowRect(rcWnd);
				rcWnd.bottom--;this->MoveWindow(rcWnd);
				rcWnd.bottom++;this->MoveWindow(rcWnd);


			}



		}

		CDialog::OnPaint();
	}


}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDBCTaskmanDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CDBCTaskmanDlg::PlaceAllCtrl(void)
{
	CRect rc;
	CRect rcTab;
	GetClientRect(rc);
	mTab.GetItemRect(0,rcTab);

	mTab.MoveWindow(0,0,rc.Width()+5,rc.Height()+5);




	rc.top=rcTab.Height()+3;


	//δ�������� �������ƣ�
	if(!theApp.FlagThemeActive)  rc.top+=15;

	if(theApp.FlagSummaryView)//����ģʽ��ֱ�ӳ�������
	{
		rc.top = 0;
	}



	pPageProcesses->MoveWindow(rc);

	pPagePerformance->MoveWindow(rc);
	pPagePerformance->PostMessageW(WM_SIZE);


	pPageDetails->MoveWindow(rc);

	pPageServices->MoveWindow(rc);

	pPageUsers->MoveWindow(rc);

	pPageStartup->MoveWindow(rc);

	//mTab.Invalidate(0);


	theApp.MenuAndTabHeight = rc.top;


	if(theApp.AppSettings.TaskManMode == 0)
	{
		/*CRect rcBtn  ;

		mBtnFewerOrMore.GetWindowRect(rcBtn);
		this->ScreenToClient(rcBtn);
		rcBtn.OffsetRect(0,-theApp.MenuAndTabHeight);
		mBtnFewerOrMore.MoveWindow(rcBtn);

		*/



	}

	//mUserAppList.MoveWindow(rc);
	//mAllCpuWave.MoveWindow(60,60,rc.Width()-80,rc.Height()-100);


	GetClientRect(rc);

	rc.InflateRect(1,1);
	rc.bottom-=50;
	mSimpleTaskList.MoveWindow(rc);

	int ScrollBarWidth= 0;
	if(mSimpleTaskList.GetItemCount()*LINE_H2>rc.Height())
	{
		ScrollBarWidth= GetSystemMetrics(SM_CXVSCROLL);
	}


	//��֤����ˮƽ������
	mSimpleTaskList.SetColumnWidth(0,rc.Width()-4-ScrollBarWidth);
	mSimpleTaskList.SetColumnWidth(1,0);
	mSimpleTaskList.SetColumnWidth(2,0);


}

void CDBCTaskmanDlg::OnSize(UINT nType, int cx, int cy)
{	


	CDialog::OnSize(nType, cx, cy);


	PlaceAllCtrl();
 
	//----------------------

	if(nType == SIZE_MAXIMIZED )
	{
		theApp.FlagWindowStatus =  MAX_WND;


	}
	else if (nType ==SIZE_MINIMIZED ) 
	{
		theApp.FlagWindowStatus =  MIN_WND;

		if(theApp.AppSettings.HideWhenMini )
			this->ShowWindow(SW_HIDE);

	}
	else   if (nType == SIZE_RESTORED)
	{

		theApp.FlagWindowStatus = NORMAL_WND;
		if(theApp.AppSettings.TaskManMode == 0)
		{
			GetWindowRect(theApp.AppSettings.rcWnd_Simple);
		}
		else
		{
			GetWindowRect(theApp.AppSettings.rcWnd);
		}

	}





}







void CDBCTaskmanDlg::InitAll(void)
{


 
//-----------------------
	InitUpTime();

	

	this->SetIcon(theApp.hBigIcon,TRUE);
	

	SetTrayIcon(NIM_ADD);


	//------------ Set TopMost  Or not  ----------------
	HWND insertAfter = theApp.AppSettings.TopMost ? HWND_TOPMOST:HWND_NOTOPMOST;
	::SetWindowPos(GetSafeHwnd(),insertAfter,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);


	theApp.pButtonFOM = & mBtnFewerOrMore;
	mBtnFewerOrMore.Type = 1; //����ť������������� Fewer details  /More Details  �л���ť



	CString StrBtn;
	StrBtn.LoadStringW(IDS_STRING_FEWERDETAILS);
	theApp.pButtonFOM->SetWindowTextW(StrBtn);




	//----------Install Hook δʹ�ã������� ---------------
	//HMODULE hModuleHook = LoadLibrary(L"DBCTaskmgr.dll"); 

	//if(hModuleHook ==NULL)
	//{
	//	MessageBox(L"Error 006 : Load ExDesktop.dll");
	//}
	//else
	//{
	//	SatrtShellHook=(PROCSETHOOK)GetProcAddress(hModuleHook,"SetHookShell");
	//	StopShellHook=(PROCUNHOOK)GetProcAddress(hModuleHook,"UnHookShell");

	//	SatrtShellHook(theApp.GetMainWnd()->GetSafeHwnd());
	//
	//}

	//---------------------------------




	WTSRegisterSessionNotification(this->GetSafeHwnd(),NOTIFY_FOR_ALL_SESSIONS);



	//User ͼ��
	HICON hIcon;
	::ExtractIconEx(L"imageres.dll",207,NULL,&hIcon,1);  

	if(hIcon)
	{
		theApp.mImagelistNormal.Create(16,16,ILC_COLOR32,1,1);
		theApp.mImagelistNormal.Add(hIcon);  //User ͼ��
		::DestroyIcon(hIcon);
	}


	CString StrTabItem,StrLoad;

	StrLoad.LoadString(STR_TABITEMS);

	for( int iTab=0; iTab<6; iTab++ )
	{
		::AfxExtractSubString(StrTabItem,StrLoad,iTab,L';');
		mTab.InsertItem(iTab,StrTabItem);
	}


	//----------tab------------
	/*mTab.InsertItem(0,L"Processes");
	mTab.InsertItem(1,L"Performance");
	mTab.InsertItem(2,L"Users");
	mTab.InsertItem(3,L"Details");
	mTab.InsertItem(4,L"Services");
	mTab.InsertItem(5,L"Startup");*/


	theApp.pMainTab  = &mTab;



	pMainMenu = GetMenu();





	//�����ڴ˴� ��ʼ�� ���򴰿ڻ�û��ɴ��� 


	pPageDetails->pServiceView = pPageServices;

	
	pPageDetails->pTaskList = & pPageProcesses->mTaskList;	//Ԥ�ȸ�ֵ ���� ��ʼ������	 ��ʼʱpPageDetails���䷢����Ϣ
	pPageDetails->InitList();//���������б� ���� pPageDetails  �б���������ȳ�ʼ��

	pPageProcesses->pPageDetails = pPageDetails;  
	pPageProcesses->InitList();



	pPagePerformance->Init();  
	pPageServices->InitList();
	pPageUsers->InitList();
	pPageStartup->InitList();





	//---------- �����б�------------


	mSimpleTaskList.SetExtendedStyle( mSimpleTaskList.GetExtendedStyle()|LVS_EX_FULLROWSELECT| LVS_OWNERDRAWFIXED|LVS_EX_DOUBLEBUFFER   );  //| LVS_EX_GRIDLINES |LVS_EX_CHECKBOXES /
	mSimpleTaskList.InsertColumn(0,L"TaskName",0);
	mSimpleTaskList.InsertColumn(1,L"Status",0) ;//���в���ʾ ���ڱ������״̬
	mSimpleTaskList.InsertColumn(2,L"Win",0) ;//���в���ʾ ���ڱ��洰�ڿɼ�



	//----------------------------------





	pPageUsers->pDetailList  = & pPageDetails->mDetailsList;
	pPageUsers->pPageDetails = pPageDetails;
	pPageDetails->pUsersView = pPageUsers;





	pPageProcesses->ShowWindow(SW_SHOW);

	pPagePerformance->ShowWindow(SW_HIDE);
	pPageDetails->ShowWindow(SW_HIDE);
	pPageServices->ShowWindow(SW_HIDE);
	pPageUsers->ShowWindow(SW_HIDE);


	PlaceAllCtrl();

	mTab.SetCurSel(theApp.AppSettings.ActiveTab);



	//--------------����ʵ�ʶ���---------- 
	NMHDR nmhdr; 
	nmhdr.code = TCN_SELCHANGE;  
	nmhdr.hwndFrom = mTab.GetSafeHwnd();  
	nmhdr.idFrom= mTab.GetDlgCtrlID();  
	::SendMessage(mTab.GetSafeHwnd(), WM_NOTIFY,MAKELONG(TCN_SELCHANGE,0), (LPARAM)(&nmhdr));


	//---------------------���ܼ���---------------------



	//mPerformanceMon.Init(); //����������������̣߳�����
	AfxBeginThread(Thread_InitPerformanceMon,this);



	pPageProcesses->pPerformanceMon = &mPerformanceMon;
	pPageUsers->pPerformanceMon = &mPerformanceMon;



	mPerformanceMon.GetCpuInfo();



	(theApp.AppSettings.TaskManMode == 0) ? MoveWindow(theApp.AppSettings.rcWnd_Simple) : MoveWindow(theApp.AppSettings.rcWnd);
	




	//--------------------------------

	this->SetTimer(0,(UINT)((double)theApp.AppSettings.TimerStep*1000.0),NULL); //���⣺һ��Ҫ�ڳ�1000֮����תΪUINT ����TimerStep ==0.5ʱ  ���Ϊ0


	if((double)theApp.AppSettings.TimerStep == 0.5)
	{
		_SetTimeTipStr(IDS_STRING_WAVETIME_FAST);
	}
	else if ((double)theApp.AppSettings.TimerStep == 1)
	{
		_SetTimeTipStr(IDS_STRING_WAVETIME_NORMAL);
	}
	else if ((double)theApp.AppSettings.TimerStep == 4)
	{
		_SetTimeTipStr(IDS_STRING_WAVETIME_SLOW);
	}



	(theApp.AppSettings.TaskManMode == 1) ? _SetToAdvanceMode(FALSE):_SetToSimpleMode(FALSE);
	






}

void CDBCTaskmanDlg::OnTimer(UINT_PTR nIDEvent)
{

	int Sec = (int)theApp.UpTimeSec;

	// FIX: ya no hace falta throttlear a 3s. Antes era obligatorio porque
	// cada llamada pintaba 14 barras con GDI y allocaba un HICON. Ahora
	// SetTrayIcon solo hace LoadIcon + Shell_NotifyIcon cuando el nivel
	// de CPU realmente cambia (ver mLastTrayCpuLevel en SetTrayIcon).
	SetTrayIcon(NIM_MODIFY);

	UpdateUpTime();

	TestHungProcess();


	int nSel=mTab.GetCurSel();

	if(theApp.StartPerformancePageTimer)
	{
		pPagePerformance->pViewBox->OnUMTimer(0,0);  // ����ҳ�ܸ���
		
	}

	if(theApp.AppSettings.TaskManMode == 0)
	{
		UpdateSimpleList();
		return;
	}


	switch(nSel)
	{
	case  0:
		pPageProcesses->OnUMTimer(0,0);
		break;
	case  2:
		pPageUsers->OnUMTimer(0,0);
		break;
	case  3:
		pPageDetails->OnUMTimer(0,0);
		break;

	}




	CDialog::OnTimer(nIDEvent);
}




BOOL CDBCTaskmanDlg::PreTranslateMessage(MSG* pMsg)
{

	int i,n;

	//UN_POPUP_MEMDETAILSDLG

	switch(pMsg->message)
	{

	case WM_KEYDOWN:
		switch (pMsg->wParam) 
		{ 
		case VK_F5:  
			pPageDetails->PostMessageW(UM_REFRESH);	pPageProcesses->RefreshList();
			break; 
		}
		break; 	
	//----------------�л����� /�߼� ģʽ----------------
	case UM_TOGGLE_TMMODE:  

		mTab.IsWindowVisible() ? _SetToSimpleMode():_SetToAdvanceMode();		 

		break; 

		//----------------���ӵ�simple list----------------
	case UM_ADD_PROC_SIMPEL_LIST:
		
		AddToSimpleList((PROCLISTDATA * )pMsg->lParam);

		break; 	

		//----------------��simple listɾ��--------------------------
	case UM_PROCEXIT: 
			
		DeleteFromSimpleList((PROCLISTDATA * )pMsg->lParam);

		break; 	

		//--------------------------�л�����ͼģʽ-----------------------
	case  UM_SUMMARYVIEW:  

		int ShowWndFlag;
		if(theApp.m_pMainWnd->GetStyle()&WS_CAPTION)
		{
			theApp.m_pMainWnd->ModifyStyle(WS_MAXIMIZEBOX|WS_MINIMIZEBOX|WS_SYSMENU|WS_CAPTION,0);
			theApp.m_pMainWnd->SetMenu(NULL);
			ShowWndFlag = SW_HIDE;			
			theApp.FlagSummaryView = TRUE;

		}
		else
		{
			theApp.m_pMainWnd->ModifyStyle(0,WS_MAXIMIZEBOX|WS_MINIMIZEBOX|WS_SYSMENU|WS_CAPTION);
			theApp.m_pMainWnd->SetMenu(pMainMenu);
			ShowWndFlag=SW_SHOW;
			theApp.FlagSummaryView = FALSE;

		}


		//theApp.FlagSummaryView ͳһ���ı������ĸ�����ժҪ��ʾ  theApp.FlagSummaryView ��������Summarymode�����⴦��ʱ�жϱ�־

		if((CPerformanceBox *)pMsg->wParam == pPagePerformance->pViewBox)
		{
			pPagePerformance->pViewBox->mPItemList.ShowWindow(ShowWndFlag);
		}
		else if((CPItemList *)pMsg->wParam == (&pPagePerformance->pViewBox->mPItemList))  //��������б�
		{
			pPagePerformance->pViewBox->ShowWindow(ShowWndFlag);
			CRect rc;
			pPagePerformance->pViewBox->mPItemList.GetWindowRect(rc);
			pPagePerformance->ScreenToClient(rc);
			if(ShowWndFlag== SW_HIDE)
			{				
				rc.right=10000;
				pPagePerformance->pViewBox->mPItemList.MoveWindow(rc);				
				
			}
			else
			{
				rc.right=200;
				pPagePerformance->pViewBox->mPItemList.MoveWindow(rc);
				

			}
		}

		pPagePerformance->mSplitter.ShowWindow(ShowWndFlag);

		mTab.ShowWindow(ShowWndFlag);
		mBtnFewerOrMore.ShowWindow(ShowWndFlag);
		pPagePerformance->mBtnOpenResMon.ShowWindow(ShowWndFlag);


		PlaceAllCtrl();

		break; 	

	case  UM_POPUP_MEMDETAILSDLG: 

		break;
	case UM_ALLINFO_OK: 		
		pPageProcesses->PostMessageW(UM_ALLINFO_OK);
		if(theApp.AppSettings.TaskManMode==0)
		{
			SortSimpleList();
			SetSimpleListItemsIcon();
		}
		break;
	case UM_RELOADSETINGS: 		
		theApp.LoadAppSettings(TRUE);
		n =	pPagePerformance->pViewBox->mPItemList.GetItemCount();
		
		for(i=0;i<n;i++)
		{
			PerferListData * pData = NULL;
			pData = ( PerferListData * ) pPagePerformance->pViewBox->mPItemList.GetItemData(i);	
			if(pData!=NULL)
			{
				if(pData->Type == PM_CPU)
				{
					pData->pWaveBox->SetColor(theApp.AppSettings.CpuColor);
					if(pPagePerformance->pViewBox->pCpuBox!=NULL)
					{
						pPagePerformance->pViewBox->pCpuBox->SetColor(theApp.AppSettings.CpuColor);
					}
					if(pPagePerformance->pViewBox->pTotalCpuBox!=NULL)
					{
						pPagePerformance->pViewBox->pTotalCpuBox->SetColor(theApp.AppSettings.CpuColor);
					}

					float A = (float)theApp.AppSettings.CpuColor.GridColorAlpha/255;
					pPagePerformance->pViewBox->pHotBox->GridColor = AlphaBlendColor(theApp.AppSettings.CpuColor.BackgroundColor,theApp.AppSettings.CpuColor.GridColorRGB,A);

				 


				}
				else if(pData->Type == PM_MEMORY)
				{
					pData->pWaveBox->SetColor(theApp.AppSettings.MemoryColor);
					if(pData->pOtherWnd!=NULL)
					{
						((CMemCompBox*)(pData->pOtherWnd))->SetColor();
						//pData->pOtherWnd->PostMessageW(UM_RELOADSETINGS);
					}

				}
				else if(pData->Type == PM_DISK)
				{
					pData->pWaveBox->SetColor(theApp.AppSettings.DiskColor);
					if(pData->pOtherWnd!=NULL)
					{
						((CWaveBox*)(pData->pOtherWnd))->SetColor(theApp.AppSettings.DiskColor);
					}
					if(pData->pInfoBox!=NULL)
					{
						pData->pInfoBox->SetColor();
					}
				}
				else if(pData->Type == PM_ETHERNET)
				{
					pData->pWaveBox->SetColor(theApp.AppSettings.NetworkColor);
					if(pData->pInfoBox!=NULL)
					{
						pData->pInfoBox->SetColor();
					}
				}

			}
		}

		break;



	}





	/*CString Str;
	if(pMsg->message == UM_HOOK_WNDCREATED  )
	{

	::GetWindowText((HWND)pMsg->wParam,Str.GetBuffer(MAX_PATH),MAX_PATH);
	Str.ReleaseBuffer();

	MessageBox(L"created:"+Str);

	}

	if(pMsg->message == UM_HOOK_WNDDESTROYED  )
	{

	::GetWindowText((HWND)pMsg->wParam,Str.GetBuffer(MAX_PATH),MAX_PATH);
	Str.ReleaseBuffer();
	MessageBox(L"destroy:"+Str);

	}
	*/



	return CDialog::PreTranslateMessage(pMsg);
}


void CDBCTaskmanDlg::OnTcnSelchangePageTab(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: Add your control notification handler code here

	theApp.AppSettings.ActiveTab =  mTab.GetCurSel();


	CWnd * pPage[6] = {pPageProcesses,pPagePerformance,pPageUsers,pPageDetails,pPageServices,pPageStartup};

	for(int PageID = 0; PageID < 6; PageID++ )
	{
		(PageID == theApp.AppSettings.ActiveTab) ? pPage[PageID]->ShowWindow(SW_SHOW) :  pPage[PageID]->ShowWindow(SW_HIDE);		
	}



	switch(theApp.AppSettings.ActiveTab)
	{
	case 0:   //Processes
		
		pPageProcesses->mTaskList._GetRedrawColumn();	

		theApp.pButtonFOM->SetParent(pPageProcesses);
		theApp.pSelPage = pPageProcesses;
		pPageProcesses->RefreshList();

		break;
	case 1: //Performance

		if(pPagePerformance->pViewBox)
		{
			pPagePerformance->pViewBox->PostMessageW(WM_TIMER);
			pPagePerformance->pViewBox->Invalidate();
		}
	

		theApp.pButtonFOM->SetParent(pPagePerformance);
		theApp.pSelPage = pPagePerformance;


		break;
	case 2: //Users
	
		theApp.pButtonFOM->SetParent(pPageUsers);
		theApp.pSelPage = pPageUsers;
		break;

	case 3: //Details
	
		if(pPageDetails->FlagEnableRefresh){ pPageDetails->OnUMTimer(0,0); pPageDetails->ReSort(FALSE); }

		theApp.pButtonFOM->SetParent(pPageDetails);
		theApp.pSelPage = pPageDetails;

		break;

	case 4: //Services
		
		theApp.pButtonFOM->SetParent(pPageServices);
		theApp.pSelPage = pPageServices;

		break;
	case 5: //Startup
	
		theApp.pButtonFOM->SetParent(pPageStartup);
		theApp.pSelPage = pPageStartup;
		break;
	}




	*pResult = 0;
}





int CDBCTaskmanDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here

   //���ò��ұ�ǡ� 
	::SetProp(this->m_hWnd,L"DBC_TASKMAN_001",(HANDLE)1); 

	InitPages();



	if(theApp.AppSettings.TaskManMode == 0)
	{
		MoveWindow(theApp.AppSettings.rcWnd_Simple);
	}
	else
	{
		MoveWindow(theApp.AppSettings.rcWnd);
	}


	return 0;
}

void CDBCTaskmanDlg::InitPages(void)
{

	//CRect rc; 
	//GetClientRect(&rc);



	CCreateContext * pContext = NULL;
	pContext = new CCreateContext; 



	pContext->m_pCurrentDoc = NULL; 
	pContext->m_pCurrentFrame = NULL; 
	pContext->m_pLastView = NULL; 
	pContext->m_pNewDocTemplate =NULL; 


	//-------------------------------- Processes

	pViewClass = RUNTIME_CLASS(CPageProcesses);
	pContext->m_pNewViewClass = pViewClass; 

	CWnd * pWnd = NULL; 
	pWnd = DYNAMIC_DOWNCAST(CWnd, pViewClass->CreateObject());
	pWnd->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, IDD_PAGE_PROCESSES, pContext); 

	pPageProcesses = DYNAMIC_DOWNCAST(CPageProcesses, pWnd); 
	pPageProcesses->SetScrollSizes(MM_TEXT, CSize(2, 2)); 
	pPageProcesses->ModifyStyleEx(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE,0);





	//--------------------------------------Peformance

	pViewClass = RUNTIME_CLASS(CPagePerformance);


	pContext->m_pNewViewClass = pViewClass; 

	pWnd = DYNAMIC_DOWNCAST(CWnd, pViewClass->CreateObject());
	pWnd->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, IDD_PAGE_PERFORMANCE, pContext); 

	pPagePerformance= DYNAMIC_DOWNCAST(CPagePerformance, pWnd); 
	pPagePerformance->SetScrollSizes(MM_TEXT, CSize(1, 1)); 
	pPagePerformance->ModifyStyleEx(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE,0);

	//-------------------------------- Processes Details  



	pViewClass = RUNTIME_CLASS(CPageDetails);

	pContext->m_pNewViewClass = pViewClass; 


	pWnd = DYNAMIC_DOWNCAST(CWnd, pViewClass->CreateObject());
	pWnd->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, IDD_PAGE_DETAILS, pContext); 

	pPageDetails = DYNAMIC_DOWNCAST(CPageDetails, pWnd); 
	pPageDetails->SetScrollSizes(MM_TEXT, CSize(2, 2)); 
	pPageDetails->ModifyStyleEx(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE,0);



	//--------------------------------------Service

	pViewClass = RUNTIME_CLASS(CPageServices);


	pContext->m_pNewViewClass = pViewClass; 

	pWnd = DYNAMIC_DOWNCAST(CWnd, pViewClass->CreateObject());
	pWnd->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, IDD_PAGE_SERVICES, pContext); 

	pPageServices= DYNAMIC_DOWNCAST(CPageServices, pWnd); 
	pPageServices->SetScrollSizes(MM_TEXT, CSize(2, 2)); 
	pPageServices->ModifyStyleEx(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE,0);


	//--------------------------------------Startup

	pViewClass = RUNTIME_CLASS(CPageStartup);


	pContext->m_pNewViewClass = pViewClass; 

	pWnd = DYNAMIC_DOWNCAST(CWnd, pViewClass->CreateObject());
	pWnd->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, IDD_PAGE_STARTUP, pContext); 

	pPageStartup= DYNAMIC_DOWNCAST(CPageStartup, pWnd); 
	pPageStartup->SetScrollSizes(MM_TEXT, CSize(2, 2)); 
	pPageStartup->ModifyStyleEx(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE,0);



	//--------------------------------------Users

	pViewClass = RUNTIME_CLASS(CPageUsers);


	pContext->m_pNewViewClass = pViewClass; 

	pWnd = DYNAMIC_DOWNCAST(CWnd, pViewClass->CreateObject());
	pWnd->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), this, IDD_PAGE_USERS, pContext); 

	pPageUsers= DYNAMIC_DOWNCAST(CPageUsers, pWnd); 
	pPageUsers->SetScrollSizes(MM_TEXT, CSize(2, 2)); 
	pPageUsers->ModifyStyleEx(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE,0);



	delete pContext; 
	pContext = NULL;




	//��ʼ��pSelPage  ����������� �������⣡��������������

	switch(theApp.AppSettings.ActiveTab)
	{
	case 0:theApp.pSelPage=pPageProcesses; break;
	case 1:theApp.pSelPage=pPagePerformance; break;
	case 2:theApp.pSelPage=pPageUsers; break;
	case 3:theApp.pSelPage=pPageDetails; break;
	case 4:theApp.pSelPage=pPageServices; break;
	case 5:	theApp.pSelPage =pPageStartup;break;

	}



	//pPageDetails->pPageProcess = pPageProcesses ;




}

void CDBCTaskmanDlg::OnSessionChange(UINT nSessionState, UINT nId)
{
	

	pPageUsers->PostMessageW(WM_WTSSESSION_CHANGE,nSessionState,nId);


	CDialog::OnSessionChange(nSessionState, nId);
}


void CDBCTaskmanDlg::OnMM_UpdatespeedHigh()
{
	SetUpdateSpeed(0.5,IDS_STRING_WAVETIME_FAST);
}

void CDBCTaskmanDlg::OnMM_UpdateSpeedNormal()
{
	SetUpdateSpeed(1,IDS_STRING_WAVETIME_NORMAL);
	
}

void CDBCTaskmanDlg::OnMM_UpdateSpeedLow()
{
	SetUpdateSpeed(4,IDS_STRING_WAVETIME_SLOW);

}

void CDBCTaskmanDlg::OnMM_UpdateSpeedPaused()
{
	// TODO: Add your command handler code here
	SetUpdateSpeed(-1,0);

}



void CDBCTaskmanDlg::UpdateUpTime(void)
{



	theApp.UpTimeSec += theApp.AppSettings.TimerStep ;




	if((int)theApp.UpTimeSec >= 60)
	{
		theApp.UpTimeSec = theApp.UpTimeSec-60;
		theApp.UpTimeMin++;
	}


	if(theApp.UpTimeMin == 60)
	{
		theApp.UpTimeMin = 0;
		theApp.UpTimeHour++;		
	}


	if(theApp.UpTimeHour == 24)
	{
		theApp.UpTimeHour=0;
		theApp.UpTimeDay++;		
	} 

}



void CDBCTaskmanDlg::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)  //��̬�޸Ĳ˵�  ON_WM_INITMENUPOPUP () Ϊ��Ӧ��Ϣ
{
	CDialog::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);


	if(!PathFileExists(theApp.StrCfgToolsPath))
	{
		pPopupMenu->RemoveMenu(ID_OPTIONS_SETTINGS,MF_BYCOMMAND);
		pPopupMenu->RemoveMenu(3,MF_BYPOSITION);
	}
	



	UINT Flag = (theApp.AppSettings.MiniOnUse)?MF_CHECKED:MF_UNCHECKED;
	pPopupMenu->CheckMenuItem(	ID_OPTIONS_MINIMIZEONUSE,MF_BYCOMMAND|Flag );

	Flag = (theApp.AppSettings.HideWhenMini)?MF_CHECKED:MF_UNCHECKED;	
	pPopupMenu->CheckMenuItem(	ID_OPTIONS_HIDEWHENMINIMIZED,MF_BYCOMMAND|Flag );

	Flag = (theApp.AppSettings.TopMost)?MF_CHECKED:MF_UNCHECKED;
	pPopupMenu->CheckMenuItem(	ID_OPTIONS_ALWAYSONTOP,MF_BYCOMMAND|Flag );




	
	int nTabSel = mTab.GetCurSel();
	Flag = (nTabSel==0 || nTabSel==2)? MF_ENABLED:MF_DISABLED|MF_GRAYED;
	pPopupMenu->EnableMenuItem(ID_VIEW_COLLAPSEALL,Flag|MF_BYCOMMAND );
	pPopupMenu->EnableMenuItem(ID_VIEW_EXPANDALL,Flag|MF_BYCOMMAND );


	//------------------------ID_VIEW_GROUPBYTYPE------------------------------------------------
	Flag = (nTabSel==0)? MF_ENABLED:MF_DISABLED|MF_GRAYED;
	pPopupMenu->EnableMenuItem(ID_VIEW_GROUPBYTYPE,Flag|MF_BYCOMMAND );
	APPLISTDATA *pData =  NULL;
	pData = (APPLISTDATA *) pPageProcesses->mTaskList.GetItemData(0);//�з���ʱ��,��0���Ƿ������
	if(pData!=NULL)
	{
		Flag = (pData->SubType == -1)?MF_CHECKED:MF_UNCHECKED;
		pPopupMenu->CheckMenuItem(	ID_VIEW_GROUPBYTYPE,MF_BYCOMMAND|Flag );
	}
	//-------------------------------------------------------------------------------------------
	



	UINT CheckID = 0;

	if(theApp.AppSettings.TimerStep == 1.0) 
	{
		CheckID = ID_UPDATESPEED_NORMAL;
	}
	else if(theApp.AppSettings.TimerStep == 0.5) 
	{
		CheckID =	ID_UPDATESPEED_HIGH; 
	}
	else if(theApp.AppSettings.TimerStep == 4) 
	{
		CheckID =	ID_UPDATESPEED_LOW;
	}
	else if(theApp.AppSettings.TimerStep == -1) 
	{
		CheckID =	ID_UPDATESPEED_PAUSED;
	}

	

	pPopupMenu->CheckMenuRadioItem(ID_UPDATESPEED_HIGH,ID_UPDATESPEED_PAUSED,CheckID,MF_BYCOMMAND);
	





	
}




void CDBCTaskmanDlg::OnClose()
{
	// TODO: Add your message handler code here and/or call default


	int i;

	for( i = 0;i< COL_COUNT_PROC;i++)
	{

		theApp.AppSettings.ColIDS_ProcList[i].ColWidth = pPageProcesses->mTaskList.GetColumnWidth(i);


	}

	for( i = 0;i< COL_COUNT_USER ;i++)
	{

		theApp.AppSettings.ColIDS_UserList[i].ColWidth =pPageUsers->mUserList.GetColumnWidth(i);

	}

	for( i = 0;i< COL_COUNT_DETAIL;i++)
	{
		theApp.AppSettings.ColIDS_DetailList[i].ColWidth=pPageDetails->mDetailsList.GetColumnWidth(i);


	}


	//-----------------------------------







	if(theApp.FlagWindowStatus == NORMAL_WND)
	{
		if(theApp.AppSettings.TaskManMode == 0)
		{
			GetWindowRect(theApp.AppSettings.rcWnd_Simple);
		}
		else
		{
			GetWindowRect(theApp.AppSettings.rcWnd);
		}

	}

	theApp.SaveAppSettings();

	CDialog::OnClose();


}




void CDBCTaskmanDlg::_SetTimeTipStr(UINT nStrID)
{
	CWnd *pWnd=NULL;
	CString Str;

	Str.LoadString(nStrID);
	pWnd=pPagePerformance->pViewBox->GetDlgItem(IDC_TIP3);
	if(pWnd!=NULL){ pWnd->SetWindowTextW(Str); }
	pWnd=pPagePerformance->pViewBox->GetDlgItem(IDC_TIP7);
	if(pWnd!=NULL){ pWnd->SetWindowTextW(Str); }

	if(theApp.AppSettings.TaskManMode == 1)
	{
			
		CString StrTemp;
		StrTemp.Format(STR_TIP1_CPU_LOGICAL,Str);


		pPagePerformance->pViewBox->mPItemList.SetItemText(0,3,StrTemp);
		if(pPagePerformance->pViewBox->pCpuBox!=NULL)
		{
			if(pPagePerformance->pViewBox->pCpuBox->IsWindowVisible())
			{
				pWnd=pPagePerformance->pViewBox->GetDlgItem(IDC_TIP1);
				if(pWnd!=NULL){ pWnd->SetWindowTextW(StrTemp); }

			}
		}
	}



	 


}





HBRUSH CDBCTaskmanDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	if(theApp.AppSettings.TaskManMode == 0)
	{

		return theApp.BkgBrush;
	}
	return hbr;
}

void CDBCTaskmanDlg::OnMM_FileRunNewTask()
{

	/************************************************************************************** 
	* �������� : ShowMyRunDlg(...) 
	* �������� : ��ʾ���Ƶ����жԻ��� 
	* ��  ��   : HWND   hWndOwner,        //�����ߴ��ھ�� 
	HICON  hIcon,            //Ҫ��ʾ��ͼ���� 
	PTCHAR pszDirectory,     //���������ť��Ŀ¼����Ŀ¼�µ��ļ���ʡ��·�� 
	PTCHAR pszTitle,         //���ڱ�������� 
	PTCHAR pszDescription,   //�����ϵ��ı����� 
	UINT   nFlags            //��ʽ 

	* ����˵�� : ��ʾ���Ƶ����жԻ��� 
	*************************************************************************************/ 

	typedef void (CALLBACK *lpfnRunDlg)(HWND, HICON, PTCHAR, PTCHAR, PTCHAR, UINT);  

	WCHAR pszDirectory[MAX_PATH];
	GetSystemDirectory(pszDirectory,MAX_PATH);

	HMODULE hModule = LoadLibrary(TEXT("Shell32.dll"));  
	if(hModule != NULL)  
	{  
		lpfnRunDlg RunDlg = (lpfnRunDlg)GetProcAddress(hModule, (LPCSTR)MAKEINTRESOURCE(61));  
		RunDlg(this->GetSafeHwnd(), NULL, pszDirectory, NULL, NULL, NULL);  
		FreeLibrary(hModule);  
		return ;  
	}  
	return ;  






}

int CDBCTaskmanDlg::ListTask(void)
{


	mSimpleTaskList.DeleteAllItems();

	int nTask = 0;



	int nID = 0;

	PROCLISTDATA *pData = NULL;

	CString StrTaskName;
	CString StrTemp;

	CProcess PInfo;
	for(nID = 0;nID< pPageDetails->mDetailsList.GetItemCount();nID++)
	{
		pData = (PROCLISTDATA * ) pPageDetails->mDetailsList.GetItemData(nID);
		if(pData == NULL) {continue;}

		if(pData->Type == APP)
		{

			int n = mSimpleTaskList.GetItemCount();
			StrTaskName = PInfo.GetVerInfoString(PInfo.GetPathName(pData->hProcess),L"FileDescription");
			StrTemp = StrTaskName;
			StrTemp.Remove(L' ');

			if(StrTemp.GetLength() == 0) 
			{
				StrTaskName = pData->Name;
				StrTaskName = StrTaskName.Left(StrTaskName.GetLength()-4);
			}

			mSimpleTaskList.InsertItem(n, StrTaskName , pData->IconIndex);
			mSimpleTaskList.SetItemData( n,(DWORD_PTR)pData );
		}



	}



	SortSimpleList();

	return nTask;
}

void CDBCTaskmanDlg::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	// TODO: Add your message handler code here and/or call default

	if(nIDCtl==IDC_SIMPLETASK)
	{
		lpMeasureItemStruct->itemHeight = LINE_H2;
	}

	CDialog::OnMeasureItem(nIDCtl, lpMeasureItemStruct);
}

void CDBCTaskmanDlg::_SetToSimpleMode(BOOL SaveWndPos)
{

	mTab.ShowWindow(SW_HIDE);
	theApp.pSelPage->ShowWindow(SW_HIDE);
	mBtnFewerOrMore.SetParent(this);			
	this->SetMenu(NULL);
	theApp.AppSettings.TaskManMode = 0;//����ģʽ
	mSimpleTaskList.ShowWindow(SW_SHOW);
	this->Invalidate();

	CRect rcBtn,rcView;
	theApp.pButtonFOM->GetWindowRect(rcBtn);
	theApp.pSelPage->GetWindowRect(rcView);
	this->ScreenToClient(rcBtn);
	this->ScreenToClient(rcView);
	rcBtn.OffsetRect(0,rcView.top);
	theApp.pButtonFOM->MoveWindow(rcBtn);

	CString StrBtn;
	StrBtn.LoadStringW(IDS_STRING_MOREDETAILS);
	mBtnFewerOrMore.SetWindowTextW(StrBtn);


	::AfxBeginThread(Thread_ListTask,this);

	
	//ListTask();


	if(SaveWndPos)
	{
		this->GetWindowRect(theApp.AppSettings.rcWnd); //save Window size for  Advance mode
		CRect rcNew;
		rcNew.CopyRect(theApp.AppSettings.rcWnd);
		rcNew.right = rcNew.left+theApp.AppSettings.rcWnd_Simple.Width(); 
		rcNew.bottom = rcNew.top+theApp.AppSettings.rcWnd_Simple.Height();
		this->MoveWindow(rcNew); //se
	}




}

void CDBCTaskmanDlg::_SetToAdvanceMode(BOOL SaveWndPos)
{

	mTab.ShowWindow(SW_SHOW);

	mSimpleTaskList.ShowWindow(SW_HIDE);

	mBtnFewerOrMore.SetParent(theApp.pSelPage);
	SetMenu(pMainMenu);
	theApp.AppSettings.TaskManMode = 1;//�߼�ģʽ
	theApp.pSelPage->PostMessageW(WM_SIZE);
	theApp.pSelPage->ShowWindow(SW_SHOW);

	CString StrBtn;
	StrBtn.LoadStringW(IDS_STRING_FEWERDETAILS);
	mBtnFewerOrMore.SetWindowTextW(StrBtn);

	pPageProcesses->PlaceAllCtrl();
	this->Invalidate();



	if(SaveWndPos)
	{
		this->GetWindowRect(theApp.AppSettings.rcWnd_Simple); //save Window size for  simple mode
		CRect rcNew;
		rcNew.CopyRect(theApp.AppSettings.rcWnd_Simple);
		rcNew.right = rcNew.left+theApp.AppSettings.rcWnd.Width(); 
		rcNew.bottom = rcNew.top+theApp.AppSettings.rcWnd.Height();
		this->MoveWindow(rcNew); //set to new size  (no move)
	}




}

void CDBCTaskmanDlg::SortSimpleList(void)
{
	if(SimpleListSortEnable)
	{
		mSimpleTaskList.SortItems(SortFuncSimpleList, 0);
	//	MSB(99)
	}
}

void CDBCTaskmanDlg::DeleteProcessItem(PROCLISTDATA * pDetailsListData)
{
	int nID = 0;


	CProcess PInfo;
	PROCLISTDATA *pThisListData;
	while(1)
	{
		pThisListData = (PROCLISTDATA * ) mSimpleTaskList.GetItemData(nID);
		if(pThisListData == NULL){break;} //�����б��������
		if(pDetailsListData == NULL){nID++; continue;}
		if(pDetailsListData->PID == pThisListData->PID)
		{	
			mSimpleTaskList.DeleteItem(nID);	

			break;
		}
		nID++;

	}
}

void CDBCTaskmanDlg::OnNMRClickSimpletaskList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: Add your control notification handler code here


	CMenu PopMenu;
	CMenu *pMenu;
	PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_BASE) );
	pMenu = PopMenu.GetSubMenu(7);  //�� ��� ��Ӧ�� �˵� �ͱ�ǩ˳���Ӧ

	CPoint CurPos ;
	GetCursorPos(&CurPos); 

	pMenu->SetDefaultItem(0,1);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN,CurPos.x,CurPos.y,this);



	*pResult = 0;
}


void CDBCTaskmanDlg::OnPop_SimplelistSwitchTo()
{
	INT nSel = 0;
	nSel = mSimpleTaskList.GetNextItem( -1, LVNI_SELECTED );

	PROCLISTDATA *pData =  NULL;

	pData = (PROCLISTDATA*)mSimpleTaskList.GetItemData(nSel);

	DWORD PID = pData->PID;


	//������ 
	if(theApp.CurrentPID == PID)
	{
		theApp.m_pMainWnd->FlashWindow(0);
		theApp.m_pMainWnd->FlashWindow(1);
		theApp.m_pMainWnd->FlashWindow(0);
		return ;
	}


	//---------------------------------------------------------------------------

	BOOL Ret = FALSE;
	CWnd *pDeskTop=CWnd::GetDesktopWindow();
	CWnd  *pAppWnd = pDeskTop->GetWindow(GW_CHILD);
	//CString  StrWndCaption;
	CString Strtemp;
	DWORD WindowPID ;

	while( pAppWnd!=NULL)
	{
		if(pAppWnd->IsWindowVisible() && (!(pAppWnd->GetExStyle()&WS_EX_TOOLWINDOW) ) )  //      WS_CAPTION
		{
			GetWindowThreadProcessId(pAppWnd->m_hWnd,&WindowPID);
			if( WindowPID == PID   )
			{

				pAppWnd->BringWindowToTop();
				pAppWnd->ShowWindow(SW_RESTORE);
				pAppWnd->SetActiveWindow();

				if(theApp.AppSettings.MiniOnUse)
				{
					theApp.m_pMainWnd->ShowWindow(SW_MINIMIZE);
				}

				break;
			}

		}
		pAppWnd=pAppWnd->GetWindow(GW_HWNDNEXT);

	}




}

void CDBCTaskmanDlg::OnPop_SimplelistEndTask()
{
	theApp.Global_EndProcessesInList(&mSimpleTaskList,FALSE);


}

void CDBCTaskmanDlg::TestHungProcess(void)
{


	int Sec = (int)theApp.UpTimeSec;
	// Cambio E: throttle from every 2s to every 4s. IsHungAppWindow issues a
	// WM_NULL to every top-level desktop window. Detecting a "not responding"
	// dialog at 4s resolution is still well inside human reaction time and
	// halves the per-tick cost of enumerating + probing all CWnd children of
	// the desktop.
	if(Sec%4!=0)  return ;


	CWnd *pDeskTop=CWnd::GetDesktopWindow();
	CWnd  *pAppWnd = pDeskTop->GetWindow(GW_CHILD);

	//CString  StrWndCaption;
	CString Strtemp;
	DWORD WindowPID ;

	while( pAppWnd!=NULL)
	{
		if(!(pAppWnd->GetExStyle()&WS_EX_TOOLWINDOW)  )  //      WS_CAPTION
		{

			if(IsHungAppWindow(pAppWnd->m_hWnd)) 
			{
				map<CWnd*,int>::iterator Iter= HungWndMap.find(pAppWnd); 

				if(Iter == HungWndMap.end())//������������Ӧ�����б� 
				{
					GetWindowThreadProcessId(pAppWnd->m_hWnd,&WindowPID);
					pPageDetails->SetProcessStatusInfo(WindowPID,PS_NOTRESPONDING);
					pPageProcesses->SetProcessStatusInfo(WindowPID,PS_NOTRESPONDING);
					SetProcessStatusInfo(WindowPID,PS_NOTRESPONDING);
					HungWndMap[pAppWnd] = WindowPID;
				}

				//HungWndMap.insert(map<CWnd*,int>::value_type(pAppWnd,WindowPID));
			}	

		}

		pAppWnd=pAppWnd->GetWindow(GW_HWNDNEXT);

	}

	//������������ iterator->first �� iterator->second �ֱ�����ؼ��ֺʹ洢������


	for (map<CWnd*,int>::iterator i=HungWndMap.begin(); i!=HungWndMap.end();  i++ ) 
	{  
		CWnd *pHungWnd = i->first;
		if(pHungWnd==NULL)
		{			
			HungWndMap.erase(i); 
			continue ;
		}	
		if (!IsHungAppWindow(pHungWnd->GetSafeHwnd()))  
		{  
			pPageDetails->SetProcessStatusInfo(i->second,PS_RUNNING);
			pPageProcesses->SetProcessStatusInfo(i->second,PS_RUNNING);
			SetProcessStatusInfo(i->second,PS_RUNNING);
			HungWndMap.erase(i);  
		} 


	}

}

void CDBCTaskmanDlg::SetProcessStatusInfo(DWORD PID, int Status)
{
	PROCLISTDATA *pData =  NULL;
	CString  StrStatus ;

	switch(Status)
	{
	case PS_RUNNING:StrStatus = L"";break;
	case PS_NOTRESPONDING:StrStatus = L"Not Responding";break;
		//case 2:StrStatus = L"Running";break;
	}



	for(int i=0;i<mSimpleTaskList.GetItemCount();i++)
	{
		pData = (PROCLISTDATA *)mSimpleTaskList.GetItemData(i);
		if(pData ==NULL) continue ;
		if(pData->PID == PID)
		{
			mSimpleTaskList.SetItemText(i,1,StrStatus);
			mSimpleTaskList.Invalidate(0);

			break;
		}
	}

}

void CDBCTaskmanDlg::OnExitSizeMove()
{
	// TODO: Add your message handler code here and/or call default

	pPageDetails->mDetailsList.RedrawAllColumn = TRUE;
	pPageDetails->mDetailsList.RedrawWindow();
	pPageDetails->mDetailsList.RedrawAllColumn = FALSE;



	CDialog::OnExitSizeMove();
}

void CDBCTaskmanDlg::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	CDialog::OnActivate(nState, pWndOther, bMinimized);

	// TODO: Add your message handler code here


	//	if(pPageProcesses==NULL) return ;

	//CRect rc;
	//pPageProcesses->mTaskList.GetClientRect(rc);

	//	
	//NMCUSTOMDRAW nmcd;
	//nmcd.hdc = pPageProcesses->mTaskList.GetDC()->m_hDC;
	//nmcd.rc.left = 0;
	//nmcd.rc.top = 0;
	//nmcd.rc.right = nmcd.rc.left + (rc.right - rc.left);
	//nmcd.rc.bottom = nmcd.rc.top + (rc.bottom - rc.top);
	//nmcd.hdr.code = NM_CUSTOMDRAW;
	//nmcd.hdr.hwndFrom = pPageProcesses->GetSafeHwnd();
	//nmcd.hdr.idFrom = pPageProcesses->GetDlgCtrlID();

	//pPageProcesses->SendMessage( WM_NOTIFY, nmcd.hdr.idFrom, (LPARAM)&nmcd);











	//pPageProcesses->mTaskList.SendMessage(WM_NOTIFY, NM_CUSTOMDRAW, (LPARAM)&pNMHDR) ;
	//pPageProcesses->PostMessageW(WM_TIMER);
}

BOOL CDBCTaskmanDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your specialized code here and/or call the base class

	UINT ID= (UINT)wParam;
	switch(ID)
	{
	case ID_VIEW_EXPANDALL:
		ExpandOrCollapse(FALSE);
		break;
	case ID_VIEW_COLLAPSEALL:
		ExpandOrCollapse(TRUE);
		break;
	case ID_OPTIONS_SETTINGS:
		ShellExecute(this->GetSafeHwnd(), L"open", theApp.StrCfgToolsPath,L"" ,_T(""),SW_SHOW);
		break;
	case ID_HELP_ABOUTTASKMANAGER:
		{CAboutDlg dlgAbout;	dlgAbout.DoModal();}
		break;
	case ID_VIEW_REFRESHNOW:
		pPageDetails->PostMessageW(UM_REFRESH);	pPageProcesses->RefreshList();
		break;
	case ID_OPTIONS_HIDEWHENMINIMIZED:
		theApp.AppSettings.HideWhenMini = !theApp.AppSettings.HideWhenMini;
		break;
	case ID_OPTIONS_MINIMIZEONUSE:
		theApp.AppSettings.MiniOnUse = !theApp.AppSettings.MiniOnUse ;
		break;
	case ID_FILE_EXIT:
		this->OnOK();
		break;
	case ID_OPTIONS_ALWAYSONTOP:
		theApp.AppSettings.TopMost = !theApp.AppSettings.TopMost ;
		HWND insertAfter = theApp.AppSettings.TopMost ? HWND_TOPMOST:HWND_NOTOPMOST;
		::SetWindowPos(GetSafeHwnd(),insertAfter,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
		break;

	}




	return CDialog::OnCommand(wParam, lParam);
}


void CDBCTaskmanDlg::UpdateSimpleList(void)
{


	SimpleListSortEnable = FALSE;//��ʱ��ֹ���� ����ʱ�Ὣ�������ӵ���� ��ֹmap���ң�������

	CWnd *pDeskTop=CWnd::GetDesktopWindow();

	CWnd  *pAppWnd = pDeskTop->GetWindow(GW_CHILD);
	DWORD WindowPID ;

	map<int,int> PIDList ;  //<PID,��ʱ���б��е����>

	int n =mSimpleTaskList.GetItemCount();
	PROCLISTDATA *pData =  NULL;
	for(int i=0;i<n;i++)
	{
		pData = (PROCLISTDATA *)mSimpleTaskList.GetItemData(i);
		if(pData == NULL)continue ;
		PIDList[pData->PID] = i;
		mSimpleTaskList.SetItemText(i,2,L"INVISIBLE");
	}


	//------------------- 

	//------------------- 
	while( pAppWnd!=NULL)
	{

		if(  !(pAppWnd->GetExStyle()&WS_EX_TOOLWINDOW)   )  //      WS_CAPTION    pAppWnd->IsWindowVisible() &&
		{
			GetWindowThreadProcessId(pAppWnd->m_hWnd, &WindowPID); // ����ҵ����������Ľ��� 


			map<int,int>::iterator Iter= PIDList.find(WindowPID); 
			if(Iter == PIDList.end())//���������б� ������
			{					
				if( pAppWnd->IsWindowVisible()  )//�ɼ�
				{
					n=pPageDetails->mDetailsList.GetItemCount();
					for(int i = 0;i<n;i++)
					{
						pData = (PROCLISTDATA *)pPageDetails->mDetailsList.GetItemData(i);
						if(pData->PID == WindowPID)
						{
							theApp.m_pMainWnd->PostMessageW(UM_ADD_PROC_SIMPEL_LIST,0,(LPARAM)pData);
							break;
						}
					}	
				}

			}
			else//�������б�  
			{
				if( pAppWnd->IsWindowVisible() )//�ɼ�
					mSimpleTaskList.SetItemText(Iter->second,2,L"VISIBLE"); //���Ϊɾ�� ��ʱ��Ҫɾ����ֹmap����Ŵ���
			}


		}
		pAppWnd=pAppWnd->GetWindow(GW_HWNDNEXT);

	}



	//��ʼʵ��ɾ���Ӻ���ǰɾ
	n =mSimpleTaskList.GetItemCount(); 
	CString StrMark;
	for(int i=n-1;i>=0;i--)
	{
		StrMark =mSimpleTaskList.GetItemText(i,2);
		if(StrMark.Compare(L"INVISIBLE")==0)
		{
			mSimpleTaskList.DeleteItem(i);
		}


	}




	SimpleListSortEnable = TRUE;

	/*if(FlagAutoSortSimpleList)
	{
	SortSimpleList();
	FlagAutoSortSimpleList = FALSE;
	}*/

}

void CDBCTaskmanDlg::_HICONFromCBitmap(CBitmap& bitmap,HICON *phIcon)
{
	HDC dc = ::GetDC(NULL);

	DestroyIcon(*phIcon);
	BITMAP bmp;  
	bitmap.GetBitmap(&bmp);  
	HBITMAP hbmMask = ::CreateCompatibleBitmap(dc,bmp.bmWidth, bmp.bmHeight);
	ICONINFO ii = {0};ii.fIcon = TRUE;
	ii.hbmColor = bitmap; 
	ii.hbmMask = hbmMask; 	 
	*phIcon= ::CreateIconIndirect(&ii);//һ��������Ҫ��ע����DestroyIcon�����ͷ�ռ�õ��ڴ漰��Դ  
	::DeleteObject(hbmMask);  
	::DeleteDC(dc); 



}

void CDBCTaskmanDlg::OnNcDestroy()
{
	CDialog::OnNcDestroy();
	// TODO: Add your message handler code here

	CWnd *pShellTray = CWnd::FindWindowW(L"Shell_TrayWnd",NULL);
	::Shell_NotifyIcon(NIM_DELETE, &mNotifyIconData);

	if(pShellTray!=NULL)
	{
		pShellTray=pShellTray->FindWindowExW(pShellTray->GetSafeHwnd(),NULL,L"TrayNotifyWnd",NULL);		
		if(pShellTray!=NULL)pShellTray->Invalidate();
	}



}



void CDBCTaskmanDlg::SetTrayIcon(DWORD Message)
{
	

	if(Message == NIM_ADD)
	{
		mNotifyIconData.uVersion  = NOTIFYICON_VERSION_4;		
		mNotifyIconData.cbSize    = sizeof(NOTIFYICONDATA); 	
		mNotifyIconData.hWnd = theApp.m_pMainWnd->GetSafeHwnd();//��������ͼ��֪ͨ��Ϣ�Ĵ��ھ��
		mNotifyIconData.uCallbackMessage = UM_NOTIFYICON;                //Ӧ�ó��������ϢID��
		mNotifyIconData.uFlags    = NIF_MESSAGE | NIF_ICON | NIF_TIP;               //ͼ������ԣ����ó�ԱuCallbackMessage��hIcon��szTip��Ч
		StringCchCopy(mNotifyIconData.szTip, 128, L"DBC Task Manager");
		mLastTrayCpuLevel = -1; // forzar icono en este primer NIM_ADD
	
	}




	// Calcular nivel de icono segun %CPU. Indice 0..11 = IconGroup127..138.
	// LoadIcon retorna HICON shared del binario; no requiere DestroyIcon.
	double Cpu = theApp.PerformanceInfo.CpuUsage;
	if(Cpu < 0.0)   Cpu = 0.0;
	if(Cpu > 100.0) Cpu = 100.0;
	int nLevel = (int)((Cpu / 100.0) * 11.0);
	if(nLevel < 0)  nLevel = 0;
	if(nLevel > 11) nLevel = 11;

	if(Message != NIM_ADD && nLevel == mLastTrayCpuLevel)
	{
		return;
	}

	HICON hTray = (HICON)::LoadIcon(theApp.m_hInstance,
		MAKEINTRESOURCE(IDI_TRAY_CPU1 + nLevel));
	if(hTray != NULL)
	{
		mNotifyIconData.hIcon = hTray;
		mLastTrayCpuLevel = nLevel;
		::Shell_NotifyIcon(Message, &mNotifyIconData);
	}
}


LRESULT CDBCTaskmanDlg::OnTrayIcon(WPARAM wParam, LPARAM lParam)
{
	if(lParam == WM_LBUTTONDBLCLK)   
	{
		//Shell_NotifyIcon(NIM_DELETE,&m_nfData);
		ShowWindow(SW_SHOWNORMAL);
		SetForegroundWindow();
	}
	else if(lParam == WM_RBUTTONUP)  //M_CONTEXTMENU
	{
		CMenu PopMenu;
		CMenu *pMenu;
		PopMenu.LoadMenuW(MAKEINTRESOURCE(IDR_MENU_SHELL) );
		pMenu = PopMenu.GetSubMenu(0);  
		CPoint CurPos ;
		GetCursorPos(&CurPos); 
		pMenu->SetDefaultItem(ID_NOTIFY_RESTORE,MF_BYCOMMAND);


		pMenu->TrackPopupMenu(TPM_LEFTALIGN,CurPos.x,CurPos.y,this);

	}
	else if(lParam == WM_LBUTTONUP)  //M_CONTEXTMENU
	{
		if( (!IsWindowVisible()) || IsIconic())
		{
			CMiniPerfermanceDlg MiniDlg;
			MiniDlg.DoModal();
		}

	}
	else if(lParam == WM_MOUSEMOVE)  //M_CONTEXTMENU
	{
		CString StrTip;
		int nCpu  = (int)(theApp.PerformanceInfo.CpuUsage       + 0.5);
		int nMem  = (int)(theApp.PerformanceInfo.MemoryUsage    + 0.5);
		int nDisk = (int)(theApp.PerformanceInfo.TotalDiskUsage + 0.5);
		int nNet  = (int)(theApp.PerformanceInfo.TotalNetUsage  + 0.5);

		// Cache: WM_MOUSEMOVE se dispara docenas de veces por segundo cuando
		// el cursor esta sobre el icono. Solo pedimos NIM_MODIFY si algun
		// valor entero (ya redondeado) cambio respecto al ultimo aplicado.
		if(nCpu == mLastTipCpu && nMem == mLastTipMem &&
		   nDisk == mLastTipDisk && nNet == mLastTipNet)
		{
			return 0;
		}
		mLastTipCpu  = nCpu;
		mLastTipMem  = nMem;
		mLastTipDisk = nDisk;
		mLastTipNet  = nNet;

		StrTip.Format(
			L"CPU     %d%%\n"
			L"Memoria %d%%\n"
			L"Disco   %d%%\n"
			L"Red     %d%%",
			nCpu, nMem, nDisk, nNet);
		StringCchCopy(mNotifyIconData.szTip, 128, StrTip);
		// Forzar al shell a releer el tooltip. Sin NIM_MODIFY aqui, Explorer
		// muestra el tip cacheado desde el NIM_ADD original y nunca se actualiza.
		mNotifyIconData.uFlags |= NIF_TIP;
		::Shell_NotifyIcon(NIM_MODIFY, &mNotifyIconData);
	}








	return 0;

}



void CDBCTaskmanDlg::OnPop_NotifyRestore()
{
	ShowWindow(SW_SHOWNORMAL);
	SetForegroundWindow();
}


void CDBCTaskmanDlg::OnMM_GroupByType()
{
	APPLISTDATA *pData =  NULL;
	pData = (APPLISTDATA *) pPageProcesses->mTaskList.GetItemData(0);//�з���ʱ���0���Ƿ������
	if(pData!=NULL)
	{
		if(pData->SubType == -1)//�б���
		{
			theApp.AppSettings.GroupByType = FALSE;
			pPageProcesses->_RemoveGroupTitle();
			pPageProcesses->ReSort(FALSE);

		}
		else
		{		

			theApp.AppSettings.GroupByType = TRUE;
			pPageProcesses->mTaskList.CurrentSortColumn=-1;
			pPageProcesses->Sort(PROCLIST_NAME);

		}
	}
}



void CDBCTaskmanDlg::_RegisterForDevChange(void)
{
	DEV_BROADCAST_DEVICEINTERFACE DevInt;
	memset(&DevInt,0,sizeof(DEV_BROADCAST_DEVICEINTERFACE));
	DevInt.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	DevInt.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	//DevInt.dbcc_classguid = DevGuid; //�豸�� GUID, ��ͬ���豸�в�ͬ�� GUID������ʵ������趨
	//hDevNotify = 

	//RegisterDeviceNotification(this->GetSafeHwnd(), &DevInt, DEVICE_NOTIFY_WINDOW_HANDLE);
	RegisterDeviceNotification(this->GetSafeHwnd(), &DevInt, DEVICE_NOTIFY_WINDOW_HANDLE|DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);

}

LRESULT CDBCTaskmanDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your specialized code here and/or call the base class

	bool allzero=true;
	//  size_t i;

	if(message==WM_DEVICECHANGE)
	{
		if(wParam==DBT_DEVICEARRIVAL)//�����豸����ϵͳ 
		{ 
			int i=2;
			int CurrentMaxDiskID = 0;
			while(1)
			{
				PerferListData * pData;
				pData =(PerferListData *)(pPagePerformance->pViewBox->mPItemList.GetItemData(i));
				if(pData==NULL)break;
				if(pData->Type == PM_ETHERNET)break;
				if(pData->ID>CurrentMaxDiskID)
				{
					CurrentMaxDiskID = pData->ID; //��ȡ��ǰ���ID ����ȷ�������ӵĴ���ID
				}
				i++;

			}

			int DiskCount = i-2;

			pPagePerformance->pViewBox->CurrentDiskCount = DiskCount;
			pPagePerformance->pViewBox->AddDiskToList(CurrentMaxDiskID+1);

			i=2;
			while(1)
			{
				PerferListData * pData;
				pData =(PerferListData *)(pPagePerformance->pViewBox->mPItemList.GetItemData(i));
				if(pData==NULL)break;
				if(pData->Type == PM_ETHERNET)break;
				i++;
			}

			int NewAddDiskCount = i-2-DiskCount;

	 

			while(1)
			{
				
				PerferListData * pData;
				pData =(PerferListData *)(pPagePerformance->pViewBox->mPItemList.GetItemData(i));
				if(pData==NULL)break;
				if(pData->Type != PM_ETHERNET) break;

			
				map<int, int>::iterator Iter = NetAdapterList.find(pData->ID); // <����ID,�б���ĿID>
				if(Iter != NetAdapterList.end())//����
				{
					
					Iter->second += NewAddDiskCount;
				
				}


				i++;
			}



			
		 
		}
		else if(wParam==DBT_DEVICEREMOVECOMPLETE)
		{
			


			int i=2;
			while(1)
			{				
				PerferListData * pData;
				pData =(PerferListData *)(pPagePerformance->pViewBox->mPItemList.GetItemData(i));
				if(pData==NULL)break;
				if(pData->Type != PM_DISK)break;

				CString StrDev ;
				StrDev.Format(L"\\\\.\\PhysicalDrive%d",pData->ID);
			
				HANDLE hDevice=CreateFile(StrDev, 0,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);

				if(hDevice!=INVALID_HANDLE_VALUE)
				{
					 CloseHandle(hDevice);
					 i++;
				}
				else
				{
					 
					
					map<int, int>::iterator Iter = NetAdapterList.find(pData->ID); // <����ID,�б���ĿID>
					if(Iter != NetAdapterList.end())//����
					{
						NetAdapterList.erase(Iter); //���
				
					}

					//ѡ����һ��
					pPagePerformance->pViewBox->mPItemList.SetItemState(i-1,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);	 


					if (pData->pInfoBox!=NULL)
					{
						pData->pInfoBox->DestroyWindow();
						pData->pInfoBox=NULL;
					}

					if (pData->pOtherWnd!=NULL)
					{
						pData->pOtherWnd->DestroyWindow();
						pData->pOtherWnd=NULL;
					}
					if (pData->pWaveBox!=NULL)
					{
						pData->pWaveBox->DestroyWindow();
						pData->pWaveBox=NULL;
					}

					pPagePerformance->pViewBox->mPItemList.DeleteItem(i);
					

					delete pData ;  pData = NULL;

					break;


				}			

				
			}



			//----------------



			while(1)
			{
				
				PerferListData * pData;
				pData =(PerferListData *)(pPagePerformance->pViewBox->mPItemList.GetItemData(i));
				if(pData==NULL)break;
				if(pData->Type != PM_ETHERNET)break;
			
				map<int, int>::iterator Iter = NetAdapterList.find(pData->ID); // <����ID,�б���ĿID>
				if(Iter != NetAdapterList.end())//����
				{					
					Iter->second --;
				
				}

				i++;
			}


			

		}
		//else if(DBT_DEVICEREMOVECOMPLETE



		//DEV_BROADCAST_HDR* pDev=(DEV_BROADCAST_HDR*)lParam; 
		//	
		//int N=pDev->dbch_devicetype;
		//if(N=DBT_DEVTYP_VOLUME )//ֻ�����ƶ��洢�豸 
		//{ 
		//	CString Str;
		//	//Str.Format(rand());

		//	//this->SetWindowTextW(L"USB Disk "+Str);
		//}
		//else
		//{
		//	CString Str;
		//	//Str.Format(rand());
		//	//this->SetWindowTextW(L"NNNNNNNNNNNNNNN"+Str);
		//}


	}



	return CDialog::WindowProc(message, wParam, lParam);
}

void CDBCTaskmanDlg::OnPop_SimplelistOpenfilelocation()
{
	INT nSel = 0;
	nSel = mSimpleTaskList.GetNextItem( -1, LVNI_SELECTED );
	PROCLISTDATA *pData =  NULL;

	pData = (PROCLISTDATA*)mSimpleTaskList.GetItemData(nSel);
	if(pData!=NULL)
	{		
		CProcess mProcInfo;
		CString StrTargetPath = mProcInfo.GetPathName(pData->hProcess);
		OpenFileLocation(StrTargetPath);
	}
}

void CDBCTaskmanDlg::OnPop_SimplelistProperties()
{
	INT nSel = 0;
	nSel = mSimpleTaskList.GetNextItem( -1, LVNI_SELECTED );
	PROCLISTDATA *pData;

	pData = (PROCLISTDATA*)mSimpleTaskList.GetItemData(nSel);
	if(pData!=NULL)
	{		
		CProcess mProcInfo;
		CString StrTargetPath = mProcInfo.GetPathName(pData->hProcess);
		OpenPropertiesDlg(StrTargetPath);
	}

}

void CDBCTaskmanDlg::OnPop_SimplelistAlwaysOnTop()
{
	//���´������� ѡ���� ������ Topmost!!!
	/*

	INT nSel = 0;
	nSel = mSimpleTaskList.GetNextItem( -1, LVNI_SELECTED );
	PROCLISTDATA *pData;

	pData = (PROCLISTDATA*)mSimpleTaskList.GetItemData(nSel);
	if(pData==NULL) return;



	CWnd *pDeskTop=CWnd::GetDesktopWindow();
	CWnd  *pAppWnd = pDeskTop->GetWindow(GW_CHILD);
	DWORD WindowPID ;

	//------------------- 
	while( pAppWnd!=NULL)
	{
	if(  !(pAppWnd->GetExStyle()&WS_EX_TOOLWINDOW)   )  //      WS_CAPTION    pAppWnd->IsWindowVisible() &&
	{
	GetWindowThreadProcessId(pAppWnd->m_hWnd, &WindowPID); // ����ҵ����������Ľ��� 

	if(pData->PID == WindowPID)
	{
	//HWND insertAfter = theApp.AppSettings.TopMost ? HWND_TOPMOST:HWND_NOTOPMOST;
	::SetWindowPos(pAppWnd->GetSafeHwnd(),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
	}

	}
	pAppWnd=pAppWnd->GetWindow(GW_HWNDNEXT);

	}

	*/
	//



}


void CDBCTaskmanDlg::InitUpTime(void)
{
	CString str,str1;     

	//��ȡϵͳ����ʱ��         

	ULONGLONG UpTime=GetTickCount64();  


	theApp.UpTimeDay=(int)(UpTime/3600000/24);
	UpTime%=(3600000*24);  



	theApp.UpTimeHour=(int)(UpTime/3600000);    //hour
	UpTime%=3600000;         


	theApp.UpTimeMin=(int)(UpTime/60000);   //Minute
	UpTime%=60000;         

	theApp.UpTimeSec= (float)(UpTime/1000);         //sec

}




void CDBCTaskmanDlg::AddToSimpleList(PROCLISTDATA * pData)
{

	CProcess PInfo;
	int nNewItem = mSimpleTaskList.GetItemCount();

	CString StrTaskName = PInfo.GetVerInfoString(PInfo.GetPathName(pData->hProcess),L"FileDescription");
	CString StrTemp = StrTaskName;

	StrTemp.Remove(L' ');
	if(StrTemp.GetLength()==0)
	{
		StrTaskName = pData->Name ;
		StrTaskName = StrTaskName.Left(StrTaskName.GetLength()-4);	
	}


	mSimpleTaskList.InsertItem(nNewItem,StrTaskName,pData->IconIndex);
	mSimpleTaskList.SetItemData(nNewItem,(DWORD_PTR)pData);
	SortSimpleList();
}

void CDBCTaskmanDlg::DeleteFromSimpleList(PROCLISTDATA * pData)
{
	pPageProcesses->DeleteProcessItem(pData);
	pPageUsers->DeleteProcessItem(pData);
	DeleteProcessItem(pData);


	::AfxBeginThread(Thread_DeleteData,pData);//�ӳ�ɾ����ֹ����δ��� �����Ѿ�ɾ����ɴ���


	/*delete  pData ;
	pData = NULL;*/
}

LRESULT CDBCTaskmanDlg::OnThemeChanged()
{
	 

	Sleep(6000);

	CloseThemeData(theApp.hTheme);
	theApp.hTheme=OpenThemeData(NULL, L"Explorer::ListView"); 


	CloseThemeData(pPageProcesses->mTaskList.hTheme);
	CloseThemeData(pPageUsers->mUserList.hTheme);
	CloseThemeData(pPageStartup->mStartupList.hTheme);

	pPageProcesses->mTaskList.hTheme=OpenThemeData(this->GetSafeHwnd(), L"Explorer::TreeView"); 	
	pPageUsers->mUserList.hTheme=OpenThemeData(this->GetSafeHwnd(), L"Explorer::TreeView"); 
	pPageStartup->mStartupList.hTheme=OpenThemeData(this->GetSafeHwnd(), L"Explorer::TreeView"); 

	theApp.SetUIColor();
	this->RedrawWindow();
	pPageProcesses->mTaskList.RedrawWindow();
	 

	
	 
	return 1;
}

void CDBCTaskmanDlg::SetSimpleListItemsIcon(void)
{

	int nCount = mSimpleTaskList.GetItemCount();

	for(int i=0;i<nCount;i++)
	{
		PROCLISTDATA *pData = NULL;
		pData=(PROCLISTDATA *) mSimpleTaskList.GetItemData(i);
		if(pData!=NULL)
		{
			mSimpleTaskList.SetItem(i,0, LVIF_IMAGE, NULL,pData->IconIndex,0,0,0);
		}

	}
	
}



void CDBCTaskmanDlg::OnFileSaveDetailsList()
{
	this->pPageDetails->PostMessageW(WM_COMMAND, ID_FILE_SAVELIST,  0);
}

void CDBCTaskmanDlg::SaveScreenshot(CString StrFileName)
{

	 
	HWND SrcWnd = this->GetSafeHwnd();


	CRect RcWnd;

	this->GetWindowRect(RcWnd);
	 
	HDC hSrcDC = ::GetWindowDC(SrcWnd);  
	//HDC hSrcDC = ::GetDC(SrcWnd);  
	int nWdith = RcWnd.Width();  
	int nHeigth =RcWnd.Height();  

	HDC hMemDC = CreateCompatibleDC(hSrcDC);  
	HBITMAP hMemBitmap = CreateCompatibleBitmap(hSrcDC, nWdith, nHeigth);  
	HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hMemBitmap); 

	//-------------------------------------------------------------------------------------------
	//ӳ��ģʽ  
	//int iMapMode = SetMapMode(hMemDC, MM_ANISOTROPIC);  
	//	SIZE szViewportExt, szWindowExt;  
	//	SetViewportExtEx(hMemDC, 1,  1,   &szViewportExt);  
	//	SetWindowExtEx(hMemDC, 1, 1, &szWindowExt);  

	//����ԭ��  
	//POINT ptOrg;  
	//	SetViewportOrgEx(hMemDC, 0, 0,  &ptOrg);  
	//-------------------------------------------------------------------------------------------

	//::PrintWindow(SrcWnd,hMemDC,0);


	//���ƴ���  
	BitBlt(hMemDC, 0, 0, nWdith, nHeigth, hSrcDC, 0, 0, SRCCOPY);  

	//-------------------------------------------------------------------------------------------     
	//	SetMapMode(hMemDC, iMapMode);  
	//	SetViewportExtEx(hMemDC, szViewportExt.cx, szViewportExt.cy, NULL);  
	//	SetWindowExtEx(hMemDC, szWindowExt.cx, szWindowExt.cy, NULL);  
	//SetViewportOrgEx(hMemDC, ptOrg.x, ptOrg.y, NULL);  
	//-------------------------------------------------------------------------------------------


	//ʹ��GDI+�����  
	Gdiplus::Bitmap ShotImage(hMemBitmap, NULL);  
	CLSID pngClsid;  
	_GetEncoderClsid(L"image/png", &pngClsid);  
	ShotImage.Save(StrFileName, &pngClsid);  

	//�����ָ�  
	SelectObject(hMemDC, hOldBitmap);  
	DeleteDC(hMemDC);  
	DeleteObject(hMemBitmap);  
	::ReleaseDC(SrcWnd, hSrcDC);  

}

int CDBCTaskmanDlg::_GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders     
	UINT  size = 0;         // size of the image encoder array in bytes     

	ImageCodecInfo* pImageCodecInfo = NULL;    

	Gdiplus::GetImageEncodersSize(&num, &size);    
	if(size == 0)    
		return -1;  // Failure     

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));    
	if(pImageCodecInfo == NULL)    
		return -1;  // Failure     

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);    

	for(UINT j = 0; j < num; ++j)    
	{    
		if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )    
		{    
			*pClsid = pImageCodecInfo[j].Clsid;    
			free(pImageCodecInfo);    
			return j;  // Success     
		}        
	}    

	free(pImageCodecInfo);    
	return -1;  // Failure     
}

void CDBCTaskmanDlg::OnFileSaveScreenshot()
{
	//Save Screenshot...

	CString StrDefaultFileName;

	StrDefaultFileName = L"DBCTaskman_";

	CString StrTime;//��ȡϵͳʱ��	
	CTime tm;
	tm=CTime::GetCurrentTime();
	StrTime=tm.Format("(%Y%m%d-%X)");
	StrTime.Remove(L':');
	StrDefaultFileName += StrTime;

	CFileDialog FileDlg	(FALSE, L".PNG", StrDefaultFileName, OFN_CREATEPROMPT | OFN_PATHMUSTEXIST , L"PNG File (*.PNG)|*.PNG||");

	if(FileDlg.DoModal() ==IDOK)
	{

		
		BringWindowToTop();	
		ShowWindow(SW_RESTORE);	
		::SetWindowPos(GetSafeHwnd(),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
	 	SetForegroundWindow();
		 
		SaveScreenshot(FileDlg.GetPathName());

		HWND insertAfter = theApp.AppSettings.TopMost ? HWND_TOPMOST:HWND_NOTOPMOST;
		::SetWindowPos(GetSafeHwnd(),insertAfter,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
			

	}
	
}

void CDBCTaskmanDlg::OnLvnKeydownSimpletaskList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);
	// TODO: Add your control notification handler code here
	if(pLVKeyDow->wVKey == VK_DELETE)
	{
		OnPop_SimplelistEndTask();
	}
	*pResult = 0;
}

void CDBCTaskmanDlg::OnDestroy()
{
	CDialog::OnDestroy();

	// TODO: Add your message handler code here
	//ɾ�������õı�ǡ� 
	::RemoveProp(m_hWnd,L"DBC_TASKMAN_001"); 
}

void CDBCTaskmanDlg::SetUpdateSpeed(float UpdateTime, UINT TimeStrID)
{

	KillTimer(0);
	theApp.AppSettings.TimerStep = UpdateTime;
	if(UpdateTime == -1) return;

	SetTimer(0,(int)(UpdateTime*1000),NULL);	
	_SetTimeTipStr(TimeStrID);
	InitUpTime();

}

void CDBCTaskmanDlg::ExpandOrCollapse(BOOL Collapse)
{

	CCoolListCtrl * pCurrentCoolList = NULL;
	int Count =0;

	if(pPageProcesses->IsWindowVisible())
	{
		pCurrentCoolList = & pPageProcesses->mTaskList;
		Count = pPageProcesses->mTaskList.GetItemCount();
	}
	else if(pPageUsers->IsWindowVisible())
	{
		pCurrentCoolList = & pPageUsers->mUserList;
		Count = pPageUsers->mUserList.GetItemCount();
	}
	else
	{
		return;
	}

	if(pCurrentCoolList == NULL)return;

	Count = pCurrentCoolList->GetItemCount();
	
	for (int i= Count-1;i>=0;i--)
	{
		APPLISTDATA *pListData  = (APPLISTDATA *) pCurrentCoolList->GetItemData(i);

		if(pListData == NULL) continue;
		if(Collapse)
		{
			if(  pListData->SubType == PARENT_ITEM_OPEN ) 
			{
				if(pPageProcesses->IsWindowVisible())pPageProcesses->_CloseSubList(i);
				if(pPageUsers->IsWindowVisible())pPageUsers->_CloseSubList(i);

			}
		}
		else if(  pListData->SubType == PARENT_ITEM_CLOSE ) 
		{
			if(pPageProcesses->IsWindowVisible())pPageProcesses->_OpenSubList(i);
			if(pPageUsers->IsWindowVisible())pPageUsers->_OpenSubList(i);
		}

	}



}


