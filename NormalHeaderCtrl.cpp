// NormalHeaderCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "DBCTaskman.h"
#include "NormalHeaderCtrl.h"


// CNormalHeaderCtrl

IMPLEMENT_DYNAMIC(CNormalHeaderCtrl, CHeaderCtrl)

CNormalHeaderCtrl::CNormalHeaderCtrl()
: pFlagSortUp(NULL)
, pCurrentSortCol(NULL)
{

}

CNormalHeaderCtrl::~CNormalHeaderCtrl()
{
}


BEGIN_MESSAGE_MAP(CNormalHeaderCtrl, CHeaderCtrl)
	ON_WM_PAINT()
	ON_MESSAGE(WM_PRINTCLIENT, OnPrintClient)
	ON_MESSAGE(WM_PRINT, OnPrintClient)
END_MESSAGE_MAP()



// CNormalHeaderCtrl message handlers



LRESULT CNormalHeaderCtrl::OnPrintClient(WPARAM wParam, LPARAM lParam)
{
    CDC* pDC = CDC::FromHandle((HDC)wParam);
    if (!pDC) return 0;

    CRect rc;
    GetClientRect(&rc);
    int cx = rc.Width();
    int cy = rc.Height();

    // 1. Crear un DC en memoria compatible
    CDC MemDC;
    MemDC.CreateCompatibleDC(pDC);

    // 2. Forzar la creación de un bitmap de 32-bits (DIBSection)
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hDIB = CreateDIBSection(pDC->GetSafeHdc(), &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hDIB || !pBits) return 0;

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(MemDC.GetSafeHdc(), hDIB);

    // 3. Delegar al OnPaint existente sobre el bitmap de 32 bits.
    //    Asi nos aseguramos de que lo que se pinta en pantalla es lo
    //    mismo que snapshotamos para el drag.
    {
        CPaintDC dummy(this);
        DefWindowProc(WM_PAINT,(WPARAM)MemDC.GetSafeHdc(),(LPARAM)0);
    }

    CRect rcArrow;
    if (pCurrentSortCol && *pCurrentSortCol >= 0)
    {
        this->GetItemRect(*pCurrentSortCol, rcArrow);
        rcArrow.bottom = rcArrow.top + 10;
        if (theApp.FlagThemeActive)
        {
            HTHEME hTheme = OpenThemeData(this->GetSafeHwnd(), L"HEADER");
            SIZE rcSize;
            if (GetThemePartSize(hTheme, MemDC.m_hDC, HP_HEADERSORTARROW, HSAS_SORTEDUP, NULL, TS_TRUE, &rcSize) == S_OK)
            {
                rcArrow.bottom = rcSize.cy;
            }
            int SortType = HSAS_SORTEDUP;
            if (pFlagSortUp && !(*pFlagSortUp)) SortType = HSAS_SORTEDDOWN;
            DrawThemeBackground(hTheme, MemDC.m_hDC, HP_HEADERSORTARROW, SortType, rcArrow, NULL);
            ::CloseThemeData(hTheme);
        }
    }

    // 4. FIX AGRESIVO PARA WINDOWS 7: forzar Alpha=0xFF en cada pixel.
    DWORD* pPixels = (DWORD*)pBits;
    int totalPixels = cx * cy;
    for (int i = 0; i < totalPixels; ++i)
    {
        pPixels[i] |= 0xFF000000;
    }

    // 5. Transferir al DC del sistema
    pDC->BitBlt(0, 0, cx, cy, &MemDC, 0, 0, SRCCOPY);

    // 6. Limpieza
    SelectObject(MemDC.GetSafeHdc(), hOldBitmap);
    DeleteObject(hDIB);
    MemDC.DeleteDC();

    return 0;
}

void CNormalHeaderCtrl::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	DefWindowProc(WM_PAINT,(WPARAM)dc.m_hDC,(LPARAM)0);

	 
 
	CRect rcArrow;
	this->GetItemRect(*pCurrentSortCol,rcArrow);
	rcArrow.bottom = rcArrow.top+10;

	


	if(theApp.FlagThemeActive)
	{
		HTHEME hTheme=OpenThemeData(this->GetSafeHwnd(), L"HEADER");
		SIZE rcSize;
		if(GetThemePartSize(hTheme,dc.m_hDC,HP_HEADERSORTARROW,HSAS_SORTEDUP,NULL,TS_TRUE,&rcSize)== S_OK)
		{			
			rcArrow.bottom =  rcSize.cy;
			
		}

		int SortType = HSAS_SORTEDUP;

		if(!(*pFlagSortUp)) SortType = HSAS_SORTEDDOWN;
		
		DrawThemeBackground(hTheme,dc.m_hDC,  HP_HEADERSORTARROW ,    SortType ,rcArrow ,NULL); 
		::CloseThemeData(hTheme);

	}
	else
	{
	}

	//setwndtext(rand())

	//dc.FillSolidRect(rcItem,RGB(0,222,0));


}
