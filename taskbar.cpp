#define _WIN32_IE 0x0500

#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <stdio.h>
#include <io.h>
#include "psapi.h"
#include "resource.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wininet.lib")

extern "C" WINBASEAPI HWND WINAPI GetConsoleWindow();

#define NID_UID 123
#define WM_TASKBARNOTIFY WM_USER+20
#define WM_TASKBARNOTIFY_MENUITEM_SHOW (WM_USER + 21)
#define WM_TASKBARNOTIFY_MENUITEM_HIDE (WM_USER + 22)
#define WM_TASKBARNOTIFY_MENUITEM_RELOAD (WM_USER + 23)
#define WM_TASKBARNOTIFY_MENUITEM_ABOUT (WM_USER + 24)
#define WM_TASKBARNOTIFY_MENUITEM_EXIT (WM_USER + 25)
#define WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE (WM_USER + 26)

HINSTANCE hInst;
HWND hWnd;
HWND hConsole;
TCHAR szTitle[64] = L"";
TCHAR szWindowClass[16] = L"taskbar";
TCHAR szCommandLine[1024] = L"";
TCHAR szTooltip[512] = L"";
TCHAR szBalloon[512] = L"";
TCHAR szEnvironment[1024] = L"";
TCHAR szProxyString[2048] = L"";
TCHAR *lpProxyList[10] = {0};
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


LPCTSTR GetWindowsProxy()
{
	static TCHAR szProxy[1024] = {0};
    HKEY hKey;
	DWORD dwData = 0;
	DWORD dwSize = sizeof(DWORD);

    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER,
		                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
									  0,
									  KEY_READ | KEY_WRITE, 
									  &hKey))
	{
		szProxy[0] = 0;
		dwSize = sizeof(szProxy)/sizeof(szProxy[0]);
		RegQueryValueExW(hKey, L"AutoConfigURL", NULL, 0, (LPBYTE)&szProxy, &dwSize);
		if (wcslen(szProxy))
			return szProxy;
		dwData = 0;
		RegQueryValueExW(hKey, L"ProxyEnable", NULL, 0, (LPBYTE)&dwData, &dwSize);
		if (dwData == 0)
			return L"";
		szProxy[0] = 0;
		dwSize = sizeof(szProxy)/sizeof(szProxy[0]);
		RegQueryValueExW(hKey, L"ProxyServer", NULL, 0, (LPBYTE)&szProxy, &dwSize);
		if (wcslen(szProxy))
			return szProxy;
    }
	return NULL;
}


BOOL SetWindowsProxy(int n)
{
	TCHAR * szProxy = lpProxyList[n];
    HKEY hKey;

    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER,
		                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
									  0,
									  KEY_READ | KEY_WRITE, 
									  &hKey))
	{
		if (wcslen(szProxy) == 0)
		{
			DWORD dwData = 0;
			RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(REG_DWORD));
			RegSetValueExW(hKey, L"AutoConfigURL", 0, REG_SZ, (LPBYTE)L"", 2);
		}
		else if (wcsstr(szProxy, L"://") != NULL)
		{
			DWORD dwData = 0;
			RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(REG_DWORD));
			RegSetValueExW(hKey, L"ProxyOverride", 0, REG_SZ, (LPBYTE)L"<local>", 16);
			RegSetValueExW(hKey, L"AutoConfigURL", 0, REG_SZ, (LPBYTE)szProxy, (lstrlen(szProxy)+1)*sizeof(TCHAR));
		}
		else
		{
			DWORD dwData = 1;
			RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(REG_DWORD));
			RegSetValueExW(hKey, L"ProxyOverride", 0, REG_SZ, (LPBYTE)L"<local>", 16);
			RegSetValueExW(hKey, L"AutoConfigURL", 0, REG_SZ, (LPBYTE)L"", 2);
			RegSetValueExW(hKey, L"ProxyServer", 0, REG_SZ, (LPBYTE)szProxy, (lstrlen(szProxy)+1)*sizeof(TCHAR));
		}
		RegCloseKey(hKey);
		InternetSetOptionW(0, INTERNET_OPTION_REFRESH, 0, 0);
		InternetSetOptionW(0, INTERNET_OPTION_SETTINGS_CHANGED, 0, 0);
    }

	return TRUE;
}


BOOL ShowPopupMenu()
{
	POINT pt;
	HMENU hSubMenu = CreatePopupMenu();
	LPCTSTR lpCurrentProxy = GetWindowsProxy();
	for (int i = 0; lpProxyList[i]; i++)
	{
		UINT uFlags = wcscmp(lpProxyList[i], lpCurrentProxy) == 0 ? MF_STRING | MF_CHECKED : MF_STRING;
		LPCTSTR lpText = wcslen(lpProxyList[i]) ? lpProxyList[i] : L"\x7981\x7528\x4ee3\x7406";
		AppendMenu(hSubMenu, uFlags, WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE+i, lpText);
	}

	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_SHOW, L"\x663e\x793a"); 	
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_HIDE, L"\x9690\x85cf");
	AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, L"\x8bbe\x7f6e IE \x4ee3\x7406");
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_RELOAD, L"\x91cd\x65b0\x8f7d\x5165");
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_EXIT,   L"\x9000\x51fa");
	GetCursorPos(&pt);
	TrackPopupMenu(hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
	PostMessage(hWnd, WM_NULL, 0, 0);
	DestroyMenu(hMenu);
	return TRUE;
}

BOOL ParseProxyList()
{
	TCHAR *sep = L"\n";
	TCHAR *pos = wcstok(szProxyString, sep);
	INT i = 0;
	lpProxyList[i++] = L"";
	while (pos && i < sizeof(lpProxyList)/sizeof(lpProxyList[0]))
	{
		lpProxyList[i++] = pos;
		pos = wcstok (NULL, sep);
	}
	lpProxyList[i] = 0;
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
   LoadString(hInst, IDS_PROXYLIST, szProxyString, sizeof(szProxyString)/sizeof(szProxyString[0])-1);

   ParseProxyList();

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
			//wprintf(L"[%s] = [%s]\n", token, pos+1);
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
		MessageBox(NULL, szCommandLine, L"Error: \x6267\x884c\x547d\x4ee4\x5931\x8d25!", MB_OK);
		ExitProcess(0);
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

void CheckMemoryLimit()
{
	WCHAR szMemoryLimit[BUFSIZ] = L"0";
	DWORD dwMemoryLimit = 0;
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

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwChildrenPid);
	if (hProcess)
	{
		GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
		CloseHandle(hProcess);
		if (pmc.WorkingSetSize > dwMemoryLimit)
		{
			SetConsoleTextAttribute(GetStdHandle(-11), 0x04);
			wprintf(L"\n\ndwChildrenPid=%d WorkingSetSize=%d large than szMemoryLimit=%s, restart.\n\n", dwChildrenPid, pmc.WorkingSetSize, szMemoryLimit);
			SetConsoleTextAttribute(GetStdHandle(-11), 0x07);
			ReloadCmdline();
		}
	}
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
			else if (WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE <= nID && nID <= WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE+sizeof(lpProxyList)/sizeof(lpProxyList[0]))
			{
				SetWindowsProxy(nID-WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE);
			}
			break;
		case WM_TIMER:
			CheckMemoryLimit();
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
	SetTimer(hWnd, 0, 30 * 1000, NULL);
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

