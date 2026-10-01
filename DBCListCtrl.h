#pragma once
#include "afxwin.h"
#include "NormalHeaderCtrl.h"
/* 

      �����ڶԻ���༭�� ������ Ҳ���ó� �Ի����� ˢ���ٶȲ��ܸ��� ������




*/





class CDBCListCtrl : public CListCtrl
{
	DECLARE_DYNAMIC(CDBCListCtrl)

public:
	CDBCListCtrl();
	virtual ~CDBCListCtrl();

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnCustomDraw(NMHDR *pNMHDR, LRESULT *pResult);
private:
	
		
public:
	COLUMNSTATUS *pColStatusArray;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSizing(UINT fwSide, LPRECT pRect);
	afx_msg void OnLvnBeginScroll(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnEndScroll(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	CRect _GetRedrawColumn( );
	int FullColumnCount;
	void InitAllColumn(COLUMNSTATUS * pStatusArray,UINT ColumnStringID,const int ColumnCount);
	BOOL RedrawAllColumn;
protected:
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult); 
	
public:
	int HotItemID;
	BOOL m_bMouseTracking;
	afx_msg void OnLvnHotTrack(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnMouseLeave();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);	// ����ʽ ��־
	BOOL FlagSortUp;
	int CurrentSortColumn;
	CNormalHeaderCtrl mHdCtrl;
protected:
	virtual void PreSubclassWindow();

public:
	COLORREF NormalTextColor;
	COLORREF HotTextColor;
	afx_msg void OnLvnItemchanged(NMHDR *pNMHDR, LRESULT *pResult);
};


