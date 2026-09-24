// PerformanceBox_Cpu.cpp

#include "stdafx.h"
#include "DBCTaskman.h"
#include "PerformanceBox.h"
#include "PerfCpuSpeed.h"

extern "C" {
#include <powrprof.h>
}
#pragma comment(lib, "PowrProf.lib")

void CPerformanceBox::GetCPUInfo(void)
{ 

	CString strPath=_T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0");//ע����Ӽ�·��  
	CRegKey regkey;//����ע��������  
	LONG lResult;//LONG�ͱ�������Ӧ���  
	lResult=regkey.Open(HKEY_LOCAL_MACHINE,LPCTSTR(strPath),KEY_QUERY_VALUE ); //��ע�����   ע�� Ҫ�� �� ��Ȩ�� KEY_ALL_ACCESS ��ĳЩ�û��ᵼ�´���
	if (lResult==ERROR_SUCCESS)  
	{  
		WCHAR chCPUName[50] = {0};  
		DWORD dwSize=50;   

		//��ȡProcessorNameString�ֶ�ֵ  
		if (ERROR_SUCCESS == regkey.QueryStringValue(_T("ProcessorNameString"),chCPUName,&dwSize))  
		{  
			StrCPUName = chCPUName;  
		}  

	}  


	nLogicalProcessor  =1;


	//��ѯCPU��Ƶ
	DWORD dwValue;
	if (ERROR_SUCCESS == regkey.QueryDWORDValue(_T("~MHz"),dwValue))
	{
		MaxCPUSpeed = dwValue;
		// Same registry value, expressed in GHz, for the Win10-style
		// cascade:  fCurrentGhz = fBaseGhz * (% Processor Performance / 100).
		// Captured once here so the cascade is stable across re-entries.
		fBaseGhz = (double)dwValue / 1000.0;
		if(!_finite(fBaseGhz) || fBaseGhz <= 0.0) fBaseGhz = 0.0;
	}
	regkey.Close();//�ر�ע���

	//UpdateData(FALSE);  

	//��ȡCPU������Ŀ  
	/*   SYSTEM_INFO si;  
	memset(&si,0,sizeof(SYSTEM_INFO));  
	GetSystemInfo(&si);  
	nLogicalProcessor = si.dwNumberOfProcessors;  
	*/

	PM.GetCpuInfo();

	nLogicalProcessor =  theApp.PerformanceInfo.nLogicalProcessor;

	if(theApp.PerformanceInfo.nLogicalProcessor == 0)
	{
		// ��ȡCPU������Ŀ  
		SYSTEM_INFO si;  
		memset(&si,0,sizeof(SYSTEM_INFO));  
		GetSystemInfo(&si);  
		nLogicalProcessor = si.dwNumberOfProcessors;  
		theApp.PerformanceInfo.nLogicalProcessor = nLogicalProcessor;


	}




}

