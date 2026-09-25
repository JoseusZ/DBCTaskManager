﻿// CoolListCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "DBCTaskman.h"
#include "CoolListCtrl.h"


// CCoolListCtrl

IMPLEMENT_DYNAMIC(CCoolListCtrl, CListCtrl)

CCoolListCtrl::CCoolListCtrl()
	: nHot(-1)
	, DrawAllColForSubItem(FALSE)
	, IsProcList(TRUE)
	, FullColumnCount(0)
	, PopMenuAction(FALSE)
	, FlagSortUp(TRUE)
	, CurrentSortColumn(0)
	, FlagDrawAllColumns(FALSE)
	, pColStatusArray(NULL)
	, m_bMouseTracking(FALSE)
	, m_nHotArrowItem(-1)
	, m_nHotArrowSubItem(-1)

{



	hTheme = OpenThemeData(this->GetSafeHwnd(), L"Explorer::TreeView"); // ◆ 如果想自绘这个效果 就不能再设置SetWindowTheme(mProcessList.GetSafeHwnd(),L"explorer",  




}

CCoolListCtrl::~CCoolListCtrl()
{
}


BEGIN_MESSAGE_MAP(CCoolListCtrl, CListCtrl)

	//ON_WM_PAINT()
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
	ON_WM_MEASUREITEM_REFLECT()
	ON_NOTIFY_REFLECT(LVN_HOTTRACK, &CCoolListCtrl::OnLvnHotTrack)
	ON_NOTIFY_REFLECT(NM_DBLCLK, &CCoolListCtrl::OnNMDblclk)
	ON_NOTIFY_REFLECT(NM_CLICK, &CCoolListCtrl::OnNMClick)
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()

	ON_NOTIFY_REFLECT(LVN_DELETEITEM, &CCoolListCtrl::OnLvnDeleteitem)
	ON_NOTIFY(HDN_ITEMCHANGINGA, 0, &CCoolListCtrl::OnHdnItemchanging)
	ON_NOTIFY(HDN_ITEMCHANGINGW, 0, &CCoolListCtrl::OnHdnItemchanging)
	ON_WM_HSCROLL()
	ON_WM_SIZE()

	ON_NOTIFY(HDN_ENDTRACKA, 0, &CCoolListCtrl::OnHdnEndtrack)
	ON_NOTIFY(HDN_ENDTRACKW, 0, &CCoolListCtrl::OnHdnEndtrack)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, &CCoolListCtrl::OnLvnItemchanged)

	ON_NOTIFY(HDN_BEGINDRAG, 0, &CCoolListCtrl::OnHdnBegindrag)
	ON_NOTIFY(HDN_ENDDRAG, 0, &CCoolListCtrl::OnHdnEnddrag)

	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, &CCoolListCtrl::OnLvnGetdispinfo)
	ON_WM_VSCROLL()
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()



// CCoolListCtrl message handlers

//

