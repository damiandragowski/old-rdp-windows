// RDServer.cpp : Defines the entry point for the application.
//
#include "stdafx.h"
#include "RDServer.h"
#include "resource.h"
#include "../RDLib/libheaders.h"
#include "../hook/hook.h"
#include "../RDCrypt/RDCrypt.h"
#include "../Compression/Compression.h"
#include <shellapi.h>
#include <string>
#include <vector>
using namespace std;

class ServSettings
{
private:
	bool			m_bPoolFullScreen;
	bool			m_bPoolConsole;
	bool			m_bBehindNAT;
	bool			m_bPasswordEnable;
	int				m_iPort;
	unsigned char	m_iCompression;
	unsigned char   m_iEncryption;
	string			m_pPassword;
	int				m_iPasswordTry;
public:
	char * const getPassword() const { return (char *)m_pPassword.c_str(); }
	bool isPoolConsole() const { return m_bPoolConsole; }
	bool isPoolFullScreen() const { return m_bPoolFullScreen; }
	bool isPasswordEnable() const { return m_bPasswordEnable; }
	unsigned char getCompression() const { return m_iCompression; }
	unsigned char getEncryption() const { return m_iEncryption; }
	int getPort() const { return m_iPort; }
	int isNAT() const { return (m_bBehindNAT)?1:0 ; }
	int getPasswordTry() const { return m_iPasswordTry; }
	void setPassword(string pass ) { m_pPassword = pass; m_bPasswordEnable = true; }

	ServSettings():m_bPoolConsole(false), m_bPoolFullScreen(false), m_bBehindNAT(false), m_bPasswordEnable(false),
		m_iPort(666), m_iCompression(ZIP_COMPRESSION), m_iEncryption(RAW_ENCRYPTION), m_iPasswordTry(3)
	{
		char * path[] = { "Software", "RDesktop", 0};
		RDRegister reg;
		reg.setBaseKey();
		if ( reg.openTree(path, true) ) {
			int i=0;
			if ( reg.readInt("Behind NAT", i) )
				m_bBehindNAT = (i==1);
			if ( reg.readInt("Pool Console", i) )
				m_bPoolConsole = (i==1);
			if ( reg.readInt("Pool FullScreen", i) )
				m_bPoolConsole = (i==1);
			
			char * temp=0;
			if ( reg.readString("Password", temp) ) {
				m_pPassword=temp;
				delete [] temp;
				m_bPasswordEnable = true;
			}
			if ( reg.readInt("Password Try", i) )
				m_iPasswordTry = i;
			if ( reg.readInt("Port", i) )
				m_iPort = i;
			if ( reg.readInt("Compression", i) )
				m_iCompression = i;
			if ( reg.readInt("Encryption", i) )
				m_iEncryption = i;
		}
		reg.closeTree();
	}
	~ServSettings() {
		char * path[] = { "Software", "RDesktop", 0};
		RDRegister reg;
		reg.setBaseKey();
		if ( reg.openTree(path, true) ) {
			reg.writeInt("Behind NAT", (m_bBehindNAT)?1:0);
			reg.writeInt("Pool Console", (m_bPoolConsole)?1:0);
			reg.writeInt("Pool FullScreen", (m_bPoolFullScreen)?1:0);
			reg.writeInt("Password Try", m_iPasswordTry);
			reg.writeInt("Port", m_iPort);
			reg.writeInt("Compression", m_iCompression);
			reg.writeInt("Encryption", m_iEncryption);
			if ( m_bPasswordEnable )
				reg.writeString("Password", (char *)m_pPassword.c_str());
		}
	}
};


ServSettings	settings;
HINSTANCE	hInst;
RDMutex		 clientUpdateMutex;
vector<Rect> clientUpdateRect;
bool		 clientupdateRectRequest;

DWORD WINAPI listenThreadProc(LPVOID lpParameter);

class ClientSettings
{
public:
	unsigned int	port;
	unsigned long	IP;
	bool			m_bBehindNAT;
	int				bitperpixel;
	int				m_iCompression;
	int				m_iEncryption;
};

