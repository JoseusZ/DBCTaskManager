// DBCTaskman.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "DBCTaskman.h"
#include "DBCTaskmanDlg.h"
#include <WinBase.h>


#include <Aclapi.h>
 
 

 



#define SE_MIN_WELL_KNOWN_PRIVILEGE     2;
#define SE_CREATE_TOKEN_PRIVILEGE     2;
#define SE_ASSIGNPRIMARYTOKEN_PRIVILEGE    3;
#define SE_LOCK_MEMORY_PRIVILEGE       4;
#define SE_INCREASE_QUOTA_PRIVILEGE                     5;
#define SE_UNSOLICITED_INPUT_PRIVILEGE                     6;
#define SE_MACHINE_ACCOUNT_PRIVILEGE                     6;
#define SE_TCB_PRIVILEGE                     7;
#define SE_SECURITY_PRIVILEGE                     8;
#define SE_TAKE_OWNERSHIP_PRIVILEGE                     9;
#define SE_LOAD_DRIVER_PRIVILEGE                     10;
#define SE_SYSTEM_PROFILE_PRIVILEGE                     11;
#define SE_SYSTEMTIME_PRIVILEGE                     12;
#define SE_PROF_SINGLE_PROCESS_PRIVILEGE                     13;
#define SE_INC_BASE_PRIORITY_PRIVILEGE                     14;
#define SE_CREATE_PAGEFILE_PRIVILEGE                     15;
#define SE_CREATE_PERMANENT_PRIVILEGE                     16;
#define SE_BACKUP_PRIVILEGE                     17;
#define SE_RESTORE_PRIVILEGE                     18;
#define SE_SHUTDOWN_PRIVILEGE                     19;
#define SE_DEBUG_PRIVILEGE                     20;
#define SE_AUDIT_PRIVILEGE                     21;
#define SE_SYSTEM_ENVIRONMENT_PRIVILEGE                     22;
#define SE_CHANGE_NOTIFY_PRIVILEGE                     23;
#define SE_REMOTE_SHUTDOWN_PRIVILEGE                     24;
#define SE_UNDOCK_PRIVILEGE                     25;
#define SE_SYNC_AGENT_PRIVILEGE                     26;
#define SE_ENABLE_DELEGATION_PRIVILEGE                     27;
#define SE_MANAGE_VOLUME_PRIVILEGE                     28;
#define SE_IMPERSONATE_PRIVILEGE                     29;
#define SE_CREATE_GLOBAL_PRIVILEGE                     30;
#define SE_INC_WORKING_SET_PRIVILEGE        (33L)








