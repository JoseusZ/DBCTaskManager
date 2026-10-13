// CoolheaderCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "DBCTaskman.h"
#include "CoolheaderCtrl.h"


// CCoolheaderCtrl

IMPLEMENT_DYNAMIC(CCoolheaderCtrl, CHeaderCtrl)

CCoolheaderCtrl::CCoolheaderCtrl()
	: Height(0)
	, PosX(0)
	, Width(0)
	, MouseTrackNow(FALSE)
	, FlagLeftBtnDown(FALSE)
	, StartDragCurPos(0)
	, WillSwapToID(0)
	, pFlagSortUp(NULL)
	, pCurrentSortCol(NULL)
	, m_iHoverItem(-1)
{
	//MyFont.CreateFont(17,0,0,0,FW_NORMAL,FALSE,FALSE,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY, 	DEFAULT_PITCH | FF_SWISS,_T("Tahoma"));

	hTheme = OpenThemeData(this->GetSafeHwnd(), L"HEADER");

	ARGB aRGB(0);
	GColor1.SetValue(aRGB);
	GColor2.SetFromCOLORREF(theApp.WndFrameColor);


}

CCoolheaderCtrl::~CCoolheaderCtrl()
{
	::CloseThemeData(hTheme);
}


BEGIN_MESSAGE_MAP(CCoolheaderCtrl, CHeaderCtrl)

	ON_MESSAGE(HDM_LAYOUT, OnLayout)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_WINDOWPOSCHANGING()
	ON_WM_CONTEXTMENU()
	ON_WM_MOUSEMOVE()

	ON_WM_MOUSELEAVE()

	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_MESSAGE(WM_THEMECHANGED, OnThemeChanged)
END_MESSAGE_MAP()



// CCoolheaderCtrl message handlers
void CCoolheaderCtrl::DrawItem(LPDRAWITEMSTRUCT  lpDrawItemStruct)
{


	int nItem = lpDrawItemStruct->itemID;

	WCHAR StrTitle[MAX_PATH];

	HDITEM              hdItem;
	hdItem.mask = HDI_FORMAT | HDI_TEXT;
	hdItem.pszText = StrTitle;
	hdItem.cchTextMax = MAX_PATH;

	this->GetItem(nItem, &hdItem);


	if (ColStatusArray[nItem].ColWidth == 0) return;


	CRect  rcItem;

	rcItem.SetRect(lpDrawItemStruct->rcItem.left, lpDrawItemStruct->rcItem.top, lpDrawItemStruct->rcItem.right, lpDrawItemStruct->rcItem.bottom);

	CDC* pDC = new   CDC;
	pDC->Attach(lpDrawItemStruct->hDC);
	pDC->SetBkMode(TRANSPARENT);
	Graphics  Grap(pDC->m_hDC);
	RectF rcF((float)rcItem.left, (float)rcItem.top + 10, (float)rcItem.Width(), (float)rcItem.Height());
	CRect rcDrageImage;
	rcDrageImage.CopyRect(rcItem);
	rcDrageImage.DeflateRect(1, 1);

	pDC->SetBkColor(theApp.WndBkgColor);


	UINT Align = DT_LEFT;

	Align = ColStatusArray[nItem].Align;




	CRect rcText, rcArrow;


	rcText.CopyRect(&rcItem);

	rcText.top += 10;
	rcText.InflateRect(-8, -5);



	if (*pCurrentSortCol == nItem)
	{
		rcArrow.CopyRect(&rcItem);

		rcArrow.bottom = rcArrow.top + 10;

		if (theApp.FlagThemeActive)
		{
			int SortType = HSAS_SORTEDUP;
			if (!(*pFlagSortUp)) SortType = HSAS_SORTEDDOWN;
			DrawThemeBackground(hTheme, pDC->m_hDC, HP_HEADERSORTARROW, SortType, rcArrow, NULL);

		}
		else
		{
		}
	}

	//pDC->SetTextColor(::GetSysColor(COLOR_HOTLIGHT ));
	if (theApp.FlagThemeActive)
	{

		pDC->SetTextColor(theApp.CoolHdrColor);
	}
	else
	{
		pDC->SetTextColor(::GetSysColor(COLOR_HOTLIGHT));
	}


	pDC->DrawText(StrTitle, &rcText, DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | Align);//���ݴӶ���������ȡ



	if (ColStatusArray[nItem].Cool)
	{



		CFont* pOldFont = pDC->SelectObject(&theApp.mTitleFont);

		rcText.top = 16;
		pDC->SetTextColor(RGB(99, 99, 99));
		pDC->DrawText(ColStatusArray[nItem].StrItem, &rcText, DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | Align);// 
		pDC->SelectObject(pOldFont);
		//MyFont.DeleteObject();


	}


	//	pDC->SelectObject(pOldBrush);
	Grap.ReleaseHDC(pDC->m_hDC);

	pDC->Detach();
	pDC->DeleteDC();
	delete   pDC;
	pDC = NULL;

}