void  CCoolListCtrl::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	//顺序  CDDS_PREPAINT->CDDS_PREERASE->CDDS_POSTERASE->CDDS_POSTPAINT






	//CFont GroupTitleFont;

	CHeaderCtrl* pHeaderCtrl = GetHeaderCtrl();



	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast <NMLVCUSTOMDRAW*> (pNMHDR);

	*pResult = 0;
	// Request item-specific notifications if this is the 
	// beginning of the paint cycle.

	//CString ST;
	//ST.Format(L"Times:%d,ItemID:%d",++UUU,pLVCD-> nmcd.dwItemSpec);
	//theApp.m_pMainWnd->SetWindowTextW(ST);
	//Sleep(500);

	CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
	if (CDDS_PREPAINT == pLVCD->nmcd.dwDrawStage)   //整体背景处理
	{


		CRect rc;
		GetClientRect(rc);

		CRect rcSubItem, rcLastItem;

		int n = GetItemCount();
		this->GetItemRect(n - 1, rcLastItem, LVIR_BOUNDS);

		int LineBottom = rc.bottom;






		CPen LinePen(0, 1, RGB(234, 213, 160));
		CPen LinePenRed(0, 1, RGB(200, 0, 0));

		CPen* OldPen = pDC->SelectObject(&LinePen);

		for (int i = 0;i < FullColumnCount;i++)
		{
			if (pColStatusArray[i].IsHiddenColumn) continue;

			GetSubItemRect(0, i, LVIR_BOUNDS, rcSubItem);
			rcSubItem.top = 0;// rcItem.bottom;
			rcSubItem.bottom = rc.bottom;

			if (!IsProcList)
			{
				if (rcLastItem.bottom < rc.bottom)  LineBottom = rcLastItem.bottom; //控制竖线线是否画到底

			}
			if (pColStatusArray[i].Cool && pColStatusArray[i].ColWidth != 0)
			{
				if (IsProcList && (rcLastItem.bottom < rc.bottom)) //补充 最底部黄色填充
				{
					CRect rcBottomFix(rcSubItem.left, rcLastItem.bottom, rcSubItem.right, rc.bottom);
					pDC->FillSolidRect(rcBottomFix, RGB(255, 244, 196));
				}

				int dx = 1;

				if (pColStatusArray[i].Percents > 90)
				{
					//OldPen = pDC->SelectObject(&LinePenRed);
				}
				//pDC-> FillSolidRect ( rcSubItem.left,0,rcSubItem.Width(),rc.Height(), RGB(255,244,196) ); 

				pDC->MoveTo(rcSubItem.left - 1, rc.top);
				pDC->LineTo(rcSubItem.left - 1, LineBottom);

				pDC->MoveTo(rcSubItem.right - 1, rc.top);
				pDC->LineTo(rcSubItem.right - 1, LineBottom);
				//OldPen = pDC->SelectObject(&LinePen);


			}

		}

		LinePen.DeleteObject();
		LinePenRed.DeleteObject();

		*pResult = CDRF_NOTIFYITEMDRAW;


	}
	else if (CDDS_ITEMPREPAINT == pLVCD->nmcd.dwDrawStage)
	{

		// This is the beginning of an item 's paint cycle.
		LVITEM lvItem;

		int  nItem = static_cast <int> (pLVCD->nmcd.dwItemSpec);

		// Determinar si la aplicacion es la ventana activa en Windows.
		// GetFocus() puede devolver NULL si ningun control hijo tiene foco,
		// por eso se valida tambien contra el nivel superior activo.
		HWND hFocus = ::GetFocus();
		CWnd* pTopLevel = GetTopLevelParent();
		BOOL bListHasFocus = (hFocus == m_hWnd) || ::IsChild(m_hWnd, hFocus) ||
			(pTopLevel != NULL && pTopLevel->GetSafeHwnd() == ::GetActiveWindow());

		//COLORREF crBkgnd; 

		CRect  rcItem;
		CRect  rcText;
		CString  StrSubItem;
		//UINT uFormat;


		// ★ FIX Y-overflow: aislar HDC para que GDI+ Graphics no contamine
		// el clip region entre items durante el mismo paint cycle.
		int nSavedDC = pDC->SaveDC();
		Graphics Graph(pDC->m_hDC);

		APPLISTDATA* pListData = (APPLISTDATA*)GetItemData(nItem);

		// FIX: si el item fue eliminado entre la decisión de pintar y la pintura
		// real (carrera con OnUMTimer / worker thread), GetItemData puede devolver
		// NULL. Salimos sin tocar pListData->* y sin alterar el flujo del paint.
		if (pListData == NULL)
		{
			*pResult = CDRF_SKIPDEFAULT;
			Graph.ReleaseHDC(pDC->m_hDC);
			pDC->RestoreDC(nSavedDC);
			return;
		}

		int nCol = pHeaderCtrl->GetItemCount();


		// Get the image index and selected/focused state of the 
		// item being drawn. 
		ZeroMemory(&lvItem, sizeof(LVITEM));

		lvItem.mask = LVIF_IMAGE | LVIF_STATE;
		lvItem.iItem = nItem;

		lvItem.iImage = nItem;
		lvItem.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
		GetItem(&lvItem);

		// Get the rect that holds the item 's icon. 



		// Get the rect that bounds the text label. 

		GetItemRect(nItem, rcItem, LVIR_BOUNDS); // 



		int ItemHeight = rcItem.Height();

		// Draw the background of the list item. Colors are selected 
		// according to the item 's state.


		CRect rcSubItem;

		//------------------------item 背景基础颜色--------------------


		//pDC-> FillSolidRect ( rcItem, theApp.WndBkgColor ); 



		//特殊原因 需自己计算出 rcItem ！！！！！！！

		int ItemW = 0;

		for (int i = 0;i < FullColumnCount; i++)  //黄色部分
		{
			ItemW += pColStatusArray[i].ColWidth; //累计列宽！

			if ((pColStatusArray[i].ColWidth == 0 || pColStatusArray[i].Redraw == 0) && (!FlagDrawAllColumns)) continue;

			if (pColStatusArray[i].Cool == FALSE) continue;

			GetSubItemRect(nItem, i, LVIR_BOUNDS, rcSubItem);

			rcSubItem.right--;

			if (pListData->pPData == NULL) //分组标题
			{

				pDC->FillSolidRect(rcSubItem, RGB(255, 249, 228));

			}
			else
			{

				if (pListData->CoolUsageArray[i] > 0)
				{

					pDC->FillSolidRect(rcSubItem, GetCoolColor(pListData->CoolUsageArray[i]));
				}
				else //if(pListData->SubType == SUB_ITEM )
				{

					pDC->FillSolidRect(rcSubItem, RGB(255, 244, 196));
				}
			}

		}

		rcItem.right = rcItem.left + ItemW;

		

		if (pListData->SubType == PARENT_ITEM_OPEN || pListData->SubType == SUB_ITEM) 
		{

			//pDC-> FillSolidRect ( rcItem, RGB(249,249,249) );
			if (IsProcList)
				Graph.FillRectangle(&SolidBrush(Color(6, 0, 0, 0)), rcItem.left, rcItem.top, rcItem.Width(), rcItem.Height());
		}


		CRect rcSelBar;
		rcSelBar.CopyRect(rcItem);

		if (pListData->SubType == SUB_ITEM) rcSelBar.left += LINE_H2;

		if (lvItem.state & LVIS_SELECTED)
		{

			if (pListData->pPData != NULL)
			{
				if (theApp.FlagThemeActive)
					DrawThemeBackground(hTheme, pDC->m_hDC, LVP_LISTITEM, bListHasFocus ? LISS_SELECTED : LISS_SELECTEDNOTFOCUS, rcSelBar, NULL);
				else
				{
					pDC->FillSolidRect(rcSelBar, bListHasFocus ? ::GetSysColor(COLOR_HIGHLIGHT) : ::GetSysColor(COLOR_BTNFACE));
					//Graph.FillRectangle(&SolidBrush(Color(60,112,192,231)),rcSelBar.left,rcSelBar.top,rcSelBar.Width(),rcSelBar.Height());
					//pDC->Draw3dRect(rcSelBar,RGB(112,192,231),RGB(112,192,231));
				}
			}

		}


		if (nItem == nHot)
		{

			if (pListData->pPData != NULL)
			{
				DrawThemeBackground(hTheme, pDC->m_hDC, LVP_LISTITEM, LISS_HOT, rcSelBar, NULL);
			}
		}






		// --------------------------------------------Draw the icon. -----------------------------------------------------



		// Get the rect that holds the item's icon.


		if (pColStatusArray[0].Redraw == 0) goto DRAWTEXT;




		if (pListData->pPData != NULL)  
		{
			int dPos;
			dPos = (ItemHeight - 16) / 2;

			CRect rcCol0;

			GetSubItemRect(nItem, 0, LVIR_LABEL, rcCol0);



			
			CRect rcArrow;

			GetSubItemRect(nItem, 0, LVIR_BOUNDS, rcArrow);

			rcArrow.right = rcArrow.left + rcArrow.Height();


			if (pListData->SubType == PARENT_ITEM_OPEN)
			{
				if (theApp.FlagThemeActive)
				{
					// Importante: la PARTE cambia segun el estado del cursor.
					// - Normal: TVP_GLYPH + GLPS_OPENED (gris nativo).
					// - Hot:    TVP_HOTGLYPH + HGLPS_OPENED (azul nativo de Win10/11).
					// Usar TVP_GLYPH con HGLPS_* renderiza el mismo glifo gris porque
					// las constantes tienen el mismo ID numerico (1, 2) en vsstyle.h.
					CPoint ptCur;
					if (::GetCursorPos(&ptCur))
						ScreenToClient(&ptCur);
					const BOOL bHot = (m_nHotArrowItem == (int)nItem) && rcArrow.PtInRect(ptCur);
					const int nPart = bHot ? TVP_HOTGLYPH : TVP_GLYPH;
					const int nState = bHot ? HGLPS_OPENED : GLPS_OPENED;
					DrawThemeBackground(hTheme, pDC->m_hDC, nPart, nState, rcArrow, NULL);
				}
				else
				{
					_DrawGroupIconClassic(pDC, rcArrow, TRUE);
				}

			}
			else if (pListData->SubType == PARENT_ITEM_CLOSE)
			{
				if (theApp.FlagThemeActive)
				{
					// Ver comentario en PARENT_ITEM_OPEN: la parte debe cambiar
					// a TVP_HOTGLYPH para obtener el chevron azul en hover.
					CPoint ptCur;
					if (::GetCursorPos(&ptCur))
						ScreenToClient(&ptCur);
					const BOOL bHot = (m_nHotArrowItem == (int)nItem) && rcArrow.PtInRect(ptCur);
					const int nPart = bHot ? TVP_HOTGLYPH : TVP_GLYPH;
					const int nState = bHot ? HGLPS_CLOSED : GLPS_CLOSED;
					DrawThemeBackground(hTheme, pDC->m_hDC, nPart, nState, rcArrow, NULL);
				}
				else
				{
					_DrawGroupIconClassic(pDC, rcArrow);
				}
			}




			if (pListData->SubType == SUB_ITEM)  
			{

				rcCol0.left += 10;  

			}

		
			if (pListData->iImage == -1)
			{
				::ImageList_Draw(theApp.mImagelistNormal.m_hImageList, 0, pDC->m_hDC, rcCol0.left + 5 + LINE_H2 - 23, rcItem.top + dPos, ILD_TRANSPARENT); //16是图标实际尺寸


			}
			else
			{


				::ImageList_Draw(hImageList, pListData->iImage, pDC->m_hDC, rcCol0.left + 5 + LINE_H2 - 23, rcItem.top + dPos, ILD_TRANSPARENT); //16是图标实际尺寸


			}

		}




		// Tweak the rect a bit for nicer-looking text alignment. 


		//------------------------------------------------- Draw  text. -------------------------------------------------

		//CRect TTT;

	DRAWTEXT:

		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor(theApp.WndTextColor);



		for (int i = 0;i < FullColumnCount; i++)
		{
			if (((pColStatusArray[i].ColWidth == 0) || (pColStatusArray[i].Redraw == 0)) && (!FlagDrawAllColumns)) continue;//未显示的直接跳过

			if ((pListData->SubType == SUB_ITEM) && (pColStatusArray[i].DrawInSubItem == FALSE))continue;

			StrSubItem = GetItemText(nItem, i);

			GetSubItemRect(nItem, i, LVIR_LABEL, rcText);

			rcText.left += 8;
			rcText.right -= 8;

			if (pListData->SubType == SUB_ITEM)
			{
				rcText.left += 10;
			}


			UINT Align = DT_LEFT;


			/*LVCOLUMNW   LvCol;
			LvCol.mask=LVCF_FMT;
			GetColumn(i,&LvCol);*/


			Align = pColStatusArray[i].Align;


			if (pListData->pPData == NULL) 
			{

				pDC->SelectObject(theApp.mTitleFont);

				rcText.left = rcText.left - 8 - 14; 

				if (IsProcList == FALSE)
				{
					rcText.right = rcItem.right;
				}
				pDC->SetTextColor(theApp.GroupTitleTextColor);
				pDC->DrawText(StrSubItem, rcText, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | Align);
				pDC->SetTextColor(RGB(0, 0, 0));


				break;
			}

			if (i == 0)
			{
				rcText.left += LINE_H2;
			}

			
			if (theApp.FlagThemeActive)
			{
				COLORREF TextColor(RGB(0, 0, 0));
				if (lvItem.state & LVIS_SELECTED)
				{

					GetThemeColor(theApp.hTheme, LVP_LISTITEM, LISS_HOTSELECTED, TMT_TEXTCOLOR, &TextColor);
					pDC->SetTextColor(TextColor);
					pDC->DrawText(StrSubItem, rcText, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | Align);
					pDC->SetTextColor(theApp.WndTextColor);
				}
				else
				{
					GetThemeColor(theApp.hTheme, LVP_LISTITEM, LISS_NORMAL, TMT_TEXTCOLOR, &TextColor);
					pDC->DrawText(StrSubItem, rcText, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | Align);
				}
			}
			else
			{
				if (lvItem.state & LVIS_SELECTED)
				{
					pDC->SetTextColor(bListHasFocus ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : ::GetSysColor(COLOR_BTNTEXT));
					pDC->DrawText(StrSubItem, rcText, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | Align);
					pDC->SetTextColor(theApp.WndTextColor);
				}
				else
				{
					pDC->DrawText(StrSubItem, rcText, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | Align);
				}

			}


		}


		//GroupTitleFont.DeleteObject();



		CRect rc;
		GetClientRect(rc);


		//pDC->SelectObject(&LinePen2);
		if (pListData->SubType == PARENT_ITEM_OPEN)  
		{

			Graph.DrawLine(&Pen(Color(23, 0, 0, 0), 1), 0, rcItem.top, rcItem.right, rcItem.top);

		}

		

		if (pListData->SubType == PARENT_ITEM_CLOSE || pListData->SubType == PARENT_ITEM_NOSUB || pListData->pPData == NULL)
		{
			if (nItem - 1 >= 0)
			{
				APPLISTDATA* pTempData = (APPLISTDATA*)GetItemData(nItem - 1);
				// FIX: validar pTempData antes de leer SubType. Mismo motivo que arriba.
				if (pTempData != NULL && pTempData->SubType == SUB_ITEM)
				{
					Graph.DrawLine(&Pen(Color(23, 0, 0, 0), 1), 0, rcItem.top, rcItem.right, rcItem.top);
				}
			}

		}


		*pResult = CDRF_SKIPDEFAULT; // We 've painted everything. 


		Graph.ReleaseHDC(pDC->m_hDC);

		// ★ FIX Y-overflow: restaurar estado original del HDC
		pDC->RestoreDC(nSavedDC);


	}


	pDC = NULL;



}
void CCoolListCtrl::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{


	lpMeasureItemStruct->itemHeight = LINE_H2;



}
void CCoolListCtrl::SetImageList(HIMAGELIST hIMGList)
{

	hImageList = hIMGList;
	::SendMessage(m_hWnd, LVM_SETIMAGELIST, (WPARAM)LVSIL_SMALL, (LPARAM)hIMGList);
	
	HDITEM              hdItem;
	hdItem.mask = HDI_FORMAT | HDI_HEIGHT;
	for (int i = 0; i < CoolheaderCtrl.GetItemCount(); i++)
	{
		CoolheaderCtrl.GetItem(i, &hdItem);
		hdItem.fmt |= HDF_OWNERDRAW;

		// hdItem.cxy = 80;
		CoolheaderCtrl.SetItem(i, &hdItem);


	}

	CoolheaderCtrl.ModifyStyle(HDS_BUTTONS, 0);





}

