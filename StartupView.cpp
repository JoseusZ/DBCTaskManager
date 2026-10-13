// StartupView.cpp : implementation file
//

#include "stdafx.h"
#include "DBCTaskman.h"
#include "StartupView.h"
//-----------------------
#include "ProcessInfo.h"
#include <TlHelp32.h>  // CreateToolhelp32Snapshot / Process32FirstW for the
                       // "Currently running" column. Native Win32, fully
                       // supported on Windows 7.

#define   MySetColumnWidth(n,x)    {mStartupList.SetColumnWidth(n,x); ColIDS_StartupList[n].ColWidth = x;}



//*********************************************************************
 

static UINT Thread_LoadSartupItem(LPVOID pParam)
{
	CPageStartup  *pView=(CPageStartup *)pParam;
	if(pView == NULL){ AfxEndThread(0,TRUE); return -1; }//


	 

 	if (CoInitialize(NULL) == S_OK)
	{		
		

		pView->mStartupList.SetRedraw(0); //必须先停止更新 否则在在现场中操作有问题
		pView->ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
		pView->ListStartupItem(L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run");
		pView->ListStartupFromMenu();
		pView->mStartupList.SetRedraw(1);
		pView->mStartupList.CoolheaderCtrl.Invalidate();
	
	 	CoUninitialize();
	 
	}

	// 
	AfxEndThread(0,TRUE);
	return 0;


}


//**********************************************************************




// CPageStartup

IMPLEMENT_DYNCREATE(CPageStartup, CFormView)

CPageStartup::CPageStartup()
	: CFormView(CPageStartup::IDD)
	
{

}

CPageStartup::~CPageStartup()
{
}

void CPageStartup::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STARTUP, mStartupList);
}

BEGIN_MESSAGE_MAP(CPageStartup, CFormView)
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
	ON_NOTIFY(NM_RCLICK, IDC_STARTUP, &CPageStartup::OnNMRClickStartup)
	ON_COMMAND(ID_STARTUPLIST_OPENFILELOCATION, &CPageStartup::OnPop_Openfilelocation)
	ON_COMMAND(ID_STARTUPLIST_PROPERTIES, &CPageStartup::OnPop_Properties)
//	ON_WM_SHOWWINDOW()
//ON_WM_PAINT()
ON_COMMAND(ID_STARTUPLIST_JUMPTOENTRY, &CPageStartup::OnPop_JumpToEntry)
ON_COMMAND(ID_STARTUPLIST_SEARCHONLINE, &CPageStartup::OnPop_Searchonline)

// Header right-click -> column visibility context menu items. A single
// ON_COMMAND_RANGE handler maps each ID_STARTUPCOL_* to the matching
// STARTUPLIST_* column index so the menu item index == column index.
ON_COMMAND_RANGE(ID_STARTUPCOL_NAME, ID_STARTUPCOL_CMDLINE, &CPageStartup::OnPop_StartupColumnToggle)
END_MESSAGE_MAP()


// CPageStartup diagnostics

#ifdef _DEBUG
void CPageStartup::AssertValid() const
{
	CFormView::AssertValid();
}

#ifndef _WIN32_WCE
void CPageStartup::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif
#endif //_DEBUG


// CPageStartup message handlers

void CPageStartup::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);

	
	/*CRect rc;	
	CRect rcParent;
	GetClientRect(rc);	 

	rc.InflateRect(1,1);	 
	
	rc.bottom-=50;	 
	mStartupList.MoveWindow(rc);*/




