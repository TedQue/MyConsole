// MyConsole.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include "MyConsoleApp.h"
#include "MyConsole.h"
#include <crtdbg.h>
#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;								// 当前实例
TCHAR szTitle[MAX_LOADSTRING];					// 标题栏文本
TCHAR szWindowClass[MAX_LOADSTRING];			// 主窗口类名
HWND hWnd;

TCHAR szChildClass[MAX_LOADSTRING];
HWND hConsoleWnd = NULL;
MyConsole *theConsole = NULL;
HWND hEditWnd = NULL;
MyEdit *theEdit = NULL;

// 此代码模块中包含的函数的前向声明:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	ChildWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
void				ResizeChildren();

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: 在此放置代码。
	MSG msg;
	HACCEL hAccelTable;

	// 初始化全局字符串
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_MYCONSOLE, szWindowClass, MAX_LOADSTRING);
	_tcscpy(szChildClass, _T("MyConsoleChildWnd"));
	MyRegisterClass(hInstance);

	// 执行应用程序初始化:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MYCONSOLE));

	// 主消息循环:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	if(theEdit) delete theEdit;
	if(theConsole) delete theConsole;

	_CrtDumpMemoryLeaks();
	return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	ATOM ret = 0;
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYCONSOLE));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_MYCONSOLE);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	ret = RegisterClassEx(&wcex);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= ChildWndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= NULL;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szChildClass;
	wcex.hIconSm		= NULL;

	ret = RegisterClassEx(&wcex);
	return ret;
}

#define curtext txt1
#define bkcolor RGB(255,255,255)

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // 将实例句柄存储在全局变量中

	hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}

	// 创建两个子窗口,分别关联 Edit 和 Console
	hEditWnd = CreateWindow(szChildClass, _T("MyEdit"), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
	if(!hEditWnd)
	{
		DWORD err = GetLastError();
		return FALSE;
	}

	hConsoleWnd = CreateWindow(szChildClass, _T("MyConsole"), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, hInstance, NULL);
	if(!hConsoleWnd)
	{
		return FALSE;
	}

	theEdit = new MyEdit(10240 * 1024, true, NULL);
	theEdit->attach(hEditWnd, MESSAGE_MODE_INTERCEPT);

	theConsole = new MyConsole(1024 * 1024, true, NULL);
	theConsole->attach(hConsoleWnd, MESSAGE_MODE_INTERCEPT);
	theConsole->setEventMask(CONSOLE_EVENT_RETRUN);

	ResizeChildren();
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// 输出版本信息
	wchar_t* ver1 = L"Console Window with ";
	wchar_t* ver2 = L" fonts supported [version 0.9 release 2015-04-23]\r\n(c) 2015 Que's C++ Studio all right reserved.\r\n";
	theConsole->write(ver1);
	theConsole->write(L"co", 2, RGB(255, 50, 50));
	theConsole->write(L"lo", 2, RGB(20, 200, 20));
	theConsole->write(L"ur", 2, RGB(50, 50, 255));
	theConsole->write(ver2);

	// 切换到输入模式
	theConsole->switchMode(CONSOLEMODE_INPUT);

	////theConsole->write(L"a\r\n", -1, CONSOLECOLOR_TEXT, CONSOLECOLOR_BK);
	////theConsole->write(L"b\r\n", -1, CONSOLECOLOR_TEXT, CONSOLECOLOR_BK);
	return TRUE;
}

void ResizeChildren()
{
	RECT rc;
	GetClientRect(hWnd, &rc);

	int midX = (rc.right - rc.left) / 2;
	int h = rc.bottom - rc.top;

	MoveWindow(hEditWnd, rc.left + 2, rc.top + 2, midX - 4, h - 4, TRUE);
	MoveWindow(hConsoleWnd, midX + 2, rc.top + 2, midX - 4, h - 4, TRUE);
}