void CCoolListCtrl::OnLvnHotTrack(NMHDR* pNMHDR, LRESULT* pResult)
{

	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	// AfxMessageBox(L"ddd");


	CRect rcItem, rcList, rcOldHot;
	this->GetClientRect(rcList);

	// FIX: si el ítem 0 no existe (lista vacía o items recién borrados),
	// GetItemRect devuelve FALSE y rcItem queda con valores indeterminados;
	// calcular NewHotID con esos valores produciría un id basura que dispara
	// repaints sobre índices que ya no existen. Salimos sin tocar nHot ni Invalidate.
	if (!this->GetItemRect(0, rcItem, LVIR_BOUNDS))
	{
		*pResult = 0;
		return;
	}


	CPoint CurPos;
	::GetCursorPos(&CurPos);

	this->ScreenToClient(&CurPos);



	int NewHotID = (CurPos.y - rcItem.top) / LINE_H2;

	if (CurPos.x > rcItem.right) NewHotID = -1;

	if (nHot != NewHotID)
	{

		nHot = NewHotID;
		Invalidate(0);

	}









	*pResult = 0;
}

void CCoolListCtrl::PreSubclassWindow() 
{
	// TODO: Add your specialized code here and/or call the base class

	CListCtrl::PreSubclassWindow();
	CoolheaderCtrl.SubclassWindow(::GetDlgItem(m_hWnd, 0));


}





