// RDClient.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "RDClient.h"
#include "../RDLib/libheaders.h"
#include "../RDCrypt/RDCrypt.h"
#include "../Compression/Compression.h"
#include <string>
#include <vector>
using namespace std;

DWORD WINAPI connectThreadProc(LPVOID lpParameter);
DWORD WINAPI readThreadProc(LPVOID lpParameter);
DWORD WINAPI sendThreadProc(LPVOID lpParameter);

LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	SplashScreen(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	ConnectDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK	PassDlg(HWND, UINT, WPARAM, LPARAM);

class AppSettings;
class ConnectionSettings;

string password;
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

vector<RecvMessage> events;
bool				eventsReady = false;
RDMutex				eventsMutex;


class AppSettings
{
	RDThread	* m_connectThread;
	bool		m_bconnectThreadRunning;


public:
	// Displaying stuff
	HWND		m_hWnd;
	HDC			m_hDC;
	HBITMAP		m_hBitmap;
	RDThread	* m_sendThread;
	RDThread	* m_readThread;
	DSocket		* m_sock;
	bool		connected;
	BitmapInfo	BI;
	int			BytesPerPixel;

	RDMutex		 updateMutex;
	vector<Rect> updateRect;
	bool		 updateRectChange;

	char		* m_backBuff;
	char		* uncompress;
	bool		paint;
	//RDSemaphore	sem;
	RDMutex			sem;
	HDC				dc;
	HBITMAP			bitmap;
	bool			first;

	int Display(HDC hdc)
	{
		if ( first ) {
			first = false;
			dc = CreateCompatibleDC(hdc);
			bitmap = CreateCompatibleBitmap(hdc, BI.bminfo.bmiHeader.biWidth, BI.bminfo.bmiHeader.biHeight);
			SelectObject(dc,bitmap);
		}
		if ( dc == 0 )
			return GetLastError();
		SetDIBits(dc, bitmap, 0, BI.bminfo.bmiHeader.biHeight, m_backBuff, &BI.bminfo, DIB_RGB_COLORS);
		//SetBitmapBits(bitmap, BI.bminfo.bmiHeader.biSizeImage, m_backBuff);
		 //= CreateDIBitmap(dc, &BI.bminfo.bmiHeader, CBM_INIT, (void *)m_backBuff, &BI.bminfo, DIB_RGB_COLORS);
		//CreateBitmap
		//HBITMAP bitmap = CreateDIBSection(hdc, &BI.bminfo, DIB_RGB_COLORS,(void **)m_backBuff,0 , 0);
		if ( bitmap == 0 )
			return GetLastError();
		if ( BitBlt(hdc, 0, 0, BI.bminfo.bmiHeader.biWidth, BI.bminfo.bmiHeader.biHeight, dc, 0 , 0, SRCCOPY) == FALSE )
			return GetLastError();
		if ( DeleteObject(bitmap) != 0 )
			return GetLastError();
		return ERROR_SUCCESS;
	}

	void UpdateBuffer(Rect & rect, char * buffToread)
	{
		int dx = rect.x2 - rect.x1;
		int dy = rect.y2 - rect.y1;
		for (int i=0; i < dy; ++i)
			memcpy((m_backBuff + ((rect.y1+i)*BI.bminfo.bmiHeader.biWidth+rect.x1)*BytesPerPixel), (buffToread+dx*i*BytesPerPixel), sizeof(BYTE)*BytesPerPixel*dx);

		/*
		int dx = rect.x2 - rect.x1;
		int dy = rect.y2 - rect.y1;
		for (int i=0; i < dy; ++i)
			memcpy((m_backBuff + ((rect.y1+i)*BI.bminfo.bmiHeader.biWidth+rect.x1)*BytesPerPixel), (buffToread+dx*i*BytesPerPixel), sizeof(BYTE)*BytesPerPixel*dx);
			*/
	}

	bool isConnectThreadRunning() const { return m_bconnectThreadRunning; }
	AppSettings():m_connectThread(0),m_bconnectThreadRunning(false),m_sock(0), connected(false), updateRectChange(false), paint(false)
	{ 
		//sem.Init(1);
		first = true;
		//memset(&updateRect, 0, sizeof(Rect));
	}
	RDThread *& getConnectThread() { return m_connectThread; }
	bool runThread()
	{
		m_connectThread = new RDThread(connectThreadProc, (void *)this);
		if ( m_connectThread == 0 )
			return false;
		return true;
	}
	void DestoryThread() {
		m_sendThread->Suspend();
		m_readThread->Suspend();
		m_connectThread->Suspend();
		m_sendThread->Terminate();
		m_sendThread->Terminate();
		m_connectThread->Terminate();
		m_connectThread->WaitForEnd(INFINITE);
		m_bconnectThreadRunning = false;
		delete m_sendThread;
		delete m_readThread;
		delete m_connectThread;
	}
};

AppSettings		   appSettings;

bool Negotiation(DSocket & server);


class ConnectionSettings
{
private:
	int		m_iPort;
	int		m_iCompression;
	int		m_iEncryption;
	string  m_sHost;
	bool	m_bBehindNAT;
public:
	ConnectionSettings():m_iPort(666), m_iCompression(ZIP_COMPRESSION), m_iEncryption(RAW_ENCRYPTION),
		m_bBehindNAT(false), m_sHost("AniaPC")
	{
	}
	~ConnectionSettings() { }
	// properties of bean
	// Port Number
	int & PortNumber() { return m_iPort; }
	// Compression
	int & Compression() { return m_iCompression; }
	// Encryption
	int & Encryption() { return m_iEncryption; }
	// Behind NAT or firewall
	bool & BehindNAT() { return m_bBehindNAT; }
	// Host name or ip adress
	string & Host() { return m_sHost; }
};

// Global Variables:
ConnectionSettings connSettings;

char * buffToread = 0;

DWORD WINAPI readThreadProc(LPVOID lpParameter)
{
	AppSettings * app = (AppSettings *)lpParameter;
	DSocket * server = app->m_sock;
	RecvMessage message;

	while ( app->connected )
	{
		if ( !server->ReadW((char *)&message, sizeof(RecvMessage)) )
			return -1;
		switch ( message.msgtype ) {
			case toServerScreenResolutionEvent:
				{
				//change screen resolution and pixel format
				if ( !server->ReadW((char *)&app->BI, sizeof(BitmapInfo)) )
					return -1;
				app->BytesPerPixel = app->BI.bminfo.bmiHeader.biBitCount/8;
				buffToread = new char[app->BI.bminfo.bmiHeader.biSizeImage];
				app->m_backBuff = new char[app->BI.bminfo.bmiHeader.biSizeImage];
				app->uncompress = new char[((int)((double)app->BI.bminfo.bmiHeader.biSizeImage*1.015))+1];
				app->paint = true;				
				break;
				}
			case toClientSendingRectBuffer:
				Rect rect;
				if ( !server->ReadW((char *)&rect, sizeof(Rect)))
					return -1;
				if ( !server->ReadW((char *)buffToread, message.msg.msglen))
					return -1;
				if ( connSettings.Compression() == RAW_COMPRESSION ) {
					app->UpdateBuffer(rect, buffToread);
				} else {
					unsigned long len=((int)((double)app->BI.bminfo.bmiHeader.biSizeImage*1.015))+1;
					uncompress((unsigned char *)app->uncompress, &len, (unsigned char *)buffToread, message.msg.msglen);
					memcpy(buffToread, app->uncompress, len);
					app->UpdateBuffer(rect, buffToread);
				}
				RECT srect;
				srect.top = rect.y1;
				srect.left = rect.x1;
				srect.right = rect.x2;
				srect.bottom = rect.y2;
				InvalidateRect(app->m_hWnd, 0, false);
				UpdateWindow(app->m_hWnd);
				break;
			case toClientUpdateRect:
				// send toServerRectEvent that we want update
				app->updateMutex.lock();
				app->updateRect.push_back(message.msg.rect);
				app->updateRectChange = true;
				app->updateMutex.unlock();
				//app->m_sendThread->Resume();
				break;
			default:
				app->connected = false;
				break;
		};
		Sleep(5);
	}	
	return 0;
}

DWORD WINAPI sendThreadProc(LPVOID lpParameter)
{
	AppSettings * app = (AppSettings *)lpParameter;
	DSocket * server = app->m_sock;
	RecvMessage message;	
	char buff[256];

	while ( app->connected )
	{
		app->updateMutex.lock();
		if ( app->updateRectChange ) {
			app->updateRectChange = false;
			for ( int i=0;i<app->updateRect.size();++i) {
				message.msgtype = toServerRectEvent ;
				message.msg.rect = app->updateRect[i];
				sprintf(buff, "rect %d, %d, %d, %d\n", message.msg.rect.x1, message.msg.rect.y1, message.msg.rect.x2, message.msg.rect.y2);
				logger.AddString(buff);
				if ( !server->SendW((char *)&message, sizeof(RecvMessage)) )
					return 0;
			}
			app->updateRect.clear();
		}
		app->updateMutex.unlock();

		eventsMutex.lock();
		if ( eventsReady == true ) {
			for ( int i=0; i < events.size(); ++i ) {
				RecvMessage message = events[i];
				server->SendW((char *)&message, sizeof(RecvMessage));
			}
			events.clear();
			eventsReady = false;
		}
		eventsMutex.unlock();

		Sleep(5);
	}
	return 0;
}


DWORD WINAPI connectThreadProc(LPVOID lpParameter)
{
	AppSettings * app = (AppSettings *)lpParameter;
	DSocket * server = new DSocket();
	server->Create();
	if ( server->Connect(connSettings.Host(), connSettings.PortNumber()) ) {
		if ( Negotiation(*server) ) {
			app->m_sock = server;
			app->connected = true;
            app->m_sendThread = new RDThread(sendThreadProc, app);
			app->m_readThread = new RDThread(readThreadProc, app);
		} else {
			delete server;
			MessageBox(0, TEXT("Too many password Retries or connection error"), TEXT("Information"), MB_ICONEXCLAMATION|MB_OK);
		}
	} else {
		delete server;
		MessageBox(0, TEXT("Could not connect"), TEXT("Error"),0);
	}
	return 0;
}


bool Negotiation(DSocket & server)
{
	Message	   headerMSG;
	WelcomeMsg msg;

	memset(&headerMSG, 0, sizeof(Message));
	if ( !server.ReadW((char *)&headerMSG, sizeof(Message)) ) {
		return false;
	}

	if ( headerMSG.msgtype != Welcome ) {
		return false;
	}

	memset((void *)&msg, 0, sizeof(WelcomeMsg));
	if ( !server.ReadW((char *)&msg, sizeof(WelcomeMsg)) ) {
		return false;
	}

	if ( !server.SendW((char *)&headerMSG, sizeof(Message)) ) {
		return false;
	}

	msg.IP = get_my_IP();
	msg.Port = 666;
	msg.isNat = connSettings.BehindNAT();
	msg.Compression = (unsigned char)connSettings.Compression();
	msg.Encryption = (unsigned char)connSettings.Encryption();

	if ( !server.SendW((char *)&msg, sizeof(WelcomeMsg)) ) {
		return false;
	}

	PasswordMsg pass;
	unsigned char m=0;
	do {
		headerMSG.msgtype = Password;
		headerMSG.msglen = sizeof(PasswordMsg);

		if ( !server.SendW((char *)&headerMSG, sizeof(Message)) ) {
			return false;
		}

		DialogBox(hInst, (LPCTSTR) IDD_PASSWORD, 0, (DLGPROC)PassDlg);

		strcpy(pass.password, (char *)password.c_str());

		if ( !server.SendW((char *)pass.password, sizeof(PasswordMsg)) ) {
			return false;
		}
		if ( !server.ReadW((char *)&headerMSG, sizeof(Message)) ) {
			return false;
		}
		m = headerMSG.msgtype;

		if ( !server.ReadW((char *)&headerMSG, sizeof(Message)) ) {
			return false;
		}
	} while ( m != GoodPassword && headerMSG.msgtype == RetryPassword);
	if ( headerMSG.msgtype == CloseConnection )
		return false;
	return true;
}

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	logger.AddString(TEXT("Client enter\n"));
	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_RDCLIENT, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_RDCLIENT);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_RDCLIENT);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCTSTR)IDC_RDCLIENT;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   appSettings.m_hWnd = hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   //ShowWindow(hWnd, nCmdShow);
   //UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	static HWND	hDlg;
	RecvMessage msg;

	switch (message) 
	{
		case WM_LBUTTONDOWN:
			{
				msg.msgtype = toServerMouseEvent;
				msg.msg.mouseevent.point.x = LOWORD(lParam);
				msg.msg.mouseevent.point.y = HIWORD(lParam);
				msg.msg.mouseevent.flags = MOUSEEVENTF_LEFTDOWN; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;

		case WM_LBUTTONUP:
			{
				msg.msgtype = toServerMouseEvent;
				msg.msg.mouseevent.point.x = LOWORD(lParam);
				msg.msg.mouseevent.point.y = HIWORD(lParam);
				msg.msg.mouseevent.flags = MOUSEEVENTF_LEFTUP; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;

		case WM_MBUTTONDOWN:
			{
				msg.msgtype = toServerMouseEvent;
				msg.msg.mouseevent.point.x = LOWORD(lParam);
				msg.msg.mouseevent.point.y = HIWORD(lParam);
				msg.msg.mouseevent.flags = MOUSEEVENTF_MIDDLEDOWN; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;

		case WM_MBUTTONUP:
			{
				msg.msgtype= toServerMouseEvent;
				msg.msg.mouseevent.point.x = LOWORD(lParam);
				msg.msg.mouseevent.point.y = HIWORD(lParam);
				msg.msg.mouseevent.flags = MOUSEEVENTF_MIDDLEUP; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;

		case WM_RBUTTONDOWN:
			{
				msg.msgtype = toServerMouseEvent;
				msg.msg.mouseevent.point.x = LOWORD(lParam);
				msg.msg.mouseevent.point.y = HIWORD(lParam);
				msg.msg.mouseevent.flags = MOUSEEVENTF_RIGHTDOWN; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;

		case WM_RBUTTONUP:
			{
				msg.msgtype = toServerMouseEvent;
				msg.msg.mouseevent.point.x = LOWORD(lParam);
				msg.msg.mouseevent.point.y = HIWORD(lParam);
				msg.msg.mouseevent.flags = MOUSEEVENTF_RIGHTUP; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;



		case WM_MOUSEMOVE:
			{
				msg.msgtype = toServerMouseEvent;
				msg.msg.mouseevent.point.x = LOWORD(lParam);
				msg.msg.mouseevent.point.y = HIWORD(lParam);
				msg.msg.mouseevent.flags = MOUSEEVENTF_MOVE; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;

		case WM_KEYDOWN:
			{
				msg.msgtype = toServerKeyEvent;
				msg.msg.keyevent.code = (int) wParam;
				msg.msg.keyevent.flags = KEYEVENTF_EXTENDEDKEY; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;
		case WM_KEYUP:
			{
				msg.msgtype = toServerKeyEvent;
				msg.msg.keyevent.code = (int) wParam;
				msg.msg.keyevent.flags = KEYEVENTF_KEYUP; 
				eventsMutex.lock();
				eventsReady  = true;
				events.push_back(msg);
				eventsMutex.unlock();
			}
			break;


	case WM_CREATE:
		hDlg = CreateDialog(hInst, (LPCTSTR)IDD_DIALOG1, hWnd, (DLGPROC)SplashScreen);
		ShowWindow(hDlg, SW_SHOW);
		UpdateWindow(hDlg);

		SetTimer(hWnd, 1001, 1500, 0);
		break;
	case WM_TIMER:
		DestroyWindow(hDlg);
		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);
		SetFocus(hWnd);
		KillTimer(hWnd, 1001);
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam); 
		wmEvent = HIWORD(wParam); 
		// Parse the menu selections:
		switch (wmId)
		{
		case ID_FILE_CONNECT:
			if ( DialogBox(hInst, (LPCTSTR)IDD_CONNECT, hWnd, (DLGPROC)ConnectDlg) == IDOK )
			{
				appSettings.runThread();
			}
			break;
		case IDM_ABOUT:
			DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		//appSettings.
		if ( appSettings.paint )
			if ( appSettings.Display(hdc) != ERROR_SUCCESS )
				MessageBox(0, TEXT("Error"), TEXT("Error"), 0);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		if ( appSettings.isConnectThreadRunning() )
			appSettings.DestoryThread();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


LRESULT CALLBACK SplashScreen(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HBITMAP hBitmap;
	static HDC	   hDC;
	PAINTSTRUCT ps;
	HDC		hdc;

	switch (message)
	{
		case WM_INITDIALOG:
			{
				HDC dc = GetDC(hDlg);
				hDC = CreateCompatibleDC(dc);
				hBitmap = LoadBitmap(hInst, (LPCTSTR) IDB_SPLASHBITMAP);
				SelectObject(hDC, hBitmap);
				ReleaseDC(hDlg, dc);
				return TRUE;
			}
		case WM_PAINT:
			{
				hdc = BeginPaint(hDlg, &ps);
				RECT rect;
				GetClientRect(hDlg, &rect);
				StretchBlt(hdc, 0, 0, rect.right, rect.bottom, hDC, 0 , 0, 500, 300, SRCCOPY);
				EndPaint(hDlg, &ps);
				return TRUE;
			}
	}
	return FALSE;
}

LRESULT CALLBACK PassDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) 
		{
			char buffer[MAX_LOADSTRING];
			char output[MAX_LOADSTRING];
			int len = GetDlgItemText(hDlg, IDC_EDIT1, (LPSTR)&buffer, MAX_LOADSTRING);
			SHA1::crypt((unsigned char *)buffer, len, output);
			password = string(output);
			EndDialog(hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

LRESULT CALLBACK ConnectDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}