//------------------------------------


	CRect rc;	
	CRect rcHeader;
	CRect rcParent;
	GetClientRect(rc);



	if(rc.Height()<110)
	{
		if(mStartupList.CoolheaderCtrl.IsWindowVisible())
			mStartupList.CoolheaderCtrl.ShowWindow(SW_HIDE);
	}
	else
	{
		if(!mStartupList.CoolheaderCtrl.IsWindowVisible())
			mStartupList.CoolheaderCtrl.ShowWindow(SW_SHOW);
	}




	rc.InflateRect(1,1);


	//不用管headerCtrl


	rc.bottom-=50;

	rc.top=mStartupList.CoolheaderCtrl.Height;	
	mStartupList.MoveWindow(rc);


	/*CWnd *pBtn= GetDlgItem(IDC_BTN_ENDTASK);

	if(pBtn!=NULL)
	{
		CRect rcBtn(rc.right-95-15,rc.bottom+25-12,rc.right-20,rc.bottom+25+12);
		pBtn->MoveWindow(rcBtn);


	}*/

	//mStartupList._GetRedrawColumn();


	mStartupList. UpdateWindow ();





//	mBtnOpenService.MoveWindow(115,rc.bottom+25-11, 200, 25 );// 此时 rc.bottom 已经上移
 
}

void CPageStartup::InitList(void)
{
	// 如果想自绘   这个效果 就不能再设置了 ！！！

	//	SetWindowTheme(mStartupList.GetSafeHwnd(),L"explorer", NULL);	
	//	::SendMessage(mStartupList.m_hWnd, LVM_SETIMAGELIST,(WPARAM)LVSIL_SMALL, (LPARAM)theApp.mImagelist.m_hImageList);



	mStartupList.SetExtendedStyle(mStartupList.GetExtendedStyle()|LVS_EX_FULLROWSELECT| LVS_OWNERDRAWFIXED|LVS_EX_DOUBLEBUFFER |LVS_EX_HEADERDRAGDROP );  //| LVS_EX_GRIDLINES |LVS_EX_CHECKBOXES

	mStartupList.ModifyStyle (0, LVS_SHAREIMAGELISTS, 0);
	mStartupList.FullColumnCount= COL_COUNT_STARTUP; //这个是全部的列 数

	if(!theApp.FlagThemeActive)
	{
		mStartupList.ModifyStyleEx(WS_EX_CLIENTEDGE,0);
		mStartupList.ModifyStyle(0,WS_BORDER);
	}

	//---------------------------------------------------------------------

	//mStartupList.pColStatusArray = COL_SAT_PROC;
	// IsHiddenColumn and Redraw are seeded from a default-visibility table
	// so the first header-context-menu toggle goes through the correct
	// branch in the toggle logic. Leaving both at FALSE for hidden
	// columns (as we did before) made the first toggle a no-op and forced
	// the user to click twice.
	const BOOL aDefaultVisible[COL_COUNT_STARTUP] =
	{
		TRUE,  // STARTUPLIST_NAME
		TRUE,  // STARTUPLIST_PUB
		TRUE,  // STARTUPLIST_STATUS
		FALSE, // STARTUPLIST_TYPE
		FALSE, // STARTUPLIST_RUNNING
		FALSE, // STARTUPLIST_DISABLEDTIME
		FALSE, // STARTUPLIST_CMDLINE
	};
	for(int i = 0;i<COL_COUNT_STARTUP;i++)
	{
		ColIDS_StartupList[i].ColWidth = 85;
		ColIDS_StartupList[i].Align = (BYTE)DT_LEFT;
		ColIDS_StartupList[i].Cool = FALSE;
		ColIDS_StartupList[i].DrawInSubItem = TRUE;
		ColIDS_StartupList[i].IsHiddenColumn = !aDefaultVisible[i];
		ColIDS_StartupList[i].Redraw = aDefaultVisible[i] ? 1 : 0;

	}

	mStartupList.IsProcList = FALSE;
	mStartupList.IsStartupList = TRUE;

	mStartupList.InitAllColumn(ColIDS_StartupList,STR_COLUMN_STARTUP ,COL_COUNT_STARTUP);//  是全部的列数

	mStartupList.SetImageList(theApp.mImagelist.m_hImageList);  //决定了可以自绘表头 必须放在InsertColumn之后!!!!!!!!!

	mStartupList.CoolheaderCtrl.Height = 45;


	// Default widths for the always-visible columns.
	MySetColumnWidth(STARTUPLIST_NAME,200);
	MySetColumnWidth(STARTUPLIST_PUB,150);
	MySetColumnWidth(STARTUPLIST_STATUS,100);

	// MySetColumnWidth(n,0) only stores ColWidth, so the desired
	// restore-width is saved in ColWStore first (the toggle logic will
	// read it back from there when the user enables the column).
	ColIDS_StartupList[STARTUPLIST_CMDLINE].ColWStore  = 250;

	// Default-hidden columns (Win10 task manager parity: user toggles via
	// header right-click). ColWidth=0 hides the column; the ColWStore
	// value above/below is what gets applied on the first toggle.
	ColIDS_StartupList[STARTUPLIST_TYPE].ColWStore        = 100;
	ColIDS_StartupList[STARTUPLIST_RUNNING].ColWStore     = 80;
	ColIDS_StartupList[STARTUPLIST_DISABLEDTIME].ColWStore= 150;

	MySetColumnWidth(STARTUPLIST_TYPE,0);
	MySetColumnWidth(STARTUPLIST_RUNNING,0);
	MySetColumnWidth(STARTUPLIST_DISABLEDTIME,0);
	MySetColumnWidth(STARTUPLIST_CMDLINE,0);
	

	mStartupList.CurrentSortColumn = -1;

	::AfxBeginThread(Thread_LoadSartupItem,this);

	//ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
	//ListStartupItem(L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run");
	//	ListStartupFromMenu();

	
 

	 

}

