// StartupView.h
#pragma once
#include "afxcmn.h"
#include "CoolListCtrl.h"

#define COL_COUNT_STARTUP  7

// Default-visible columns (kept in header on first load and when the user
// resets to defaults). Order MUST match STR_COLUMN_STARTUP and the
// ID_STARTUPCOL_* menu IDs in DBCTaskman.rc / IDR_POPMENU_COLUMN.
// Default visible on first load: Name, Publisher, Status (indices 0-2).
#define STARTUPLIST_NAME           0
#define STARTUPLIST_PUB            1
#define STARTUPLIST_STATUS         2
#define STARTUPLIST_TYPE           3
#define STARTUPLIST_RUNNING        4
#define STARTUPLIST_DISABLEDTIME   5
#define STARTUPLIST_CMDLINE        6

struct STARTUPITEMDATA
{
	CString  FilePath;
	CString  EntryPath;
	// Name of the per-item subkey under MSConfig\startupreg (or
	// MSConfig\startupfolder) for rows that came from there. Empty for
	// rows read from a live Run key. Used by the right-click handler to
	// relocate the entry back to the active Run key when re-enabling.
	CString  MsConfigSubKey;
	// True when the row originated from HKCU\...\Run. Required so the
	// Enable handler re-opens HKCU when restoring the value.
	BOOL     bIsHkcu;
};

// CPageStartup form view
class CPageStartup : public CFormView
{
	DECLARE_DYNCREATE(CPageStartup)
protected:
	CPageStartup();           // protected constructor used by dynamic creation
	virtual ~CPageStartup();
public:
	enum { IDD = IDD_PAGE_STARTUP };
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	CCoolListCtrl mStartupList;
	void InitList(void);

	// Lists every enabled value under
	//   HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run
	//   HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Run
	//   HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run
	// bHklm selects the hive; StrRegPath is the path under that hive.
	// Pass StrIsEmpty=TRUE when listing from HKCU (different open call).
	void ListStartupItem(CString StrRegPath, BOOL bHklm = TRUE, BOOL bIsHkcu = FALSE);

	// Reads the items msconfig moved out of the active Run keys when the
	// user unchecked them in the "Inicio de Windows" tab. Each subkey of
	//   HKLM\SOFTWARE\Microsoft\Shared Tools\MSConfig\startupreg
	// carries the original (key, item, hkey, command) plus a DWORD
	// timestamp (YEAR/MONTH/DAY/HOUR/MINUTE/SECOND).
	void ListMsConfigDisabledReg(void);

	// Same idea for items msconfig disabled from the Startup folder.
	//   HKLM\SOFTWARE\Microsoft\Shared Tools\MSConfig\startupfolder
	void ListMsConfigDisabledFolder(void);