void CPerformanceBox::_UpdateCpuInfoBox(BOOL UpdateAll)
{
	if(!ShowThisPage) return ;


	PERFORMANCE_INFORMATION MyPMInfo;
	ZeroMemory(&MyPMInfo, sizeof(PERFORMANCE_INFORMATION));
	MyPMInfo.cb = sizeof(PERFORMANCE_INFORMATION);
	GetPerformanceInfo(&MyPMInfo,sizeof(PERFORMANCE_INFORMATION));

	theApp.PerformanceInfo.ProcessCount =MyPMInfo.ProcessCount ;

	//MSG(0)

	CString StrUpTime;




	StrUpTime.Format(L"%d:%02d:%02d:%02d",theApp.UpTimeDay,theApp.UpTimeHour,theApp.UpTimeMin,(int)theApp.UpTimeSec);

	double  CurrentSpeed = 0;
	double  nThreads;
	double  nHandles;


	nThreads = MyPMInfo.ThreadCount;//PM.PdhGetInfo(L"\\System\\Threads");
	nHandles = MyPMInfo.HandleCount;//PM.PdhGetInfo(L"\\Process(_Total)\\Handle Count");


	//CurrentSpeed = PM.PdhGetInfo(L"\\Processor Information(0,0)\\Processor Frequency");


	
	PROCESSOR_POWER_INFORMATION  *PPInfo = new PROCESSOR_POWER_INFORMATION [theApp.PerformanceInfo.ProcessCount] ;
	NTSTATUS Ret = CallNtPowerInformation( ProcessorInformation,NULL, 0,PPInfo,sizeof(PROCESSOR_POWER_INFORMATION)*theApp.PerformanceInfo.ProcessCount );

	 

	CurrentSpeed =  PPInfo[0].CurrentMhz;	
	CurrentSpeed=CurrentSpeed/1000;//ע�����cpuƵ���� ����1000����1024;






   /// CPU ��ǰ�ٶȸ��ں���  UpdateAllPMInfo �л�ȡ ���������б� ��ͬ��
 
	//����ʱ��ǰ��ȡһ�η�������ʾ

	pInfoBoxCpu->Info[0].StrInfo.Format(L"%0.0f%%", theApp.PerformanceInfo.CpuUsage) ;
	//pInfoBoxCpu->Info[1].StrInfo.Format(L"%0.2f GHz",CurrentSpeed);
	pInfoBoxCpu->Info[2].StrInfo.Format(L"%d",theApp.PerformanceInfo.ProcessCount);  //Processes
	pInfoBoxCpu->Info[3].StrInfo.Format(L"%d",(int)nThreads);  //Threads
	pInfoBoxCpu->Info[4].StrInfo.Format(L"%d",(int)nHandles);//Handles
	pInfoBoxCpu->Info[5].StrInfo  = StrUpTime ;//Up time
  
 

	 

	if(UpdateAll)
	{
		CString StrCpuInfo;

		CString StrMaxSpeed;

		if(MaxCPUSpeed>1000) //ע���������1024����
		{
			MaxCPUSpeed= MaxCPUSpeed/1000;
			StrMaxSpeed.Format(L"%.2f GHz",MaxCPUSpeed);

		}
		else
		{
			StrMaxSpeed.Format(L"%.2f MHz",MaxCPUSpeed);
		}

		StrCpuInfo.Format(L"%s\n%d\n%d\n%d\n%s\n%s\n%s\n%s",StrMaxSpeed, theApp.PerformanceInfo.nPhysicalProcessorPackages,theApp.PerformanceInfo.nProcessorCores,theApp.PerformanceInfo.nLogicalProcessor,L" ",theApp.PerformanceInfo.StrL1Cache,theApp.PerformanceInfo.StrL2Cache,theApp.PerformanceInfo.StrL3Cache);

		pInfoBoxCpu->Info[6].StrInfo  = StrCpuInfo ;

	} 

	delete []  PPInfo;
}


