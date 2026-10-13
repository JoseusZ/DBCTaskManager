#pragma once


// CNormalHeaderCtrl

class CNormalHeaderCtrl : public CHeaderCtrl
{
	DECLARE_DYNAMIC(CNormalHeaderCtrl)

public:
	CNormalHeaderCtrl();
	virtual ~CNormalHeaderCtrl();

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnPaint();
	// FIX bug del cuadro negro durante drag nativo (HDS_DRAGDROP) en
	// Windows 7: el DWM usa Alpha=0 para negro. Forzamos el canal alpha
	// a 0xFF sobre una DIBSection de 32 bits. Misma tecnica que
	// CCoolheaderCtrl; aqui reutiliza el OnPaint existente.
	afx_msg LRESULT OnPrintClient(WPARAM wParam, LPARAM lParam);
	BOOL *pFlagSortUp;
	int *pCurrentSortCol;
};