#ifdef _DEBUG
#define new DEBUG_NEW
#endif

 
static BOOL  EnableSpecificPrivilege(LPCTSTR lpPrivilegeName)
{

	HANDLE hToken = NULL;
	TOKEN_PRIVILEGES Token_Privilege;
	BOOL bRet = TRUE;

	do 
	{
		if (0 == OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		{
			MessageBox(NULL,L"OpenProcessToken Error",NULL,MB_OK|MB_ICONSTOP);
			bRet = FALSE;
			break;
		}

		if (0 == LookupPrivilegeValue(NULL, lpPrivilegeName, &Token_Privilege.Privileges[0].Luid))
		{
			MessageBox(NULL,L"LookupPrivilegeValue Error",NULL,MB_OK|MB_ICONSTOP); 
			bRet = FALSE;
			break;
		}

		Token_Privilege.PrivilegeCount = 1;
		Token_Privilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		//Token_Privilege.Privileges[0].Luid.LowPart=17;//SE_BACKUP_PRIVILEGE
		//Token_Privilege.Privileges[0].Luid.HighPart=0;


		if (0 == AdjustTokenPrivileges(hToken, FALSE, &Token_Privilege, sizeof(Token_Privilege), NULL,NULL))
		{
			
			MessageBox(NULL,L"AdjustTokenPrivileges Error",NULL,MB_OK|MB_ICONSTOP);

			bRet = FALSE;
			break;
		}

	} while (false);

	if (NULL != hToken)
	{
		CloseHandle(hToken);
	}

	return bRet;

}




//--------------------


static LONG CrashTip(EXCEPTION_POINTERS *pException)   
{      
    // ���������Ӵ��������������Ĵ���     //   
  
    // �����Ե���һ���Ի���Ϊ����   
    //  
	CString  StrError =L"whoops!";

	//StrError.Format(L"Error!   %d",::GetLastError());

	MessageBox(NULL, StrError, _T("DBC Task Manager"), MB_OK);     
    
	 
    return EXCEPTION_EXECUTE_HANDLER;   
}   










// CDBCTaskmanApp

BEGIN_MESSAGE_MAP(CDBCTaskmanApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CDBCTaskmanApp construction

CDBCTaskmanApp::CDBCTaskmanApp()
: CurrentPID(0)
, nAppWnd(0)
, nBkgWnd(0)
, UpTimeSec(0)
, UpTimeMin(0)
, UpTimeHour(0)
, UpTimeDay(0)

, StartPerformancePageTimer(FALSE)
, pButtonFOM(NULL)
, FlagWindowStatus(0)
, pMainTab(NULL)
, pSelPage(NULL)
, MenuAndTabHeight(0)
, StrCfgFileName(_T(""))

, FlagThemeActive(TRUE)
, FlagIsX64(FALSE)
, FlagIsAdminNow(FALSE)
, FlagSummaryView(FALSE)
, StrAppFullPath(_T(""))
, StrCfgToolsPath(_T(""))
, FlagSysIs32Bit(FALSE)
, IsChineseEdition(FALSE)
, mSvchostIconIndex(-1)
, mGenericAppIconIndex(0)
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CDBCTaskmanApp object

CDBCTaskmanApp theApp;

map<DWORD,PVOID> Map_PidToData;
map<CWnd*,int> HungWndMap;
map<int, int> NetAdapterList;

// FIX T2: Critical section global que protege Map_PidToData.
// Inicializada estaticamente (los CRITICAL_SECTION de Win32 no requieren
// InitializeCriticalSection explicito cuando son globales? en realidad SI
// lo requieren, por eso el codigo defensivo en InitCriticalSectionsData).
// Ver OnLockInit() / LockInit() mas adelante.
CRITICAL_SECTION g_MapDataLock;


// CDBCTaskmanApp initialization

BOOL CDBCTaskmanApp::InitInstance()
{

	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.




	


		

	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();





	AfxEnableControlContainer();




	HRESULT hres = 0;  
  
 //  hres = CoInitializeEx(0,  COINIT_APARTMENTTHREADED    );   //COINIT_MULTITHREADED  

	CoInitializeEx(0,  COINIT_APARTMENTTHREADED    );   //COINIT_MULTITHREADED
 //  hres = CoInitialize(0);   //
    if (hres!=S_OK)  
    {  
       AfxMessageBox(L"Failed to initialize COM library. "   );
       
    }  





	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	 SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	



 


	 //-------------------------------------------
	 InitAll();
	 //-----------------------------



//======================================================


//              ����ʵ��ֻ����һ��ʵ��
//              ��� ���Ի���  OnCreate  OnDestory 

	 if( AppSettings.OnlyOneInstance )
	 {
		 if( IsInstanceExist()) {return FALSE;}
	 }

	

//==========================================================================











//	 if( !_IsAdministratorNow()) 
	 {

		// MSB(0);
		//  ElevateCurrentProcess(L"Administrator"); //NT AUTHORITY\SYSTEM"
		  
		//  return 0;

	 }

   



 
	

	CDBCTaskmanDlg dlg;

	dlg.MoveWindow(theApp.AppSettings.rcWnd_Simple);

	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

int CDBCTaskmanApp::ExitInstance()
{
	// TODO: Add your specialized code here and/or call the base class


	SaveAppSettings();

	DeleteCriticalSection(&mIconCacheLock);

	CoUninitialize();
	GdiplusShutdown(m_gdiplusToken);
	return CWinApp::ExitInstance();
}


/*
WriteProfileBinary	Writes binary data to an entry in the application's .INI file.
WriteProfileInt    	Writes an integer to an entry in the application's .INI file.
WriteProfileString	Writes a string to an entry in the application's .INI file.

GetProfileBinary	Retrieves binary data from an entry in the application's .INI file.
GetProfileInt	Retrieves an integer from an entry in the application's .INI file.
GetProfileString

*/

void CDBCTaskmanApp::LoadAppSettings(BOOL ReLoad)
{
	 
  
	BOOL Ret;

	if(!ReLoad)
	{
		AppSettings.TimerStep = 1.0 ;
		AppSettings.ProcessorDisplayMode = 0;
		AppSettings.ShowKernelTime = FALSE;	 

		AppSettings.ActiveTab=0;
		AppSettings.TaskManMode=0; //Ĭ�ϼ��ģʽ

		AppSettings.PerformanceListShowGraph = 1;

		AppSettings.GroupByType = TRUE;
		//-----------------
		AppSettings.ProcList_MemPercents=0;
		AppSettings.ProcList_DiskPercents=0;
		AppSettings.ProcList_NetPercents=0;

		AppSettings.UserList_MemPercents=0;
		AppSettings.UserList_DiskPercents=0;
		AppSettings.UserList_NetPercents=0;
	}

	//-------------------------

	CFile CfgFile;
	Ret = CfgFile.Open(StrCfgFileName,CFile::modeRead);

	//-------------------------



	
	int i;
	 
	if(Ret)
	{

			WCHAR Mark[] =L"DBCSTUDIOTASKMAN";
			WCHAR Mark2[] =L"DDDDDDDDDDDDDDDD";
			DWORD  DataVer;

			
			


			CfgFile.Read(&Mark2,sizeof(Mark) 	);	
			if(ReLoad)
			{
				APPSETTINGS SettingsReload;
				CfgFile.Read(&SettingsReload,sizeof(APPSETTINGS));
				if(SettingsReload.Ver!= CFG_DATA_VER || (lstrcmp(Mark2,Mark)!=0) ) //��ֹ���ݴ��ң�����
				{
					return;
				}
				memcpy(&AppSettings.CpuColor,&SettingsReload.CpuColor,sizeof(GRAPHCOLOR));
				memcpy(&AppSettings.MemoryColor,&SettingsReload.MemoryColor,sizeof(GRAPHCOLOR));
				memcpy(&AppSettings.DiskColor,&SettingsReload.DiskColor,sizeof(GRAPHCOLOR));			
				memcpy(&AppSettings.NetworkColor,&SettingsReload.NetworkColor,sizeof(GRAPHCOLOR));
				CfgFile.Close();
				return;

			}
			else
			{
				CfgFile.Read(&AppSettings,sizeof(APPSETTINGS));
			}
			CfgFile.Close();

			DataVer = AppSettings.Ver;
			if(DataVer!= CFG_DATA_VER || (lstrcmp(Mark2,Mark)!=0) ) //��ֹ���ݴ��ң�����
			{
				
					goto LOADDEFAULT;
				
			}
		
		 

	}
	else //����ʧ����ΪĬ��ֵ
	{
LOADDEFAULT:
		AppSettings.rcWnd.SetRect(0,0,690,590);
		AppSettings.rcWnd_Simple.SetRect(0,0,490,515);
		AppSettings.TimerStep = 1.0 ; 


		//CPU
		AppSettings.CpuColor.BackgroundColor=RGB(255,255,255);
		AppSettings.CpuColor.BorderColor = RGB(17,125,187);
		AppSettings.CpuColor.GridColorRGB = RGB(17,125,187);
		AppSettings.CpuColor.GridColorAlpha = 30;
		AppSettings.CpuColor.LineAColor =  Gdiplus::Color(255,17,125,187);
		AppSettings.CpuColor.LineBColor =  Gdiplus::Color(255,17,125,187);
		AppSettings.CpuColor.FillAColor =  Gdiplus::Color(20,17,125,187);
		AppSettings.CpuColor.FillBColor =  Gdiplus::Color(16,17,125,187);

		//Mem
		AppSettings.MemoryColor.BackgroundColor=RGB(255,255,255);
		AppSettings.MemoryColor.BorderColor = RGB(139,18,174);
		AppSettings.MemoryColor.GridColorRGB = RGB(139,18,174);
		AppSettings.MemoryColor.GridColorAlpha = 30;
		AppSettings.MemoryColor.LineAColor =  Gdiplus::Color(255,139,18,174);
		AppSettings.MemoryColor.LineBColor =  Gdiplus::Color(255,139,18,174);
		AppSettings.MemoryColor.FillAColor =  Gdiplus::Color(20,139,18,174);
		AppSettings.MemoryColor.FillBColor =  Gdiplus::Color(16,139,18,174);

		//Disk

		AppSettings.DiskColor.BackgroundColor=RGB(255,255,255);
		AppSettings.DiskColor.BorderColor = RGB(77,166,12);
		AppSettings.DiskColor.GridColorRGB = RGB(77,166,12);
		AppSettings.DiskColor.GridColorAlpha = 30;
		AppSettings.DiskColor.LineAColor =  Gdiplus::Color(255,77,166,12);
		AppSettings.DiskColor.LineBColor =  Gdiplus::Color(255,77,166,12);
		AppSettings.DiskColor.FillAColor =  Gdiplus::Color(20,77,166,12);
		AppSettings.DiskColor.FillBColor =  Gdiplus::Color(16,77,166,12);

		//Net
		AppSettings.NetworkColor.BackgroundColor=RGB(255,255,255);
		AppSettings.NetworkColor.BorderColor = RGB(167,79,1);
		AppSettings.NetworkColor.GridColorRGB = RGB(167,79,1);
		AppSettings.NetworkColor.GridColorAlpha = 30;
		AppSettings.NetworkColor.LineAColor =  Gdiplus::Color(255,167,79,1);
		AppSettings.NetworkColor.LineBColor =  Gdiplus::Color(255,167,79,1);
		AppSettings.NetworkColor.FillAColor =  Gdiplus::Color(20,167,79,1);
		AppSettings.NetworkColor.FillBColor =  Gdiplus::Color(16,167,79,1);


	
		//----------------

		AppSettings.ProcessorDisplayMode = 0;	
		AppSettings.ShowKernelTime = FALSE;	 
		AppSettings.PerformanceListShowGraph = 1;

		AppSettings.TopMost = FALSE;
		AppSettings.MiniOnUse = FALSE;
		AppSettings.OnlyOneInstance = TRUE;


		for( i = 0;i< COL_COUNT_PROC;i++)
		{
			AppSettings.ColIDS_ProcList[i].ColWidth =0 ;
			AppSettings.ColIDS_ProcList[i].IsHiddenColumn =TRUE ;
			AppSettings.ColIDS_ProcList[i].Redraw = 0;
			AppSettings.ColIDS_ProcList[i].Align = (BYTE)DT_LEFT;
			if(i>=7)//��ɫ
			{
				AppSettings.ColIDS_ProcList[i].Cool=TRUE;
				AppSettings.ColIDS_ProcList[i].Align = (BYTE)DT_RIGHT;
				
			}
			if(i==PROCLIST_PID )AppSettings.ColIDS_ProcList[i].Align = (BYTE)DT_RIGHT;

		}
		for( i = 0;i< COL_COUNT_USER ;i++)
		{
			AppSettings.ColIDS_UserList[i].ColWidth =0 ;
			AppSettings.ColIDS_UserList[i].Redraw = 0;
			AppSettings.ColIDS_UserList[i].IsHiddenColumn =TRUE ;
			AppSettings.ColIDS_UserList[i].Align = (BYTE)DT_LEFT;

			if(i>=5)//��ɫ
			{
				AppSettings.ColIDS_UserList[i].Cool=TRUE;
				AppSettings.ColIDS_UserList[i].Align = (BYTE)DT_RIGHT;
				
			}

		}
		for( i = 0;i< COL_COUNT_DETAIL;i++)
		{
			AppSettings.ColIDS_DetailList[i].ColWidth=0;
			AppSettings.ColIDS_DetailList[i].Redraw=0;
		 
				

			if(i>=DETAILLIST_CPU && i<=DETAILLIST_IO_OB)
			{
				AppSettings.ColIDS_DetailList[i].Align = (BYTE)DT_RIGHT;
			}
			else
			{
				AppSettings.ColIDS_DetailList[i].Align = (BYTE)DT_LEFT;
			}
			 
		}

		//Ĭ�ϵ���ʾ��

		// P1-process
		COL_SAT_PROC[PROCLIST_NAME].ColWidth= COL_SAT_PROC[PROCLIST_STATUS].ColWidth=COL_SAT_PROC[PROCLIST_CPU].ColWidth = COL_SAT_PROC[PROCLIST_MEMORY].ColWidth= COL_SAT_PROC[PROCLIST_DISK].ColWidth = COL_SAT_PROC[PROCLIST_NETWORK].ColWidth=80;
		COL_SAT_PROC[PROCLIST_NAME].ColWidth= 200;
		COL_SAT_PROC[PROCLIST_NAME].Redraw= COL_SAT_PROC[PROCLIST_STATUS].Redraw=COL_SAT_PROC[PROCLIST_CPU].Redraw = COL_SAT_PROC[PROCLIST_MEMORY].Redraw= COL_SAT_PROC[PROCLIST_DISK].Redraw = COL_SAT_PROC[PROCLIST_NETWORK].Redraw=1;
        COL_SAT_PROC[PROCLIST_NAME].IsHiddenColumn= COL_SAT_PROC[PROCLIST_STATUS].IsHiddenColumn=COL_SAT_PROC[PROCLIST_CPU].IsHiddenColumn = COL_SAT_PROC[PROCLIST_MEMORY].IsHiddenColumn= COL_SAT_PROC[PROCLIST_DISK].IsHiddenColumn = COL_SAT_PROC[PROCLIST_NETWORK].IsHiddenColumn= FALSE;
		

		// P3-User
		COL_SAT_USER[USERLIST_USER].ColWidth  = COL_SAT_USER[USERLIST_STATUS].ColWidth = COL_SAT_USER[USERLIST_CPU].ColWidth= COL_SAT_USER[USERLIST_MEMORY].ColWidth=COL_SAT_USER[USERLIST_DISK].ColWidth=COL_SAT_USER[USERLIST_NETWORK].ColWidth=80;
		COL_SAT_USER[USERLIST_USER].ColWidth = 200;
		COL_SAT_USER[USERLIST_USER].Redraw  = COL_SAT_USER[USERLIST_STATUS].Redraw = COL_SAT_USER[USERLIST_CPU].Redraw= COL_SAT_USER[USERLIST_MEMORY].Redraw=COL_SAT_USER[USERLIST_DISK].Redraw=COL_SAT_USER[USERLIST_NETWORK].Redraw=1;
		COL_SAT_USER[USERLIST_USER].IsHiddenColumn  = COL_SAT_USER[USERLIST_STATUS].IsHiddenColumn = COL_SAT_USER[USERLIST_CPU].IsHiddenColumn= COL_SAT_USER[USERLIST_MEMORY].IsHiddenColumn=COL_SAT_USER[USERLIST_DISK].IsHiddenColumn=COL_SAT_USER[USERLIST_NETWORK].IsHiddenColumn=FALSE;


		// P4-DETAILS
		COL_SAT_DETAIL[DETAILLIST_NAME].ColWidth  = COL_SAT_DETAIL[DETAILLIST_TYPE].ColWidth = COL_SAT_DETAIL[DETAILLIST_PID].ColWidth= COL_SAT_DETAIL[DETAILLIST_STATUS].ColWidth=COL_SAT_DETAIL[DETAILLIST_USER].ColWidth=COL_SAT_DETAIL[DETAILLIST_CPU].ColWidth=COL_SAT_DETAIL[DETAILLIST_MEM_PWS].ColWidth=COL_SAT_DETAIL[DETAILLIST_DESCRIPTION].ColWidth=123;
		COL_SAT_DETAIL[DETAILLIST_NAME].Redraw  = COL_SAT_DETAIL[DETAILLIST_TYPE].Redraw = COL_SAT_DETAIL[DETAILLIST_PID].Redraw= COL_SAT_DETAIL[DETAILLIST_STATUS].Redraw=COL_SAT_DETAIL[DETAILLIST_USER].Redraw=COL_SAT_DETAIL[DETAILLIST_CPU].Redraw=COL_SAT_DETAIL[DETAILLIST_MEM_PWS].Redraw=COL_SAT_DETAIL[DETAILLIST_DESCRIPTION].Redraw=1;
 

		
	}

	//------------------------


	//��Զ��ʾ����

	COL_SAT_PROC[PROCLIST_NAME].Redraw = COL_SAT_USER[USERLIST_USER].Redraw = COL_SAT_DETAIL[DETAILLIST_NAME].Redraw = 1;



	//��װ�������ڴ�����

	ULONGLONG  TotalMemoryInKilobytes;

	API_GETINSTALLMEM GetPhysicallyInstalledSystemMemory ;

	
	HINSTANCE hDll= ::LoadLibrary(L"Kernel32.dll");	

	if(hDll!=NULL)
	{
		GetPhysicallyInstalledSystemMemory = NULL; 
		GetPhysicallyInstalledSystemMemory=(API_GETINSTALLMEM)GetProcAddress(hDll, "GetPhysicallyInstalledSystemMemory");
		FreeLibrary(hDll); 
	}




    Ret = GetPhysicallyInstalledSystemMemory(&TotalMemoryInKilobytes);
 

	if(Ret)
	{
		PerformanceInfo.InstalledMemKB = TotalMemoryInKilobytes;
	}
	else
	{
		MEMORYSTATUSEX  MemStatus;
		MemStatus.dwLength = sizeof(MemStatus);
		BOOL Ret = GlobalMemoryStatusEx(&MemStatus);

		if(Ret)
		{
			theApp.PerformanceInfo.TotalPhysMem = MemStatus.ullTotalPhys;
		}
		PerformanceInfo.InstalledMemKB = PerformanceInfo.TotalPhysMem/1024;
	}



	theApp.PerformanceInfo.TotalNetUsage = 0;
	theApp.PerformanceInfo.TotalDiskUsage = 0;
	



	//-------------------



//	AppSettings.CpuColor.LineColor=GetPrivateProfileInt(L"CPU",L"Border",0x0,L"Color.ini");
 //
	 

	

	



}

void CDBCTaskmanApp::SaveAppSettings(void)
{
	

	CFile CfgFile;
	BOOL Ret = CfgFile.Open(StrCfgFileName,CFile::modeCreate |CFile::modeReadWrite );

	if(Ret)
	{
		WCHAR Mark[] =L"DBCSTUDIOTASKMAN";
		
		AppSettings.Ver = CFG_DATA_VER;

		CfgFile.Write(Mark,sizeof(Mark));	
		CfgFile.Write(&AppSettings,sizeof(APPSETTINGS));
		

		CfgFile.Close();



	}


	//WriteProfileBinary(L"",L"AppSettings",(LPBYTE)&AppSettings,sizeof(AppSettings));



}

void CDBCTaskmanApp::_UpgradeRights(void)
{


 
	MyAdjustPrivilege RtlAdjustPrivilege = (MyAdjustPrivilege)GetProcAddress(GetModuleHandle(L"ntdll.dll"),"RtlAdjustPrivilege");
  
 
	BOOLEAN s;
	 RtlAdjustPrivilege(0x14 ,1,0,&s);
 

	 FlagIsAdminNow = _IsAdministratorNow();


//------------







	/*



	 HANDLE tokenHandle;



    if (NT_SUCCESS(PhOpenProcessToken(&tokenHandle,TOKEN_ADJUST_PRIVILEGES,NtCurrentProcess())))
    {
        CHAR privilegesBuffer[FIELD_OFFSET(TOKEN_PRIVILEGES, Privileges) + sizeof(LUID_AND_ATTRIBUTES) * 8];
        PTOKEN_PRIVILEGES privileges;
        ULONG i;

        privileges = (PTOKEN_PRIVILEGES)privilegesBuffer;
        privileges->PrivilegeCount = 8;

        for (i = 0; i < privileges->PrivilegeCount; i++)
        {
            privileges->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;
            privileges->Privileges[i].Luid.HighPart = 0;
        }

        privileges->Privileges[0].Luid.LowPart = SE_DEBUG_PRIVILEGE;
        privileges->Privileges[1].Luid.LowPart = SE_INC_BASE_PRIORITY_PRIVILEGE;
        privileges->Privileges[2].Luid.LowPart = SE_INC_WORKING_SET_PRIVILEGE;
        privileges->Privileges[3].Luid.LowPart = SE_LOAD_DRIVER_PRIVILEGE;
        privileges->Privileges[4].Luid.LowPart = SE_PROF_SINGLE_PROCESS_PRIVILEGE;
        privileges->Privileges[5].Luid.LowPart = SE_RESTORE_PRIVILEGE;
        privileges->Privileges[6].Luid.LowPart = SE_SHUTDOWN_PRIVILEGE;
        privileges->Privileges[7].Luid.LowPart = SE_TAKE_OWNERSHIP_PRIVILEGE;


		MyNtAdjustPrivilegeToken   NtAdjustPrivilegesToken;
		NtAdjustPrivilegesToken = (MyNtAdjustPrivilegeToken)GetProcAddress(GetModuleHandle(L"ntdll"),"NtAdjustPrivilegesToken");
		MyNtClose NtClose = (MyNtClose)GetProcAddress(GetModuleHandle(L"ntdll"),"NtClose");
		 

        NtAdjustPrivilegesToken(
            tokenHandle,
            FALSE,
            privileges,
            0,
            NULL,
            NULL
            );

        NtClose(tokenHandle);
    }*/




	// DuplicateTokenEx


 



 
}

BOOL  CDBCTaskmanApp::ElevateCurrentProcess(CString  StrCmdLine)
{
	// IFileOperation ifile;


//return 0;

	 

//-------------------------------------

	TCHAR szPath[MAX_PATH] = {0};

	if (::GetModuleFileName(NULL, szPath, MAX_PATH))

	{

		// Launch itself as administrator.

		SHELLEXECUTEINFO sei = { sizeof(SHELLEXECUTEINFO) };

		sei.lpVerb = _T("runas");

		sei.lpFile = szPath;

		sei.lpParameters = (LPCTSTR)StrCmdLine;

		// sei.hwnd = hWnd;

		sei.nShow = SW_SHOWNORMAL;



		if (!ShellExecuteEx(&sei))

		{

			DWORD dwStatus = GetLastError();

			if (dwStatus == ERROR_CANCELLED)

			{

				// The user refused to allow privileges elevation.

				return FALSE;

			}

			else

				if (dwStatus == ERROR_FILE_NOT_FOUND)

				{

					// The file defined by lpFile was not found and

					// an error message popped up.

					return FALSE;

				}



				return FALSE;



}

return TRUE;

}

return FALSE;
}

BOOL CDBCTaskmanApp::_IsAdministratorNow(void)
{
 
	BOOL bIsElevated = FALSE;
	HANDLE hToken = NULL;
	UINT16 uWinVer = LOWORD(GetVersion());
	uWinVer = MAKEWORD(HIBYTE(uWinVer),LOBYTE(uWinVer));

	if (uWinVer < 0x0600) //����VISTA��Windows7
		return(FALSE);

	if (OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken))
	{

		struct {
			DWORD TokenIsElevated;
		} /*TOKEN_ELEVATION*/te;
		DWORD dwReturnLength = 0;

		if (GetTokenInformation(hToken,/*TokenElevation*/(_TOKEN_INFORMATION_CLASS)20,&te,sizeof(te),&dwReturnLength)) {
			if (dwReturnLength == sizeof(te))
				bIsElevated = te.TokenIsElevated;
		}
		CloseHandle( hToken );
	}
	return bIsElevated;
 
}

void CDBCTaskmanApp::InitAll(void)
{

	// FIX T2: inicializar el CRITICAL_SECTION global que protege
	// Map_PidToData. Se hace aqui porque InitInstance
	// todavia no ha construido ninguna ventana (es seguro inicializar
	// un objeto de sincronizacion antes de PumpMessage).
	InitializeCriticalSection(&g_MapDataLock);

	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)CrashTip);
	 	

	//----------------------�����Ƿ� Ϊx64��-------------------
	
	LPVOID   Test;
	int nTest= sizeof(Test);   //x64 ��nTest == 8 x86 ==4;
	if(nTest == 8)   
	{
		FlagIsX64 = TRUE;		
	}
	else
	{
		FlagIsX64=FALSE;
	}
	 

	// ----------- �����Ƿ�Ϊ���İ� ���� html �� �ı��ļ� ��һЩ��ʾ��Ҫ�������԰汾��ͬ���� ----------- 
	CString StrCharCode;
	StrCharCode.LoadString(STR_CHARCODE);
	IsChineseEdition =(StrCharCode.CompareNoCase(L"gb18030")==0);







	 //--------------  Set Full Path name for Config file  ------------------
	 DWORD BuffSize=MAX_PATH;

	 GetModuleFileName(NULL,StrAppFullPath.GetBuffer(BuffSize),BuffSize);  
	 StrAppFullPath.ReleaseBuffer();

	 CString StrTemp;
	 StrCfgFileName = StrAppFullPath;
	 StrTemp = StrAppFullPath;

	 ::ExtractIconEx(StrAppFullPath,0,&hBigIcon,NULL,1);
	 StrTemp = StrCfgFileName.Left(StrCfgFileName.ReverseFind(L'\\'));
	 StrCfgFileName = StrTemp + L"\\DBCTaskman.CFG";
	 StrCfgToolsPath = StrTemp+L"\\DTMCFG.exe";

	 WCHAR WcWow64Path[MAX_PATH];
	 FlagSysIs32Bit = FALSE;
	 if ( GetSystemWow64Directory(WcWow64Path,MAX_PATH) == 0)
	 {
		 if(GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) FlagSysIs32Bit = TRUE;
	 }



	 


	  //---------------��ȡ  �߼����������� ---------------
		
	 SYSTEM_INFO sysInfo;
	 GetSystemInfo(&sysInfo);
	 LogicalProcessorsCount = sysInfo.dwNumberOfProcessors;

 



	 //   MSB_S(StrCfgFileName)


	 LoadAllString();


	 _UpgradeRights();


	 LoadAppSettings();


	//---------------------- ���б����� ------------------------
	//---------------���������Ҫ�õ���� �����ڴ�һ���Դ�-----------------
	FlagThemeActive  = IsThemeActive();
	hTheme=OpenThemeData(NULL, L"Explorer::ListView"); // �� ������Ի����Ч�� �Ͳ���������SetWindowTheme(mProcessList.GetSafeHwnd(),L"explorer", NULL);�� ������


	//------------------------

	SHFILEINFO sfi;
	HIMAGELIST himlSmall;

	CString  StrSysDir;
	GetWindowsDirectory(StrSysDir.GetBuffer(MAX_PATH), MAX_PATH);
	StrSysDir.ReleaseBuffer();	
	himlSmall = (HIMAGELIST)SHGetFileInfo(StrSysDir, 0, &sfi, sizeof(SHFILEINFO),SHGFI_SYSICONINDEX | SHGFI_SMALLICON |SHGFI_ICON   );

		
	if(himlSmall != NULL)
	{	
		mImagelist.Attach(himlSmall);
	}

	//-----------����ͼ������----------------------
	HICON hIcon = theApp.LoadIcon( MAKEINTRESOURCE(IDI_SVCHOST) );
	if(hIcon != NULL)
	{
		// FIX: capturar el indice EXACTO devuelto por Add, no GetImageCount()-1.
		// En Win7, el shell catalog attached puede tener tamanos variables
		// y depender de GetImageCount()-1 hacia que SvchostIconIndex apuntase
		// al icono por defecto del shell (que en algunos sistemas resuelve
		// al icono del taskmgr.exe -> el bug que veias en Win7).
		int nIdx = mImagelist.Add(hIcon);
		if(nIdx >= 0)
		{
			mSvchostIconIndex = nIdx;
		}
		DestroyIcon(hIcon);
	}

	//-----------------------------------------

	// Cache de iconos: lock. NO tocamos mImagelist ni el Attach.
	// svchost esta en el slot GetImageCount()-1 (lo acabamos de Add).
	// Pre-cacheamos su ruta comun "svchost.exe" para hit directo.
	InitializeCriticalSection(&mIconCacheLock);
	{
		int svchostSlot = mImagelist.GetImageCount() - 1;
		if (svchostSlot > 0) {
			EnterCriticalSection(&mIconCacheLock);
			mIconCache[CString(L"svchost.exe")] = (void*)(INT_PTR)svchostSlot;
			LeaveCriticalSection(&mIconCacheLock);
		}
	}


	HINSTANCE hDll= ::LoadLibrary(L"Kernel32.dll");
	GetProcessDEPPolicy = NULL; 
	GetProcessDEPPolicy=(MYAPIGETPROCDEP)GetProcAddress(hDll, "GetProcessDEPPolicy");
	
	FreeLibrary(hDll); 

	 


//-------------------------------

	
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

//-----------get system colors ---------------------

	SetUIColor();
//-----------------------------------------------------

	PerformanceInfo.ProcessCount =0;



	CurrentPID = GetCurrentProcessId();
	HANDLE hProcess = OpenProcess( PROCESS_SET_INFORMATION     ,FALSE, CurrentPID);  //ע�� PPROCESS_SET_INFORMATION Ҫ�������ȼ� Ҫ���Ȩ��
	BOOL Ret=SetPriorityClass( hProcess, HIGH_PRIORITY_CLASS );
	CloseHandle(hProcess);	


	UpTimeSec= 0;
    UpTimeMin = UpTimeHour = UpTimeDay=0;


//-------------------------

	
	 CString StrFontName;

	 StrFontName.LoadStringW(IDS_FONTNAME);
	 mTitleFont.CreateFont(17,   
						 0,                         // nWidth
						 0,                         // nEscapement
						 0,                         // nOrientation
						 FW_NORMAL,                 // nWeight     FW_NORMAL,     FW_BOLD
						 FALSE,                     // bItalic
						 FALSE,                     // bUnderline�»��߱�ǣ���Ҫ�»��߰��������ó�TRUE
						 0,                         // cStrikeOut
						 DEFAULT_CHARSET,              // nCharSet
						 OUT_DEFAULT_PRECIS,        // nOutPrecision
						 CLIP_DEFAULT_PRECIS,       // nClipPrecision
						 DEFAULT_QUALITY,           // nQuality
						 DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
						 StrFontName);                 // lpszFacename





}