void CPerformanceBox::_GetLogicalProcessorUsage(CWaveBox *pBox,double * pTotalUsage,double * pTotalKernelUsage )
{
	if(!spi || !MyNtQuerySystemInformation || nLogicalProcessor <= 0)
		return;

	memset(spi,0,sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nLogicalProcessor);

	unsigned long bytesreturned;
	MyNtQuerySystemInformation(SystemProcessorPerformanceInformation,spi, (sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)*nLogicalProcessor),&bytesreturned);


	double TotalCpuUsage = 0;
	double TotalKernelCpuUsage = 0;


	CString Str0,Str1,StrTemp;

	for(int cpuloopcount = 0; cpuloopcount < nLogicalProcessor; cpuloopcount++) 
	{
		//-----------����ǰ��---------------
		/*for(int i=0;i<61-1;i++)
		{
		pBox->Num[cpuloopcount][i] = pBox->Num[cpuloopcount][i+1];
		pBox->Num2[cpuloopcount][i] = pBox->Num2[cpuloopcount][i+1];
		}*/

		memmove(&pBox->Num[cpuloopcount][0],&pBox->Num[cpuloopcount][1],ArraySize);
		memmove(&pBox->Num2[cpuloopcount][0],&pBox->Num2[cpuloopcount][1],ArraySize);




		LONGLONG TimeUserAndKernel=  (spi[cpuloopcount].KernelTime.QuadPart + spi[cpuloopcount].UserTime.QuadPart) - (spi_old[cpuloopcount].KernelTime.QuadPart + spi_old[cpuloopcount].UserTime.QuadPart);

		LONGLONG TimeIdle = spi[cpuloopcount].IdleTime.QuadPart - spi_old[cpuloopcount].IdleTime.QuadPart ;

		//------------------------
		double CpuUsage =0;
		if(TimeUserAndKernel>0)
		{
			CpuUsage = 100 - (double)( TimeIdle * 100  / TimeUserAndKernel);
		}


		LONGLONG TimeUser = spi[cpuloopcount].UserTime.QuadPart-spi_old[cpuloopcount].UserTime.QuadPart;

		LONGLONG TimeKernel =  spi[cpuloopcount].KernelTime.QuadPart  - spi_old[cpuloopcount].KernelTime.QuadPart ;


		//  double CpuUsag_Kernel = (double)( TimeKernel*100/TimeUserAndKernel);	  

		double CpuUsag_Kernel = 0;
		if(TimeUserAndKernel>0)
		{
			CpuUsag_Kernel = 100  - (double)(  (TimeIdle+TimeUser)* 100  /TimeUserAndKernel);
		}



		pBox->Num[cpuloopcount][60] = ( float)  (CpuUsage/100 ); //
		pBox->Num2[cpuloopcount][60] =( float)  (CpuUsag_Kernel/100  );

		spi_old[cpuloopcount] = spi[cpuloopcount];

		TotalCpuUsage = TotalCpuUsage + pBox->Num[cpuloopcount][60];
		TotalKernelCpuUsage = TotalKernelCpuUsage + pBox->Num2[cpuloopcount][60];
	}

	TotalCpuUsage = TotalCpuUsage/nLogicalProcessor;
	TotalKernelCpuUsage = TotalKernelCpuUsage/nLogicalProcessor;

	* pTotalUsage = TotalCpuUsage;
	*pTotalKernelUsage = TotalKernelCpuUsage;



}


void CPerformanceBox::_DetermineRowCol(int nLogicalProcessor , int * pNRow, int * pNCol)//ȷ����cpu����ͼ��Ϊ���м�����ʾ
{
	int nRow =  (int)sqrt( (float) nLogicalProcessor );



	while(1)
	{
		if(	nLogicalProcessor%nRow == 0) //�����������������Ϊ���� �����һ ֱ��������
		{
			break;
		}
		else
		{
			nRow--;
		}

	}

	* pNRow = nRow;
	* pNCol = nLogicalProcessor/nRow;




}


