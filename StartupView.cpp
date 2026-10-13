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
	// If the operator toggled Enable/Disable before this background
	// load finished, bail out so we don't race with ReloadStartupList.
	if(pView->m_bLoadInProgress) { AfxEndThread(0,TRUE); return 0; }
	pView->m_bLoadInProgress = TRUE;


	 

 	if (CoInitialize(NULL) == S_OK)
	{		
		

		pView->mStartupList.SetRedraw(0); //必须先停止更新 否则在在现场中操作有问题
		// ---- ENABLED registry items (HKLM, Wow64, HKCU) - matches msconfig ----
		pView->ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", TRUE, FALSE);
		pView->ListStartupItem(L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run", TRUE, FALSE);
		pView->ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", FALSE, TRUE);
		// ---- ENABLED shortcuts from the Startup folder (All Users) ----
		pView->ListStartupFromMenu();
		// ---- DISABLED items moved by msconfig into Shared Tools\MSConfig ----
		pView->ListMsConfigDisabledReg();
		pView->ListMsConfigDisabledFolder();
		pView->mStartupList.SetRedraw(1);
		pView->mStartupList.CoolheaderCtrl.Invalidate();
	
		CoUninitialize();

			}
			pView->m_bLoadInProgress = FALSE;

			//
			AfxEndThread(0,TRUE);
	return 0;


}


//**********************************************************************




// CPageStartup

IMPLEMENT_DYNCREATE(CPageStartup, CFormView)

CPageStartup::CPageStartup()
	: CFormView(CPageStartup::IDD)
	, m_bLoadInProgress(FALSE)

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
ON_COMMAND(ID_STARTUPLIST_DISABLE, &CPageStartup::OnPop_DisableEnable)

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

void CPageStartup::ListStartupItem(CString StrRegPath, BOOL bHklm, BOOL bIsHkcu)
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
	HKEY hRoot = bIsHkcu ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
		lResult=regkey.Open(hRoot,LPCTSTR(StrRegPath),KEY_QUERY_VALUE );

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

			if(Type == REG_SZ || Type == REG_EXPAND_SZ)
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
					int QuotePos = StrCmd.ReverseFind(L'"');
					if(QuotePos > 0)
						StrPath = StrCmd.Left(QuotePos);
					else
						StrPath = StrCmd;
					StrPath.Remove(L'"');
					// REG_EXPAND_SZ values contain %SystemRoot% etc. SHGetFileInfo
					// can't - we have to expand env vars first to get an icon.
					WCHAR Buf2[MAX_PATH] = {0};
					if(ExpandEnvironmentStringsW(StrPath, Buf2, MAX_PATH) > 0)
						StrPath = Buf2;
					SHGetFileInfo (StrPath,     FILE_ATTRIBUTE_NORMAL,     &sfi,      sizeof(sfi),SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES |SHGFI_ICON  );

				}


		 
				CString StrRoot = bIsHkcu ? L"HKEY_CURRENT_USER\\" : L"HKEY_LOCAL_MACHINE\\";
				CString StrType = (StrRegPath.Find(L"Wow6432Node") >= 0) ? L"Registry (WOW64)" : L"Registry";
				AddItem(nStartID,StrValue,StrCmd,StrPath,StrRoot+StrRegPath,StrType,sfi.iIcon, L"Enabled", CString(), CString(), bIsHkcu);
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

	// Decide what the toggle action is for the selected row. Status
	// column (index 2) tells us whether this row is Enabled or Disabled.
	// We swap the label accordingly so the menu makes sense regardless
	// of which row was right-clicked.
	const CString StrStatus = mStartupList.GetItemText(nSel, STARTUPLIST_STATUS);
	const BOOL bIsDisabled = (StrStatus.CompareNoCase(L"Disabled") == 0);

	pMenu->ModifyMenuW(ID_STARTUPLIST_DISABLE,
		MF_BYCOMMAND | MF_STRING | MF_ENABLED,
		ID_STARTUPLIST_DISABLE,
		bIsDisabled ? L"Enable" : L"Disable");

	//pMenu->EnableMenuItem(

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
	

		AddItem(nStartID,FFData.cFileName,StrFilePath,StrFilePath,StrSearchPath,L"Startup Group",sfi.iIcon, L"Enabled", CString(), CString(), FALSE);

		nStartID++;


		
	} while (FindNextFile(hFind, &FFData) != 0);

	FindClose(hFind);


	return 0;
}