void CDBCTaskmanApp::SetUIColor(void)
{
	WndBkgColor = ::GetSysColor(COLOR_WINDOW );
	WndTextColor = ::GetSysColor(COLOR_WINDOWTEXT );
	WndFrameColor = ::GetSysColor(COLOR_WINDOWFRAME );
	DisableTextColor=::GetSysColor(COLOR_GRAYTEXT  );

	if(FlagThemeActive)
	{
		GetThemeColor(theApp.hTheme,LVP_LISTITEM,LISS_HOTSELECTED,TMT_TEXTCOLOR ,&List_NormalTextColor);
		GetThemeColor(theApp.hTheme,LVP_LISTITEM,LISS_NORMAL,TMT_TEXTCOLOR ,&List_HotTextColor);

		//header
		HTHEME hTheme = OpenThemeData(NULL, L"TEXTSTYLE"); 	
		GetThemeColor(hTheme,TEXT_HYPERLINKTEXT,TS_HYPERLINK_NORMAL,TMT_TEXTCOLOR ,&CoolHdrColor);		
		::CloseThemeData(hTheme);

		hTheme = OpenThemeData(NULL, L"EXPLORERBAR"); 
		GetThemeColor(hTheme,EBP_NORMALGROUPBACKGROUND,0,TMT_TEXTCOLOR ,&GroupTitleTextColor);
		::CloseThemeData(hTheme);





	}
	else
	{
		List_NormalTextColor = ::GetSysColor(COLOR_WINDOWTEXT );
		List_HotTextColor = ::GetSysColor(COLOR_HIGHLIGHTTEXT );

		CoolHdrColor = WndBkgColor;
	}

	

	if(BkgBrush.m_hObject!=NULL)
	{
		BkgBrush.DeleteObject();
		BkgBrush.CreateSysColorBrush(COLOR_WINDOW);
			 
	}
	else
	{
		BkgBrush.CreateSolidBrush(WndBkgColor);
	}
 

}


