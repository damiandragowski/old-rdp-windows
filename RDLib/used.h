#ifndef DUSED__H
#define DUSED__H

#include "stdafx.h"

void KeyState(BYTE key);
BOOL CALLBACK KillScreenSaverFunc(HWND hwnd, LPARAM lParam);
void KillScreenSaver();
extern const TCHAR appname [];
class OneInstance
{
	HANDLE mutex;
	TCHAR * appname;
	bool del;
public:
	OneInstance(const TCHAR * _appname) {
		appname = _tcsdup(_appname);
	}
	bool OneHandler();
	virtual ~OneInstance() {
		if ( del )
			CloseHandle(mutex);
	}
};

void GetIPAddrString(char *buffer, int buflen);
unsigned long get_my_IP();

#endif