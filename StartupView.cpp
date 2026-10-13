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
	CPageStartup* pView = (CPageStartup*)pParam;
	if (pView == NULL) { AfxEndThread(0, TRUE); return -1; }//
	// If the operator toggled Enable/Disable before this background
	// load finished, bail out so we don't race with ReloadStartupList.
	if (pView->m_bLoadInProgress) { AfxEndThread(0, TRUE); return 0; }
	pView->m_bLoadInProgress = TRUE;

	if (CoInitialize(NULL) == S_OK)
	{
		pView->mStartupList.SetRedraw(0); //Debe detener actualizacion
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

	AfxEndThread(0, TRUE);
	return 0;
}

//**********************************************************************

// CPageStartup
IMPLEMENT_DYNCREATE(CPageStartup, CFormView)

CPageStartup::CPageStartup()
	: CFormView(CPageStartup::IDD)
	, m_bLoadInProgress(FALSE)
	, m_nSortColumn(-1)
	, m_bSortAscending(TRUE)
{
	// No saved layout on a fresh build. 0xFF acts as a sentinel meaning
	// "this column was never shown yet" so OnPop_StartupColumnToggle can
	// decide between "right end" and "restored position" on the first
	// re-enable.
	for (int i = 0; i < COL_COUNT_STARTUP; i++)
		m_SavedIOrder[i] = 0xFF;
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

	CRect rc;
	CRect rcHeader;
	CRect rcParent;
	GetClientRect(rc);

	if (rc.Height() < 110)
	{
		if (mStartupList.CoolheaderCtrl.IsWindowVisible())
			mStartupList.CoolheaderCtrl.ShowWindow(SW_HIDE);
	}
	else
	{
		if (!mStartupList.CoolheaderCtrl.IsWindowVisible())
			mStartupList.CoolheaderCtrl.ShowWindow(SW_SHOW);
	}

	rc.InflateRect(1, 1);

	rc.bottom -= 50;
	rc.top = mStartupList.CoolheaderCtrl.Height;
	mStartupList.MoveWindow(rc);

	mStartupList.UpdateWindow();
}

void CPageStartup::InitList(void)
{
	mStartupList.SetExtendedStyle(mStartupList.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_OWNERDRAWFIXED | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);

	mStartupList.ModifyStyle(0, LVS_SHAREIMAGELISTS, 0);
	mStartupList.FullColumnCount = COL_COUNT_STARTUP;

	if (!theApp.FlagThemeActive)
	{
		mStartupList.ModifyStyleEx(WS_EX_CLIENTEDGE, 0);
		mStartupList.ModifyStyle(0, WS_BORDER);
	}

	const BOOL aDefaultVisible[COL_COUNT_STARTUP] =
	{
		TRUE,  // STARTUPLIST_NAME
		TRUE,  // STARTUPLIST_PUB
		TRUE,  // STARTUPLIST_STATUS
		TRUE,  // STARTUPLIST_TYPE    
		FALSE, // STARTUPLIST_RUNNING
		FALSE, // STARTUPLIST_DISABLEDTIME
		FALSE, // STARTUPLIST_CMDLINE
	};
	for (int i = 0;i < COL_COUNT_STARTUP;i++)
	{
		ColIDS_StartupList[i].ColWidth = 85;
		ColIDS_StartupList[i].Align = (BYTE)DT_LEFT;
		ColIDS_StartupList[i].Cool = FALSE;
		ColIDS_StartupList[i].DrawInSubItem = TRUE;
		ColIDS_StartupList[i].IsHiddenColumn = !aDefaultVisible[i];
		ColIDS_StartupList[i].Redraw = aDefaultVisible[i] ? 1 : 0;
	}

	const BOOL bHasSavedColumns = _LoadStartupColumnState();

	mStartupList.IsProcList = FALSE;
	mStartupList.IsStartupList = TRUE;

	mStartupList.InitAllColumn(ColIDS_StartupList, STR_COLUMN_STARTUP, COL_COUNT_STARTUP);

	mStartupList.SetImageList(theApp.mImagelist.m_hImageList);

	mStartupList.CoolheaderCtrl.Height = 45;

	if (!bHasSavedColumns)
	{
		MySetColumnWidth(STARTUPLIST_NAME, 200);
		MySetColumnWidth(STARTUPLIST_PUB, 150);
		MySetColumnWidth(STARTUPLIST_STATUS, 100);
		MySetColumnWidth(STARTUPLIST_TYPE, 100);

		ColIDS_StartupList[STARTUPLIST_CMDLINE].ColWStore = 250;
		ColIDS_StartupList[STARTUPLIST_RUNNING].ColWStore = 80;
		ColIDS_StartupList[STARTUPLIST_DISABLEDTIME].ColWStore = 150;

		MySetColumnWidth(STARTUPLIST_RUNNING, 0);
		MySetColumnWidth(STARTUPLIST_DISABLEDTIME, 0);
		MySetColumnWidth(STARTUPLIST_CMDLINE, 0);
	}
	else
	{
		for (int i = 0; i < COL_COUNT_STARTUP; i++)
			MySetColumnWidth(i, ColIDS_StartupList[i].ColWidth);
	}

	for (int iSync = 0; iSync < COL_COUNT_STARTUP; iSync++)
	{
		LVCOLUMNW lvCol;
		lvCol.mask = LVCF_ORDER;
		if (mStartupList.GetColumn(iSync, &lvCol))
			ColIDS_StartupList[iSync].iOrder = (BYTE)lvCol.iOrder;
	}

	if (bHasSavedColumns)
	{
		LVCOLUMNW lvName;
		lvName.mask = LVCF_ORDER;
		lvName.iOrder = 0;
		mStartupList.SetColumn(STARTUPLIST_NAME, &lvName);
		ColIDS_StartupList[STARTUPLIST_NAME].iOrder = 0;
		m_SavedIOrder[STARTUPLIST_NAME] = 0;

		for (int i = 1; i < COL_COUNT_STARTUP; i++)
		{
			// CORRECCIÓN: NO omitir las columnas ocultas. Si se omiten, el CListCtrl
			// les asignará automáticamente iOrder = 0, lo que desplazará a la columna Name.
			LVCOLUMNW lvCol;
			lvCol.mask = LVCF_ORDER;
			lvCol.iOrder = ColIDS_StartupList[i].iOrder;
			// Prevención secundaria por si quedó grabado un 0 ilegítimo:
			if (lvCol.iOrder == 0) lvCol.iOrder = (BYTE)i;
			mStartupList.SetColumn(i, &lvCol);
		}

		for (int i = 0; i < COL_COUNT_STARTUP; i++)
		{
			LVCOLUMNW lvCol;
			lvCol.mask = LVCF_ORDER;
			if (mStartupList.GetColumn(i, &lvCol))
				ColIDS_StartupList[i].iOrder = (BYTE)lvCol.iOrder;
		}

		lvName.mask = LVCF_ORDER;
		lvName.iOrder = 0;
		mStartupList.SetColumn(STARTUPLIST_NAME, &lvName);
		ColIDS_StartupList[STARTUPLIST_NAME].iOrder = 0;
	}

	mStartupList.CurrentSortColumn = -1;

	::AfxBeginThread(Thread_LoadSartupItem, this);
}