void CCoolListCtrl::OnNMDblclk(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: Add your control notification handler code here

	if (pNMItemActivate->iItem >= 0)
	{
		APPLISTDATA* pData = (APPLISTDATA*)GetItemData(pNMItemActivate->iItem);
		if (pData->pPData != NULL)
		{
			this->GetParent()->PostMessage(UM_DBCLICK_LSIT, (WPARAM)pNMItemActivate->iItem);
		}
	}

	*pResult = 0;
}

void CCoolListCtrl::OnNMClick(NMHDR *pNMHDR, LRESULT *pResult)
{

	LPNMITEMACTIVATE pItemActivate = (LPNMITEMACTIVATE)pNMHDR;
	*pResult = 0;

	if (pItemActivate->iItem >= 0)
	{
		APPLISTDATA* pData = (APPLISTDATA*)GetItemData(pItemActivate->iItem);
		if (pData != NULL)
		{
			
			if ((pData->pPData != NULL) && (pItemActivate->ptAction.x < LINE_H2))
			{
				this->GetParent()->PostMessage(UM_DBCLICK_LSIT, (WPARAM)pItemActivate->iItem);
			}
		}

		// Notificar al padre para refrescar el panel de detalle del item activo.
		this->GetParent()->PostMessageW(UM_ITEMCHANGED_COOLLIST);
	}
}

void CCoolListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{

	int nLastColumnRight = 0;
	CHeaderCtrl* pHeader = GetHeaderCtrl();
	if (pHeader && pHeader->GetItemCount() > 0 && GetItemCount() > 0)
	{
		CRect rcLastSubItem;
		if (GetSubItemRect(0, pHeader->GetItemCount() - 1, LVIR_BOUNDS, rcLastSubItem))
		{
			nLastColumnRight = rcLastSubItem.right;
		}
	}

	if (nLastColumnRight > 0 && point.x > nLastColumnRight)
	{
		SetFocus();

		// Limpia solo los items actualmente seleccionados (sin SetItemState(-1, ...),
		// que en algunos estilos de CListCtrl activa LVIS_SELECTED en todas las filas).
		POSITION pos = GetFirstSelectedItemPosition();
		while (pos)
		{
			int nItem = GetNextSelectedItem(pos);
			SetItemState(nItem, 0, LVIS_SELECTED | LVIS_FOCUSED);
		}

		// Retornamos sin llamar a CListCtrl::OnLButtonDown para impedir
		// que el control nativo aplique la seleccion en la zona vacia.
		return;
	}

	// Clic dentro del area de columnas: delegamos a la clase base para que
	// Windows aplique la seleccion nativa durante WM_LBUTTONUP. La expansion
	// por clic simple sobre la flecha se gestiona desde OnNMClick.
	CListCtrl::OnLButtonDown(nFlags, point);
}

//void CCoolListCtrl::OnLvnDeleteallitems(NMHDR *pNMHDR, LRESULT *pResult)
//{
//	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
//	// TODO: Add your control notification handler code here
//
//	*pResult = 0;
//}

void CCoolListCtrl::OnLvnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	APPLISTDATA* pData = (APPLISTDATA*)GetItemData(pNMLV->iItem);

	if (pData != NULL)
	{

		delete pData;
		pData = NULL;
	}


	*pResult = 0;
}

int CCoolListCtrl::DeleteItemAndSub(int iItem)
{


	APPLISTDATA* pData = NULL;
	APPLISTDATA* pDelData = NULL;

	pData = (APPLISTDATA*)GetItemData(iItem);


	SetRedraw(0);
	if (pData->SubType == PARENT_ITEM_OPEN)
	{
		int Pos = iItem + 1;


		while (1)  //注意删除后 会造成位置变化所以只需要删除同以位置项即可
		{

			pDelData = (APPLISTDATA*)GetItemData(Pos);
			if (pDelData->SubType == SUB_ITEM)
			{
				DeleteItem(Pos);

			}
			else
			{
				break;
			}

		}

	}

	if (pData->SubType != -1)
		DeleteItem(iItem);
	SetRedraw(1);



	return 0;
}



CRect CCoolListCtrl::_GetRedrawColumn(void)
{

	CRect rc, rcSub, rcTemp;
	GetClientRect(rc);

	if (pColStatusArray == NULL) return  rc;



	for (int n = 0;n < FullColumnCount;n++)
	{
		if (pColStatusArray[n].ColWidth == 0)
		{

			pColStatusArray[n].Redraw = 0;
		}
		else
		{
			GetSubItemRect(0, n, LVIR_BOUNDS, rcSub);
			rcSub.top = 0;rcSub.bottom = 20;
			if (IntersectRect(rcTemp, rc, rcSub))
			{
				pColStatusArray[n].Redraw = 1;
			}
			else
			{
				pColStatusArray[n].Redraw = 0;
			}

		}

	}


	return  rc;

}