void CPageStartup::ListStartupItem(CString StrRegPath)
{

	int nStartID = mStartupList.GetItemCount();


	//APPLISTDATA * pData = new APPLISTDATA;
	//pData->ItemType = -1;
	//pData->pPData = NULL;
	//mStartupList.InsertItem(nStartID,StrRegPath );	
	//mStartupList.SetItemData(nStartID,(DWORD_PTR)pData);
	//nStartID++;




	CRegKey regkey;//定义注册表类对象  
	LONG lResult;//LONG型变量－反应结果  
	lResult=regkey.Open(HKEY_LOCAL_MACHINE,LPCTSTR(StrRegPath),KEY_QUERY_VALUE ); //打开注册表键   注意 要用 读 的权限 KEY_ALL_ACCESS 在某些用户会导致错误

	if (lResult==ERROR_SUCCESS)  
	{  
		WCHAR StrData[MAX_PATH] = {0};  
		WCHAR StrValue[MAX_PATH] = {0}; 
		DWORD dwSize=90;   
		

		int i=0;
		while(1)
		{
			DWORD dwLong,dwSize,Type;

			dwLong = dwSize =MAX_PATH;
			if( ::RegEnumValue(regkey.m_hKey, i, StrValue, &dwLong, NULL, &Type, (LPBYTE) StrData, &dwSize ) == ERROR_NO_MORE_ITEMS )
			{
				
				break;
			}

			if(Type == REG_SZ )
			{
				SHFILEINFO sfi;
				memset(&sfi, 0, sizeof(sfi));
				CString StrCmd = StrData;
				CString StrPath ;
			//	StrCmd.Remove(L'"'); //不要去除这个
				//StrPath = StrCmd;
			//	SHGetFileInfo (StrCmd,  FILE_ATTRIBUTE_NORMAL,     &sfi,      sizeof(sfi),SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES |SHGFI_ICON  );

				//if(sfi.iIcon<=0)
				{
					StrPath=StrCmd.Left(StrCmd.ReverseFind(L'"'));
					StrPath.Remove(L'"');
					SHGetFileInfo (StrPath,     FILE_ATTRIBUTE_NORMAL,     &sfi,      sizeof(sfi),SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES |SHGFI_ICON  );

				}


		 
				AddItem(nStartID,StrValue,StrCmd,StrPath,L"HKEY_LOCAL_MACHINE\\"+StrRegPath,L"Registry",sfi.iIcon );
				
				nStartID++;
			}

			
			i++;
		}
	}  

	regkey.Close();//关闭注册表  



}