void CPageStartup::AddItem(int ID,CString StrTitle,CString StrCmd,CString StrFilePath,CString StrEntryPath,CString StrStartupType,int IconID,
	const CString& StrStatus, const CString& StrDisabledTime,
	const CString& StrMsCfgSubKey, BOOL bIsHkcuItem)
{

	APPLISTDATA * pData = new APPLISTDATA;
	STARTUPITEMDATA * pInfData  = new STARTUPITEMDATA;

	pInfData->FilePath = StrFilePath;
	pInfData->EntryPath = StrEntryPath;
	pInfData->MsConfigSubKey = StrMsCfgSubKey;
	pInfData->bIsHkcu = bIsHkcuItem;

	mStartupList.InsertItem(ID,StrTitle );
	mStartupList.SetItemText(ID,STARTUPLIST_CMDLINE,StrCmd);
	mStartupList.SetItemText(ID,STARTUPLIST_TYPE,StrStartupType);

	// Status is now supplied by the caller so disabled msconfig entries
	// can be shown next to enabled ones with the right label. Enabled by
	// default (callers that pass empty string get Enabled).
	mStartupList.SetItemText(ID, STARTUPLIST_STATUS,
		StrStatus.IsEmpty() ? CString(L"Enabled") : StrStatus);

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

	// Time disabled: msconfig-disabled rows pass a formatted timestamp;
	// every other row keeps the em-dash placeholder so the column stays
	// obviously inactive for items that were never disabled.
	mStartupList.SetItemText(ID, STARTUPLIST_DISABLEDTIME,
		StrDisabledTime.IsEmpty() ? CString(L"\u2014") : StrDisabledTime);

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

// ============================================================================
// Enable / Disable startup entries - mirrors HKLM's MSCONFIG behavior.
// ============================================================================
//
// (forward declaration of the recursive registry-delete helper used by
//  the Disable->Enable path; full definition lives further below.)
static LONG _RegDeleteTreeW(HKEY hParent, LPCTSTR SubKey);
//
// Two transitions are possible, dispatched by the row's current status:
//
//   Enabled -> Disabled:
//     1. Open the hive the row currently lives in (HKLM\...\Run,
//        HKLM\...\Wow6432Node\...\Run, or HKCU\...\Run).
//     2. Read the value name (item) and data (command).
//     3. Generate a new GUID-shaped subkey name.
//     4. Create HKLM\SOFTWARE\Microsoft\Shared Tools\MSConfig\startupreg\<name>
//        and write:
//            item    = original value name
//            key     = original path (e.g. SOFTWARE\Microsoft\Windows\CurrentVersion\Run)
//            hkey    = original hive ("HKLM" or "HKCU")
//            command = original data
//            YEAR/MONTH/D... = now()
//     5. Delete the original value from the Run key.
//
//   Disabled -> Enabled:
//     1. Open HKLM\...\Shared Tools\MSConfig\startupreg\<GUID subkey>.
//     2. Read item / key / hkey / command.
//     3. Open the original hive, set the value back.
//     4. Recursively delete the GUID subkey under MSConfig\startupreg.
//
// Folder rows (Startup Group) get a similar treatment against
// MSConfig\startupfolder, but the Run/Reg path used to bring them back is
// the Startup folder under ProgramData. Folder-disabled rows have
// MsConfigSubKey pointing to that subkey.
void CPageStartup::OnPop_DisableEnable()
{
	int nSel = mStartupList.GetNextItem(-1, LVNI_SELECTED);
	if(nSel < 0) return;

	APPLISTDATA *pData = (APPLISTDATA *)mStartupList.GetItemData(nSel);
	if(pData == NULL || pData->pPData == NULL) return;

	STARTUPITEMDATA *pInfData = (STARTUPITEMDATA *)pData->pPData;

	const CString StrItem       = mStartupList.GetItemText(nSel, STARTUPLIST_NAME);
	const CString StrType       = mStartupList.GetItemText(nSel, STARTUPLIST_TYPE);
	const CString StrStatus     = mStartupList.GetItemText(nSel, STARTUPLIST_STATUS);
	const BOOL    bIsDisabled   = (StrStatus.CompareNoCase(L"Disabled") == 0);
	const BOOL    bIsFolder     = (StrType.CompareNoCase(L"Startup Group") == 0)
								|| (StrType.CompareNoCase(L"Startup folder") == 0);

	if(bIsDisabled)
	{
		// -------- RE-ENABLE --------
		// Disabled entries live under HKLM\Shared Tools\MSConfig\...
		// We must read the original (key/hkey/item/command) and put the
		// value back where it used to live.
		const CString kBasePath = bIsFolder
			? CString(L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupfolder")
			: CString(L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupreg");
		const CString kSubKey   = pInfData->MsConfigSubKey;
		if(kSubKey.IsEmpty())
		{
			// Defensive fallback: enumerate MSConfig\startupreg and match
			// by item name. Shouldn't normally hit because the loaders
			// always populate MsConfigSubKey for disabled rows.
			return;
		}

		CRegKey regMsCfg;
		if(regMsCfg.Open(HKEY_LOCAL_MACHINE, kBasePath + L"\\" + kSubKey, KEY_READ) != ERROR_SUCCESS)
			return;

		TCHAR Buf[MAX_PATH] = {0}; DWORD cb = sizeof(Buf);
		CString StrMsKey, StrMsHkey, StrMsItem, StrMsCmd;
		if(regMsCfg.QueryStringValue(L"key",     Buf, &cb) == ERROR_SUCCESS) StrMsKey = Buf;
		ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
		if(regMsCfg.QueryStringValue(L"hkey",    Buf, &cb) == ERROR_SUCCESS) StrMsHkey = Buf;
		ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
		if(regMsCfg.QueryStringValue(L"item",    Buf, &cb) == ERROR_SUCCESS) StrMsItem = Buf;
		ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
		if(regMsCfg.QueryStringValue(L"command", Buf, &cb) == ERROR_SUCCESS) StrMsCmd = Buf;
		regMsCfg.Close();

		if(StrMsItem.IsEmpty()) return;

		// For folder rows there's nothing to write back to the registry:
		// the shortcut itself remains in the Startup folder and we just
		// drop the MSConfig marker. For registry rows we must restore the
		// value under the original hive+path.
		if(!bIsFolder)
		{
			HKEY hRoot = StrMsHkey.CompareNoCase(L"HKCU") == 0
				? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
			CRegKey regRun;
			if(regRun.Open(hRoot, StrMsKey, KEY_SET_VALUE) == ERROR_SUCCESS)
			{
				// REG_SZ vs REG_EXPAND_SZ: msconfig stores the original
				// type as the value data here is exactly what was in the
				// Run key. We try REG_EXPAND_SZ first when the data
				// contains a "%" so "%SystemRoot%" stays as an env-var
				// reference (matches what we removed).
				if(StrMsCmd.Find(L'%') >= 0)
					regRun.SetValue(StrMsItem, REG_EXPAND_SZ, (LPCTSTR)StrMsCmd, (DWORD)(StrMsCmd.GetLength()+1)*sizeof(TCHAR));
				else
					regRun.SetValue(StrMsItem, REG_SZ, (LPCTSTR)StrMsCmd, (DWORD)(StrMsCmd.GetLength()+1)*sizeof(TCHAR));
				regRun.Close();
			}
		}

		// Delete the MSConfig GUID subkey (recursive).
		_RegDeleteTreeW(HKEY_LOCAL_MACHINE,
			kBasePath + L"\\" + kSubKey);
	}
	else
	{
		// -------- DISABLE --------
		// Read the live value from the Run key, copy it under
		// MSConfig\startupreg (or startupfolder for shortcut rows) with a
		// fresh GUID subkey, and finally delete the original.
		if(bIsFolder)
		{
			// Folder shortcuts: just record a marker under startupfolder
			// and delete the .lnk from the Startup folder. (We are not
			// implementing the .lnk deletion here - folder-disable needs
			// a separate path. Leaving bIsFolder supported on Enable.)
			return;
		}

		CString StrRunPath = (StrType.Compare(L"Registry (WOW64)") == 0)
			? CString(L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run")
			: CString(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");

		HKEY hRoot = pInfData->bIsHkcu ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
		CRegKey regRun;
		if(regRun.Open(hRoot, StrRunPath, KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS)
			return;

		TCHAR Buf[MAX_PATH] = {0}; DWORD cb = sizeof(Buf), dwType = 0;
		LONG lRet = ::RegQueryValueEx(regRun.m_hKey, StrItem, NULL, &dwType,
			(LPBYTE)Buf, &cb);
		if(lRet != ERROR_SUCCESS || (dwType != REG_SZ && dwType != REG_EXPAND_SZ))
		{
			regRun.Close();
			return;
		}
		CString StrCmd = Buf;

		// Write a new GUID-shaped subkey under MSConfig\startupreg.
		CString StrNewSub = _MakeMsConfigSubKeyName();
		CRegKey regMsCfg;
		if(regMsCfg.Create(HKEY_LOCAL_MACHINE,
			L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupreg\\" + StrNewSub) != ERROR_SUCCESS)
		{
			regRun.Close();
			return;
		}

		regMsCfg.SetStringValue(L"item",    StrItem);
		regMsCfg.SetStringValue(L"key",     StrRunPath);
		regMsCfg.SetStringValue(L"hkey",    pInfData->bIsHkcu ? L"HKCU" : L"HKLM");
		regMsCfg.SetStringValue(L"command", StrCmd);

		SYSTEMTIME st;
		GetSystemTime(&st);
		regMsCfg.SetDWORDValue(L"YEAR",   st.wYear);
		regMsCfg.SetDWORDValue(L"MONTH",  st.wMonth);
		regMsCfg.SetDWORDValue(L"DAY",    st.wDay);
		regMsCfg.SetDWORDValue(L"HOUR",   st.wHour);
		regMsCfg.SetDWORDValue(L"MINUTE", st.wMinute);
		regMsCfg.SetDWORDValue(L"SECOND", st.wSecond);
		regMsCfg.Close();

		// Delete the live value - it's now in MSConfig\startupreg.
		::RegDeleteValue(regRun.m_hKey, StrItem);
		regRun.Close();
	}

	// Reflect the change in the UI immediately.
	ReloadStartupList();
}

// Clear every row from the list and re-run the load thread so the new
// registry state is reflected without leaving stale items behind.

// Win32 RegDeleteKeyW only deletes leaf keys with no children. MSConfig
// subkeys do have children, so use a small recursive helper built on top
// of the underlying RegEnumKeyEx + RegDeleteKeyW.
static LONG _RegDeleteTreeW(HKEY hParent, LPCTSTR SubKey)
{
	HKEY hSub = NULL;
	if(RegOpenKeyEx(hParent, SubKey, 0, KEY_READ | KEY_WRITE, &hSub) != ERROR_SUCCESS)
		return RegDeleteKey(hParent, SubKey);

	DWORD dwIdx = 0;
	TCHAR Name[MAX_PATH] = {0};
	DWORD cchName = MAX_PATH;
	FILETIME ft;
	while(RegEnumKeyEx(hSub, dwIdx, Name, &cchName, NULL, NULL, NULL, &ft) != ERROR_NO_MORE_ITEMS)
	{
		_RegDeleteTreeW(hSub, Name);
		cchName = MAX_PATH;
		ZeroMemory(Name, sizeof(Name));
	}
	RegCloseKey(hSub);
	return RegDeleteKey(hParent, SubKey);
}

void CPageStartup::ReloadStartupList()
{
	// Avoid re-entering while a previous background load is still
	// walking the registry. The background load is short, but the
	// operator could right-click faster than it finishes - and a
	// double-load used to corrupt the heap (STATUS_HEAP_CORRUPTION).
	if(m_bLoadInProgress) return;

	m_bLoadInProgress = TRUE;

	// Synchronous reload: walks the same registry keys as the worker
	// thread, but inline. The earlier "free each row's APPLISTDATA +
	// STARTUPITEMDATA then DeleteAllItems then AfxBeginThread" sequence
	// raced with the still-running original load thread and corrupted
	// the list's internal state, so we do NOT touch lParam or call
	// DeleteAllItems here - we just reset the visual state and re-emit.
	mStartupList.DeleteAllItems();
	_LoadStartupItemsSync();

	m_bLoadInProgress = FALSE;
}

// Generate a unique msconfig-style subkey name. msconfig uses 8 hex chars
// in upper-case; we add a 4-digit suffix to avoid collisions.
CString CPageStartup::_MakeMsConfigSubKeyName()
{
	const DWORD d = GetTickCount();
	CString s;
	s.Format(L"%08X%04X", d, rand() & 0xFFFF);
	return s;
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

// Synchronous loader. Same work Thread_LoadSartupItem does, but on the
// UI thread so the registry walk and the list edits can't overlap with
// anything. Used by ReloadStartupList after the operator toggles an
// Enable/Disable state - launching a second worker thread was racing
// with the original InitList thread and corrupting the heap.
void CPageStartup::_LoadStartupItemsSync()
{
	mStartupList.SetRedraw(0);
	ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", TRUE, FALSE);
	ListStartupItem(L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run", TRUE, FALSE);
	ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", FALSE, TRUE);
	ListStartupFromMenu();
	ListMsConfigDisabledReg();
	ListMsConfigDisabledFolder();
	mStartupList.SetRedraw(1);
	mStartupList.CoolheaderCtrl.Invalidate();
	mStartupList.Invalidate();
}

// ============================================================================
// MSConfig-disabled startup items
// ============================================================================
// When the user unchecks a startup entry in msconfig -> "Inicio de Windows",
// msconfig does NOT delete it. It MOVES the (key, hkey, command) triple
// from the live Run keys into:
//
//     HKLM\SOFTWARE\Microsoft\Shared Tools\MSConfig\startupreg
//     HKLM\SOFTWARE\Microsoft\Shared Tools\MSConfig\startupfolder
//
// Each subkey under startupreg is a GUID/auto-name and contains REG_SZ
// values: "key", "item", "hkey", "command" - the exact original hive, path
// under that hive, value name, and command line. Plus DWORD values YEAR,
// MONTH, DAY, HOUR, MINUTE, SECOND that record when the user disabled it.
//
// We enumerate every subkey and emit one row per disabled item, exactly as
// msconfig's UI does, so the operator can see what was disabled, where it
// used to live, the original command, and when it was disabled.

// Enumerate every subkey under the MSConfig disabled-registry location
// and add a row to the Startup list. Each subkey holds the full original
// metadata (hkey, key, command) plus the disable timestamp.
void CPageStartup::ListMsConfigDisabledReg(void)
{
	const TCHAR *kParent =
		L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupreg";

	int nStartID = mStartupList.GetItemCount();

	// HKEY_LOCAL_MACHINE is hardcoded here - this is the only hive
	// where msconfig stores disabled registry items, regardless of
	// which hive they originally came from (HKLM or HKCU).
	CRegKey regParent;
	LONG lRes = regParent.Open(HKEY_LOCAL_MACHINE, kParent, KEY_READ | KEY_ENUMERATE_SUB_KEYS);
	if(lRes != ERROR_SUCCESS) { regParent.Close(); return; }

	TCHAR SubKeyName[MAX_PATH] = {0};
	DWORD cchName = MAX_PATH;
	DWORD dwIdx = 0;

	while(RegEnumKeyEx(regParent.m_hKey, dwIdx, SubKeyName, &cchName,
		NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS)
	{
		// Open the per-item subkey (GUID or auto-name).
		CRegKey regItem;
		if(regItem.Open(regParent.m_hKey, SubKeyName, KEY_READ) == ERROR_SUCCESS)
		{
			// Read the four REG_SZ fields msconfig recorded.
			CString StrItem, StrKey, StrHkey, StrCmd;
			TCHAR Buf[MAX_PATH] = {0};
			DWORD cb = sizeof(Buf);
			if(regItem.QueryStringValue(L"item", Buf, &cb) == ERROR_SUCCESS)
				StrItem = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if(regItem.QueryStringValue(L"key", Buf, &cb) == ERROR_SUCCESS)
				StrKey = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if(regItem.QueryStringValue(L"hkey", Buf, &cb) == ERROR_SUCCESS)
				StrHkey = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if(regItem.QueryStringValue(L"command", Buf, &cb) == ERROR_SUCCESS)
				StrCmd = Buf;

			// Read the disable timestamp (DWORD Y/M/D/H/M/S).
			DWORD dwYear = 0, dwMonth = 0, dwDay = 0,
				dwHour = 0, dwMinute = 0, dwSecond = 0;
			regItem.QueryDWORDValue(L"YEAR",   dwYear);
			regItem.QueryDWORDValue(L"MONTH",  dwMonth);
			regItem.QueryDWORDValue(L"DAY",    dwDay);
			regItem.QueryDWORDValue(L"HOUR",   dwHour);
			regItem.QueryDWORDValue(L"MINUTE", dwMinute);
			regItem.QueryDWORDValue(L"SECOND", dwSecond);

			CString StrTime;
			if(dwYear != 0)
			{
				StrTime.Format(L"%02u/%02u/%04u %02u:%02u:%02u",
					dwMonth, dwDay, dwYear, dwHour, dwMinute, dwSecond);
			}

			// Reconstruct the original location path.
			CString StrRoot = (StrHkey.CompareNoCase(L"HKCU") == 0)
				? CString(L"HKEY_CURRENT_USER\\")
				: CString(L"HKEY_LOCAL_MACHINE\\");
			CString StrEntry = StrRoot + StrKey;

			// Resolve the executable icon from the command line. Many
			// entries wrap the path in quotes (e.g. "C:\Program Files\..."
			//   args). SHGetFileInfo wants the raw executable path so we
			// strip leading+trailing quotes when present.
			SHFILEINFO sfi;
			memset(&sfi, 0, sizeof(sfi));
			CString StrPath = StrCmd;
			if(StrPath.GetLength() >= 2 && StrPath[0] == L'"' && StrPath[StrPath.GetLength()-1] == L'"')
			{
				StrPath = StrPath.Mid(1, StrPath.GetLength() - 2);
			}
			SHGetFileInfo(StrPath, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
				SHGFI_SMALLICON | SHGFI_SYSICONINDEX |
				SHGFI_USEFILEATTRIBUTES | SHGFI_ICON);

			AddItem(nStartID, StrItem, StrCmd, StrPath, StrEntry,
				L"Registry", sfi.iIcon, L"Disabled", StrTime,
				SubKeyName, StrHkey.CompareNoCase(L"HKCU") == 0);
			nStartID++;
		}

		regItem.Close();

		dwIdx++;
		cchName = MAX_PATH;
		ZeroMemory(SubKeyName, sizeof(SubKeyName));
	}

	regParent.Close();
}

// Enumerate the MSConfig disabled-folder location and add a row per
// disabled shortcut that used to live in the Startup folder. Same data
// layout (key/item/folder/command + timestamp).
void CPageStartup::ListMsConfigDisabledFolder(void)
{
	const TCHAR *kParent =
		L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupfolder";

	int nStartID = mStartupList.GetItemCount();

	CRegKey regParent;
	LONG lRes = regParent.Open(HKEY_LOCAL_MACHINE, kParent, KEY_READ | KEY_ENUMERATE_SUB_KEYS);
	if(lRes != ERROR_SUCCESS) { regParent.Close(); return; }

	TCHAR SubKeyName[MAX_PATH] = {0};
	DWORD cchName = MAX_PATH;
	DWORD dwIdx = 0;

	while(RegEnumKeyEx(regParent.m_hKey, dwIdx, SubKeyName, &cchName,
		NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS)
	{
		CRegKey regItem;
		if(regItem.Open(regParent.m_hKey, SubKeyName, KEY_READ) == ERROR_SUCCESS)
		{
			CString StrItem, StrFolder, StrCmd;
			TCHAR Buf[MAX_PATH] = {0};
			DWORD cb = sizeof(Buf);
			if(regItem.QueryStringValue(L"item", Buf, &cb) == ERROR_SUCCESS)
				StrItem = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if(regItem.QueryStringValue(L"folder", Buf, &cb) == ERROR_SUCCESS)
				StrFolder = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if(regItem.QueryStringValue(L"command", Buf, &cb) == ERROR_SUCCESS)
				StrCmd = Buf;

			DWORD dwYear = 0, dwMonth = 0, dwDay = 0,
				dwHour = 0, dwMinute = 0, dwSecond = 0;
			regItem.QueryDWORDValue(L"YEAR",   dwYear);
			regItem.QueryDWORDValue(L"MONTH",  dwMonth);
			regItem.QueryDWORDValue(L"DAY",    dwDay);
			regItem.QueryDWORDValue(L"HOUR",   dwHour);
			regItem.QueryDWORDValue(L"MINUTE", dwMinute);
			regItem.QueryDWORDValue(L"SECOND", dwSecond);

			CString StrTime;
			if(dwYear != 0)
			{
				StrTime.Format(L"%02u/%02u/%04u %02u:%02u:%02u",
					dwMonth, dwDay, dwYear, dwHour, dwMinute, dwSecond);
			}

			SHFILEINFO sfi;
			memset(&sfi, 0, sizeof(sfi));
			SHGetFileInfo(StrCmd, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
				SHGFI_SMALLICON | SHGFI_SYSICONINDEX |
				SHGFI_USEFILEATTRIBUTES | SHGFI_ICON);

			AddItem(nStartID, StrItem, StrCmd, StrCmd, StrFolder,
				L"Startup folder", sfi.iIcon, L"Disabled", StrTime);
			nStartID++;
		}

		regItem.Close();

		dwIdx++;
		cchName = MAX_PATH;
		ZeroMemory(SubKeyName, sizeof(SubKeyName));
	}

	regParent.Close();
}
