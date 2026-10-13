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
	void ListStartupItem(CString StrRegPath);
	COLUMNSTATUS_EX     ColIDS_StartupList[COL_COUNT_STARTUP];
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnNMRClickStartup(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnPop_Openfilelocation();
	afx_msg void OnPop_Properties();
	int ListStartupFromMenu(void);
	void AddItem(int ID,CString StrTitle,CString StrCmd,CString StrFilePath,CString StrEntryPath,CString StrStartupType,int IconID);
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

	// Returns TRUE if an executable matching StrTargetPath (case-insensitive
	// basename match) is currently running. Uses TlHelp32 which is native
	// Win32 available since Windows 95 / fully supported on Windows 7.
	BOOL _IsProcessRunning(const CString& StrTargetPath);
};