LRESULT CALLBACK ChildWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static bool checked = false;
	int wmId, wmEvent;
	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// 分析菜单选择:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		//case ID_AUTOWRAP:
		//	CheckMenuItem(GetMenu(hWnd), ID_AUTOWRAP, checked ? MF_UNCHECKED : MF_CHECKED);
		//	checked = !checked;
		//	theConsole->getActiveBuffer()->enableAutoWrap(checked);

		//	break;
		//case ID_SYSTEM_FIXED_FONT:
		//	theConsole->getActiveBuffer()->setFont((HFONT)GetStockObject(SYSTEM_FIXED_FONT), 0, 0, 4);
		//	theConsole->refresh();
		//	CheckMenuItem(GetMenu(hWnd), ID_SYSTEM_FIXED_FONT, MF_CHECKED);
		//	CheckMenuItem(GetMenu(hWnd), ID_DFTGUI_FONT,  MF_UNCHECKED);
		//	break;
		//case ID_DFTGUI_FONT:
		//	{
		//		theConsole->getActiveBuffer()->setFont((HFONT)GetStockObject(DEFAULT_GUI_FONT), 3, 0, 4);
		//		theConsole->refresh();
		//		CheckMenuItem(GetMenu(hWnd), ID_SYSTEM_FIXED_FONT, MF_UNCHECKED);
		//		CheckMenuItem(GetMenu(hWnd), ID_DFTGUI_FONT, MF_CHECKED);
		//	}
		//	break;
		//case ID_TEST_5000:
		//	{
		//		theConsole->switchMode(CONSOLEMODE_OUTPUT);
		//		TCHAR txt[200] = {0};
		//		for(int i = 0; i < 1000; i++)
		//		{
		//			_stprintf(txt, _T("测试连续输出,row[%d]\r\n"), i);
		//			theConsole->write(txt, _tcslen(txt), RGB(255,0,0), 0);
		//			if(i % 10 == 0)
		//			{
		//				EatAllMessage();
		//			}
		//		}
		//		theConsole->switchMode(CONSOLEMODE_INPUT);
		//	}
		//	break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	//case WM_LBUTTONDOWN:
	//{
	//	TCHAR* txt = _T("消息 WM_LBUTTONDOWN Received.\n");
	//	theConsole->append(txt, _tcslen(txt), RGB(255,0,0), bkcolor);
	//}
	//break;
	//case WM_RBUTTONDOWN:
	//{
	//	TCHAR* txt = _T("消息 WM_RBUTTONDOWN Received.\n");
	//	theConsole->append(txt, _tcslen(txt), RGB(0,255,0), bkcolor);
	//}
	//break;
	//case WM_KEYDOWN:
	//{
	//	TCHAR* txt = _T("消息 WM_KEYDOWN Received.\n");
	//	theConsole->append(txt, _tcslen(txt), RGB(0,0,255), bkcolor);
	//}
	//break;
	case WM_CONSOLE_EVENT:
	{
		wchar_t buf[2048] = {0};
		int bufLen = theConsole->read(buf, 2048);

		theConsole->switchMode(CONSOLEMODE_OUTPUT);

		wchar_t* cmdTitle = L"Input:";
		theConsole->write(cmdTitle, wcslen(cmdTitle), RGB(255, 50, 50));
		theConsole->write(buf, bufLen);
		wchar_t* cmdEnd = L"\r\nEcho:";
		theConsole->write(cmdEnd, wcslen(cmdEnd), RGB(255, 50, 50));
		theConsole->write(buf, bufLen);
		theConsole->write(L"\r\n", 2, 0);

		theConsole->switchMode(CONSOLEMODE_INPUT);

		// return 1 表示事件已经被处理
		return 1;
	}
	break;
	case WM_SIZE:
	{
		ResizeChildren();
	}
	break;
	case WM_ERASEBKGND:
	{
		HDC hDc = (HDC)wParam;
		RECT rc;
		GetClientRect(hWnd, &rc);
		FillRect(hDc, &rc, (HBRUSH)GetStockObject(GRAY_BRUSH));
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

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