HBRUSH CPageStartup::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
		return theApp.BkgBrush;
}

void CPageStartup::OnNMRClickStartup(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: Add your control notification handler code here

 

	
	int  nSel = mStartupList.GetNextItem( -1, LVNI_SELECTED );

	APPLISTDATA *pData =NULL;
	pData=(APPLISTDATA *)mStartupList.GetItemData(nSel);
	if(pData==NULL)return;
	if(pData->pPData == NULL )return ; //分组标题不弹出菜单！！！


	CMenu PopMenu;
	CMenu *pMenu = NULL;

	PopMenu.LoadMenuW(MAKEINTRESOURCE( IDR_POPMENU_BASE) );

	
	
	pMenu = PopMenu.GetSubMenu(8);  //是 子项右键 对应的 菜单  
	

	

	CPoint CurPos ;
	GetCursorPos(&CurPos); 

	//pMenu->EnableMenuItem(

	pMenu->EnableMenuItem(ID_STARTUPLIST_DISABLE,MF_DISABLED|MF_GRAYED|MF_BYCOMMAND );

	//pMenu->ModifyMenuW(ID_STARTUPLIST_DISABLE,MF_BYCOMMAND,ID_STARTUPLIST_DISABLE,Str);



	
	CString StrType = mStartupList.GetItemText(nSel,3);
	if(StrType.Compare(L"Registry")!=0)
	{
		pMenu->DeleteMenu( ID_STARTUPLIST_JUMPTOENTRY,  MF_BYCOMMAND );
	}
	pMenu->TrackPopupMenu(TPM_LEFTALIGN,CurPos.x,CurPos.y,this);









	*pResult = 0;
}

void CPageStartup::OnPop_Openfilelocation()
{
	int nSel = mStartupList.GetNextItem( -1, LVNI_SELECTED );
	APPLISTDATA * pData = NULL ;
	STARTUPITEMDATA * pInfData = NULL ;

	pData = (APPLISTDATA *)mStartupList.GetItemData(nSel);

	if(pData == NULL) return;
	pInfData = (STARTUPITEMDATA *)(pData->pPData);
	if(pInfData == NULL) return;



	OpenFileLocation(pInfData->FilePath);
	
	
}

void CPageStartup::OnPop_Properties()
{
	// TODO: Add your command handler code here

	int nSel = mStartupList.GetNextItem( -1, LVNI_SELECTED );
	APPLISTDATA * pData = NULL ;
	STARTUPITEMDATA * pInfData  = NULL;

	pData = (APPLISTDATA *)mStartupList.GetItemData(nSel);

	if(pData == NULL) return;
	pInfData = (STARTUPITEMDATA *)(pData->pPData);
	if(pInfData == NULL) return;



	OpenPropertiesDlg(pInfData->FilePath);

}

int CPageStartup::ListStartupFromMenu(void)
{
	CString StrSearchPath;
	CString StrFilePath;

	int nStartID = mStartupList.GetItemCount();


	::GetSystemDirectory(StrSearchPath.GetBuffer(MAX_PATH),MAX_PATH);
	StrSearchPath.ReleaseBuffer();
	StrSearchPath=StrSearchPath.Left(3);
	StrSearchPath=StrSearchPath+L"ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\";
	


//----------------------------------------

	HANDLE hFind;
 
	WIN32_FIND_DATA FFData;
	hFind = FindFirstFile(StrSearchPath+L"*.*", &FFData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return 0;
	}
	do
	{
		if( wcscmp(FFData.cFileName,L".")==0 || wcscmp(FFData.cFileName,L"..")==0 || wcscmp(FFData.cFileName,L"desktop.ini")==0 ) continue;

	
		StrFilePath =  FFData.cFileName;
		StrFilePath = StrSearchPath+StrFilePath;
		SHFILEINFO sfi;
		memset(&sfi, 0, sizeof(sfi));
		
		SHGetFileInfo (StrFilePath,  FILE_ATTRIBUTE_NORMAL,     &sfi,      sizeof(sfi),SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES |SHGFI_ICON  );
	

		AddItem(nStartID,FFData.cFileName,StrFilePath,StrFilePath,StrSearchPath,L"Startup Group",sfi.iIcon);

		nStartID++;


		
	} while (FindNextFile(hFind, &FFData) != 0);

	FindClose(hFind);


	return 0;
}