class Region
{
	HRGN	mainRGN;
public:
	Region() {
		mainRGN = CreateRectRgn(0,0,0,0);
	}
	~Region() {
		DeleteObject(mainRGN);
	}
	bool Union(Rect & rect) 
	{
		HRGN hRGN = CreateRectRgn(rect.x1, rect.y1, rect.x2, rect.y2);
		if ( hRGN == 0 )
			return false;
		if ( CombineRgn(mainRGN, mainRGN, hRGN, RGN_OR) == ERROR )
			return false;
		DeleteObject(hRGN);
		return true;
	}
	bool Intersect(Rect & rect) {
		HRGN hRGN = CreateRectRgn(rect.x1, rect.y1, rect.x2, rect.y2);
		if ( hRGN == 0 )
			return false;
		if ( CombineRgn(mainRGN, mainRGN, hRGN, RGN_AND) == ERROR )
			return false;
		DeleteObject(hRGN);
		return true;
	}
	bool SetRegion(Rect & rect) {
		return SetRectRgn(mainRGN, rect.x1, rect.y1, rect.x2, rect.y2) != 0;
	}
	int getRectangles(vector<Rect> & rects)
	{
		DWORD buffsize = GetRegionData(mainRGN, 0, NULL);
		if (!buffsize)
			return 0;
		RECT result;
		if ( GetRgnBox(mainRGN, &result) == NULLREGION )
			return 0;

		unsigned char * buffer = new unsigned char[buffsize];
		if ( GetRegionData(mainRGN, buffsize, (LPRGNDATA)buffer) != buffsize )
			return 0;

		LPRGNDATA region_data = (LPRGNDATA)buffer;
		int nCount = region_data->rdh.nCount;

		long current_y = INT_MIN;
		long start_i=0, end_i=-1;
		for (long i=0; i<nCount; i++) {
			RECT ra = ((RECT*)&region_data->Buffer[0])[i];
			Rect rect;
			rect.x1 = (unsigned short)ra.left;
			rect.x2 = (unsigned short)ra.right;
			rect.y1 = (unsigned short)ra.top;
			rect.y2 = (unsigned short)ra.bottom;
			if (rect.y1 == current_y) {
				end_i = i;
			} else {
				for (long j=start_i; j<=end_i; j++) {
					RECT s = ((RECT*)&region_data->Buffer[0])[j];
					Rect r;
					r.x1 = (unsigned short)s.left;
					r.x2 = (unsigned short)s.right;
					r.y1 = (unsigned short)s.top;
					r.y2 = (unsigned short)s.bottom;
					rects.push_back(r);
				}
				start_i = i;
				end_i = i;
				current_y = rect.y1;
			}
		}
		for (long j=start_i; j<=end_i; j++) {
			RECT s = ((RECT*)&region_data->Buffer[0])[j];
			Rect r;
			r.x1 = (unsigned short)s.left;
			r.x2 = (unsigned short)s.right;
			r.y1 = (unsigned short)s.top;
			r.y2 = (unsigned short)s.bottom;
			rects.push_back(r);
		}
		return rects.size();
	}
};

class Server
{
public:
	RDThread	* listenThread;
	// threads
	RDThread	* hookThread;
	RDThread	* sendThread;
	RDThread	* readThread;

	bool		  connected;
	bool		  m_bClientIsActive;
	// listen sock
	DSocket		  sock;
	// client sock
	DSocket		* clientSocket;
	int			  port;
	ClientSettings cli;
	RDesktopCap	* desktop;
	BitmapInfo	* BI;
	RDMutex		  updateRectMutex;
	Region		  updateRect;
	bool		  updateReady;
public:
	Server():connected(true),m_bClientIsActive(false), clientSocket(0), desktop(0)
	{
		updateReady = false;
		//memset(&updateRect, 0, sizeof(Rect));
		listenThread = new RDThread(listenThreadProc, this);
	}
	~Server()
	{
		if ( m_bClientIsActive ) {
			hookThread->PostThreadMessage(WM_QUIT,0,0);
			hookThread->WaitForEnd(INFINITE);
			sendThread->Terminate();
			readThread->Terminate();
			delete sendThread;
			delete readThread;
			delete hookThread;
			delete desktop;
			delete clientSocket;
			m_bClientIsActive = false;
		}
		listenThread->Terminate();	
		delete listenThread;
	}
};