LRESULT CCoolheaderCtrl::OnLayout(WPARAM wParam, LPARAM lParam)
{
	//return 1;

	LRESULT lResult = CHeaderCtrl::DefWindowProc(HDM_LAYOUT, 0, lParam);
	HD_LAYOUT& hdl = *(HD_LAYOUT*)lParam;
	RECT* prc = hdl.prc;
	WINDOWPOS* pwpos = hdl.pwpos;



	pwpos->y = 0;
	//pwpos->cx = Width ;
	pwpos->cy = Height;//46+10; 
	prc->top = 0;//46+10;



	return  lResult;

}  // OnLayout
void CCoolheaderCtrl::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	// TODO: Add your message handler code here

	// Do not call CHeaderCtrl::OnPaint() for painting messages


	CRect rc, rcItem;
	int n = this->GetItemCount();
	//this->GetItemRect(n-1,rcLastItem); 
	this->GetClientRect(rc);

	int  SumColWidth = 0;

	CDC MemDC; 
	CBitmap MemMap;
	CBitmap* pOldBm;

	MemDC.CreateCompatibleDC(&dc);
	MemMap.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	pOldBm = MemDC.SelectObject(&MemMap);




	MemDC.FillSolidRect(rc, theApp.WndBkgColor);

	// Dejar que el control pinte texto/arrow normalmente. Es la unica
	// fuente de texto del header; no se duplica.
	DefWindowProc(WM_PAINT, (WPARAM)MemDC.m_hDC, (LPARAM)0);

	Graphics  Grap(MemDC.m_hDC);

	RectF rcF((float)rc.left, (float)rc.top + 10, 1.0, (float)rc.Height());
	LinearGradientBrush  LBr(rcF, GColor1, GColor2, LinearGradientModeVertical);
	LinearGradientBrush  LBrRed(rcF, Color(0, 255, 0, 0), Color(128, 255, 0, 0), LinearGradientModeVertical);
	//Grap.FillRectangle(&LBr,rc.left,10,1,rc.Height());


	CRect rcFix;
	rcFix.CopyRect(rc);

	if (!theApp.FlagThemeActive)
	{
		rcFix.top = rcFix.bottom - 2;
		MemDC.FillSolidRect(rcFix, theApp.WndBkgColor);
	}

	n = this->GetItemCount();

		CRect rcStore;
		for (int i = 0;i < n;i++)
		{
			this->GetItemRect(i, rcItem);

			if (ColStatusArray[i].ColWidth == 0)
				continue;

			rcStore.CopyRect(rcItem);
			SumColWidth = SumColWidth + rcItem.Width();

			bool bIsHover = (theApp.FlagThemeActive && hTheme && i == m_iHoverItem);

			if (theApp.FlagThemeActive)
			{

				MemDC.FillSolidRect(rcItem, theApp.WndBkgColor);

				if (bIsHover)
				{
					DrawThemeBackground(hTheme, MemDC.m_hDC, HP_HEADERITEM, HIS_HOT, &rcItem, NULL);
				}

				PaintCellTextArrow(MemDC, i, rcItem);
			}
			else
			{

				if (bIsHover)
				{
					DrawThemeBackground(hTheme, MemDC.m_hDC, HP_HEADERITEM, HIS_HOT, &rcItem, NULL);
					PaintCellTextArrow(MemDC, i, rcItem);
				}
			}

			if (ColStatusArray[i].Percents > 90)
			{
				Grap.FillRectangle(&LBrRed, rcStore.left, 10, rcStore.Width(), rcStore.Height());
				//MemDC.Draw3dRect(rcStore.left,rcStore.top,rcStore.Width(),rcStore.Height(),RGB(200,99,0),RGB(200,99,0));
			}
		}

	rc.left = SumColWidth;

		MemDC.FillSolidRect(rc, theApp.WndBkgColor);
		//	MemDC.FillSolidRect(CRect(SumColWidth,0,SumColWidth+2,10),RGB(255,255,255));


		Grap.FillRectangle(&LBr, rc.left, 10, 1, rc.Height());

	for (int k = 0; k < n; k++)
	{
		if (ColStatusArray[k].ColWidth == 0) continue;
		CRect rcDiv;
		if (!this->GetItemRect(k, rcDiv)) continue;
		int x = rcDiv.right;
		int h = rcDiv.Height();
		if (h <= 0) continue;
		RectF rcDG((REAL)x, (REAL)rcDiv.top, 1.0f, (REAL)h);
		Color cTop(0, 0, 0, 0);
		Color cBot(255, 0, 0, 0);
		LinearGradientBrush LBrDiv(rcDG, cTop, cBot, LinearGradientModeVertical);
		LBrDiv.SetWrapMode(WrapModeTile);
		Grap.FillRectangle(&LBrDiv, (REAL)x, (REAL)rcDiv.top, 1.0f, (REAL)h);
	}

	CPen mPen;

	mPen.CreatePen(0, 1, theApp.WndFrameColor);

	CPen* OldPen = MemDC.SelectObject(&mPen);



	rc.left = 0;

	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &MemDC, 0, 0, SRCCOPY);


	Grap.ReleaseHDC(MemDC);

	MemDC.SelectObject(pOldBm);

	mPen.DeleteObject();

	MemDC.DeleteDC();
	MemMap.DeleteObject();






}