void CPageStartup::AddItem(int ID,CString StrTitle,CString StrCmd,CString StrFilePath,CString StrEntryPath,CString StrStartupType,int IconID)
{

	APPLISTDATA * pData = new APPLISTDATA;
	STARTUPITEMDATA * pInfData  = new STARTUPITEMDATA;

	pInfData->FilePath = StrFilePath;
	pInfData->EntryPath = StrEntryPath;

	mStartupList.InsertItem(ID,StrTitle );
	mStartupList.SetItemText(ID,STARTUPLIST_CMDLINE,StrCmd);
	mStartupList.SetItemText(ID,STARTUPLIST_TYPE,StrStartupType);

	// Status: Disable is currently a disabled/greyed menu item, so every
	// registered startup is by definition Enabled in this build. When Disable
	// is implemented, this is where the per-row state will be written.
	mStartupList.SetItemText(ID, STARTUPLIST_STATUS, L"Enabled");

	// Currently running uses CreateToolhelp32Snapshot + Process32First/Next
	// (TlHelp32, available since Win95; full Win7 support). Cheap enough
	// per-row because the snapshot is taken before the loop and reused.
	CString StrResolved = StrFilePath;
	if(StrResolved.Right(4).CompareNoCase(L".lnk")==0)
	{
		CString StrLnkTarget = _GetShortcuTarget(StrResolved);
		if(!StrLnkTarget.IsEmpty()) StrResolved = StrLnkTarget;
	}
	mStartupList.SetItemText(ID, STARTUPLIST_RUNNING,
		_IsProcessRunning(StrResolved) ? L"Yes" : L"No");

	// Time disabled: the per-value FILETIME the Win10 taskmgr shows is
	// not exposed by Win32; only the parent key's last-write time is.
	// Since the Disable action isn't implemented yet we keep an em-dash
	// placeholder so the column is rendered but obviously inactive.
	mStartupList.SetItemText(ID, STARTUPLIST_DISABLEDTIME, L"\u2014");

	mStartupList.SetItemData(ID,(DWORD_PTR)pData);
	pData->SubType =  PARENT_ITEM_NOSUB;
	pData->iImage = IconID;
	pData->ItemType = 0;
	pData->pPData = pInfData;

	CProcess mProcInfo;
	if(StrFilePath.Right(4).CompareNoCase(L".lnk")==0)
	{
		StrFilePath = _GetShortcuTarget(StrFilePath);
	}
	mStartupList.SetItemText(ID,STARTUPLIST_PUB,mProcInfo.GetVerInfoString(StrFilePath,L"CompanyName")  );

	//Sleep(1000);




}


CString CPageStartup::_GetShortcuTarget(CString StrLnkFile)
{

	CString StrRet;
	WCHAR lpDescFile[MAX_PATH];

	//memset(lpDescFile,0,MAX_PATH);

	IShellLink *pShellLink;

	HRESULT bReturn = CoCreateInstance (CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,IID_IShellLink, (void **)&pShellLink) >= 0;
	if(bReturn)
	{
		IPersistFile *ppf;
		bReturn = pShellLink->QueryInterface(IID_IPersistFile, (void **)&ppf) >= 0;
		if(bReturn)
		{
			bReturn = ppf->Load(StrLnkFile, TRUE) >= 0;
			if(bReturn)
			{
				pShellLink->GetPath(lpDescFile, MAX_PATH, NULL, 0);
			}
			ppf->Release ();
		}
		pShellLink->Release ();
	}

	StrRet = lpDescFile;
	return StrRet;


}