void Check(Rect & rect, unsigned short w, unsigned short h)
{
	if ( rect.x2 > w )
		rect.x2 = w;
	if ( rect.x1 > w )
		rect.x1 = w;
	if ( rect.y1 > h )
		rect.y1 = h;
	if ( rect.y2 > h)
		rect.y1 = h;
	if ( rect.x1 >= rect.x2 )
		rect.x1 = rect.x2;
	if ( rect.y1 >= rect.y2 )
		rect.y1 = rect.y2;
}

DWORD WINAPI hookThreadProc(LPVOID lpParameter)
{
	Server * server = (Server *) lpParameter;
	bool	update = true;
	BOOL	idle_skip = FALSE;
	ULONG	idle_skip_count = 0;
	MSG		msg;

	SetHooks(GetCurrentThreadId(), UpdatehookMessage);

	while ( update ) {
		if (!PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) {
			if (idle_skip) {
				idle_skip = FALSE;
				if (idle_skip_count++ < 4) {
					Sleep(5);
					continue;
				}
			}
			idle_skip_count = 0;

			// send to client update information
			server->updateRectMutex.lock();
			server->updateReady = true;
			server->updateRectMutex.unlock();



			if (!WaitMessage())
				break;
		} else if (msg.message == UpdatehookMessage) {
			Rect rect;
			rect.x1 = LOWORD(msg.wParam);
			rect.y1 = HIWORD(msg.wParam);
			rect.x2 = LOWORD(msg.lParam);
			rect.y2 = HIWORD(msg.lParam);
			Check(rect, server->BI->bminfo.bmiHeader.biWidth, server->BI->bminfo.bmiHeader.biHeight);
			server->updateRectMutex.lock();
			server->updateRect.Union(rect);
			server->updateRectMutex.unlock();
			idle_skip = true;
		} else if (msg.message == WM_QUIT) {
			update = false;
			break;
		} else {
			DispatchMessage(&msg);
			idle_skip = true;
		}
	}

	UnSetHooks(GetCurrentThreadId());
	return 0;
}

