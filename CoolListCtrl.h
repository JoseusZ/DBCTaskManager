#pragma once
#include "coolheaderctrl.h"
#include "afxwin.h"

 




typedef  struct  CoolListData
{

	BYTE   ItemType;
	int   SubType;
	DWORD iImage;
	void  * pPData;
	CString  StrWnd;
	HWND   hMainWnd;
	int   nSubItem;
	ULONGLONG  IOLast;



	double CoolUsageArray[11] ;

	int SortID;
	CString StrTitle;

	// FIX B1: nivel de indentacion estilo Win10 (0 = raiz, 1 = hijo de
	// raiz, 2 = nieto, etc.). Se asigna en Phase B3/B4 segun la cadena
	// de ParentPID del proceso. Solo se usa visualmente (no afecta
	// posicionamiento ni relaciones padre-hijo en el list control);
	// la lista sigue siendo PLANA (no eliminamos items). Esto es lo
	// que hace el taskmgr nativo de Win10: lista plana + indentacion.
	int  IndentLevel;

	// FIX T5: puntero al APPLISTDATA padre (solo valido cuando
	// SubType == SUB_ITEM). Permite que ReSort() copie el texto de las
	// columnas numericas desde el padre REAL de cada sub-item, no desde
	// el ultimo PARENT_ITEM_OPEN visto en el recorrido (que era la causa
	// raiz del bug: LastParentItemID podia apuntar al padre equivocado
	// o a -1 si un sub-item aparecia antes que cualquier POpen).
	//
	// NOTA: usamos "struct CoolListData *" en lugar de "APPLISTDATA *"
	// porque el typedef todavia no esta disponible dentro del propio
	// struct (C++ no permite autoreferenciar el typedef, pero SI el
	// nombre del struct). APPLISTDATA == CoolListData por el typedef
	// de la linea siguiente, asi que el cast es identico en runtime.
	struct CoolListData *pParent;

}COOLLISTDATA,APPLISTDATA,USERLISTDATA;



// CCoolListCtrl

class CCoolListCtrl : public CListCtrl
{
	DECLARE_DYNAMIC(CCoolListCtrl)

public:
	CCoolListCtrl();
	virtual ~CCoolListCtrl();
	afx_msg void OnCustomDraw(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct); 

	 

protected:
	DECLARE_MESSAGE_MAP()
public:
	COLUMNSTATUS_EX *pColStatusArray;
	HIMAGELIST hImageList;
	void SetImageList(HIMAGELIST hIMGList);
	HTHEME hTheme;
	
	afx_msg void OnLvnHotTrack(NMHDR *pNMHDR, LRESULT *pResult);
	DWORD nHot;
 	CCoolheaderCtrl CoolheaderCtrl;
protected:
	virtual void PreSubclassWindow();
public:
 

	afx_msg void OnNMDblclk(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMClick(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	BOOL m_bMouseTracking;     // TrackMouseEvent activo
	int  m_nHotArrowItem;      // Item sobre el que esta el cursor (-1 = ninguno)
	int  m_nHotArrowSubItem;   // Subitem (normalmente 0, columna Name)
	afx_msg void OnLvnDeleteitem(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	int DeleteItemAndSub(int iItem);
	BOOL DrawAllColForSubItem;	 
 	

	BOOL IsProcList;

public:
	CRect _GetRedrawColumn(void);
	int FullColumnCount;
	void InitAllColumn(COLUMNSTATUS_EX * pStatusArray,UINT ColumnStringID,const int ColumnCount,int iStartCool=-1);
protected:
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);

public:



	afx_msg void OnHdnItemchanging(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	
	void SetHeaderPosSize(void);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	afx_msg void OnHdnEndtrack(NMHDR *pNMHDR, LRESULT *pResult);
	
	BOOL PopMenuAction;
 
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
 

 
	afx_msg void OnLvnItemchanged(NMHDR *pNMHDR, LRESULT *pResult);
 
	afx_msg void OnHdnBegindrag(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnHdnEnddrag(NMHDR *pNMHDR, LRESULT *pResult);
	
	BOOL FlagSortUp;
	int CurrentSortColumn;
//	virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
//	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
public:
	afx_msg void OnLvnGetdispinfo(NMHDR *pNMHDR, LRESULT *pResult);

	//int XXX;


	COLORREF GetCoolColor(double Hot);

	// Memory-only heat map: smooth RGB interpolation between absolute MB
	// anchors (620 MB -> (249,230,136), 1072 MB -> (253,212,94)). The CPU,
	// Disk and Network heat maps keep using the discrete GetCoolColor()
	// bucket palette above. The two functions are selected at the call site
	// by inspecting whether the value is a fraction (0..1) or a MB value
	// (> 1) so the column type does not need to be known here.
	COLORREF GetMemoryHeatColor(double MemMb);

	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
protected:
//	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
public:
	BOOL FlagDrawAllColumns;
	BOOL ShowOrHideColumn(int iCol);
	void MySetItemText(int iItem, int iCol, CString StrToSet);
	void _DrawGroupIconClassic(CDC * pdc,CRect Rc,BOOL Open = FALSE);
	
};


