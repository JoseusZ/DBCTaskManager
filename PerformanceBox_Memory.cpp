// PerformanceBox_Memory.cpp

#include "stdafx.h"
#include "DBCTaskman.h"
#include "PerformanceBox.h"
#include "SMBIOS.h"

void CPerformanceBox::_UpdateMemoryInfoBox(BOOL UpdateAll )
{


	if(!ShowThisPage) return ;

	double  Num,Num2;

	CString StrInUse;

	Num = (double)(theApp.PerformanceInfo.InUsePhysMem/1024/1024);
	StrInUse.Format(L"%0.1f MB",Num );

	if(Num>=1024)
	{
		Num= Num/1024;
		StrInUse.Format(L"%0.1f GB",Num );
	}
	//=-------------

	CString StrAvialable;

	//Num =(double)(MyPMInfo.PhysicalAvailable*MyPMInfo.PageSize/1024/1024);
	Num = (double)(( MyPMInfo.PhysicalAvailable*MyPMInfo.PageSize)/1024/1024);
	StrAvialable.Format(L"%0.1f MB",Num );
	if(Num>=1024)
	{
		Num= Num/1024;
		StrAvialable.Format(L"%0.1f GB",Num );
	}

	//---------------Committed--------------

	CString StrCommitCurrent,StrCommit;

	double CommitCurrent = (double)MyPMInfo.CommitPeak*MyPMInfo.PageSize;
	double CommitTotal = (double)MyPMInfo.CommitLimit*MyPMInfo.PageSize;




	Num = (double)(CommitCurrent/1024/1024);
	StrCommitCurrent.Format(L"%0.1f",Num );
	Num2 = (double)(CommitTotal/1024/1024);
	StrCommit.Format(L"%0.1f MB",Num2 );
	if(Num2>=1024)
	{
		Num= Num/1024;
		StrCommitCurrent.Format(L"%0.1f",Num );
		Num2= Num2/1024;
		StrCommit.Format(L"%0.1f GB",Num2 );
	}

	//------------------ Paged Pool ------------------

	CString StrPagedPool;
	//Num = PM.PdhGetInfo(L"\\Memory\\Pool Paged Bytes");
	Num =(double)(MyPMInfo.KernelPaged*MyPMInfo.PageSize);
	Num= Num/1024/1024;
	StrPagedPool.Format(L"%0.1f MB",Num );
	if(Num>=1024)
	{
		Num= Num/1024;
		StrPagedPool.Format(L"%0.1f GB",Num );
	}
	//------------------ Non-Paged Pool ------------------


	CString StrNonPagedPool;
	//Num = PM.PdhGetInfo(L"\\Memory\\Pool Nonpaged Bytes");
	Num =(double)(MyPMInfo.KernelNonpaged*MyPMInfo.PageSize);

	Num= Num/1024/1024;
	StrNonPagedPool.Format(L"%0.1f MB",Num );
	if(Num>=1024)
	{
		Num= Num/1024;
		StrNonPagedPool.Format(L"%0.1f GB",Num );
	}





	//-----------StrSystemCache ------------



	CString StrSystemCache;
	//Num =(double)(MyPMInfo.SystemCache*MyPMInfo.PageSize);
	Num = theApp.PerformanceInfo.Mem_Modified+theApp.PerformanceInfo.Mem_Standby;
	Num= Num/1024/1024;
	StrSystemCache.Format(L"%0.1f MB",Num );
	if(Num>=1024)
	{
		Num= Num/1024;
		StrSystemCache.Format(L"%0.1f GB",Num );
	}











	/*CString TTT;
	TTT.Format(L"%d",MemStatus.ullTotalPhys/1024/1024/1024);
	theApp.GetMainWnd()->SetWindowTextW(TTT);*/



	pInfoBoxMemory->Info[0].StrInfo  = StrInUse ;
	pInfoBoxMemory->Info[1].StrInfo  = StrAvialable ;
	pInfoBoxMemory->Info[2].StrInfo  = StrCommitCurrent+L"/"+StrCommit;  
	pInfoBoxMemory->Info[3].StrInfo  = StrSystemCache ;  
	pInfoBoxMemory->Info[4].StrInfo  = StrPagedPool ; 
	pInfoBoxMemory->Info[5].StrInfo  = StrNonPagedPool ; 


	if(UpdateAll)
	{

		CSMBIOS  MySMBios;

		UINT  Speed;
		int  nSlotUsed;
		int  nSlot;
		WCHAR   StrFormFactor[100];


		theApp.PerformanceInfo.StrMemoryType = MySMBios.GetMemoryInfo(&nSlotUsed,&nSlot,&Speed,StrFormFactor);



		if(theApp.PerformanceInfo.TotalPhysMem ==0)
		{
			MEMORYSTATUSEX  MemStatus;
			MemStatus.dwLength = sizeof(MemStatus);
			BOOL Ret = GlobalMemoryStatusEx(&MemStatus);
			if(Ret)
			{
				theApp.PerformanceInfo.TotalPhysMem = MemStatus.ullTotalPhys;
			}
		}

		//StringCchCat(StrFormFactor,100,L"\n");

		double MemHWReserved = ((double)theApp.PerformanceInfo.InstalledMemKB*1024-theApp.PerformanceInfo.TotalPhysMem)/1024/1024; //MB



		pInfoBoxMemory->Info[6].StrInfo.Format(L"%d MHz \n%d of %d \n%s \n%0.0f MB",Speed,nSlotUsed,nSlot,StrFormFactor,MemHWReserved);  


       



	}



}