void CPageStartup::ListStartupItem(CString StrRegPath, BOOL bHklm, BOOL bIsHkcu)
{
	int nStartID = mStartupList.GetItemCount();

	CRegKey regkey;
	LONG lResult;
	HKEY hRoot = bIsHkcu ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
	lResult = regkey.Open(hRoot, LPCTSTR(StrRegPath), KEY_QUERY_VALUE);

	if (lResult == ERROR_SUCCESS)
	{
		WCHAR StrData[MAX_PATH] = { 0 };
		WCHAR StrValue[MAX_PATH] = { 0 };
		DWORD dwSize = 90;

		int i = 0;
		while (1)
		{
			DWORD dwLong, dwSize, Type;

			dwLong = dwSize = MAX_PATH;
			if (::RegEnumValue(regkey.m_hKey, i, StrValue, &dwLong, NULL, &Type, (LPBYTE)StrData, &dwSize) == ERROR_NO_MORE_ITEMS)
			{
				break;
			}

			if (Type == REG_SZ || Type == REG_EXPAND_SZ)
			{
				SHFILEINFO sfi;
				memset(&sfi, 0, sizeof(sfi));
				CString StrCmd = StrData;
				CString StrPath;

				{
					int QuotePos = StrCmd.ReverseFind(L'"');
					if (QuotePos > 0)
						StrPath = StrCmd.Left(QuotePos);
					else
						StrPath = StrCmd;
					StrPath.Remove(L'"');

					WCHAR Buf2[MAX_PATH] = { 0 };
					if (ExpandEnvironmentStringsW(StrPath, Buf2, MAX_PATH) > 0)
						StrPath = Buf2;
					SHGetFileInfo(StrPath, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_ICON);
				}

				CString StrRoot = bIsHkcu ? L"HKEY_CURRENT_USER\\" : L"HKEY_LOCAL_MACHINE\\";
				CString StrType = (StrRegPath.Find(L"Wow6432Node") >= 0) ? L"Registry (WOW64)" : L"Registry";
				AddItem(nStartID, StrValue, StrCmd, StrPath, StrRoot + StrRegPath, StrType, sfi.iIcon, L"Enabled", CString(), CString(), bIsHkcu);
				nStartID++;
			}
			i++;
		}
	}
	regkey.Close();
}

HBRUSH CPageStartup::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	return theApp.BkgBrush;
}

void CPageStartup::OnNMRClickStartup(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	int  nSel = mStartupList.GetNextItem(-1, LVNI_SELECTED);

	APPLISTDATA* pData = NULL;
	pData = (APPLISTDATA*)mStartupList.GetItemData(nSel);
	if (pData == NULL)return;
	if (pData->pPData == NULL)return;

	CMenu PopMenu;
	CMenu* pMenu = NULL;

	PopMenu.LoadMenuW(MAKEINTRESOURCE(IDR_POPMENU_BASE));
	pMenu = PopMenu.GetSubMenu(8);

	CPoint CurPos;
	GetCursorPos(&CurPos);

	const CString StrStatus = mStartupList.GetItemText(nSel, STARTUPLIST_STATUS);
	const BOOL bIsDisabled = (StrStatus.CompareNoCase(L"Disabled") == 0);

	pMenu->ModifyMenuW(ID_STARTUPLIST_DISABLE,
		MF_BYCOMMAND | MF_STRING | MF_ENABLED,
		ID_STARTUPLIST_DISABLE,
		bIsDisabled ? L"Enable" : L"Disable");

	CString StrType = mStartupList.GetItemText(nSel, 3);
	if (StrType.Compare(L"Registry") != 0)
	{
		pMenu->DeleteMenu(ID_STARTUPLIST_JUMPTOENTRY, MF_BYCOMMAND);
	}
	pMenu->TrackPopupMenu(TPM_LEFTALIGN, CurPos.x, CurPos.y, this);

	*pResult = 0;
}

void CPageStartup::OnPop_Openfilelocation()
{
	int nSel = mStartupList.GetNextItem(-1, LVNI_SELECTED);
	APPLISTDATA* pData = NULL;
	STARTUPITEMDATA* pInfData = NULL;

	pData = (APPLISTDATA*)mStartupList.GetItemData(nSel);

	if (pData == NULL) return;
	pInfData = (STARTUPITEMDATA*)(pData->pPData);
	if (pInfData == NULL) return;

	OpenFileLocation(pInfData->FilePath);
}

void CPageStartup::OnPop_Properties()
{
	int nSel = mStartupList.GetNextItem(-1, LVNI_SELECTED);
	APPLISTDATA* pData = NULL;
	STARTUPITEMDATA* pInfData = NULL;

	pData = (APPLISTDATA*)mStartupList.GetItemData(nSel);

	if (pData == NULL) return;
	pInfData = (STARTUPITEMDATA*)(pData->pPData);
	if (pInfData == NULL) return;

	OpenPropertiesDlg(pInfData->FilePath);
}

