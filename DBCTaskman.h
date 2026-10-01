// DBCTaskman.h : main header file for the PROJECT_NAME application
//
 
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif


#include "resource.h"		// main symbols
#include <atlbase.h>
#include "afxwin.h"

 

 
// CDBCTaskmanApp:
// See DBCTaskman.cpp for the implementation of this class
//

struct  GRAPHCOLOR
{
	COLORREF BackgroundColor;
	COLORREF BorderColor;
	COLORREF  GridColorRGB;
	BYTE GridColorAlpha;
	Gdiplus::Color LineAColor;
	Gdiplus::Color FillAColor;
	Gdiplus::Color LineBColor;
	Gdiplus::Color FillBColor;
	
};


struct  PERFORMANCEINFO
{ 
	DWORD  ProcessCount;

	double CpuUsage;	 
	double MemoryUsage;
	double TotalNetUsage;
	double TotalDiskUsage;



#ifdef _M_X64
	DWORDLONG  InUsePhysMem;
#else
	DWORD  InUsePhysMem;
#endif

	DOUBLE  Mem_Modified;
	DOUBLE  Mem_Standby;



	//-------------------�̶���Ϣ
#ifdef _M_X64
	ULONGLONG  InstalledMemKB;
	DWORDLONG  TotalPhysMem;
#else
	DWORD  InstalledMemKB;
	DWORD  TotalPhysMem;
#endif

	int nNUMA_node;
	int nPhysicalProcessorPackages;
	int nProcessorCores;
	int nLogicalProcessor;
	CString  StrL1Cache;
	CString  StrL2Cache;
	CString  StrL3Cache;

	CString  StrMemoryType;




};










struct  APPSETTINGS
{ 
	DWORD Ver;
	//---status -------------
	CRect rcWnd;
	CRect rcWnd_Simple;
	BYTE ActiveTab;
	//BYTE  ShowWndMode;

	COLUMNSTATUS_EX     ColIDS_ProcList[COL_COUNT_PROC];
	COLUMNSTATUS        ColIDS_DetailList[COL_COUNT_DETAIL];
	COLUMNSTATUS_EX     ColIDS_UserList[COL_COUNT_USER];

	//------------------Settings-------------

	BYTE ProcessorDisplayMode;//��������ʾģʽ ����/�߼�/����
	BOOL TopMost;
	BOOL MiniOnUse;
	BOOL HideWhenMini;
	BOOL ShowKernelTime;
	BYTE TaskManMode;//�����߼�ģʽ
	BOOL GroupByType;
	float  TimerStep;
	BYTE PerformanceListShowGraph ;
	BOOL OnlyOneInstance;

	//------------------Percents Or Values------------------

	BYTE ProcList_MemPercents;
	BYTE ProcList_DiskPercents;
	BYTE ProcList_NetPercents;

	BYTE UserList_MemPercents;
	BYTE UserList_DiskPercents;
	BYTE UserList_NetPercents;


	//------------------------------

	GRAPHCOLOR CpuColor;
	GRAPHCOLOR MemoryColor;
	GRAPHCOLOR DiskColor;
	GRAPHCOLOR NetworkColor;


};




//-------------------------------------------------------------------------

static BOOL CALLBACK  TaskDlgCallback(HWND hWnd, UINT notification,WPARAM wParam,LPARAM lParam,LONG_PTR data)
{


	

	HWND hBoxUI = NULL ;
	HWND hCtrlNotifySink = NULL ;
	HWND hButton = NULL ;
	if(data != NULL)
	{
		hBoxUI = FindWindowEx(hWnd,NULL,L"DirectUIHWND",NULL);
		if(hBoxUI)
		{
			while(hCtrlNotifySink = FindWindowEx(hBoxUI,hCtrlNotifySink,NULL,NULL))
			{
				hButton = FindWindowEx(hCtrlNotifySink,NULL,NULL,NULL);
				WCHAR WcWindowText[MAX_PATH]={L'\0'};
				GetWindowText(hButton,WcWindowText,MAX_PATH);

				if(wcscmp(WcWindowText,(LPWSTR)data) == 0 )	break;	
			}
		}
	}

	
	 

	if(notification == TDN_DIALOG_CONSTRUCTED)
	{	 
		if(hButton!=NULL)EnableWindow(hButton,FALSE);
		 

	}
	else if(notification == TDN_VERIFICATION_CLICKED)
	{
		 if(hButton!=NULL)EnableWindow(hButton,(BOOL)wParam);
	}
	return 0;
}