BOOL CCoolheaderCtrl::OnEraseBkgnd(CDC* pDC)
{
	// TODO: Add your message handler code here and/or call default

	return TRUE;
	//return CHeaderCtrl::OnEraseBkgnd(pDC);
}


void CCoolheaderCtrl::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
	//CRect rc;
	//this->GetParent()->GetClientRect(rc);
	lpwndpos->y = 0;
	lpwndpos->x = PosX;
	//lpwndpos->cx=PosX+rc.Width();


	//CHeaderCtrl::OnWindowPosChanging(lpwndpos);
}

void CCoolheaderCtrl::OnContextMenu(CWnd* pWnd, CPoint point)
{


	GetParent()->PostMessageW(UM_HEADER_RCLICK);



}




void CCoolheaderCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	if (!MouseTrackNow)
	{
		TRACKMOUSEEVENT csTME;
		csTME.cbSize = sizeof(csTME);
		csTME.dwFlags = TME_LEAVE | TME_HOVER;
		csTME.hwndTrack = m_hWnd;
		csTME.dwHoverTime = 10;  
		::_TrackMouseEvent(&csTME); 
		MouseTrackNow = TRUE;  
	}


	int nCount = GetItemCount();
	int iNewHover = -1;
	for (int i = 0; i < nCount; i++)
	{
		CRect rcCol;
		if (GetItemRect(i, rcCol) && rcCol.PtInRect(point))
		{
			iNewHover = i;
			break;
		}
	}

	if (iNewHover != m_iHoverItem)
	{
		int iOld = m_iHoverItem;
		m_iHoverItem = iNewHover;

		CRect rcInv;
		if (iOld >= 0 && GetItemRect(iOld, rcInv))
		{
			InvalidateRect(rcInv, FALSE);
		}
		if (iNewHover >= 0 && GetItemRect(iNewHover, rcInv))
		{
			InvalidateRect(rcInv, FALSE);
		}
	}

	CHeaderCtrl::OnMouseMove(nFlags, point);
}



