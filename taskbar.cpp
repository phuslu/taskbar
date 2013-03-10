#define _WIN32_IE 0x0500

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <process.h>
#include <psapi.h>
#include "resource.h"

#pragma comment (lib, "psapi.lib" )

extern "C" WINBASEAPI HWND WINAPI GetConsoleWindow();

#define MAX_LOADSTRING 1024
#define NID_UID 123
#define WM_TASKBARNOTIFY WM_USER+20
#define WM_TASKBARNOTIFY_MENUITEM_SHOW WM_USER + 21
#define WM_TASKBARNOTIFY_MENUITEM_HIDE WM_USER + 22
#define WM_TASKBARNOTIFY_MENUITEM_RELOAD WM_USER + 23
#define WM_TASKBARNOTIFY_MENUITEM_ABOUT WM_USER + 24
#define WM_TASKBARNOTIFY_MENUITEM_EXIT WM_USER + 25

HINSTANCE hInst;
HWND hWnd;
HWND hConsole;
TCHAR szTitle[MAX_LOADSTRING] = L"";
TCHAR szWindowClass[MAX_LOADSTRING] = L"taskbar";
TCHAR szCommandLine[MAX_LOADSTRING] = L"";
TCHAR szTooltip[MAX_LOADSTRING] = L"";
TCHAR szBalloon[MAX_LOADSTRING] = L"";
TCHAR szEnvironment[MAX_LOADSTRING] = L"";
volatile DWORD dwChildrenPid;

static DWORD GetProcessId(HANDLE hProcess)
{
	// https://gist.github.com/kusma/268888
	typedef DWORD (WINAPI *pfnGPI)(HANDLE);
	typedef ULONG (WINAPI *pfnNTQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);
 
	static int first = 1;
	static pfnGPI GetProcessId;
	static pfnNTQIP ZwQueryInformationProcess;
	if (first)
	{
		first = 0;
		GetProcessId = (pfnGPI)GetProcAddress(
		    GetModuleHandleW(L"KERNEL32.DLL"), "GetProcessId");
		if (!GetProcessId)
			ZwQueryInformationProcess = (pfnNTQIP)GetProcAddress(
			    GetModuleHandleW(L"NTDLL.DLL"),
			    "ZwQueryInformationProcess");
	}
	if (GetProcessId)
		return GetProcessId(hProcess);
	if (ZwQueryInformationProcess)
	{
		struct
		{
			PVOID Reserved1;
			PVOID PebBaseAddress;
			PVOID Reserved2[2];
			ULONG UniqueProcessId;
			PVOID Reserved3;
		} pbi;
		ZwQueryInformationProcess(hProcess, 0, &pbi, sizeof(pbi), 0);
		return pbi.UniqueProcessId;
	}
	return 0;
}

BOOL ShowTrayIcon()
{
	NOTIFYICONDATA nid;
	ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
	nid.cbSize = (DWORD)sizeof(NOTIFYICONDATA);
	nid.hWnd   = hWnd;
	nid.uID	   = NID_UID;
	nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
	nid.dwInfoFlags=NIIF_INFO;
	nid.uCallbackMessage = WM_TASKBARNOTIFY;	
	nid.hIcon = LoadIcon(hInst, (LPCTSTR)IDI_SMALL);
	lstrcpy(nid.szTip, szTooltip);
    if (lstrlen(szBalloon) > 0)
	{
		nid.uFlags |= NIF_INFO;
		lstrcpy(nid.szInfo, szBalloon);
		lstrcpy(nid.szInfoTitle,szTitle);
	}
	Shell_NotifyIcon(NIM_ADD, &nid);
	return TRUE;
}

BOOL DeleteTrayIcon()
{
	NOTIFYICONDATA nid;
	nid.cbSize = (DWORD)sizeof(NOTIFYICONDATA);
	nid.hWnd   = hWnd;
	nid.uID	   = NID_UID;
	Shell_NotifyIcon(NIM_DELETE, &nid);
	return TRUE;
}