static int NewStyleMessageBox(HWND ParentWnd,CString StrCaption,CString StrMainTip,CString StrContent,UINT BunttonCount,TASKDIALOG_BUTTON *pTaskDlgBtnArray,int GrayBtnID,CString StrCheckButton = NULL)
{
	TASKDIALOGCONFIG MsgDialog = { sizeof (TASKDIALOGCONFIG) };

	int SelectedButtonID = 0;
	int SelectedRadioButtonID = 0;
	BOOL VerificationChecked = FALSE;

	MsgDialog.hwndParent = ParentWnd;
	MsgDialog.pszWindowTitle = StrCaption;
	MsgDialog.pszMainInstruction = StrMainTip;

	MsgDialog.dwFlags = MsgDialog.dwFlags|TDF_USE_HICON_MAIN;
	MsgDialog.hMainIcon = ::LoadIcon(NULL,IDI_EXCLAMATION);
	MsgDialog.pszContent = StrContent;
	MsgDialog.pszVerificationText = StrCheckButton;
	
	MsgDialog.pButtons = pTaskDlgBtnArray;
	//MsgDialog.cButtons = _countof(pTaskDlgBtnArray);		 _countof������ָ��
	MsgDialog.cButtons = BunttonCount ;
	MsgDialog.pfCallback =(PFTASKDIALOGCALLBACK) TaskDlgCallback;
	if(GrayBtnID>=0)MsgDialog.lpCallbackData = (LONG_PTR)(pTaskDlgBtnArray[GrayBtnID].pszButtonText);
	
	HRESULT result = ::TaskDialogIndirect(&MsgDialog,&SelectedButtonID,&SelectedRadioButtonID,&VerificationChecked);
	
	return SelectedButtonID ;
	
	
}












//------------------��������չ��Ϣ�� ��ͣ�� �������ݣ�����������-------------
//
//static HHOOK hMsgBoxHook;  
//static LPCBT_CREATEWND cbt_createwnd;
//
//static CString StrBtnOK;
//static CString StrBtnCancel;
//static CString StrInfoTitle; //���ӵĴ��ֱ���
//
//static WNDPROC OldWndProc;  


//static INT_PTR CALLBACK MessageBoxProc(HWND hWnd,UINT Msg,WPARAM wParam,LPARAM lParam)  
//{  
//	
//	switch(Msg)  
//	{  
//
//
//	case   WM_CTLCOLORSTATIC:  	
//		break;	
//	default:  
//		return CallWindowProc(OldWndProc,hWnd,Msg,wParam,lParam);  
//	}  
//	return 0;  
//}  
//
//
//
//
//static LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam)  
//{  
//   
//    HWND hWndMsgBox;  
//    HWND hWndButton; 
//	 
//	//CWnd WndMsgBox;
//  
//    if(nCode < 0)  
//        return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);  
//  
//	switch(nCode)  
//	{  
//	case HCBT_CREATEWND:  
//		cbt_createwnd=(LPCBT_CREATEWND)lParam;  
//		//  cbt_createwnd->lpcs->style|=WS_MINIMIZEBOX;//MSDN��˵�ĺ�����ˣ����ﲢ��������  
//		// cbt_createwnd->lpcs->cx=500;  
//		// cbt_createwnd->lpcs->cy=366; 
//		break;  
//	case HCBT_ACTIVATE:   
//		// Get handle to the message box!  
//		hWndMsgBox = (HWND)wParam;   
//		
//		//SetWindowLong(hWndMsgBox, GWL_USERDATA, (LONG)this);  
//	//	OldWndProc=(WNDPROC)SetWindowLongPtr(hWndMsgBox,-4,(LONG_PTR)MessageBoxProc);
//		hWndButton = GetDlgItem(hWndMsgBox, IDOK); 
//
//		CSize  TextSize;
//	
//		if(hWndButton && StrBtnOK!=L"")	
//		{			
//			GetTextExtentExPoint(::GetDC(hWndButton),StrBtnOK,StrBtnOK.GetLength(),1024,NULL,NULL,&TextSize);
//			
//			//MSB(TextSize.cx)
//			RECT rcBtn; ::GetWindowRect(hWndButton,&rcBtn);
//
//
//			POINT ptLT; ptLT.x=rcBtn.left;   ptLT.y=rcBtn.top ;
//			POINT ptRB; ptRB.x=rcBtn.right;  ptRB.y=rcBtn.bottom;
//
//			::ScreenToClient(hWndMsgBox,&ptLT);  
//			::ScreenToClient(hWndMsgBox,&ptRB);	
//
//
//
//			ptLT.x = ptRB.x-(TextSize.cx+18);
//			::MoveWindow(hWndButton,ptLT.x,ptLT.y,TextSize.cx+18,rcBtn.bottom-rcBtn.top,1);
//		
//			SetWindowText(hWndButton, StrBtnOK); 
//
//
//		}
//
//		hWndButton = GetDlgItem(hWndMsgBox, IDCANCEL); 
//		if(hWndButton && StrBtnCancel!=L"")	
//		{
//			
//			SetWindowText(hWndButton, StrBtnCancel); 	
//		}
//
//		//::ShowWindow(hwndButton,SW_SHOW);
//		if(StrInfoTitle!= L"")
//		{
//			CRect rc;
//			HWND hWndInfo; 
//			hWndInfo = GetDlgItem(hWndMsgBox, 0xFFFF);		
//			if(hWndInfo)
//			{
//				::GetWindowRect(hWndInfo,rc);
//				CPoint Pt(rc.left,rc.top);
//				ScreenToClient(hWndMsgBox,&Pt);			
//				HWND hWndInfTitle =  CreateWindow(L"static",StrInfoTitle,WS_CHILD|WS_VISIBLE|SS_NOTIFY,Pt.x,Pt.y,rc.Width(),48,hWndMsgBox,NULL,GetModuleHandle(NULL),NULL); 
//
//				HFONT hFont =  (HFONT)GetStockObject(DEFAULT_GUI_FONT); 
//				LOGFONT logFont;
//				memset(&logFont, 0, sizeof(logFont));
//				GetObject(hFont, sizeof(logFont), &logFont);
//				logFont.lfHeight = 20;			 
//				hFont=::CreateFontIndirect(&logFont);				
//
//				::SendMessage(hWndInfTitle,WM_SETFONT,(WPARAM)hFont,0);  //���ÿؼ�����  
//
//				//::DeleteObject(hFont);
//			}
//		
//
//		}
//		return 0;  
//
//	}  
//  
//    return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);  
//}  