DWORD WINAPI sendThreadProc(LPVOID lpParameter)
{
	Server * server = ( Server * )lpParameter;
	DSocket	* Client = server->clientSocket;
	RecvMessage message;

	while ( server->m_bClientIsActive )
	{
		if ( server->desktop->m_bDisplayChange ) {
			// here we will disable all and send Bitmap info
			server->desktop->CaptureScreen();
			server->desktop->m_bDisplayChange = false;
			server->desktop->m_bPalleteChange = false;


			memset(&message, 0, sizeof(RecvMessage));
			message.msgtype = toServerScreenResolutionEvent;
			message.msg.msglen = sizeof(BitmapInfo);
			if( !Client->SendW((char *)&message, sizeof(RecvMessage)) )
				return 0;
			if( !Client->SendW((char *)server->BI, sizeof(BitmapInfo)) )
				return 0;

			// send full screen update
			memset(&message, 0, sizeof(RecvMessage));
			message.msgtype = toClientUpdateRect;
			message.msg.rect.x1 = 0;
			message.msg.rect.y1 = 0;
			message.msg.rect.x2 = server->BI->bminfo.bmiHeader.biWidth;
			message.msg.rect.y2 = server->BI->bminfo.bmiHeader.biHeight;
			if( !Client->SendW((char *)&message, sizeof(RecvMessage)) )
				return 0;
		}

		clientUpdateMutex.lock();
		if ( clientupdateRectRequest ) {
			clientupdateRectRequest = false;
			server->desktop->CaptureScreen();
			for(int i=0;i < clientUpdateRect.size(); ++i)
			{
				Rect rect = clientUpdateRect[i];
				char * buffTosend = 0;
				int len = 0;
				message.msgtype = toClientSendingRectBuffer;
				if ( server->cli.m_iCompression == RAW_COMPRESSION ) {
					// tu byl cpature screen
					buffTosend = (char *)server->desktop->getBuffer(rect, len);
					message.msg.msglen = len;
					if( !Client->SendW((char *)&message, sizeof(RecvMessage)) )
						return 0;
					if( !Client->SendW((char *)&rect, sizeof(Rect)) )
						return 0;
					if( !Client->SendW((char *)buffTosend, len) )
						return 0;
					buffTosend = 0;
				} else {
					buffTosend = (char *)server->desktop->getCompressedBuffer(rect, len);
					message.msg.msglen = len;
					if( !Client->SendW((char *)&message, sizeof(RecvMessage)) )
						return 0;
					if( !Client->SendW((char *)&rect, sizeof(Rect)) )
						return 0;
					if( !Client->SendW((char *)buffTosend, len) )
						return 0;
					buffTosend = 0;
				}
			}
			clientUpdateRect.clear();
			clientUpdateMutex.unlock();
		} else 
			clientUpdateMutex.unlock();



		server->updateRectMutex.lock();
		if ( server->updateReady ) {
			server->updateReady = false;
			vector<Rect> rects;
			rects.reserve(100);
			int ret = server->updateRect.getRectangles(rects);
			server->updateRectMutex.unlock();
			if ( ret != 0 ) {
				for ( int i=0;i < rects.size(); ++i) {
					message.msgtype = toClientUpdateRect;
					message.msg.rect = rects[i];
					if( !Client->SendW((char *)&message, sizeof(RecvMessage)) )
						return 0;
				}
			}
		} else
			server->updateRectMutex.unlock();

	}

	return 0;
}


DWORD WINAPI readThreadProc(LPVOID lpParameter)
{
	Server * server = (Server *)lpParameter;
	RecvMessage msg;
	Point      mouse;
	DSocket   * client = server->clientSocket;

	//POINT	   p;
	//GetCursorPos(&p);
	//mouse.x = p.x;
	//mouse.y = p.y;

	while ( server->m_bClientIsActive )
	{
		memset(&msg, 0, sizeof(RecvMessage));
		if ( !client->ReadW((char *)&msg, sizeof(RecvMessage)) )
			return 0;
		switch(msg.msgtype)
		{
			case toServerKeyEvent:
				{
					INPUT input[1];
					::ZeroMemory(input, sizeof(input));        
					input[0].type = INPUT_KEYBOARD;
					input[0].ki.wVk = msg.msg.keyevent.code;        
					input[0].ki.dwFlags = msg.msg.keyevent.flags;
					::SendInput(1, input, sizeof(INPUT));
				}
				break;
			case toServerMouseEvent:
				if ( msg.msg.mouseevent.flags==MOUSEEVENTF_MOVE ) {
					mouse.x = msg.msg.mouseevent.point.x;
					mouse.y = msg.msg.mouseevent.point.y;
					SetCursorPos(msg.msg.mouseevent.point.x, msg.msg.mouseevent.point.y);
				} else
					::mouse_event(msg.msg.mouseevent.flags, mouse.x-msg.msg.mouseevent.point.x, mouse.x-msg.msg.mouseevent.point.y, 0, 0);
				break;
			case toServerRectEvent:
				clientUpdateMutex.lock();
				clientUpdateRect.push_back(msg.msg.rect);
				clientupdateRectRequest = true;
				clientUpdateMutex.unlock();
				break;
		};
	}
	return 0;
}