double CPerformanceBox::_GetSurrentCpuSpeed(void)  // Retorna GHz
{
	// ------------------------------------------------------------------
	//  Frequency cascade (autonomous, no OS-version helper).
	//
	//  PDH "% Processor Performance" is the authoritative source on
	//  Win10/11 / Ryzen / modern CPUs but the counter is missing or
	//  capped near 100 % on Windows 7 and on legacy Intel (Sandy Bridge
	//  family) CPUs. The cascade below is fully decoupled from any
	//  OS-version check and is driven by the PDH sample (when available)
	//  plus the raw CallNtPowerInformation reading (CurMhz / MaxMhz)
	//  and theApp.PerformanceInfo.CpuUsage.
	//
	//  Priority 1 (CASO A) - PDH ready AND pct > 100
	//      Modern CPU in native Turbo. Trust PDH directly:
	//          fBaseGhz * (pct / 100.0)
	//
	//  Priority 2 (CASO B) - high-load Turbo estimator. Triggers when
	//      the cores are pinned near MaxMhz. Two entry paths so both
	//      Win10 capped-PDH and PDH-less Win7 / legacy end up in the
	//      same boost formula:
	//        B1. PDH ready + pct > 70 + CurMhz ~ MaxMhz
	//              (Win10 + Intel PDH driver capping Turbo).
	//        B2. PDH not ready + CpuUsage > 70
	//              (Windows 7 / no PDH counter available).
	//      Scale: 1.00x at 70 % load -> 1.25x at 100 % load, applied
	//      on top of fBaseGhz.
	//
	//  Priority 3 (CASO C) - PDH ready, pct in (0, 100]
	//      Win10/11 idle / mid load. Report the PDH-derived clock
	//      directly so P-state downshifts are reflected accurately
	//      (CurMhz can drop well below fBaseGhz at idle).
	//
	//  Priority 4 - CallNtPowerInformation CurrentMhz (raw P-state).
	//  Priority 5 - fBaseGhz fallback (nominal base clock, 2.50 GHz
	//               as last-resort default).
	// ------------------------------------------------------------------

	// 1. Sample CallNtPowerInformation. Allocate the full per-processor
	//    array (was a stack overrun risk in older revisions when nLP > 1).
	int nLP = theApp.PerformanceInfo.nLogicalProcessor;
	if(nLP < 1) nLP = 1;
	PROCESSOR_POWER_INFORMATION *PPInfo = new PROCESSOR_POWER_INFORMATION[nLP];
	memset(PPInfo, 0, sizeof(PROCESSOR_POWER_INFORMATION)*nLP);
	CallNtPowerInformation(ProcessorInformation, NULL, 0,
		PPInfo, sizeof(PROCESSOR_POWER_INFORMATION)*nLP);

	ULONG CurMhz = PPInfo[0].CurrentMhz;
	ULONG MaxMhz = PPInfo[0].MaxMhz;
	delete [] PPInfo;

	if(_finite(fBaseGhz) == 0 || fBaseGhz < 0.0) fBaseGhz = 0.0;

	// 2. Sample PDH "% Processor Performance" (if the counter resolved).
	PerfPdhCpuPerf_Start();
	BOOL pdhReady = (PerfPdhCpuPerf_GetReady() != 0);
	double pct = pdhReady ? PerfPdhCpuPerf_GetPct() : 0.0;
	if(!_finite(pct)) pct = 0.0;

	// Normalize CpuUsage to the 0..100 scale. Current code already
	// stores it pre-multiplied by 100 (PerformanceBox.cpp:1799), but
	// accept a 0..1 form defensively in case the convention changes.
	double usage = theApp.PerformanceInfo.CpuUsage;
	if(!_finite(usage)) usage = 0.0;
	if(usage <= 1.0 && usage > 0.0) usage *= 100.0;

	// CASO A - Modern CPUs (Win10/11 / Ryzen) where PDH pct can exceed
	//          100 % in Turbo. Trust the PDH reading at face value.
	if(pdhReady && pct > 100.0 && fBaseGhz > 0.0)
		return fBaseGhz * (pct / 100.0);

	// CASO B - High-load Turbo estimator. Activates when the cores are
	//          pinned at MaxMhz, regardless of whether PDH is available.
	if(fBaseGhz > 0.0)
	{
		BOOL pinnedByPdh = pdhReady
			&& pct > 70.0
			&& CurMhz > 0
			&& CurMhz >= (MaxMhz > 100 ? MaxMhz - 100 : 0);
		BOOL pinnedByUsage = !pdhReady && usage > 70.0;

		if(pinnedByPdh || pinnedByUsage)
		{
			// 1.00x at 70 % load -> 1.25x at 100 % load.
			double driver = pinnedByPdh ? pct : usage;
			double boostFactor = 1.0 + (0.25 * ((driver - 70.0) / 30.0));
			return fBaseGhz * boostFactor;
		}
	}

	// CASO C - Win10/11 idle / mid load: report the PDH-derived clock
	//          directly so P-state downshifts are reflected accurately
	//          (CurMhz can drop well below fBaseGhz at idle).
	if(pdhReady && pct > 0.0 && fBaseGhz > 0.0)
		return fBaseGhz * (pct / 100.0);

	// Fallback: CallNtPowerInformation CurrentMhz (raw P-state reading).
	if(CurMhz > 50 && CurMhz < 100000)
		return (double)CurMhz / 1000.0;

	// Last resort: nominal base clock from registry ~MHz, or 2.50 GHz
	// default if even that could not be read.
	return (fBaseGhz > 0.0) ? fBaseGhz : 2.50;
}