void CDBCTaskmanApp::LoadAllString(void)
{
	STR_FONTNAME.LoadString(IDS_STR_FONTNAME);

	MAIN_CAPTION.LoadString(IDS_MAIN_CAPTION);
	STR_GROUP_APP.LoadString(IDS_STR_GROUP_APP);
	STR_GROUP_BKG.LoadString(IDS_STR_GROUP_BKG);
	STR_GROUP_WIN.LoadString(IDS_STR_GROUP_WIN);
	//-----------Cpu
	STR_CPUINFO_0 .LoadString(IDS_STR_CPUINFO_0);
	STR_CPUINFO_1.LoadString(IDS_STR_CPUINFO_1);
	STR_CPUINFO_2.LoadString(IDS_STR_CPUINFO_2);
	STR_CPUINFO_3.LoadString(IDS_STR_CPUINFO_3);
	STR_CPUINFO_4 .LoadString(IDS_STR_CPUINFO_4);
	STR_CPUINFO_5.LoadString(IDS_STR_CPUINFO_5);
	STR_CPUINFO_6 .LoadString(IDS_STR_CPUINFO_6);
	STR_TIP1_CPU_TOTAL.LoadString(IDS_STR_TIP1_CPU_TOTAL);
	STR_TIP1_CPU_LOGICAL.LoadString(IDS_STR_TIP1_CPU_LOGICAL);
	STR_TIP1_CPU_NUMA.LoadString(IDS_STR_TIP1_CPU_NUMA);

	//------------Memory
	STR_MEMINFO_0.LoadString(IDS_STR_MEMINFO_0);
	STR_MEMINFO_1.LoadString(IDS_STR_MEMINFO_1);
	STR_MEMINFO_2.LoadString(IDS_STR_MEMINFO_2);
	STR_MEMINFO_3.LoadString(IDS_STR_MEMINFO_3);
	STR_MEMINFO_4.LoadString(IDS_STR_MEMINFO_4);
	STR_MEMINFO_5.LoadString(IDS_STR_MEMINFO_5);
	STR_MEMINFO_6.LoadString(IDS_STR_MEMINFO_6);
	STR_TIP3_MEMORY.LoadString(IDS_STR_TIP3_MEMORY);
	STR_TIP5_MEMORY.LoadString(IDS_STR_TIP5_MEMORY);
	//----------Disk
	STR_DISKINFO_0.LoadString(IDS_STR_DISKINFO_0);
	STR_DISKINFO_1.LoadString(IDS_STR_DISKINFO_1);
	STR_DISKINFO_2.LoadString(IDS_STR_DISKINFO_2);
	STR_DISKINFO_3.LoadString(IDS_STR_DISKINFO_3);
	STR_DISKINFO_6.LoadString(IDS_STR_DISKINFO_6);

	STR_TIP5_DISK.LoadString(IDS_STR_TIP5_DISK);
	//-------------Network
	STR_NETWORKINFO_0.LoadString(IDS_STR_NETWORKINFO_0);
	STR_NETWORKINFO_2.LoadString(IDS_STR_NETWORKINFO_2);
	STR_NETWORKINFO_6.LoadString(IDS_STR_NETWORKINFO_6);
	STR_TIP3_NETWORK.LoadString(IDS_STR_TIP3_NETWORK);

	//---------  properties dialog  of Process -----------

	STR_PROPERTIESDLG_CAPTION.LoadString(IDS_STR_PROPERTIESDLG_CAPTION);
}