void CCoolheaderCtrl::OnMouseLeave()
{
	// TODO: Add your message handler code here and/or call default

	FlagLeftBtnDown = FALSE;
	MouseTrackNow = FALSE;

	if (m_iHoverItem != -1)
	{
		int iOld = m_iHoverItem;
		m_iHoverItem = -1;
		CRect rc;
		if (GetItemRect(iOld, rc))
		{
			InvalidateRect(rc, FALSE);
		}
	}

	CHeaderCtrl::OnMouseLeave();
}



void CCoolheaderCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	if (FlagLeftBtnDown)
	{

		FlagLeftBtnDown = FALSE;
		int n = this->GetItemCount();
		CRect rc;

		int iClick = -1;
		for (int i = 0;i < n;i++)
		{
			GetItemRect(i, rc);
			if (rc.PtInRect(point))
			{
				iClick = i;
				Invalidate();
				GetParent()->PostMessageW(UM_HEADER_LCLICK, iClick);
				break;
			}
		}





	}
	CHeaderCtrl::OnLButtonUp(nFlags, point);
}

void CCoolheaderCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	if (GetCursor() == AfxGetApp()->LoadStandardCursor(IDC_ARROW))
	{
		FlagLeftBtnDown = TRUE;
		StartDragCurPos = point.x;
	}

	CHeaderCtrl::OnLButtonDown(nFlags, point);
}

BOOL CCoolheaderCtrl::OnCommand(WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your specialized code here and/or call the base class

	this->GetParent()->PostMessageW(WM_COMMAND, wParam, lParam);
	return CHeaderCtrl::OnCommand(wParam, lParam);
}

LRESULT CCoolheaderCtrl::OnThemeChanged(WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = DefWindowProc(WM_THEMECHANGED, wParam, lParam);


	if (hTheme)
	{
		::CloseThemeData(hTheme);
		hTheme = NULL;
	}
	hTheme = ::OpenThemeData(this->GetSafeHwnd(), L"HEADER");

	m_iHoverItem = -1;
	Invalidate();
	return lResult;
}

void CCoolheaderCtrl::PaintCellTextArrow(CDC& memDC, int iCol, const CRect& rcCell)
{
	if (ColStatusArray[iCol].ColWidth == 0)
		return;

	WCHAR StrTitle[MAX_PATH];
	HDITEM hd;
	hd.mask = HDI_FORMAT | HDI_TEXT;
	hd.pszText = StrTitle;
	hd.cchTextMax = MAX_PATH;
	if (!this->GetItem(iCol, &hd))
		return;

	UINT Align = ColStatusArray[iCol].Align;

	CRect rcText(rcCell);
	rcText.top += 10;
	rcText.InflateRect(-8, -5);

	CFont* pFont = GetFont();
	CFont* pOldFont = NULL;
	if (pFont != NULL && pFont->GetSafeHandle() != NULL)
	{
		pOldFont = memDC.SelectObject(pFont);
	}

	if (*pCurrentSortCol == iCol && hTheme)
	{
		CRect rcArrow(rcCell);
		rcArrow.bottom = rcArrow.top + 10;
		int SortType = HSAS_SORTEDUP;
		if (!(*pFlagSortUp)) SortType = HSAS_SORTEDDOWN;
		DrawThemeBackground(hTheme, memDC.m_hDC, HP_HEADERSORTARROW, SortType, rcArrow, NULL);
	}

	memDC.SetBkMode(TRANSPARENT);
	memDC.SetTextColor(theApp.CoolHdrColor);
	memDC.DrawText(StrTitle, &rcText, DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS | Align);

	if (ColStatusArray[iCol].Cool)
	{
		CFont* pOldFont2 = memDC.SelectObject(&theApp.mTitleFont);
		rcText.top = 16;
		memDC.SetTextColor(RGB(99, 99, 99));
		memDC.DrawText(ColStatusArray[iCol].StrItem, &rcText, DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | Align);
		memDC.SelectObject(pOldFont2);
	}

	if (pOldFont != NULL)
	{
		memDC.SelectObject(pOldFont);
	}
}