// Snapshot the running process list once and check whether any process is
// hosting the executable whose path matches StrTargetPath. Matches by
// filename only (case-insensitive), which is what the user means by
// "this startup is currently running" and avoids being thrown off by
// fully-qualified path variations / 32-vs-64 bit sys32 redirection.
BOOL CPageStartup::_IsProcessRunning(const CString& StrTargetPath)
{
	if(StrTargetPath.IsEmpty()) return FALSE;

	CString StrTargetName = StrTargetPath;
	StrTargetName.MakeLower();
	int SlashPos = StrTargetName.ReverseFind(L'\\');
	if(SlashPos >= 0) StrTargetName = StrTargetName.Mid(SlashPos + 1);
	if(StrTargetName.IsEmpty()) return FALSE;

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hSnap == INVALID_HANDLE_VALUE) return FALSE;

	PROCESSENTRY32W pe;
	ZeroMemory(&pe, sizeof(pe));
	pe.dwSize = sizeof(pe);

	BOOL bFound = FALSE;
	if(Process32FirstW(hSnap, &pe))
	{
		do
		{
			// pe.szExeFile can contain just the executable name for normal
			// processes; for some host processes (svchost, etc.) it can
			// include the full path. Lower-case and check both.
			CString StrExe = pe.szExeFile;
			StrExe.MakeLower();
			int ExeSlash = StrExe.ReverseFind(L'\\');
			CString StrExeName = (ExeSlash >= 0) ? StrExe.Mid(ExeSlash + 1) : StrExe;
			if(StrExeName == StrTargetName || StrExe == StrTargetName)
			{
				bFound = TRUE;
				break;
			}
		} while(Process32NextW(hSnap, &pe));
	}

	CloseHandle(hSnap);
	return bFound;
}



BOOL CPageStartup::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class

	if(pMsg->message==UM_ADD_STARTUPITEM)
	{
		//ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
	//ListStartupItem(L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run");
	//	ListStartupFromMenu();



	}

	// Header right-click -> show the column visibility popup, identical
	// visual behavior to ProcessesView/UsersView. CCoolheaderCtrl sends
	// UM_HEADER_RCLICK via PostMessageW to the parent (this view) when the
	// user right-clicks the header area.
	if(pMsg->message == UM_HEADER_RCLICK)
	{
		_OnHeaderRClick_Startup();
		return TRUE;
	}

	return CFormView::PreTranslateMessage(pMsg);
}

// Build and show the column visibility context menu for the Startup
// header. IDR_POPMENU_COLUMN submenu 3 ("Startup") lists every toggleable
// column with an MF_CHECKED marker on the ones that are currently shown.
// The order of entries MUST match STARTUPLIST_* indices because the menu
// position (i) is mapped to column i.
void CPageStartup::_OnHeaderRClick_Startup(void)
{
	CPoint pt;
	GetCursorPos(&pt);

	CMenu PopMenu;
	CMenu *pMenu = NULL;
	PopMenu.LoadMenuW(MAKEINTRESOURCE(IDR_POPMENU_COLUMN));
	pMenu = PopMenu.GetSubMenu(3);  // POPUP "Startup" is the 4th submenu

	if(pMenu == NULL) return;

	// Mark visible columns. IsHiddenColumn is the source of truth set by
	// ShowOrHideColumn(); ColWidth==0 is the bootstrap default for the
	// hidden-by-default columns.
	int nItem = pMenu->GetMenuItemCount();
	if(nItem > 0)
	{
		for(int i = 0; i < nItem; i++)
		{
			if(!ColIDS_StartupList[i].IsHiddenColumn && ColIDS_StartupList[i].ColWidth != 0)
			{
				pMenu->CheckMenuItem(i, MF_BYPOSITION | MF_CHECKED);
			}
		}
	}

	pMenu->TrackPopupMenu(TPM_LEFTALIGN, pt.x, pt.y, &(mStartupList.CoolheaderCtrl));
}