bool Negotiation(DSocket * client, Server * server)
{
	Message	   headerMSG;
	WelcomeMsg msg;

	headerMSG.msgtype = Welcome;
	headerMSG.msglen = sizeof(WelcomeMsg);
	if ( !client->SendW((char *)&headerMSG, sizeof(Message)) ) {
		logger.AddString(TEXT("Error in sending Welcome Message\n"));
		return false;
	}
	msg.IP = get_my_IP();
	msg.isNat = settings.isNAT();
	msg.Port = server->port;
	msg.Compression = settings.getCompression(); // it is not importnat because only client decide
	msg.Encryption = settings.getEncryption();  // it is not importnat because only client decide

	if ( !client->SendW((char *)&msg, sizeof(WelcomeMsg)) ) {
		logger.AddString(TEXT("Error in sending Welcome Message\n"));
		return false;
	}

	if ( !client->ReadW((char *)&headerMSG, sizeof(Message)) ) {
		logger.AddString(TEXT("Client didn;t respond properly\n"));
		return false;
	}
	if ( headerMSG.msgtype != Welcome ) {
		logger.AddString(TEXT("Somethink wrong with client"));
		return false;
	}
	if ( !client->ReadW((char *)&msg, sizeof(WelcomeMsg)) ) {
		logger.AddString(TEXT("Client didn;t respond properly\n"));
		return false;
	}

	server->cli.IP = msg.IP;
	server->cli.port = msg.Port;
	server->cli.m_bBehindNAT = (msg.isNat==1);
	server->cli.m_iCompression = msg.Compression;
	server->cli.m_iEncryption = msg.Encryption;

	register int i=0;
	PasswordMsg pass;

	for ( ;i < settings.getPasswordTry(); ++i ) {
		headerMSG.msgtype = Password;
		headerMSG.msglen = sizeof(PasswordMsg);
		if ( !client->ReadW((char *)&headerMSG, sizeof(Message)) ) {
			logger.AddString(TEXT("Client didn;t respond properly,should send password\n"));
			return false;
		}
		if ( !client->ReadW((char *)pass.password, sizeof(PasswordMsg)) ) {
			logger.AddString(TEXT("Client didn;t respond properly,sending password error\n"));
			return false;
		}
		if ( strcmp(settings.getPassword(), pass.password) == 0 ) {
			headerMSG.msgtype = GoodPassword;
			headerMSG.msglen = 0;
			if ( !client->SendW((char *)&headerMSG, sizeof(Message)) ) {
				logger.AddString(TEXT("Error in sending GoodPassword Message\n"));
				return false;
			}
			if ( !client->SendW((char *)&headerMSG, sizeof(Message)) ) {
				logger.AddString(TEXT("Error in sending GoodPassword Message\n"));
				return false;
			}
			return true;
		} 
		headerMSG.msgtype = WrongPassword;
		headerMSG.msglen = 0;
		if ( !client->SendW((char *)&headerMSG, sizeof(Message)) ) {
			logger.AddString(TEXT("Error in sending WrondPassword Message\n"));
			return false;
		}
		if ( i != settings.getPasswordTry() ) {
			headerMSG.msgtype = RetryPassword;
			headerMSG.msglen = 0;
			if ( !client->SendW((char *)&headerMSG, sizeof(Message)) ) {
				logger.AddString(TEXT("Error in sending RetryPassword Message\n"));
				return false;
			}
		}
	}
	if ( i == settings.getPasswordTry() ) {
		headerMSG.msgtype = CloseConnection;
		headerMSG.msglen = 0;
		client->SendW((char *)&headerMSG, sizeof(Message));
		// close connection
		logger.AddString(TEXT("Sending close connection to client\n"));
		logger.AddString(TEXT("Wrong password after all retries\n"));
		return false;
	}
	return true;
}