	COLUMNSTATUS_EX     ColIDS_StartupList[COL_COUNT_STARTUP];
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnNMRClickStartup(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPop_Openfilelocation();
	afx_msg void OnPop_Properties();
	int ListStartupFromMenu(void);
	void AddItem(int ID, CString StrTitle, CString StrCmd, CString StrFilePath, CString StrEntryPath, CString StrStartupType, int IconID,
		const CString& StrStatus = CString(), const CString& StrDisabledTime = CString(),
		const CString& StrMsCfgSubKey = CString(), BOOL bIsHkcuItem = FALSE);
	CString _GetShortcuTarget(CString StrLnkFile);


	virtual BOOL PreTranslateMessage(MSG* pMsg);
	void _JumpToRegKey(CString StrTargetKey);
	afx_msg void OnPop_JumpToEntry();
	afx_msg void OnPop_Searchonline();

	// Header right-click -> column visibility context menu.
	// Loads IDR_POPMENU_COLUMN submenu "Startup", checks currently visible
	// ones with MF_CHECKED, then TrackPopupMenu. Identical visual pattern
	// to CPageProcesses / CPageUsers, but with the Startup column IDs.
	void _OnHeaderRClick_Startup(void);

	// ON_COMMAND_RANGE entry for ID_STARTUPCOL_NAME..ID_STARTUPCOL_CMDLINE.
	// Maps the command ID to the corresponding STARTUPLIST_* column index
	// and calls mStartupList.ShowOrHideColumn() to flip visibility.
	afx_msg void OnPop_StartupColumnToggle(UINT nID);

	// Called after the user finishes dragging a header column. Persists
	// the new iOrder values so the layout survives a restart.
	afx_msg void OnHdnEndDrag_Startup(NMHDR* pNMHDR, LRESULT* pResult);

	// Returns TRUE if an executable matching StrTargetPath (case-insensitive
	// basename match) is currently running. Uses TlHelp32 which is native
	// Win32 available since Windows 95 / fully supported on Windows 7.
	BOOL _IsProcessRunning(const CString& StrTargetPath);

	// Right-click "Disable" / "Enable" handler. Moves the value between
	// the live Run key and HKLM\...\Shared Tools\MSConfig\startupreg so
	// the operator gets the exact same on-disk result msconfig produces.
	// bDisable = TRUE to disable, FALSE to re-enable.
	afx_msg void OnPop_DisableEnable();

	// Clear and reload the entire startup list. Called after toggling an
	// item so the visible row reflects the new registry state.
	void ReloadStartupList();

	// Generate a GUID-shaped subkey name (8 hex chars + suffix) that
	// msconfig uses for entries under MSConfig\startupreg. The format
	// mirrors what msconfig itself writes so a re-enabled item blends
	// in with the rest of the registry tree.
	static CString _MakeMsConfigSubKeyName();

	// Synchronous variant of Thread_LoadSartupItem. Runs on the UI
	// thread so the registry walk can't race with a stale worker
	// thread still touching the list (which is what was crashing
	// before with STATUS_HEAP_CORRUPTION 0xc0000374).
	void _LoadStartupItemsSync();

	// Persist the column visibility/width state across runs.
	// Storage: HKCU\Software\DBCTaskManager\Startup\Columns,
	// one DWORD value per column ("ColName".."ColCmdline") so a
	// partial read or a future column insertion doesn't disturb the
	// other slots.
	void _SaveStartupColumnState();
	BOOL _LoadStartupColumnState();

	// Set to TRUE for the duration of any background load and checked at
	// the top of the reload path so we never spawn a second load while
	// the first is still walking the registry.
	volatile BOOL m_bLoadInProgress;

	// Per-column iOrder cached from HKCU at load time. Used by the toggle
	// handler to remember where a hidden column was last positioned, so
	// re-enabling it drops it back in its old slot instead of the right
	// edge. 0xFF marks "no saved position" (treat as right-end on show).
	BYTE m_SavedIOrder[COL_COUNT_STARTUP];

	// -1 = no active sort; otherwise the index of the column currently
	// driving the order. m_bSortAscending holds the direction (TRUE =
	// ascending, FALSE = descending). The pair is reapplied after every
	// reload so enable/disable keeps the order the user picked.
	int  m_nSortColumn;
	BOOL m_bSortAscending;

	// Sort the list by the text in column nCol. bFlipDirection = TRUE
	// when the call comes from a header click (toggle ascending/descending
	// when the same column is clicked twice); FALSE when re-applying the
	// current sort after a reload so the direction is preserved.
	//
	// Mirrors the pattern used by CPageProcesses / CPageUsers: sets the
	// CCoolListCtrl's CurrentSortColumn + FlagSortUp (the cool header
	// arrow is drawn from them), repopulates each APPLISTDATA::SortID
	// with the current visual index, and calls SortItems (not Ex).
	void Sort(int nCol, BOOL bFlipDirection = TRUE);

	// SortItems callback. lParam1/lParam2 are the APPLISTDATA* stored
	// via SetItemData in AddItem (NOT visual indices); the visual index
	// is pData->SortID which Sort() refreshes right before calling
	// SortItems. CompareNoCase matches the native Task Manager.
	static int CALLBACK SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
};