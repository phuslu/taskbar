#define _WIN32_IE 0x0500

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include "resource.h"

extern "C" WINBASEAPI HWND WINAPI GetConsoleWindow();

#define MAX_LOADSTRING 1024
#define NID_UID 123
#define WM_TASKBARNOTIFY WM_USER+20
#define WM_TASKBARNOTIFY_MENUITEM_SHOW WM_USER + 21
#define WM_TASKBARNOTIFY_MENUITEM_HIDE WM_USER + 22
#define WM_TASKBARNOTIFY_MENUITEM_EXIT WM_USER + 23

HINSTANCE hInst;
HWND hWnd;
HWND hConsole;					
TCHAR szTitle[MAX_LOADSTRING] = L"";
TCHAR szWindowClass[MAX_LOADSTRING] = L"taskbar";
TCHAR szCommandLine[MAX_LOADSTRING] = L"";
TCHAR szTooltip[MAX_LOADSTRING] = L"";
TCHAR szBalloon[MAX_LOADSTRING] = L"";
TCHAR szVisible[MAX_LOADSTRING] = L"";
TCHAR szEnvironment[MAX_LOADSTRING] = L"";

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
    AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_EXIT, L"\x9000\x51fa");
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    PostMessage(hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
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

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;
   LoadString(hInst, IDS_TITLE, szTitle, sizeof(szTitle)/sizeof(szTitle[0])-1);
   LoadString(hInst, IDS_CMDLINE, szCommandLine, sizeof(szCommandLine)/sizeof(szCommandLine[0])-1);
   LoadString(hInst, IDS_TOOLTIP, szTooltip, sizeof(szTooltip)/sizeof(szTooltip[0])-1);
   LoadString(hInst, IDS_BALLOON, szBalloon, sizeof(szBalloon)/sizeof(szBalloon[0])-1);
   LoadString(hInst, IDS_VISIBLE, szVisible, sizeof(szVisible)/sizeof(szVisible[0])-1);
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
	TCHAR *token = wcstok(szEnvironment, sep);
	while(token!=NULL)
	{
		TCHAR *pos = wcschr(token, L'=');
		if (pos)
		{
			*pos = 0;
			TCHAR* key = token;
			TCHAR* value = pos + 1;
			SetEnvironmentVariableW(key, value);
		}
		token = wcstok(NULL, sep);
	}
	return TRUE;
}

BOOL CreateConsole()
{
	AllocConsole();
	_wfreopen(L"CONIN$",  L"r+t", stdin);
	_wfreopen(L"CONOUT$", L"w+t", stdout);
	hConsole = GetConsoleWindow();
	if (lstrcmp(szVisible, L"") == 0 || lstrcmp(szVisible, L"0") == 0)
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
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	else
	{
		wprintf(L"ExecCmdline \"%s\" failed!\n", szCommandLine);
	}
	return TRUE;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
 	MyRegisterClass(hInstance);
	if (!InitInstance (hInstance, SW_HIDE)) 
	{
		return FALSE;
	}
	CreateConsole();
	CDCurrentDirectory();
	SetEenvironment();
	ExecCmdline();
	ShowTrayIcon();
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}