int CPageStartup::ListStartupFromMenu(void)
{
	CString StrSearchPath;
	CString StrFilePath;

	int nStartID = mStartupList.GetItemCount();

	::GetSystemDirectory(StrSearchPath.GetBuffer(MAX_PATH), MAX_PATH);
	StrSearchPath.ReleaseBuffer();
	StrSearchPath = StrSearchPath.Left(3);
	StrSearchPath = StrSearchPath + L"ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\";

	HANDLE hFind;

	WIN32_FIND_DATA FFData;
	hFind = FindFirstFile(StrSearchPath + L"*.*", &FFData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return 0;
	}
	do
	{
		if (wcscmp(FFData.cFileName, L".") == 0 || wcscmp(FFData.cFileName, L"..") == 0 || wcscmp(FFData.cFileName, L"desktop.ini") == 0) continue;

		StrFilePath = FFData.cFileName;
		StrFilePath = StrSearchPath + StrFilePath;
		SHFILEINFO sfi;
		memset(&sfi, 0, sizeof(sfi));

		SHGetFileInfo(StrFilePath, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_ICON);

		AddItem(nStartID, FFData.cFileName, StrFilePath, StrFilePath, StrSearchPath, L"Startup Group", sfi.iIcon, L"Enabled", CString(), CString(), FALSE);

		nStartID++;

	} while (FindNextFile(hFind, &FFData) != 0);

	FindClose(hFind);

	return 0;
}

void CPageStartup::AddItem(int ID, CString StrTitle, CString StrCmd, CString StrFilePath, CString StrEntryPath, CString StrStartupType, int IconID,
	const CString& StrStatus, const CString& StrDisabledTime,
	const CString& StrMsCfgSubKey, BOOL bIsHkcuItem)
{

	APPLISTDATA* pData = new APPLISTDATA;
	STARTUPITEMDATA* pInfData = new STARTUPITEMDATA;

	pInfData->FilePath = StrFilePath;
	pInfData->EntryPath = StrEntryPath;
	pInfData->MsConfigSubKey = StrMsCfgSubKey;
	pInfData->bIsHkcu = bIsHkcuItem;

	mStartupList.InsertItem(ID, StrTitle);
	mStartupList.SetItemText(ID, STARTUPLIST_CMDLINE, StrCmd);
	mStartupList.SetItemText(ID, STARTUPLIST_TYPE, StrStartupType);

	mStartupList.SetItemText(ID, STARTUPLIST_STATUS,
		StrStatus.IsEmpty() ? CString(L"Enabled") : StrStatus);

	CString StrResolved = StrFilePath;
	if (StrResolved.Right(4).CompareNoCase(L".lnk") == 0)
	{
		CString StrLnkTarget = _GetShortcuTarget(StrResolved);
		if (!StrLnkTarget.IsEmpty()) StrResolved = StrLnkTarget;
	}
	mStartupList.SetItemText(ID, STARTUPLIST_RUNNING,
		_IsProcessRunning(StrResolved) ? L"Yes" : L"No");

	mStartupList.SetItemText(ID, STARTUPLIST_DISABLEDTIME,
		StrDisabledTime.IsEmpty() ? CString(L"\u2014") : StrDisabledTime);

	mStartupList.SetItemData(ID, (DWORD_PTR)pData);
	pData->SubType = PARENT_ITEM_NOSUB;
	pData->iImage = IconID;
	pData->ItemType = 0;
	pData->pPData = pInfData;

	CProcess mProcInfo;
	if (StrFilePath.Right(4).CompareNoCase(L".lnk") == 0)
	{
		StrFilePath = _GetShortcuTarget(StrFilePath);
	}
	mStartupList.SetItemText(ID, STARTUPLIST_PUB, mProcInfo.GetVerInfoString(StrFilePath, L"CompanyName"));
}


CString CPageStartup::_GetShortcuTarget(CString StrLnkFile)
{
	CString StrRet;
	WCHAR lpDescFile[MAX_PATH];

	IShellLink* pShellLink;

	HRESULT bReturn = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pShellLink) >= 0;
	if (bReturn)
	{
		IPersistFile* ppf;
		bReturn = pShellLink->QueryInterface(IID_IPersistFile, (void**)&ppf) >= 0;
		if (bReturn)
		{
			bReturn = ppf->Load(StrLnkFile, TRUE) >= 0;
			if (bReturn)
			{
				pShellLink->GetPath(lpDescFile, MAX_PATH, NULL, 0);
			}
			ppf->Release();
		}
		pShellLink->Release();
	}

	StrRet = lpDescFile;
	return StrRet;
}

BOOL CPageStartup::_IsProcessRunning(const CString& StrTargetPath)
{
	if (StrTargetPath.IsEmpty()) return FALSE;

	CString StrTargetName = StrTargetPath;
	StrTargetName.MakeLower();
	int SlashPos = StrTargetName.ReverseFind(L'\\');
	if (SlashPos >= 0) StrTargetName = StrTargetName.Mid(SlashPos + 1);
	if (StrTargetName.IsEmpty()) return FALSE;

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE) return FALSE;

	PROCESSENTRY32W pe;
	ZeroMemory(&pe, sizeof(pe));
	pe.dwSize = sizeof(pe);

	BOOL bFound = FALSE;
	if (Process32FirstW(hSnap, &pe))
	{
		do
		{
			CString StrExe = pe.szExeFile;
			StrExe.MakeLower();
			int ExeSlash = StrExe.ReverseFind(L'\\');
			CString StrExeName = (ExeSlash >= 0) ? StrExe.Mid(ExeSlash + 1) : StrExe;
			if (StrExeName == StrTargetName || StrExe == StrTargetName)
			{
				bFound = TRUE;
				break;
			}
		} while (Process32NextW(hSnap, &pe));
	}

	CloseHandle(hSnap);
	return bFound;
}

