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


double CPerformanceBox::_GetSurrentCpuSpeed(void)  //��λ�� GHz
{
	double  CurrentSpeed = 0.0;
	// Bug fix Win7: previous code used a single PROCESSOR_POWER_INFORMATION on stack
	// but told CallNtPowerInformation the buffer was nLogicalProcessor structs wide.
	// When nLogicalProcessor > 1 the kernel writes past the single struct and
	// corrupts the stack of the caller (eventually corrupting UpdateAllPMInfo GS cookie).
	int nLP = theApp.PerformanceInfo.nLogicalProcessor;
	if(nLP < 1) nLP = 1;
	PROCESSOR_POWER_INFORMATION  *PPInfo = new PROCESSOR_POWER_INFORMATION[nLP];
	memset(PPInfo, 0, sizeof(PROCESSOR_POWER_INFORMATION)*nLP);
	CallNtPowerInformation( ProcessorInformation,NULL, 0,PPInfo, sizeof(PROCESSOR_POWER_INFORMATION)*nLP );

	ULONG CurMhz = PPInfo[0].CurrentMhz;
	ULONG MaxMhz = PPInfo[0].MaxMhz;

	delete [] PPInfo;

	// Defensive clamp: garbage from API (or 0/NaN when API fails) must not reach display.
	if(_finite((double)CurMhz) == 0) CurMhz = 0;
	if(CurMhz > 100000) CurMhz = 100000;
	if(_finite((double)MaxMhz) == 0) MaxMhz = 0;
	if(MaxMhz > 100000) MaxMhz = 100000;

	// Preferred: the WMI-based current-clock-speed monitor. On Windows 10 +
	// modern CPUs (AMD Zen CPPC, Intel Speed Shift) this reflects the actual
	// current P-state, including turbo / boost - the same source Win10's
	// native task manager uses. Polled every 2 s on a background thread.
	PerfWmiCpu_Start();
	if(PerfWmiCpu_GetReady() != 0)
	{
		LONG wmi = PerfWmiCpu_GetMhz();
		if(wmi > 50 && wmi < 100000)
		{
			CurrentSpeed = (double)wmi / 1000.0;
			return CurrentSpeed;
		}
	}

	// Fallback: the OS-reported CurrentMhz when it is *less* than MaxMhz
	// (i.e. the OS actually has live P-state info). Works on older CPUs and
	// on Win7 + Intel with proper ACPI.
	if(CurMhz > 0 && MaxMhz > 0 && CurMhz < MaxMhz)
	{
		CurrentSpeed = (double)CurMhz / 1000.0;
		return CurrentSpeed;
	}

	// Fallback: live RDTSC measurement. Works on Intel CPUs where TSC ticks
	// at the core clock, NOT on AMD Zen (TSC is rated-frequency there).
	PerfCpuSpeed_Start();
	if(PerfCpuSpeed_GetReady() != 0)
	{
		LONG measured = PerfCpuSpeed_GetMhz();
		if(measured > 0 && measured < 100000)
		{
			CurrentSpeed = (double)measured / 1000.0;
			return CurrentSpeed;
		}
	}

	// Last resort: report MaxMhz / 1000 so the field is at least populated.
	if(MaxMhz > 0)
	{
		CurrentSpeed = (double)MaxMhz / 1000.0;
	}
	else if(CurMhz > 0)
	{
		CurrentSpeed = (double)CurMhz / 1000.0;
	}

	return CurrentSpeed;
}