BOOL CDBCTaskmanApp::IsInstanceExist(void)
{
	 


	// HANDLE m_hMutex = CreateMutex(NULL, FALSE, L"DBC_TASKMAN_MUTE"); 

	 HANDLE hSem=CreateSemaphore(NULL,1,1,L"DBC_TASKMAN_001"); 

	 if(hSem) //�ű���󴴽��ɹ��� 
	 { 		  
		 if(ERROR_ALREADY_EXISTS==GetLastError()) //�ű�����Ѿ����ڣ����������һ��ʵ�������С�
		 { 
			 CloseHandle(hSem); //�ر��ź�������� 

			 //��ȡ���洰�ڵ�һ���Ӵ��ڡ� 
			 HWND hWndPrev=::GetWindow(::GetDesktopWindow(),GW_CHILD); 
			 while(::IsWindow(hWndPrev)) 
			 { 				 
				 if(::GetProp(hWndPrev,L"DBC_TASKMAN_001")) //�жϴ����Ƿ�������Ԥ�����õı�ǣ����У���������Ѱ�ҵĴ��ڣ���������� 
				 {					
					 if (::IsIconic(hWndPrev)) {::ShowWindow(hWndPrev,SW_RESTORE);} 	 //�������������С������ָ����С�� 				 
					 ::SetForegroundWindow(hWndPrev); //��Ӧ�ó���������ڼ�� 
					 return TRUE; //����TRUE���� ʵ���Ѿ�����   �˳�ʵ���� 
				 } 			
				 hWndPrev = ::GetWindow(hWndPrev,GW_HWNDNEXT); 	 //����Ѱ����һ�����ڡ� 
			 } 
			// AfxMessageBox("����һ��ʵ�������У����Ҳ������������ڣ�"); 
		 } 
	 } 
	 else 
	 { 
		 AfxMessageBox(L"Error: CreateSemaphore()"); 
		 return TRUE; 
	 } 

	return FALSE;
}

