#pragma once
#include "afxwin.h"
#include "gdipluscolor.h"


// CCoolheaderCtrl

//struct HDRITEMDATA
//{
//	BOOL Cool;
//	CString StrItem;
//};

class CCoolheaderCtrl : public CHeaderCtrl
{
	DECLARE_DYNAMIC(CCoolheaderCtrl)

public:
	CCoolheaderCtrl();
	virtual ~CCoolheaderCtrl();
	COLUMNSTATUS_EX  * ColStatusArray;

protected:
	DECLARE_MESSAGE_MAP()
public:
	virtual void DrawItem(LPDRAWITEMSTRUCT  lpDrawItemStruct );	

	afx_msg  LRESULT OnLayout(WPARAM wParam, LPARAM lParam) ;
	afx_msg void OnPaint();
	// FIX bug del cuadro negro durante drag nativo (HDS_DRAGDROP):
	// toda la logica de OnPaint se extrae a DrawHeader(CDC*) para que
	// tambien la use el handler de WM_PRINTCLIENT. Asi el snapshot que
	// toma el sistema durante el drag sale en color, no invertido/negro.
	void DrawHeader(CDC* pDC);
	afx_msg LRESULT OnPrintClient(WPARAM wParam, LPARAM lParam);


	int Height;
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);	 

	afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);


	int PosX;
	int Width;
//protected:
//	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
public:
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);


	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	
	afx_msg void OnMouseLeave();
	BOOL  MouseTrackNow;
	BOOL FlagLeftBtnDown;
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	LRESULT OnThemeChanged(WPARAM, LPARAM);
	int StartDragCurPos;
	int WillSwapToID;
	BOOL *pFlagSortUp;
	int *pCurrentSortCol;
	HTHEME hTheme;
		int m_iHoverItem;

	protected:
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
public:
	Gdiplus::Color GColor1;
	Gdiplus::Color GColor2;
	void PaintCellTextArrow(CDC& memDC, int iCol, const CRect& rcCell);
};