void CPageStartup::_JumpToRegKey(CString StrTargetKey)
{

	CString strPath=_T("Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit\\Favorites");//注册表子键路径  
	CRegKey regkey;//定义注册表类对象  
	LONG lResult;//LONG型变量－反应结果  
	lResult=regkey.Open(HKEY_CURRENT_USER,LPCTSTR(strPath),KEY_WRITE|KEY_QUERY_VALUE  ); //打开注册表键  读写两种权限 

	int FavItemCount = 0;
	CString StrValueName ;

	if (lResult==ERROR_SUCCESS)  
	{ 	 

		StrValueName.Format(L"%d",int(GetTickCount()+rand()) );
		regkey.SetStringValue(StrValueName,L"Computer\\"+StrTargetKey);

		WCHAR StrValue[MAX_PATH] = {0}; 
		WCHAR StrData[MAX_PATH] = {0};  

		while(1)
		{
			DWORD dwLong,dwSize,Type;

			dwLong = dwSize =MAX_PATH;
			if( ::RegEnumValue(regkey.m_hKey, FavItemCount, StrValue, &dwLong, NULL, &Type, (LPBYTE)StrData, &dwSize ) == ERROR_NO_MORE_ITEMS )
			{				
				break;
			}	

			FavItemCount++;
		}


	}  



	//--------------



	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcInfo;


	ZeroMemory( &StartupInfo,sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	ZeroMemory( &ProcInfo,sizeof(ProcInfo));

	WCHAR WinDir[MAX_PATH];

	GetWindowsDirectory(WinDir,MAX_PATH);

	CString StrRegedit=WinDir;

	StrRegedit=StrRegedit+L"\\regedit.exe";
	//MSB_S(StrRegedit)

	if(CreateProcess(StrRegedit,NULL,NULL,NULL,FALSE,0,NULL,NULL,&StartupInfo,&ProcInfo) ==0 ) return;
	WaitForInputIdle(ProcInfo.hProcess,INFINITE);	 


	DWORD WindowPID ;

	CWnd *pDeskTop=CWnd::GetDesktopWindow();
	CWnd  *pWnd = pDeskTop->GetWindow(GW_CHILD);


	while( pWnd!=NULL)
	{		
		GetWindowThreadProcessId(pWnd->m_hWnd, &WindowPID); // 获得找到窗口所属的进程 

		if(pWnd->IsWindowVisible()&& (!(pWnd->GetExStyle()&WS_EX_TOOLWINDOW))   ) 
		{
			if(WindowPID == ProcInfo.dwProcessId)
			{
				pWnd->ShowWindow(SW_MAXIMIZE);
				break;
			}
		}

		pWnd=pWnd->GetWindow(GW_HWNDNEXT);

	}

	if(pWnd==NULL)return;

	CMenu *pMainMenu=NULL;
	CMenu *pSubMenu=NULL;
	pMainMenu = pWnd->GetMenu();

	if(pMainMenu!=NULL)
	{
		//MSB(9)
		pSubMenu = pMainMenu->GetSubMenu(3);
		if(pSubMenu!=NULL)
		{

			UINT ID = 1281+FavItemCount;//pSubMenu->GetMenuItemID(3);
			pWnd->SendMessage(WM_COMMAND,ID);

			//MSB(ID)
		}
	}

	//Sleep(5000);

	::RegDeleteValue(regkey.m_hKey,StrValueName);
	regkey.Close();//关闭注册表  

}

void CPageStartup::OnPop_JumpToEntry()
{

	int nSel = mStartupList.GetNextItem( -1, LVNI_SELECTED );
	APPLISTDATA * pData = NULL ;
	STARTUPITEMDATA * pInfData = NULL ;

	pData = (APPLISTDATA *)mStartupList.GetItemData(nSel);

	if(pData == NULL) return;
	pInfData = (STARTUPITEMDATA *)(pData->pPData);
	if(pInfData == NULL) return;


	CString StrType = mStartupList.GetItemText(nSel,3);
	 _JumpToRegKey(pInfData->EntryPath);
	

}

void CPageStartup::OnPop_Searchonline()
{
	int nSel = mStartupList.GetNextItem( -1, LVNI_SELECTED );
	SearchOnline(mStartupList.GetItemText(nSel,0));

}

// Single ON_COMMAND_RANGE handler for every header-context-menu column
// toggle. The mapping ID_STARTUPCOL_NAME + i == STARTUPLIST_* (i) is
// guaranteed by the contiguous ID allocation in resource.h.
//
// We deliberately do NOT call CCoolListCtrl::ShowOrHideColumn here. That
// function sets the hidden column's LVCOLUMN.iOrder to 0, which is the
// project's documented way of making the cool header skip a column -
// but it has the side effect of moving the hidden column to position 0
// of the header, which causes every visible column to shift one slot to
// the right (perceived as "items jumping to the beginning"). Since the
// CListCtrl already drops 0-width columns from the header, we just
// toggle ColWidth and the state flags in our own ColIDS array and let
// the list control + cool header repaint.
void CPageStartup::OnPop_StartupColumnToggle(UINT nID)
{
	int iCol = (int)(nID - ID_STARTUPCOL_NAME);
	if(iCol < 0 || iCol >= COL_COUNT_STARTUP) return;

	COLUMNSTATUS_EX& Col = ColIDS_StartupList[iCol];

	// LVM_SETCOLUMNWIDTH dispara HDN_ITEMCHANGING, y OnHdnItemchanging
	// tiene limites minimos (80 px para Name/Publisher/Status del
	// StartupView). Sin este flag, ocultar Name (cx=0) seria forzado
	// otra vez a 80 px y la columna no se ocultaria (solo el dato
	// desaparecia por Redraw=0). PopMenuAction desactiva ese minimo y
	// la reescritura de IsHiddenColumn durante HDN_ITEMCHANGING, igual
	// que ya hace ShowOrHideColumn en CoolListCtrl.
	mStartupList.PopMenuAction = TRUE;

	if (Col.IsHiddenColumn)
	{
		// SHOW. Use ColWStore as the restore width; fall back to a
		// per-column default if ColWStore was never set (first-time
		// toggle of a default-hidden column).
		int DefWidth = Col.ColWStore;
		if (DefWidth <= 0)
		{
			switch (iCol)
			{
			case STARTUPLIST_NAME:    DefWidth = 200; break;
			case STARTUPLIST_PUB:     DefWidth = 150; break;
			case STARTUPLIST_STATUS:  DefWidth = 100; break;
			case STARTUPLIST_TYPE:    DefWidth = 100; break;
			case STARTUPLIST_RUNNING: DefWidth = 80;  break;
			case STARTUPLIST_DISABLEDTIME: DefWidth = 150; break;
			case STARTUPLIST_CMDLINE: DefWidth = 250; break;
			default:                  DefWidth = 100; break;
			}
		}

		Col.ColWidth    = DefWidth;
		Col.ColWStore   = DefWidth;
		Col.Redraw      = 1;
		Col.IsHiddenColumn = FALSE;
		mStartupList.SetColumnWidth(iCol, DefWidth);
	}
	else
	{
		// HIDE. Remember the current width so we can restore it on the
		// next toggle, then collapse the column to 0 pixels. No iOrder
		// change - the list control already drops 0-width columns from
		// the header without re-shuffling the visible ones.
		Col.ColWStore      = Col.ColWidth;
		Col.ColWidth       = 0;
		Col.Redraw         = 0;
		Col.IsHiddenColumn = TRUE;
		mStartupList.SetColumnWidth(iCol, 0);
	}

	mStartupList.CoolheaderCtrl.Invalidate();
	mStartupList.Invalidate();
}