int CDBCTaskmanApp::Global_ShowOperateTip(CString StrProcessName, UINT MainTipStrID,  UINT ContentStrID,  UINT ButtonStrID, UINT GrayBtnID, CString StrCheckBox)
{
	CString StrCaption,StrMainTip ,StrContent,StrBtnOK,StrBtnCancel,StrTemp,StrBtnAll;	


	StrBtnAll.LoadStringW(ButtonStrID);
	AfxExtractSubString(StrBtnOK,StrBtnAll,0,L';');
	AfxExtractSubString(StrBtnCancel,StrBtnAll,1,L';');	
	StrTemp.LoadStringW(MainTipStrID);
	StrContent.LoadStringW(ContentStrID);
	
	//ע�� StrTipTitle ����%s��ʽ ��������ֱ����һ���������ܻ���� �ڴ��ͻ���±������� _debugger_hook_dummy = 0;���Բ�Ҫֱ����һ������ ���� �����Ǹ� ����
	StrMainTip.Format(StrTemp,StrProcessName);


	theApp.m_pMainWnd->GetWindowText(StrCaption);	
	TASKDIALOG_BUTTON Buttons[] ={ { IDOK, StrBtnOK },   { IDCANCEL, StrBtnCancel }};	

	

	return NewStyleMessageBox(theApp.m_pMainWnd->GetSafeHwnd(),StrCaption,StrMainTip,StrContent,2,Buttons,GrayBtnID,StrCheckBox);
}