BOOL CPageStartup::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == UM_ADD_STARTUPITEM)
	{
	}

	if (pMsg->message == UM_HEADER_LCLICK)
	{
		// CCoolheaderCtrl intercepta el click izquierdo sobre la cabecera
		// y lo reenvia al padre via UM_HEADER_LCLICK con wParam = indice
		// de columna. Mismo patron que CPageProcesses / CPageUsers.
		// Sort() se encarga de invertir el sentido si se vuelve a hacer
		// click en la misma columna.
		Sort((int)pMsg->wParam);
		return TRUE;
	}

	if (pMsg->message == UM_HEADER_RCLICK)
	{
		_OnHeaderRClick_Startup();
		return TRUE;
	}

	return CFormView::PreTranslateMessage(pMsg);
}

void CPageStartup::_OnHeaderRClick_Startup(void)
{
	CPoint pt;
	GetCursorPos(&pt);

	CMenu PopMenu;
	CMenu* pMenu = NULL;
	PopMenu.LoadMenuW(MAKEINTRESOURCE(IDR_POPMENU_COLUMN));
	pMenu = PopMenu.GetSubMenu(3);

	if (pMenu == NULL) return;

	if (pMenu->GetMenuItemCount() > 0)
		pMenu->DeleteMenu(0, MF_BYPOSITION);

	int nItem = pMenu->GetMenuItemCount();
	if (nItem > 0)
	{
		for (int i = 1; i <= COL_COUNT_STARTUP && i <= nItem; i++)
		{
			const int ColIdx = i;
			if (ColIdx >= COL_COUNT_STARTUP) break;
			if (!ColIDS_StartupList[ColIdx].IsHiddenColumn && ColIDS_StartupList[ColIdx].ColWidth != 0)
			{
				pMenu->CheckMenuItem(i - 1, MF_BYPOSITION | MF_CHECKED);
			}
		}
	}

	pMenu->TrackPopupMenu(TPM_LEFTALIGN, pt.x, pt.y, &(mStartupList.CoolheaderCtrl));
}