//static int MsgBoxEx(HWND hwnd, CString szText, CString szCaption, UINT uType,CString BtnOK=NULL,  CString BtnCancel=NULL, CString InfoTitle=NULL)  
//{  
//	StrBtnOK=BtnOK;
//	StrBtnCancel = BtnCancel;
//	StrInfoTitle = InfoTitle;
//
//	if(szCaption ==L"")
//	{
//
//		AfxGetApp()->GetMainWnd()->GetWindowTextW(szCaption);
//	}
//
//
//	int retval;  
//	hMsgBoxHook = SetWindowsHookEx(WH_CBT,CBTProc,NULL,GetCurrentThreadId());  
//	
//	if(InfoTitle!=L"")
//	{
//		//��������
//		szText= L"\n\n\n\n"+szText;
//	}
//
//	retval = MessageBox(hwnd, szText, szCaption, uType);  
//	
//	UnhookWindowsHookEx(hMsgBoxHook);  
//
//	return retval;  

//}  




//
//static  DWORD  _GetVersion()
//{
//	HINSTANCE hinstDll;  
//    DWORD dwVersion = 0;  
//  
//    /* For security purposes, LoadLibrary should be provided with a fully-qualified  
//       path to the DLL. The lpszDllName variable should be tested to ensure that it  
//       is a fully qualified path before it is used. */  
//    hinstDll = LoadLibrary(lpszDllName);  
//      
//    if(hinstDll)  
//    {  
//        DLLGETVERSIONPROC pDllGetVersion;  
//        pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hinstDll, "DllGetVersion");  
//  
//        /* Because some DLLs might not implement this function, you must test for  
//           it explicitly. Depending on the particular DLL, the lack of a DllGetVersion  
//           function can be a useful indicator of the version. */  
//  
//        if(pDllGetVersion)  
//        {  
//            DLLVERSIONINFO dvi;  
//            HRESULT hr;  
//  
//            ZeroMemory(&dvi, sizeof(dvi));  
//            dvi.info1.cbSize = sizeof(dvi);  
//  
//            hr = (*pDllGetVersion)(&dvi);  
//  
//            if(SUCCEEDED(hr))  
//            {  
//               dwVersion = PACKVERSION(dvi.info1.dwMajorVersion, dvi.info1.dwMinorVersion);  
//            }  
//        }  
//        FreeLibrary(hinstDll);  
//    }  
//    return dwVersion;  
//}


//-------------------------------------------------------------------------



class CDBCTaskmanApp	: public CWinApp
{
public:
	CDBCTaskmanApp();

// Overrides
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation
public:
 
	
	//
	CImageList  mImagelist;
	CImageList  mImagelistNormal;
	CBrush  BkgBrush;

	// Cache de iconos por path. Evita re-extraer el mismo HICON en cada
	// Refresh(). Thread-safe via mIconCacheLock.
	CMapStringToPtr  mIconCache;
	CRITICAL_SECTION mIconCacheLock;