void CDBCTaskmanApp::Global_EndProcessesInList(CListCtrl* pList,BOOL ShowTipWhenOnlyOne)
{
	if(pList==NULL) return;

	BOOL EnableTerminate = FALSE;
	int Selectedcount = pList->GetSelectedCount();
	
	
	PPROCLISTDATA * pDataArray = new PPROCLISTDATA[Selectedcount];		

	POSITION SelPos = pList->GetFirstSelectedItemPosition();		
	CString StrProcessNames= L"";
	int i=0;
	while (SelPos)
	{
		int iItem =  pList->GetNextSelectedItem(SelPos);
		pDataArray[i] = NULL;
		pDataArray[i] = (PROCLISTDATA *) pList->GetItemData(iItem);
		StrProcessNames = StrProcessNames+L"\n"+pDataArray[i]->Name;
		i++;			 
	}

	if(Selectedcount>1)
	{
		EnableTerminate = (theApp.Global_ShowOperateTip(StrProcessNames,STR_ENDMULTPROC_MAINTIP,STR_ENDMULTPROC_CONTENT,STR_ENDMULTPROC_BTN) ==IDOK);


	}
	else //����
	{
		if(ShowTipWhenOnlyOne)
		{
			 

			StrProcessNames.Remove(L'\n');
			if(( (pDataArray[0]->Name.CompareNoCase(L"csrss.exe")==0 )||(pDataArray[0]->Name.CompareNoCase(L"smss.exe")==0 )||(pDataArray[0]->Name.CompareNoCase(L"wininit.exe")==0 )) && (pDataArray[0]->User.Compare(L"SYSTEM")==0) )
			{				
				CString StrCheckBtn;
				StrCheckBtn.LoadString(STR_ENDSYSPROC_OTHER);
				EnableTerminate = (theApp.Global_ShowOperateTip(StrProcessNames,STR_ENDSYSPROC_MAINTIP,STR_ENDSYSPROC_CONTENT,STR_ENDSYSPROC_BTN,0,StrCheckBtn)==IDOK);	
			}
			else
			{
				EnableTerminate = (theApp.Global_ShowOperateTip(StrProcessNames,STR_ENDPROC_MAINTIP,STR_ENDPROC_CONTENT,STR_ENDPROC_BTN)==IDOK);	
			}
					
			
		}
		else
		{
			EnableTerminate = TRUE;
		}
	}
	
		
	if(EnableTerminate)
	{
		for( i=0;i<Selectedcount;i++)
		{
			if(( (pDataArray[i]->Name.CompareNoCase(L"csrss.exe")==0 )||(pDataArray[i]->Name.CompareNoCase(L"smss.exe")==0 )||(pDataArray[i]->Name.CompareNoCase(L"wininit.exe")==0 )) && (pDataArray[i]->User.Compare(L"SYSTEM")==0) )
			{				
				CString StrCheckBtn;
				StrCheckBtn.LoadString(STR_ENDSYSPROC_OTHER);
				if(theApp.Global_ShowOperateTip(pDataArray[i]->Name,STR_ENDSYSPROC_MAINTIP,STR_ENDSYSPROC_CONTENT,STR_ENDSYSPROC_BTN,0,StrCheckBtn)!=IDOK)continue;	
			}
			
			if(pDataArray[i] != NULL)TerminateProcess(pDataArray[i]->hProcess, 4);
		}
	}

	delete [] pDataArray;
	pDataArray = NULL;

}

// ============================================================================
// DIAG: logging defensivo opt-in para la lista de procesos (movido desde
// Global.cpp, que NO esta enlazado al vcxproj).
// ============================================================================
// Se activa creando un archivo VACIO llamado `dbc_listdiag.log` junto al .exe.
// Salida en `dbc_listdiag.out` (append, throttled a 4Hz desde el callback).
// ============================================================================
#include <stdio.h>
#include <stdarg.h>