DWORD WINAPI listenThreadProc(LPVOID lpParameter)
{
	Server * server = (Server *) lpParameter;
	if ( !server->sock.Create() ) {
		server->connected = false;
		logger.AddString(TEXT("Couldn't Create socket in thread"));
		return -1;
	}
		
	server->port = settings.getPort();
	if ( !server->sock.Bind(server->port) ) {
		server->connected = false;
		logger.AddString(TEXT("Couldn't Bind port in thread"));
		return -1;
	}

	if ( !server->sock.Listen() ) {
		server->connected = false;
		logger.AddString(TEXT("Couldn't listen in thread"));
		return -1;
	}

	while ( server->connected ) {
		DSocket * client = server->sock.Accept();
		if ( client != 0 ) {
			if ( server->m_bClientIsActive ) 
			{
				// here send message that we have already client
				Message msg;
				msg.msgtype = MoreConnection;
				msg.msglen = 0;
				if ( !client->SendW((char *)&msg, sizeof(Message)) ) {
					client->Close();
					delete client;
					logger.AddString(TEXT("Error while sending moreconnection message"));
				} else {
					msg.msgtype = CloseConnection;
					msg.msglen = 0;
					if ( !client->SendW((char *)&msg, sizeof(Message)) ) {
						client->Close();
						delete client;
						logger.AddString(TEXT("Error while sending CloseConnection message"));
					}
				}
			}
			else {
				server->clientSocket = client;
				server->m_bClientIsActive = true;
				// connection negociation
				if ( Negotiation(client, server) ) {
					server->desktop = new RDesktopCap();
					server->desktop->Initialize(hInst);
					server->BI = &server->desktop->getBitmapInfo();
					server->sendThread = new RDThread(sendThreadProc, server);
					server->readThread = new RDThread(readThreadProc, server);
					server->hookThread = new RDThread(hookThreadProc, server);

				} else {
					server->m_bClientIsActive = false;
					client->Close();
					delete client;
					server->clientSocket = 0;
					logger.AddString(TEXT("Negtiation error\n"));
				}
			}
		} else {		
			logger.AddString(TEXT("Accept Error\n"));
		}
	}
	server->sock.Close();
	return 0;
}



class AppSettings
{
public:
	AppSettings():server(0), m_bListenMode(false)
	{
	}
	~AppSettings()
	{
		if ( m_bListenMode )
			if ( server )
				delete server;
	}
	Server	   * server;
	bool	   m_bListenMode;
};

class ServerWindow
{
	HINSTANCE	m_hInstance;
	WNDCLASSEX	m_cWndClass;
	HICON		m_hIcon;
	HMENU		m_hMenu;
	NOTIFYICONDATA	m_nid;
public:
	HWND		m_hWnd;
	ServerWindow(HINSTANCE hInst);
	~ServerWindow(void);
	void SendTrayMsg(DWORD msg);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
};





TCHAR		AppName[MAX_LOADSTRING];					
HWND		hWndMain;
AppSettings app;