void CCoolListCtrl::InitAllColumn(COLUMNSTATUS_EX* pStatusArray, UINT ColumnStringID, const int ColumnCount, int iStartCool)
{

	if (pStatusArray == NULL) return;


	this->FullColumnCount = ColumnCount;

	CString StrCol, StrLoad;


	int SubID = 0;


	LVCOLUMNW  Col;
	Col.mask = LVCF_ORDER;
	Col.iOrder = 0;

	StrLoad.LoadString(ColumnStringID);

	for (int iArray = 0; iArray < ColumnCount; iArray++)
	{

		AfxExtractSubString(StrCol, StrLoad, iArray, L';');
		InsertColumn(iArray, StrCol, 0, pStatusArray[iArray].ColWidth);

		if (pStatusArray[iArray].ColWidth == 0)
		{
			SetColumn(iArray, &Col);

		}

		pStatusArray[iArray].DrawInSubItem = FALSE;
		pStatusArray[iArray].iOrder = 0;
		pStatusArray[iArray].ColWStore = 123;



	}

	pStatusArray[0].DrawInSubItem = TRUE;

	CoolheaderCtrl.ColStatusArray = pColStatusArray = pStatusArray;
	CoolheaderCtrl.Height = 56;
	CoolheaderCtrl.SetParent(this->GetParent());
	SetHeaderPosSize();




	CoolheaderCtrl.ModifyStyle(0, HDS_BUTTONS);
	CoolheaderCtrl.pCurrentSortCol = &CurrentSortColumn;
	CoolheaderCtrl.pFlagSortUp = &FlagSortUp;





}

BOOL CCoolListCtrl::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	// TODO: Add your specialized code here and/or call the base class

	HD_NOTIFY* pHDNotify = (HD_NOTIFY*)lParam;

	switch (((NMHDR*)lParam)->code)
	{
	case   HDN_BEGINTRACKW:
	case   HDN_BEGINTRACKA:
	case   HDN_DIVIDERDBLCLICKA:
	case   HDN_DIVIDERDBLCLICKW:       //   pHDNotify->iItem —设定为自己不想改变的列值，比如pHDNotify->iItem=0，就是第一列     //固定列宽
		if (pColStatusArray[pHDNotify->iItem].IsHiddenColumn)
		{
			*pResult = TRUE;                                 //   disable   tracking      
			return   TRUE;
		}


		break;
	}


	return CListCtrl::OnNotify(wParam, lParam, pResult);
}




void CCoolListCtrl::OnHdnItemchanging(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
	//注意 表头移动出列表后 拖动表头 原来位置处的项目不会实时 自动刷新  此函数 可以弥补这个！！！！
	//不要删除 有用！！！ 不用添加代码！！！！

	if (!(GetParent()->IsWindowVisible()))return;

	//	if(phdr->pitem->cxy<5&& phdr->pitem->)phdr->pitem->cxy=5;
	if (pColStatusArray != NULL)
	{
		if (!PopMenuAction) //NotPopMenu用于区分菜单增减列的动作
		{
			//MSBOX(666)
			pColStatusArray[phdr->iItem].ColWidth = phdr->pitem->cxy;
			if (pColStatusArray[phdr->iItem].ColWidth > 0)
			{
				pColStatusArray[phdr->iItem].IsHiddenColumn = FALSE;
			}

		}
	}




	PopMenuAction = FALSE;


	*pResult = 0;
}

void CCoolListCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// TODO: Add your message handler code here and/or call default


	SetHeaderPosSize();



	CListCtrl::OnHScroll(nSBCode, nPos, pScrollBar);
}


void CCoolListCtrl::SetHeaderPosSize(void)
{
	CRect rcItem;
	CRect rc;
	this->GetItemRect(0, rcItem, LVIR_BOUNDS);
	this->GetClientRect(rc);



	//this->GetScrollPos(

	CoolheaderCtrl.PosX = rcItem.left;

	int ScrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);
	// setwndtext(CoolheaderCtrl.PosX )

	//CoolheaderCtrl.MoveWindow(CoolheaderCtrl.PosX,0,-CoolheaderCtrl.PosX+rc.Width()+ScrollBarWidth,CoolheaderCtrl.Height);
	CoolheaderCtrl.SetWindowPos(NULL, CoolheaderCtrl.PosX, 0, -CoolheaderCtrl.PosX + rc.Width() + ScrollBarWidth, CoolheaderCtrl.Height, SWP_NOOWNERZORDER);

}

void CCoolListCtrl::OnSize(UINT nType, int cx, int cy)
{
	CListCtrl::OnSize(nType, cx, cy);
	SetHeaderPosSize();
}



void CCoolListCtrl::OnHdnEndtrack(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
	// TODO: Add your control notification handler code here

	SetColumnWidth(phdr->iItem, phdr->pitem->cxy);

	this->CoolheaderCtrl.FlagLeftBtnDown = FALSE;
	*pResult = 0;
}



