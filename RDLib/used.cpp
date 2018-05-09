#include "used.h"

void KeyState(BYTE key)
{
	BYTE keyState[256];
	
	GetKeyboardState((LPBYTE)&keyState);

	if(keyState[key] & 1)
	{
		keybd_event(key, 0, KEYEVENTF_EXTENDEDKEY, 0);
		keybd_event(key, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
	}
}

BOOL CALLBACK KillScreenSaverFunc(HWND hwnd, LPARAM lParam)
{
	char buffer[256];

	if ((GetClassName(hwnd, buffer, 256) != 0) && (strcmp(buffer, "WindowsScreenSaverClass") == 0))
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	return TRUE;
}

void KillScreenSaver()
{
	OSVERSIONINFO osversioninfo;
	osversioninfo.dwOSVersionInfoSize = sizeof(osversioninfo);

	if (!GetVersionEx(&osversioninfo))
		return;


	switch (osversioninfo.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_WINDOWS:
		{
			HWND hsswnd = FindWindow ("WindowsScreenSaverClass", NULL);
			if (hsswnd != NULL)
				PostMessage(hsswnd, WM_CLOSE, 0, 0); 
			break;
		} 
	case VER_PLATFORM_WIN32_NT:
		{
			HDESK hDesk = OpenDesktop("Screen-saver", 0, FALSE,	DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS);
			if (hDesk != NULL)
			{
				EnumDesktopWindows(hDesk, (WNDENUMPROC) &KillScreenSaverFunc, 0);
				CloseDesktop(hDesk);
				SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE, 0, SPIF_SENDWININICHANGE); 
			}
			break;
		}
	}
}

const TCHAR appname [] = "RDesktop";

bool OneInstance::OneHandler()
{
	mutex = CreateMutex(NULL, FALSE, appname);
	if (mutex == NULL) {
		del = false;
		return false;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		del = false;
		return false;
	}
	del = true;
	return true;
}

void GetIPAddrString(char *buffer, int buflen) {
    char namebuf[256];

    if (gethostname(namebuf, 256) != 0) {
		strncpy(buffer, "Host name unavailable", buflen);
		return;
    };

    HOSTENT *ph = gethostbyname(namebuf);
    if (!ph) {
		strncpy(buffer, "IP address unavailable", buflen);
		return;
    };

    *buffer = '\0';
    char digtxt[5];
    for (int i = 0; ph->h_addr_list[i]; i++) {
    	for (int j = 0; j < ph->h_length; j++) {
			sprintf(digtxt, "%d.", (unsigned char) ph->h_addr_list[i][j]);
			strncat(buffer, digtxt, (buflen-1)-strlen(buffer));
		}	
		buffer[strlen(buffer)-1] = '\0';
		if (ph->h_addr_list[i+1] != 0)
			strncat(buffer, ", ", (buflen-1)-strlen(buffer));
    }
}

	/***
	  *
	  *	get IP in netword order
	  *
	 ***/
unsigned long get_my_IP() {
	char name[256];
	struct hostent * hostent_ptr;
	int ret;
	ret = gethostname (name, 80);
	if(ret == -1) {
		return 0;
	}
	hostent_ptr = gethostbyname(name);
	if(hostent_ptr == NULL)
	{
		return 0;
	}
	return ((struct in_addr *)hostent_ptr->h_addr_list[0])->s_addr; 
}