	// Lookup + lazy-load. Hit = O(1). Miss = mismo SHGetFileInfo del
	// _GetIconIndex original, mismo iIcon, mismo shell catalog attached.
	int  GetIconIndexCached(LPCTSTR lpszPath);

	// FIX: indice REAL del icono svchost dentro de la imagen lista. Antes
	// ProcessesView calculaba SvchostIconIndex = mImagelist.GetImageCount()-1
	// y asuma que el Add() del SVCHOST.ico habia sido el ultimo. En Win7 el
	// shell catalog que se Attach puede llegar en distintos tamanos y ese
	// calculo no es fiable -> caia al icono por defecto del shell (que en
	// algunos sistemas resuelve al icono del taskmgr.exe). Aqui guardamos
	// el indice exacto asignado por el Add() en InitInstance.
	int mSvchostIconIndex;
	int mGenericAppIconIndex; // 0 fallback por defecto

	ULONG_PTR m_gdiplusToken;  

	PERFORMANCEINFO PerformanceInfo;
	DECLARE_MESSAGE_MAP()
public:
	DWORD CurrentPID;
	int nAppWnd;
	int nBkgWnd;
	
	APPSETTINGS AppSettings;
	float UpTimeSec;
	int UpTimeMin;
	int UpTimeHour;
	int UpTimeDay;

	MYAPIGETPROCDEP GetProcessDEPPolicy;
	 
	BOOL StartPerformancePageTimer;
	void LoadAppSettings(BOOL ReLoad = FALSE);
	void SaveAppSettings(void);
	CWnd *pButtonFOM;
	void _UpgradeRights(void);
	BYTE FlagWindowStatus;
	BOOL  ElevateCurrentProcess(CString  StrCmdLine);
	BOOL _IsAdministratorNow(void);
	//

	CTabCtrl *pMainTab;
	CFormView *pSelPage;
	int MenuAndTabHeight;
	HTHEME hTheme;
	CString StrCfgFileName;

	CFont mTitleFont;	

	BOOL FlagThemeActive;
	void InitAll(void);
	BOOL FlagIsX64;
	
	BOOL FlagIsAdminNow;
	BOOL FlagSummaryView;
	COLORREF WndBkgColor;
	COLORREF WndTextColor;
	COLORREF CoolHdrColor;
	COLORREF GroupTitleTextColor;
	COLORREF DisableTextColor;
	void SetUIColor(void);

	COLORREF WndFrameColor;



	HICON hBigIcon;
	void LoadAllString(void);
	BOOL IsInstanceExist(void);
	CString StrAppFullPath;
	int Global_ShowOperateTip(CString StrProcessName,UINT MainTipStrID,UINT ContentStrID,UINT ButtonStrID, UINT GrayBtnID = -1, CString StrCheckBox = NULL);
	CString StrCfgToolsPath;
	void Global_EndProcessesInList(CListCtrl* pList,BOOL ShowTipWhenOnlyOne);
	BOOL FlagSysIs32Bit;
	BOOL IsChineseEdition;
};


// ----------------------------------------------------------------------------
// DIAG: lista de procesos � logging defensivo opt-in.
// Se activa creando un archivo vacio llamado dbc_listdiag.log junto al exe.
// El archivo de salida es dbc_listdiag.out. Usar para diagnosticar
// desbordamientos, items colgados, items duplicados, etc.
// ----------------------------------------------------------------------------


// (eliminado #endif huerfano)
extern BOOL _ListDiagEnabled(void);
extern void _ListDiagLog(const char* fmt, ...);
extern void _ListDiagDump(const char* szTag, int nCount, void* pListCtrl);
extern CDBCTaskmanApp theApp;
extern map<DWORD,PVOID> Map_PidToData;
extern  map<CWnd*,int> HungWndMap;
extern  map<int, int> NetAdapterList;

// FIX T2: CRITICAL_SECTION global (API Win32, igual patron que
// PerfPidIoCache.cpp:s_PidIoLock) que protege todos los accesos a
// Map_PidToData. Map_PidToData se lee y se escribe desde multiples
// sitios (Thread_MonitorCreateAndExit, OnUMTimer, _OpenSubList de
// UsersView, OnPop_Properties, _EndProcessTree, GetThreadsConutAllItem).
// Sin lock, una iteracion puede leer memoria liberada cuando otro hilo
// borra una entrada. Usar siempre MAP_LOCK / MAP_UNLOCK alrededor de
// CUALQUIER acceso al map. El lock es muy poco contended (accesos
// tipicos son <1ms) y solo afecta a paths de UI no al hot path.
extern CRITICAL_SECTION g_MapDataLock;