BOOL ShowPopupMenu()
{
    POINT pt;
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_SHOW, L"\x663e\x793a"); 	
    AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_HIDE, L"\x9690\x85cf");
    AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_RELOAD, L"\x91cd\x65b0\x8f7d\x5165");
    // AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_ABOUT,  L"\x5173\x4e8e");
    AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_EXIT,   L"\x9000\x51fa");
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessage(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
	return TRUE;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;
   LoadString(hInst, IDS_TITLE, szTitle, sizeof(szTitle)/sizeof(szTitle[0])-1);
   LoadString(hInst, IDS_CMDLINE, szCommandLine, sizeof(szCommandLine)/sizeof(szCommandLine[0])-1);
   LoadString(hInst, IDS_TOOLTIP, szTooltip, sizeof(szTooltip)/sizeof(szTooltip[0])-1);
   LoadString(hInst, IDS_BALLOON, szBalloon, sizeof(szBalloon)/sizeof(szBalloon[0])-1);
   LoadString(hInst, IDS_ENVIRONMENT, szEnvironment, sizeof(szEnvironment)/sizeof(szEnvironment[0])-1);

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPED|WS_SYSMENU,
      NULL, NULL, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

BOOL CDCurrentDirectory()
{
	TCHAR szPath[4096] = L"";
	GetModuleFileName(NULL, szPath, sizeof(szPath)/sizeof(szPath[0])-1);
	*wcsrchr(szPath, L'\\') = 0;
	SetCurrentDirectory(szPath);
	return TRUE;
}

BOOL SetEenvironment()
{
	TCHAR *sep = L"\n";
	TCHAR *pos = NULL;
	TCHAR *token = wcstok(szEnvironment, sep);
	while(token != NULL)
	{
		if (pos = wcschr(token, L'='))
		{
			*pos = 0;
			SetEnvironmentVariableW(token, pos+1);
			// wprintf(L"[%s] = [%s]\n", token, pos+1);
		}
		token = wcstok(NULL, sep);
	}
	return TRUE;
}

BOOL CreateConsole()
{
	TCHAR szVisible[BUFSIZ] = L"";

	AllocConsole();
	_wfreopen(L"CONIN$",  L"r+t", stdin);
	_wfreopen(L"CONOUT$", L"w+t", stdout);
	hConsole = GetConsoleWindow();
	
	if (GetEnvironmentVariableW(L"VISIBLE", szVisible, BUFSIZ-1) && szVisible[0] == L'0')
	{
		ShowWindow(hConsole, SW_HIDE);
	}
	else
	{
		SetForegroundWindow(hConsole);
	}
	return TRUE;
}

BOOL ExecCmdline()
{
	SetWindowText(hConsole, szTitle);
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = TRUE;
	BOOL bRet = CreateProcess(NULL, szCommandLine, NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi);
	if(bRet)
	{
		dwChildrenPid = GetProcessId(pi.hProcess);
	}
	else
	{
		wprintf(L"ExecCmdline \"%s\" failed!\n", szCommandLine);
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return TRUE;
}

BOOL ReloadCmdline()
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwChildrenPid);
	if (hProcess)
	{
		TerminateProcess(hProcess, 0);
	}
	ShowWindow(hConsole, SW_SHOW);
	SetForegroundWindow(hConsole);
	wprintf(L"\n\n");
	Sleep(200);
	ExecCmdline();
	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int nID;
	switch (message) 
	{
		case WM_TASKBARNOTIFY:
			if (lParam == WM_LBUTTONUP)
			{
				ShowWindow(hConsole, !IsWindowVisible(hConsole));
				SetForegroundWindow(hConsole);
			}
			else if (lParam == WM_RBUTTONUP)
			{
				ShowPopupMenu();
			}
			break;
		case WM_COMMAND:
			nID = LOWORD(wParam);
			if (nID == WM_TASKBARNOTIFY_MENUITEM_SHOW)
			{
				ShowWindow(hConsole, SW_SHOW);
				SetForegroundWindow(hConsole);
			}
			else if (nID == WM_TASKBARNOTIFY_MENUITEM_HIDE)
			{
				ShowWindow(hConsole, SW_HIDE);
			}
			if (nID == WM_TASKBARNOTIFY_MENUITEM_RELOAD)
			{
				ReloadCmdline();
			}
			else if (nID == WM_TASKBARNOTIFY_MENUITEM_ABOUT)
			{
				MessageBoxW(hWnd, szTooltip, szWindowClass, 0);
			}
			else if (nID == WM_TASKBARNOTIFY_MENUITEM_EXIT)
			{
				DeleteTrayIcon();
				PostMessage(hConsole, WM_CLOSE, 0, 0);
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_TASKBAR);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCTSTR)NULL;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

void WatchdogThreadEntryPoint(void* args)
{
	WCHAR szMemoryLimit[BUFSIZ] = L"0";
	WCHAR szMemoryLimitInterval[BUFSIZ] = L"60";
	DWORD dwMemoryLimit = 0;
	DWORD dwMemoryLimitInterval = 60;
	PROCESS_MEMORY_COUNTERS pmc;

	if (!GetEnvironmentVariableW(L"MEMORY_LIMIT", szMemoryLimit, BUFSIZ-1))
	{
		return;
	}
	dwMemoryLimit = _wtoi(szMemoryLimit);
	if (dwMemoryLimit == 0)
	{
		return;
	}
	switch (szMemoryLimit[lstrlen(szMemoryLimit)-1])
	{
		case 'K':
		case 'k':
			dwMemoryLimit *= 1024;
			break;
		case 'M':
		case 'm':
			dwMemoryLimit *= 1024*1024;
			break;
		case 'G':
		case 'g':
			dwMemoryLimit *= 1024*1024*1024;
		default:
			break;
	}

	if (GetEnvironmentVariableW(L"MEMORY_LIMIT_INTERVAL", szMemoryLimitInterval, BUFSIZ-1))
	{
		dwMemoryLimitInterval = _wtoi(szMemoryLimitInterval);;
	}

	while (1)
	{
		Sleep(dwMemoryLimitInterval * 1000);
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwChildrenPid);
		if (hProcess)
		{
			GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
			if (pmc.WorkingSetSize > dwMemoryLimit)
			{
				SetConsoleTextAttribute(GetStdHandle(-11), 0x04);
				wprintf(L"\n\ndwChildrenPid=%d WorkingSetSize=%d large than szMemoryLimit=%s, restart.\n\n", dwChildrenPid, pmc.WorkingSetSize, szMemoryLimit);
				SetConsoleTextAttribute(GetStdHandle(-11), 0x07);
				ReloadCmdline();
			}
		}
	}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
 	MyRegisterClass(hInstance);
	if (!InitInstance (hInstance, SW_HIDE)) 
	{
		return FALSE;
	}
	SetEenvironment();
	CreateConsole();
	CDCurrentDirectory();
	ExecCmdline();
	ShowTrayIcon();
	_beginthread(WatchdogThreadEntryPoint, 0, NULL);
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}