void CCoolListCtrl::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	// TODO: Add your message handler code here
	// Do not call CListCtrl::OnPaint() for painting messages





}

BOOL CCoolListCtrl::OnEraseBkgnd(CDC* pDC)
{
	// TODO: Add your message handler code here and/or call default





	return TRUE;
	//return CListCtrl::OnEraseBkgnd(pDC);
}



void CCoolListCtrl::OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	//	this->GetParent()->PostMessageW(UM_ITEMCHANGED_COOLLIST);

	this->Invalidate(0);

	*pResult = 0;
}


void CCoolListCtrl::OnHdnBegindrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
	// TODO: Add your control notification handler code here

	if (phdr->iItem == 0)
	{
		*pResult = 1;
		return;
	}


	this->CoolheaderCtrl.FlagLeftBtnDown = FALSE;


	*pResult = 0;
}

void CCoolListCtrl::OnHdnEnddrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
	// TODO: Add your control notification handler code here


	LVCOLUMNW  Col;
	Col.mask = LVCF_ORDER | LVCF_SUBITEM;
	GetColumn(0, &Col);
	int iOrder_FirstShow = Col.iOrder;



	if (phdr->pitem->iOrder <= iOrder_FirstShow) //禁止其他列插在 第一列前
	{

		phdr->pitem->iOrder += 2;
		*pResult = 1;
		return;

	}
	if (phdr->iItem == 0)
	{
		*pResult = 1;
		return;
	}



	this->CoolheaderCtrl.FlagLeftBtnDown = FALSE;



	this->Invalidate();

	*pResult = 0;
}




void CCoolListCtrl::OnLvnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	// TODO: Add your control notification handler code here

	SetHeaderPosSize();
	*pResult = 0;
}




COLORREF CCoolListCtrl::GetCoolColor(double Hot)
{
	COLORREF RetColor = RGB(255, 244, 196);



	//由于只显示两位小数 所以如果 Hot>0 会造成错误 --数值显示0颜色不对应

	if (Hot > 0.0001 && Hot <= 0.2)
	{
		RetColor = RGB(249, 236, 168);
	}
	else if (Hot > 0.2 && Hot <= 0.4)
	{
		RetColor = RGB(255, 228, 135);
	}
	else if (Hot > 0.4 && Hot <= 0.6)
	{
		RetColor = RGB(255, 210, 100);
	}
	else if (Hot > 0.6 && Hot <= 0.8)
	{
		RetColor = RGB(250, 184, 25);
	}
	else if (Hot > 0.8)
	{
		RetColor = RGB(255, 167, 29);
	}



	return RetColor;
}

void CCoolListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// TODO: Add your message handler code here and/or call default

	this->Invalidate();
	CListCtrl::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CCoolListCtrl::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CListCtrl::OnShowWindow(bShow, nStatus);

	// TODO: Add your message handler code here

}


BOOL CCoolListCtrl::ShowOrHideColumn(int iCol)
{

	BOOL Ret;
	LVCOLUMNW  Col, Col0;
	Col.mask = LVCF_ORDER | LVCF_SUBITEM;
	Col0.mask = LVCF_ORDER | LVCF_SUBITEM;


	PopMenuAction = TRUE;
	GetColumn(iCol, &Col);
	GetColumn(0, &Col0);

	if (pColStatusArray[iCol].IsHiddenColumn) //如果已经隐藏则变为显示状态
	{

		pColStatusArray[iCol].ColWidth = pColStatusArray[iCol].ColWStore;
		pColStatusArray[iCol].Redraw = 1;
		pColStatusArray[iCol].IsHiddenColumn = FALSE;
		//MSB(pColStatusArray[iCol].ColWidth)
		if (pColStatusArray[iCol].iOrder < Col0.iOrder)
		{
			Col.iOrder = Col0.iOrder;//注意不要加1 否则反而不正确！
		}
		else
		{
			Col.iOrder = pColStatusArray[iCol].iOrder;
		}

		SetColumn(iCol, &Col);
		SetColumnWidth(iCol, pColStatusArray[iCol].ColWidth);
		Ret = TRUE;




	}
	else//否则 隐藏对应列
	{
		pColStatusArray[iCol].iOrder = Col.iOrder;//保持隐藏前位置
		pColStatusArray[iCol].ColWStore = GetColumnWidth(iCol);
		pColStatusArray[iCol].ColWidth = 0;
		pColStatusArray[iCol].Redraw = 0;
		pColStatusArray[iCol].IsHiddenColumn = TRUE; //决定隐现的主要标志
		Col.iOrder = 0;
		SetColumn(iCol, &Col);
		SetColumnWidth(iCol, 0);
		Ret = FALSE;

	}

	return Ret;

}

void CCoolListCtrl::MySetItemText(int iItem, int iCol, CString StrToSet)
{
	LVITEM Item;
	Item.iItem = iItem;
	Item.iSubItem = iCol;
	Item.mask = LVIF_TEXT;
	Item.cchTextMax = MAX_PATH;
	Item.pszText = (LPTSTR)(LPCTSTR)(StrToSet);
	SetItem(&Item);
}