void CPageStartup::_JumpToRegKey(CString StrTargetKey)
{
	CString strPath = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit\\Favorites");
	CRegKey regkey;
	LONG lResult;
	lResult = regkey.Open(HKEY_CURRENT_USER, LPCTSTR(strPath), KEY_WRITE | KEY_QUERY_VALUE);

	int FavItemCount = 0;
	CString StrValueName;

	if (lResult == ERROR_SUCCESS)
	{
		StrValueName.Format(L"%d", int(GetTickCount() + rand()));
		regkey.SetStringValue(StrValueName, L"Computer\\" + StrTargetKey);

		WCHAR StrValue[MAX_PATH] = { 0 };
		WCHAR StrData[MAX_PATH] = { 0 };

		while (1)
		{
			DWORD dwLong, dwSize, Type;

			dwLong = dwSize = MAX_PATH;
			if (::RegEnumValue(regkey.m_hKey, FavItemCount, StrValue, &dwLong, NULL, &Type, (LPBYTE)StrData, &dwSize) == ERROR_NO_MORE_ITEMS)
			{
				break;
			}
			FavItemCount++;
		}
	}

	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcInfo;

	ZeroMemory(&StartupInfo, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	ZeroMemory(&ProcInfo, sizeof(ProcInfo));

	WCHAR WinDir[MAX_PATH];
	GetWindowsDirectory(WinDir, MAX_PATH);

	CString StrRegedit = WinDir;
	StrRegedit = StrRegedit + L"\\regedit.exe";

	if (CreateProcess(StrRegedit, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &StartupInfo, &ProcInfo) == 0) return;
	WaitForInputIdle(ProcInfo.hProcess, INFINITE);

	DWORD WindowPID;
	CWnd* pDeskTop = CWnd::GetDesktopWindow();
	CWnd* pWnd = pDeskTop->GetWindow(GW_CHILD);

	while (pWnd != NULL)
	{
		GetWindowThreadProcessId(pWnd->m_hWnd, &WindowPID);

		if (pWnd->IsWindowVisible() && (!(pWnd->GetExStyle() & WS_EX_TOOLWINDOW)))
		{
			if (WindowPID == ProcInfo.dwProcessId)
			{
				pWnd->ShowWindow(SW_MAXIMIZE);
				break;
			}
		}
		pWnd = pWnd->GetWindow(GW_HWNDNEXT);
	}

	if (pWnd == NULL)return;

	CMenu* pMainMenu = NULL;
	CMenu* pSubMenu = NULL;
	pMainMenu = pWnd->GetMenu();

	if (pMainMenu != NULL)
	{
		pSubMenu = pMainMenu->GetSubMenu(3);
		if (pSubMenu != NULL)
		{
			UINT ID = 1281 + FavItemCount;
			pWnd->SendMessage(WM_COMMAND, ID);
		}
	}

	::RegDeleteValue(regkey.m_hKey, StrValueName);
	regkey.Close();
}

void CPageStartup::OnPop_JumpToEntry()
{
	int nSel = mStartupList.GetNextItem(-1, LVNI_SELECTED);
	APPLISTDATA* pData = NULL;
	STARTUPITEMDATA* pInfData = NULL;

	pData = (APPLISTDATA*)mStartupList.GetItemData(nSel);

	if (pData == NULL) return;
	pInfData = (STARTUPITEMDATA*)(pData->pPData);
	if (pInfData == NULL) return;

	CString StrType = mStartupList.GetItemText(nSel, 3);
	_JumpToRegKey(pInfData->EntryPath);
}

void CPageStartup::OnPop_Searchonline()
{
	int nSel = mStartupList.GetNextItem(-1, LVNI_SELECTED);
	SearchOnline(mStartupList.GetItemText(nSel, 0));
}

static LONG _RegDeleteTreeW(HKEY hParent, LPCTSTR SubKey);

void CPageStartup::OnPop_DisableEnable()
{
	int nSel = mStartupList.GetNextItem(-1, LVNI_SELECTED);
	if (nSel < 0) return;

	APPLISTDATA* pData = (APPLISTDATA*)mStartupList.GetItemData(nSel);
	if (pData == NULL || pData->pPData == NULL) return;

	STARTUPITEMDATA* pInfData = (STARTUPITEMDATA*)pData->pPData;

	const CString StrItem = mStartupList.GetItemText(nSel, STARTUPLIST_NAME);
	const CString StrType = mStartupList.GetItemText(nSel, STARTUPLIST_TYPE);
	const CString StrStatus = mStartupList.GetItemText(nSel, STARTUPLIST_STATUS);
	const BOOL    bIsDisabled = (StrStatus.CompareNoCase(L"Disabled") == 0);
	const BOOL    bIsFolder = (StrType.CompareNoCase(L"Startup Group") == 0)
		|| (StrType.CompareNoCase(L"Startup folder") == 0);

	if (bIsDisabled)
	{
		const CString kBasePath = bIsFolder
			? CString(L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupfolder")
			: CString(L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupreg");
		const CString kSubKey = pInfData->MsConfigSubKey;
		if (kSubKey.IsEmpty())
		{
			return;
		}

		CRegKey regMsCfg;
		if (regMsCfg.Open(HKEY_LOCAL_MACHINE, kBasePath + L"\\" + kSubKey, KEY_READ) != ERROR_SUCCESS)
			return;

		TCHAR Buf[MAX_PATH] = { 0 }; DWORD cb = sizeof(Buf);
		CString StrMsKey, StrMsHkey, StrMsItem, StrMsCmd;
		if (regMsCfg.QueryStringValue(L"key", Buf, &cb) == ERROR_SUCCESS) StrMsKey = Buf;
		ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
		if (regMsCfg.QueryStringValue(L"hkey", Buf, &cb) == ERROR_SUCCESS) StrMsHkey = Buf;
		ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
		if (regMsCfg.QueryStringValue(L"item", Buf, &cb) == ERROR_SUCCESS) StrMsItem = Buf;
		ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
		if (regMsCfg.QueryStringValue(L"command", Buf, &cb) == ERROR_SUCCESS) StrMsCmd = Buf;
		regMsCfg.Close();

		if (StrMsItem.IsEmpty()) return;

		if (!bIsFolder)
		{
			HKEY hRoot = StrMsHkey.CompareNoCase(L"HKCU") == 0
				? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
			CRegKey regRun;
			if (regRun.Open(hRoot, StrMsKey, KEY_SET_VALUE) == ERROR_SUCCESS)
			{
				if (StrMsCmd.Find(L'%') >= 0)
					regRun.SetValue(StrMsItem, REG_EXPAND_SZ, (LPCTSTR)StrMsCmd, (DWORD)(StrMsCmd.GetLength() + 1) * sizeof(TCHAR));
				else
					regRun.SetValue(StrMsItem, REG_SZ, (LPCTSTR)StrMsCmd, (DWORD)(StrMsCmd.GetLength() + 1) * sizeof(TCHAR));
				regRun.Close();
			}
		}

		_RegDeleteTreeW(HKEY_LOCAL_MACHINE,
			kBasePath + L"\\" + kSubKey);
	}
	else
	{
		if (bIsFolder)
		{
			return;
		}

		CString StrRunPath = (StrType.Compare(L"Registry (WOW64)") == 0)
			? CString(L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run")
			: CString(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");

		HKEY hRoot = pInfData->bIsHkcu ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
		CRegKey regRun;
		if (regRun.Open(hRoot, StrRunPath, KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS)
			return;

		TCHAR Buf[MAX_PATH] = { 0 }; DWORD cb = sizeof(Buf), dwType = 0;
		LONG lRet = ::RegQueryValueEx(regRun.m_hKey, StrItem, NULL, &dwType,
			(LPBYTE)Buf, &cb);
		if (lRet != ERROR_SUCCESS || (dwType != REG_SZ && dwType != REG_EXPAND_SZ))
		{
			regRun.Close();
			return;
		}
		CString StrCmd = Buf;

		CString StrNewSub = _MakeMsConfigSubKeyName();
		CRegKey regMsCfg;
		if (regMsCfg.Create(HKEY_LOCAL_MACHINE,
			L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupreg\\" + StrNewSub) != ERROR_SUCCESS)
		{
			regRun.Close();
			return;
		}

		regMsCfg.SetStringValue(L"item", StrItem);
		regMsCfg.SetStringValue(L"key", StrRunPath);
		regMsCfg.SetStringValue(L"hkey", pInfData->bIsHkcu ? L"HKCU" : L"HKLM");
		regMsCfg.SetStringValue(L"command", StrCmd);

		SYSTEMTIME st;
		GetLocalTime(&st);
		regMsCfg.SetDWORDValue(L"YEAR", st.wYear);
		regMsCfg.SetDWORDValue(L"MONTH", st.wMonth);
		regMsCfg.SetDWORDValue(L"DAY", st.wDay);
		regMsCfg.SetDWORDValue(L"HOUR", st.wHour);
		regMsCfg.SetDWORDValue(L"MINUTE", st.wMinute);
		regMsCfg.SetDWORDValue(L"SECOND", st.wSecond);
		regMsCfg.Close();

		::RegDeleteValue(regRun.m_hKey, StrItem);
		regRun.Close();
	}

	ReloadStartupList();
}

static LONG _RegDeleteTreeW(HKEY hParent, LPCTSTR SubKey)
{
	HKEY hSub = NULL;
	if (RegOpenKeyEx(hParent, SubKey, 0, KEY_READ | KEY_WRITE, &hSub) != ERROR_SUCCESS)
		return RegDeleteKey(hParent, SubKey);

	DWORD dwIdx = 0;
	TCHAR Name[MAX_PATH] = { 0 };
	DWORD cchName = MAX_PATH;
	FILETIME ft;
	while (RegEnumKeyEx(hSub, dwIdx, Name, &cchName, NULL, NULL, NULL, &ft) != ERROR_NO_MORE_ITEMS)
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
	if (m_bLoadInProgress) return;
	m_bLoadInProgress = TRUE;

	mStartupList.DeleteAllItems();
	_LoadStartupItemsSync();

	m_bLoadInProgress = FALSE;
}

CString CPageStartup::_MakeMsConfigSubKeyName()
{
	const DWORD d = GetTickCount();
	CString s;
	s.Format(L"%08X%04X", d, rand() & 0xFFFF);
	return s;
}

void CPageStartup::OnPop_StartupColumnToggle(UINT nID)
{
	int iCol = (int)(nID - ID_STARTUPCOL_NAME);
	if (iCol < 0 || iCol >= COL_COUNT_STARTUP) return;

	if (iCol == STARTUPLIST_NAME) return;

	COLUMNSTATUS_EX& Col = ColIDS_StartupList[iCol];

	mStartupList.PopMenuAction = TRUE;

	if (Col.IsHiddenColumn)
	{
		// SHOW
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

		LVCOLUMNW lvCol;
		lvCol.mask = LVCF_ORDER;
		// CORRECCIÓN: Nos aseguramos de no usar nunca 0 para evitar pisar la columna Name.
		if (m_SavedIOrder[iCol] != 0xFF && m_SavedIOrder[iCol] != 0)
		{
			lvCol.iOrder = m_SavedIOrder[iCol];
		}
		else
		{
			int MaxOrder = 0;
			for (int j = 1; j < COL_COUNT_STARTUP; j++)
			{
				if (ColIDS_StartupList[j].IsHiddenColumn) continue;
				if ((int)ColIDS_StartupList[j].iOrder > MaxOrder)
					MaxOrder = ColIDS_StartupList[j].iOrder;
			}
			lvCol.iOrder = MaxOrder + 1;
		}
		mStartupList.SetColumn(iCol, &lvCol);

		Col.ColWidth = DefWidth;
		Col.ColWStore = DefWidth;
		Col.Redraw = 1;
		Col.IsHiddenColumn = FALSE;
		Col.iOrder = (BYTE)lvCol.iOrder;
		mStartupList.SetColumnWidth(iCol, DefWidth);
	}
	else
	{
		// HIDE
		LVCOLUMNW lvCol;
		lvCol.mask = LVCF_ORDER;
		if (mStartupList.GetColumn(iCol, &lvCol))
			m_SavedIOrder[iCol] = (BYTE)lvCol.iOrder;

		// CORRECCIÓN FATAL: NUNCA se debe asignar lvCol.iOrder = 0 al ocultar.
		// Eso es lo que empujaba físicamente al Name a la derecha[cite: 6].
		// Solo colapsamos el ancho a 0 y mantenemos su orden intacto.

		Col.ColWStore = Col.ColWidth;
		Col.ColWidth = 0;
		Col.Redraw = 0;
		Col.IsHiddenColumn = TRUE;
		mStartupList.SetColumnWidth(iCol, 0);
	}

	_SaveStartupColumnState();

	mStartupList.CoolheaderCtrl.Invalidate();
	mStartupList.Invalidate();
}

void CPageStartup::_LoadStartupItemsSync()
{
	mStartupList.SetRedraw(0);
	ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", TRUE, FALSE);
	ListStartupItem(L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run", TRUE, FALSE);
	ListStartupItem(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", FALSE, TRUE);
	ListStartupFromMenu();
	ListMsConfigDisabledReg();
	ListMsConfigDisabledFolder();

	// Si el usuario tenía un orden activo, reaplicarlo para que los items
	// no se desordenen al deshabilitar/habilitar (Recargar tras Enable/
	// Disable pasa por aquí). Se omite si todavía no hay columna elegida.
	// bFlipDirection=FALSE conserva el sentido elegido por el usuario.
	if (m_nSortColumn != -1)
	{
		Sort(m_nSortColumn, FALSE);
	}

	mStartupList.SetRedraw(1);
	mStartupList.CoolheaderCtrl.Invalidate();
	mStartupList.Invalidate();
}

static const TCHAR* kStartupColNames[COL_COUNT_STARTUP] =
{
	L"ColName",     L"ColPub",      L"ColStatus",   L"ColType",
	L"ColRunning",  L"ColTime",     L"ColCmdline"
};

void CPageStartup::_SaveStartupColumnState()
{
	CRegKey reg;
	if (reg.Create(HKEY_CURRENT_USER,
		L"Software\\DBCTaskManager\\Startup\\Columns") != ERROR_SUCCESS)
		return;
	for (int i = 0; i < COL_COUNT_STARTUP; i++)
	{
		const COLUMNSTATUS_EX& Col = ColIDS_StartupList[i];
		BYTE OrderToSave = Col.IsHiddenColumn
			? m_SavedIOrder[i]
			: Col.iOrder;
		if (OrderToSave == 0xFF) OrderToSave = (BYTE)i;
		if (i == STARTUPLIST_NAME) OrderToSave = 0;
		DWORD dwVal = ((Col.IsHiddenColumn ? 0 : 1) << 31) |
			(((DWORD)OrderToSave & 0xFF) << 16) |
			(Col.ColWidth & 0xFFFF);
		reg.SetDWORDValue(kStartupColNames[i], dwVal);
	}
	reg.Close();
}

BOOL CPageStartup::_LoadStartupColumnState()
{
	CRegKey reg;
	if (reg.Open(HKEY_CURRENT_USER,
		L"Software\\DBCTaskManager\\Startup\\Columns",
		KEY_READ) != ERROR_SUCCESS) return FALSE;

	BOOL HasSaved = FALSE;
	for (int i = 0; i < COL_COUNT_STARTUP; i++)
	{
		DWORD dwVal = 0;
		if (reg.QueryDWORDValue(kStartupColNames[i], dwVal) != ERROR_SUCCESS)
			continue;
		HasSaved = TRUE;
		ColIDS_StartupList[i].IsHiddenColumn = ((dwVal & 0x80000000) == 0);
		ColIDS_StartupList[i].ColWidth = (dwVal & 0xFFFF);

		BYTE SavedOrder = (BYTE)((dwVal >> 16) & 0xFF);

		// CORRECCIÓN DE REGISTRO: Si el bug anterior grabó un 0 ilegítimo 
		// en otra columna que no sea Name, la devolvemos a su índice natural
		// para evitar que desplace a la columna Name.
		if (SavedOrder == 0x00 && i != STARTUPLIST_NAME)
			SavedOrder = (BYTE)i;

		m_SavedIOrder[i] = SavedOrder;

		if (ColIDS_StartupList[i].IsHiddenColumn)
		{
			switch (i)
			{
			case STARTUPLIST_NAME:           ColIDS_StartupList[i].ColWStore = 200; break;
			case STARTUPLIST_PUB:            ColIDS_StartupList[i].ColWStore = 150; break;
			case STARTUPLIST_STATUS:         ColIDS_StartupList[i].ColWStore = 100; break;
			case STARTUPLIST_TYPE:           ColIDS_StartupList[i].ColWStore = 100; break;
			case STARTUPLIST_RUNNING:        ColIDS_StartupList[i].ColWStore = 80;  break;
			case STARTUPLIST_DISABLEDTIME:   ColIDS_StartupList[i].ColWStore = 150; break;
			case STARTUPLIST_CMDLINE:        ColIDS_StartupList[i].ColWStore = 250; break;
			}
		}
		else
		{
			ColIDS_StartupList[i].ColWStore = ColIDS_StartupList[i].ColWidth;
		}

		ColIDS_StartupList[i].iOrder = SavedOrder;
		ColIDS_StartupList[i].Redraw = ColIDS_StartupList[i].IsHiddenColumn ? 0 : 1;
	}
	reg.Close();
	return HasSaved;
}

void CPageStartup::ListMsConfigDisabledReg(void)
{
	const TCHAR* kParent =
		L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupreg";

	int nStartID = mStartupList.GetItemCount();

	CRegKey regParent;
	LONG lRes = regParent.Open(HKEY_LOCAL_MACHINE, kParent, KEY_READ | KEY_ENUMERATE_SUB_KEYS);
	if (lRes != ERROR_SUCCESS) { regParent.Close(); return; }

	TCHAR SubKeyName[MAX_PATH] = { 0 };
	DWORD cchName = MAX_PATH;
	DWORD dwIdx = 0;

	while (RegEnumKeyEx(regParent.m_hKey, dwIdx, SubKeyName, &cchName,
		NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS)
	{
		CRegKey regItem;
		if (regItem.Open(regParent.m_hKey, SubKeyName, KEY_READ) == ERROR_SUCCESS)
		{
			CString StrItem, StrKey, StrHkey, StrCmd;
			TCHAR Buf[MAX_PATH] = { 0 };
			DWORD cb = sizeof(Buf);
			if (regItem.QueryStringValue(L"item", Buf, &cb) == ERROR_SUCCESS)
				StrItem = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if (regItem.QueryStringValue(L"key", Buf, &cb) == ERROR_SUCCESS)
				StrKey = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if (regItem.QueryStringValue(L"hkey", Buf, &cb) == ERROR_SUCCESS)
				StrHkey = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if (regItem.QueryStringValue(L"command", Buf, &cb) == ERROR_SUCCESS)
				StrCmd = Buf;

			DWORD dwYear = 0, dwMonth = 0, dwDay = 0,
				dwHour = 0, dwMinute = 0, dwSecond = 0;
			regItem.QueryDWORDValue(L"YEAR", dwYear);
			regItem.QueryDWORDValue(L"MONTH", dwMonth);
			regItem.QueryDWORDValue(L"DAY", dwDay);
			regItem.QueryDWORDValue(L"HOUR", dwHour);
			regItem.QueryDWORDValue(L"MINUTE", dwMinute);
			regItem.QueryDWORDValue(L"SECOND", dwSecond);

			CString StrTime;
			if (dwYear != 0)
			{
				StrTime.Format(L"%02u/%02u/%04u %02u:%02u:%02u",
					dwMonth, dwDay, dwYear, dwHour, dwMinute, dwSecond);
			}

			CString StrRoot = (StrHkey.CompareNoCase(L"HKCU") == 0)
				? CString(L"HKEY_CURRENT_USER\\")
				: CString(L"HKEY_LOCAL_MACHINE\\");
			CString StrEntry = StrRoot + StrKey;

			SHFILEINFO sfi;
			memset(&sfi, 0, sizeof(sfi));
			CString StrPath = StrCmd;
			if (StrPath.GetLength() >= 2 && StrPath[0] == L'"' && StrPath[StrPath.GetLength() - 1] == L'"')
			{
				StrPath = StrPath.Mid(1, StrPath.GetLength() - 2);
			}

			WCHAR BufExp[MAX_PATH] = { 0 };
			if (ExpandEnvironmentStringsW(StrPath, BufExp, MAX_PATH) > 0)
				StrPath = BufExp;
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

void CPageStartup::ListMsConfigDisabledFolder(void)
{
	const TCHAR* kParent =
		L"SOFTWARE\\Microsoft\\Shared Tools\\MSConfig\\startupfolder";

	int nStartID = mStartupList.GetItemCount();

	CRegKey regParent;
	LONG lRes = regParent.Open(HKEY_LOCAL_MACHINE, kParent, KEY_READ | KEY_ENUMERATE_SUB_KEYS);
	if (lRes != ERROR_SUCCESS) { regParent.Close(); return; }

	TCHAR SubKeyName[MAX_PATH] = { 0 };
	DWORD cchName = MAX_PATH;
	DWORD dwIdx = 0;

	while (RegEnumKeyEx(regParent.m_hKey, dwIdx, SubKeyName, &cchName,
		NULL, NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS)
	{
		CRegKey regItem;
		if (regItem.Open(regParent.m_hKey, SubKeyName, KEY_READ) == ERROR_SUCCESS)
		{
			CString StrItem, StrFolder, StrCmd;
			TCHAR Buf[MAX_PATH] = { 0 };
			DWORD cb = sizeof(Buf);
			if (regItem.QueryStringValue(L"item", Buf, &cb) == ERROR_SUCCESS)
				StrItem = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if (regItem.QueryStringValue(L"folder", Buf, &cb) == ERROR_SUCCESS)
				StrFolder = Buf;
			ZeroMemory(Buf, sizeof(Buf)); cb = sizeof(Buf);
			if (regItem.QueryStringValue(L"command", Buf, &cb) == ERROR_SUCCESS)
				StrCmd = Buf;

			DWORD dwYear = 0, dwMonth = 0, dwDay = 0,
				dwHour = 0, dwMinute = 0, dwSecond = 0;
			regItem.QueryDWORDValue(L"YEAR", dwYear);
			regItem.QueryDWORDValue(L"MONTH", dwMonth);
			regItem.QueryDWORDValue(L"DAY", dwDay);
			regItem.QueryDWORDValue(L"HOUR", dwHour);
			regItem.QueryDWORDValue(L"MINUTE", dwMinute);
			regItem.QueryDWORDValue(L"SECOND", dwSecond);

			CString StrTime;
			if (dwYear != 0)
			{
				StrTime.Format(L"%02u/%02u/%04u %02u:%02u:%02u",
					dwMonth, dwDay, dwYear, dwHour, dwMinute, dwSecond);
			}

			SHFILEINFO sfi;
			memset(&sfi, 0, sizeof(sfi));

			WCHAR BufExp[MAX_PATH] = { 0 };
			CString StrPath = StrCmd;
			if (ExpandEnvironmentStringsW(StrCmd, BufExp, MAX_PATH) > 0)
				StrPath = BufExp;
			SHGetFileInfo(StrPath, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
				SHGFI_SMALLICON | SHGFI_SYSICONINDEX |
				SHGFI_USEFILEATTRIBUTES | SHGFI_ICON);

			AddItem(nStartID, StrItem, StrCmd, StrPath, StrFolder,
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

// ============================================================================
// LÓGICA DE ORDENAMIENTO DE COLUMNAS
// ============================================================================
//
// CCoolheaderCtrl intercepta el click izquierdo sobre la cabecera y lo
// reenvía al padre via UM_HEADER_LCLICK (ver CCoolheaderCtrl::OnLButtonUp).
// Esto se traduce en CPageStartup::PreTranslateMessage -> Sort().
//
// CCoolListCtrl ya tiene Cableado pCurrentSortCol -> CurrentSortColumn y
// pFlagSortUp -> FlagSortUp en su InitAllColumn, asi que con solo asignar
// esos dos miembros la cabecera redibuja la flecha nativa arriba de la
// columna activa (DrawItem + PaintCellTextArrow en CCoolheaderCtrl leen
// esos punteros para decidir si pintar HP_HEADERSORTARROW/HSAS_SORTED*).
//
// Sort() reusa el mismo patron que CPageProcesses / CPageUsers:
//   1. Decide sentido (flip si se repite la columna, TRUE si es nueva).
//   2. Repuebla APPLISTDATA::SortID con el indice visual actual para que
//      el callback tenga un mapa valido (lParam1/lParam2 en SortItems
//      son los punteros de SetItemData, NO indices visuales).
//   3. mStartupList.SortItems(SortFunc, nCol) -- NO SortItemsEx.

void CPageStartup::Sort(int nCol, BOOL bFlipDirection)
{
	if (nCol < 0 || nCol >= COL_COUNT_STARTUP) return;

	// Columnas con ancho 0 estan ocultas; ordenarlas no tiene sentido y
	// ademas GetItemText devuelve "" para celdas vacias.
	if (ColIDS_StartupList[nCol].ColWidth == 0) return;

	if (bFlipDirection)
	{
		if (mStartupList.CurrentSortColumn == nCol)
		{
			mStartupList.FlagSortUp = !mStartupList.FlagSortUp;
		}
		else
		{
			mStartupList.FlagSortUp = TRUE;
		}
	}

	// Persistir el estado para que un Reload (Enable/Disable) lo pueda
	// reaplicar sin volver a pedirselo al usuario.
	mStartupList.CurrentSortColumn = nCol;
	m_nSortColumn = nCol;
	m_bSortAscending = mStartupList.FlagSortUp;

	// Repoblar SortID en cada APPLISTDATA. SortFunc lo necesita para
	// traducir los punteros lParam1/lParam2 al indice visual que va a
	// pasar a GetItemText.
	int nCount = mStartupList.GetItemCount();
	for (int i = 0; i < nCount; i++)
	{
		APPLISTDATA* pData = (APPLISTDATA*)mStartupList.GetItemData(i);
		if (pData != NULL) pData->SortID = i;
	}

	mStartupList.SortItems(SortFunc, nCol);

	// Forzar repintado de la cabecera para que se vea la flecha al cambiar
	// de columna. El cool header lee CurrentSortColumn/FlagSortUp en
	// DrawItem y en PaintCellTextArrow (OnPaint).
	mStartupList.CoolheaderCtrl.Invalidate();
}

int CALLBACK CPageStartup::SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	int nCol = (int)lParamSort;

	APPLISTDATA* pData1 = (APPLISTDATA*)lParam1;
	APPLISTDATA* pData2 = (APPLISTDATA*)lParam2;

	if (pData1 == NULL || pData2 == NULL) return 0;

	// Mismo patron que Sort_Details / Sort_Processes / Sort_Users:
	// la lista se obtiene a traves de theApp.pSelPage porque el callback
	// SortItems no recibe un puntero explicito al CListCtrl.
	CCoolListCtrl* pList = &((CPageStartup*)theApp.pSelPage)->mStartupList;

	// SortID es el indice visual, repoblado por Sort() justo antes de
	// llamar a SortItems. GetItemText(SortID, nCol) devuelve el texto
	// que se va a comparar.
	CString s1 = pList->GetItemText(pData1->SortID, nCol);
	CString s2 = pList->GetItemText(pData2->SortID, nCol);

	int nRes = s1.CompareNoCase(s2);
	if (!pList->FlagSortUp) nRes = -nRes;
	return nRes;
}