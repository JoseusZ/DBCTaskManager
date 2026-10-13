// StartupView.h
#pragma once
#include "afxcmn.h"
#include "CoolListCtrl.h"

#define COL_COUNT_STARTUP  7
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
	CString  MsConfigSubKey;
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
	void ListStartupItem(CString StrRegPath, BOOL bHklm = TRUE, BOOL bIsHkcu = FALSE);
	void ListMsConfigDisabledReg(void);
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
	void _OnHeaderRClick_Startup(void);
	afx_msg void OnPop_StartupColumnToggle(UINT nID);
	afx_msg void OnHdnEndDrag_Startup(NMHDR* pNMHDR, LRESULT* pResult);
	BOOL _IsProcessRunning(const CString& StrTargetPath);
	afx_msg void OnPop_DisableEnable();
	void ReloadStartupList();
	static CString _MakeMsConfigSubKeyName();
	void _LoadStartupItemsSync();
	void _SaveStartupColumnState();
	BOOL _LoadStartupColumnState();

	volatile BOOL m_bLoadInProgress;

	BYTE m_SavedIOrder[COL_COUNT_STARTUP];
};