void CCoolListCtrl::_DrawGroupIconClassic(CDC* pdc, CRect Rc, BOOL Open)
{

	int MidW = Rc.Height() / 2;

	CRect RcNew;
	RcNew.left = Rc.left + MidW - 5;
	RcNew.top = Rc.top + MidW - 5;

	RcNew.right = RcNew.left + 9;	RcNew.bottom = RcNew.top + 9;


	pdc->FillSolidRect(RcNew, RGB(255, 255, 255));
	pdc->Draw3dRect(RcNew, RGB(128, 128, 128), RGB(128, 128, 128));

	int MidY = RcNew.top + 4;
	int MidX = RcNew.left + 4;


	//pdc->MoveTo(Rc.left+2,MidY);
	//pdc->LineTo(Rc.right-2,MidY);
	pdc->FillSolidRect(RcNew.left + 2, MidY, RcNew.Height() - 4, 1, RGB(0, 0, 0));

	if (!Open)
	{
		pdc->FillSolidRect(MidX, RcNew.top + 2, 1, RcNew.Height() - 4, RGB(0, 0, 0));
	}


}

// ---------------------------------------------------------------------------
// Rastreo del cursor para el efecto hover sobre el chevron de expansion.
// El dibujado normal del control ya pinta la fila hot (mediante OnLvnHotTrack
// y nHot). Aqui solo anadimos la deteccion especifica del rect del chevron
// para conmutar GLPS_OPENED/CLOSED a GLPS_HOTED en OnCustomDraw.
// ---------------------------------------------------------------------------

// Devuelve el item y subitem cuyo chevron esta bajo 'point', o (-1,-1) si no
// hay chevron bajo el cursor. Recorre solo filas padre (PARENT_ITEM_OPEN/CLOSE).
static void _FindArrowUnderCursor(CCoolListCtrl* pList, CPoint point, int& nItem, int& nSubItem)
{
	nItem = -1;
	nSubItem = -1;
	if (pList == NULL) return;

	const int nCount = pList->GetItemCount();
	for (int i = 0; i < nCount; i++)
	{
		APPLISTDATA* pData = (APPLISTDATA*)pList->GetItemData(i);
		if (pData == NULL) continue;
		if (pData->SubType != PARENT_ITEM_OPEN && pData->SubType != PARENT_ITEM_CLOSE)
			continue;

		CRect rcArrow;
		if (!pList->GetSubItemRect(i, 0, LVIR_BOUNDS, rcArrow))
			continue;

		// El chevron ocupa solo el cuadrado inicial de la primera columna,
		// del mismo ancho que la altura de la fila (como en OnCustomDraw).
		rcArrow.right = rcArrow.left + rcArrow.Height();

		if (rcArrow.PtInRect(point))
		{
			nItem = i;
			nSubItem = 0;
			return;
		}
	}
}

void CCoolListCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	// Si aun no estamos rastreando, solicitar WM_MOUSELEAVE al sistema.
	if (!m_bMouseTracking)
	{
		TRACKMOUSEEVENT tme = { 0 };
		tme.cbSize = sizeof(tme);
		tme.hwndTrack = m_hWnd;
		tme.dwFlags = TME_LEAVE;
		tme.dwHoverTime = 0;
		_TrackMouseEvent(&tme);
		m_bMouseTracking = TRUE;
	}

	// Determinar si el cursor esta sobre algun chevron de expansion.
	int nNewHotItem = -1;
	int nNewHotSub = -1;
	_FindArrowUnderCursor(this, point, nNewHotItem, nNewHotSub);

	if (nNewHotItem != m_nHotArrowItem || nNewHotSub != m_nHotArrowSubItem)
	{
		// Invalidar SOLO el area del chevron anterior y del nuevo para evitar
		// repintar todo el control y reducir el parpadeo. Usamos la fila del
		// item como aproximacion conservadora del rect a repintar.
		if (m_nHotArrowItem >= 0)
		{
			CRect rcRow;
			if (GetItemRect(m_nHotArrowItem, rcRow, LVIR_BOUNDS))
				InvalidateRect(rcRow, FALSE);
		}
		if (nNewHotItem >= 0)
		{
			CRect rcRow;
			if (GetItemRect(nNewHotItem, rcRow, LVIR_BOUNDS))
				InvalidateRect(rcRow, FALSE);
		}

		m_nHotArrowItem = nNewHotItem;
		m_nHotArrowSubItem = nNewHotSub;
	}

	CListCtrl::OnMouseMove(nFlags, point);
}

void CCoolListCtrl::OnMouseLeave()
{
	m_bMouseTracking = FALSE;

	// Si saliamos del control con un chevron hot, repintarlo para volver a
	// su estado normal (GLPS_OPENED/GLPS_CLOSED en lugar de GLPS_HOTED).
	if (m_nHotArrowItem >= 0)
	{
		CRect rcRow;
		if (GetItemRect(m_nHotArrowItem, rcRow, LVIR_BOUNDS))
		{
			m_nHotArrowItem = -1;
			m_nHotArrowSubItem = -1;
			InvalidateRect(rcRow, FALSE);
		}
		else
		{
			m_nHotArrowItem = -1;
			m_nHotArrowSubItem = -1;
		}
	}
}