int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	MSG msg;
	HACCEL hAccelTable;
	hInst = hInstance;
	logger.AddString(TEXT("Application Entry point\n"));
	LoadString(hInstance, IDC_APP_NAME, AppName, MAX_LOADSTRING);
	OneInstance inst(AppName);

	if ( !inst.OneHandler() ) {
		logger.AddString(TEXT("There is already one instance of application\n"));
		MessageBox(0, TEXT("Sorry there can be only one handler of this application"), TEXT("Error information"), 0);
		return -1;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_RDSERVER);
	SetProcessShutdownParameters(0x100, 0);
	
	// Here We Create Window/Tray

	ServerWindow window(hInstance);

	logger.AddString(TEXT("Entering to main message loop\n"));
	while (GetMessage(&msg, 0, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	logger.AddString(TEXT("Quit Application\n"));
	return (int) msg.wParam;
}


ServerWindow::ServerWindow(HINSTANCE hInstance)
{
	TCHAR szWindowClass[MAX_LOADSTRING];
	LoadString(hInstance, IDC_RDSERVER, szWindowClass, MAX_LOADSTRING);
	m_hInstance = hInstance;
	m_cWndClass.cbSize			= sizeof(m_cWndClass);
	m_cWndClass.style			= 0;
	m_cWndClass.lpfnWndProc		= ServerWindow::WndProc;
	m_cWndClass.cbClsExtra		= 0;
	m_cWndClass.cbWndExtra		= 0;
	m_cWndClass.hInstance		= m_hInstance;
	m_cWndClass.hIcon			= LoadIcon(0, (LPCTSTR)IDI_RDSERVER);
	m_cWndClass.hCursor			= LoadCursor(0, (LPCTSTR)IDC_ARROW);
	m_cWndClass.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
	m_cWndClass.lpszMenuName	= (const char *) 0;
	m_cWndClass.lpszClassName	= szWindowClass;
	m_cWndClass.hIconSm			= LoadIcon(NULL, (LPCTSTR)IDI_RDSERVER);

	if ( RegisterClassEx(&m_cWndClass) == 0 ) {
		logger.AddString(TEXT("Registering class error\n"));
		PostQuitMessage(0);
		return;
	}

	m_hWnd = CreateWindow(szWindowClass, szWindowClass, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 	CW_USEDEFAULT,	400, 400, 0, 0, hInstance, 0);
	if ( m_hWnd == 0 ) {
		logger.AddString(TEXT("Create window error\n"));
		PostQuitMessage(0);
		return;
	}
	SetWindowLong(m_hWnd, GWL_USERDATA, (long)(this));	
	m_hMenu = LoadMenu(hInstance, (LPCTSTR)IDM_MENU);
	m_hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
	SendTrayMsg(NIM_ADD);
}

ServerWindow::~ServerWindow(void)
{
	SendTrayMsg(NIM_DELETE);
}

void ServerWindow::SendTrayMsg(DWORD msg)
{
	m_nid.hWnd = m_hWnd;
	m_nid.cbSize = sizeof(m_nid);
	m_nid.uID = IDI_SMALL;
	m_nid.hIcon = m_hIcon;
	m_nid.uFlags = NIF_ICON | NIF_MESSAGE;
	m_nid.uCallbackMessage = WM_USER+3;

	if (LoadString(m_hInstance, IDC_TIP, m_nid.szTip, sizeof(m_nid.szTip)))
	{
	    m_nid.uFlags |= NIF_TIP;
	}
	if ( Shell_NotifyIcon(msg, &m_nid) == FALSE )
		logger.AddString(TEXT("can notify shell"));
}

LRESULT CALLBACK	SettingDlg(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK ServerWindow::WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	ServerWindow * _this = (ServerWindow *) GetWindowLong(hWnd, GWL_USERDATA);
	switch (iMsg)
	{
		case WM_CREATE:
			return 0;
		

		case WM_USER+3:
		{
			HMENU menu = GetSubMenu(_this->m_hMenu, 0);
			POINT mouse;
			GetCursorPos(&mouse);
			if (lParam == WM_RBUTTONUP)
			{
				if (menu == 0) { 
					logger.AddString(TEXT("Can load Menu"));
					return 0;
				}
				SetMenuDefaultItem(menu, 0, TRUE);
				SetForegroundWindow(_this->m_nid.hWnd);
				TrackPopupMenu(menu, 0, mouse.x, mouse.y, 0, _this->m_nid.hWnd, 0);
			}
			return 0;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_EMPTY_LISTENFORCLIENT:
					if ( settings.isPasswordEnable() ) {
						app.server = new Server();
						if ( app.server )
							app.m_bListenMode = true;
					} else {
						MessageBox(0, "You have to set password first.\nGo to settings menu", "information", 0);
					}
					break;
				case ID_EMPTY_CLOSEREMOTEDESKTOP:
					PostMessage(hWnd, WM_CLOSE, 0, 0);
					break;
				case ID_EMPTY_SETTINGS:
					DialogBox(hInst, (LPCTSTR)IDD_PROPERT, hWnd, (DLGPROC)SettingDlg);
					break;
			};
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;


	};
	return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

LRESULT CALLBACK SettingDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDCANCEL:
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			case IDOK:
			{
				TCHAR password[MAX_LOADSTRING];
				char output[MAX_LOADSTRING];
				int len = GetDlgItemText(hDlg, IDC_EDIT1, password, MAX_LOADSTRING);
				if ( len == 0 ) {
					MessageBox(hDlg, TEXT("Password must be set"), TEXT("error"), 0);
					return TRUE;
				}

				SHA1::crypt((unsigned char *)password, len, (char *)output);
				settings.setPassword(output);
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			};
			break;
		}
	}
	return FALSE;
}