// Cache de iconos: lookup O(1) por path de ejecutable. Miss = mismo
// SHGetFileInfo que el codigo original de _GetIconIndex (mismos flags,
// mismo iIcon -> valido en mImagelist shell catalog attached).
// Thread-safe via mIconCacheLock.
//
// Por que arregla la lentitud de carga:
//   ANTES: cada Refresh() llamaba SHGetFileInfo por cada proceso,
//   sincronamente, aunque ya se hubiera visto ese path. Resultado:
//   145 SHGetFileInfo * N refrescos/seg = UI jank + iconos que tardan.
//   AHORA: la primera vez tarda (es lo que es, sync). Las siguientes es
//   O(1) -> cero re-pago del shell catalog en updates.
int CDBCTaskmanApp::GetIconIndexCached(LPCTSTR lpszPath)
{
	if (lpszPath == NULL || lpszPath[0] == _T('\0')) {
		return 0;
	}

	CString key(lpszPath);
	EnterCriticalSection(&mIconCacheLock);
	void* pVal = NULL;
	if (mIconCache.Lookup(key, pVal)) {
		LeaveCriticalSection(&mIconCacheLock);
		return (int)(INT_PTR)pVal;
	}
	LeaveCriticalSection(&mIconCacheLock);

	// Miss: extraer con la MISMA logica que el _GetIconIndex original.
	// Mismos flags -> mismo iIcon -> valido en mImagelist.
	SHFILEINFO sfi;
	memset(&sfi, 0, sizeof(sfi));
	SHGetFileInfo(lpszPath, FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
		SHGFI_SMALLICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_ICON);

	int nIndex = sfi.iIcon;

	EnterCriticalSection(&mIconCacheLock);
	mIconCache[key] = (void*)(INT_PTR)nIndex;
	LeaveCriticalSection(&mIconCacheLock);
	return nIndex;
}

BOOL _ListDiagEnabled(void)
{
   // Re-chequea en cada llamada si el archivo toggle existe.
   // Es case-INsensitive en Windows pero usamos la ruta exacta para que
   // un archivo llamado .OUT preexistente (de runs anteriores con el toggle
   // activo) no confunda al operador. La latencia de GetFileAttributesA
   // es del orden de microsegundos para un FS local, despreciable frente
   // al throttling de 250ms en el callback.
   return (GetFileAttributesA("dbc_listdiag.log") != INVALID_FILE_ATTRIBUTES);
}

void _ListDiagLog(const char* fmt, ...)
{
   if (!_ListDiagEnabled()) return;
   FILE* f = NULL;
   if (fopen_s(&f, "dbc_listdiag.out", "a") != 0 || !f) return;
   SYSTEMTIME st; GetLocalTime(&st);
   fprintf(f, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
   va_list ap; va_start(ap, fmt);
   vfprintf(f, fmt, ap);
   va_end(ap);
   fputc('\n', f);
   fclose(f);
}

// Dump resumido del estado del list control: cuenta items, separadores,
// items con SubType=SUB_ITEM, items con pPData NULL, sub-items huerfanos.
// Llamar desde Sort_Processes, _OpenSubList, FillAllItemData y OnUMTimer.
//
// FIX A: la deteccion de huerfanos ya no se limita a "la fila anterior
// tiene el mismo PID". Ahora usa pParent (introducido por FIX T5) para
// localizar al padre REAL del sub-item, y se considera huerfano solo si
// el padre no esta donde deberia estar o si el pPData del padre ya no
// es el que el sub-item guarda como padre. Esto elimina falsos
// positivos cuando hay varios POpen y sub-items intercalados.
//
// Para sub-items sin pParent (legacy, binarios anteriores al fix T5),
// se hace una busqueda hacia atras limitada a 8 filas buscando mismo
// PID en un POpen. Si no se encuentra, se considera huerfano.
void _ListDiagDump(const char* szTag, int nCount, void* pListCtrl)
{
   if (!_ListDiagEnabled()) return;
   if (pListCtrl == NULL) {
       _ListDiagLog("[%s] count=%d pListCtrl=NULL", szTag, nCount);
       return;
   }
   CListCtrl* pL = (CListCtrl*)pListCtrl;
   int real = pL->GetItemCount();
   int headers = 0, parentsClose = 0, parentsOpen = 0, parentsNoSub = 0;
   int subs = 0, nullData = 0, nullPData = 0, orphanSubs = 0;
   for (int i = 0; i < real; i++) {
       APPLISTDATA* pd = (APPLISTDATA*)pL->GetItemData(i);
       if (pd == NULL) {
          nullData++;
          // FIX T8 diag: localizar el item corrupto para entender
          // por qué tiene GetItemData==NULL a pesar de seguir en
          // la lista. Dump del texto de la fila 0 para contexto.
          {
             CString s0 = pL->GetItemText(i, 0);
             _ListDiagLog("  nullData at i=%d name=\"%s\" totalTextLen=%d",
                i, (LPCSTR)(CStringA)s0.Left(40), s0.GetLength());
          }
          continue;
       }
       if (pd->pPData == NULL) { nullPData++; headers++; continue; }
       if (pd->SubType == -1) headers++;
       else if (pd->SubType == PARENT_ITEM_CLOSE) parentsClose++;
       else if (pd->SubType == PARENT_ITEM_OPEN) parentsOpen++;
       else if (pd->SubType == PARENT_ITEM_NOSUB) parentsNoSub++;
       else if (pd->SubType == SUB_ITEM) {
           subs++;
           BOOL bOrphan = TRUE;

           // Camino primario: usar pParent (establecido por _OpenSubList).
           if (pd->pParent != NULL) {
               APPLISTDATA* pp = pd->pParent;
               // El padre es valido si:
               //  - pd->pPData == pp->pPData (mismo PROCLISTDATA; el padre
               //    del sub-item ES este APPLISTDATA).
               //  - pp->SubType != SUB_ITEM (un sub-item no puede ser padre).
               //  - pp->pPData no es NULL.
               if (pp->pPData != NULL && pp->SubType != SUB_ITEM
                   && pp->pPData == pd->pPData) {
                   bOrphan = FALSE;
               }
           } else {
               // Fallback para sub-items sin pParent (legacy). Buscamos
               // hacia atras un POpen/PClose con mismo PID, dentro de un
               // limite razonable (los sub-items siempre bajo a su padre).
               DWORD myPID = ((PROCLISTDATA*)pd->pPData)->PID;
               for (int j = i - 1; j >= 0 && j >= i - 8; j--) {
                   APPLISTDATA* pp = (APPLISTDATA*)pL->GetItemData(j);
                   if (pp == NULL || pp->pPData == NULL) continue;
                   if (pp->SubType != PARENT_ITEM_OPEN && pp->SubType != PARENT_ITEM_CLOSE)
                       continue;
                   if (((PROCLISTDATA*)pp->pPData)->PID == myPID) {
                       bOrphan = FALSE;
                       break;
                   }
               }
           }

           if (bOrphan) orphanSubs++;
       }
   }
   _ListDiagLog("[%s] count=%d real=%d hdr=%d PClose=%d POpen=%d PNoSub=%d Sub=%d nullData=%d nullPData=%d orphanSub=%d",
       szTag, nCount, real, headers, parentsClose, parentsOpen, parentsNoSub, subs, nullData, nullPData, orphanSubs);
}
