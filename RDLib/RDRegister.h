#ifndef RDREGISTER_H
#define RDREGISTER_H
#include "stdafx.h"
#pragma once
#include <string>
using namespace std;

class RDRegister
{
private:
	HKEY hKey;
	HKEY base;
public:
	RDRegister(void);
	virtual ~RDRegister(void);
	void closeTree() 
	{
		RegCloseKey(hKey);
	}
	void setBaseKey(HKEY Key = HKEY_CURRENT_USER);
	bool openTree(char ** path, bool create)
	{
		HKEY key = base;
		DWORD flags = KEY_READ | KEY_WRITE;
		if ( create ) 
			flags |= KEY_CREATE_SUB_KEY;
		for(int i=0;path[i];++i) {
			HKEY newkey;
			DWORD ret;
			DWORD dw;
			if (create) {
				ret = RegCreateKeyEx(key, path[i], 0, REG_NONE,REG_OPTION_NON_VOLATILE, flags, 0, &newkey, &dw);
			} else {
				ret = RegOpenKeyEx(key, path[i], 0, flags, &newkey);
			}
			if (key && (key != base)) 
				RegCloseKey(key);
			key = newkey;
			if (ret != ERROR_SUCCESS) {
				return false;
			} 			
		}
		hKey = key;
		return true;
	}

	bool writeString(const char * key, const char * const text)
	{
		return (RegSetValueEx(hKey, key, 0,	REG_SZ, (unsigned char *)text, (DWORD)strlen(text)+1) == ERROR_SUCCESS);
	}

	bool readString(const char * key, char *& text)
	{
		DWORD type;
		DWORD size = sizeof(DWORD);

		if (RegQueryValueEx(hKey, key, 0, &type, (unsigned char *)0, &size) == ERROR_SUCCESS)
		{
			text = new char[size+1];
			if (RegQueryValueEx(hKey, key, 0, &type, (unsigned char *)text, &size) == ERROR_SUCCESS)
			{
				if (type == REG_SZ)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool writeInt(const char * key, int i)
	{
		return (RegSetValueEx(hKey, key, 0,	REG_DWORD, (unsigned char *)&i,	(DWORD)sizeof(int)) == ERROR_SUCCESS);
	}
	bool readInt(const char * key, int & value)
	{
		DWORD type;
		DWORD size = sizeof(DWORD);

		if (RegQueryValueEx(hKey, key, 0, &type, (unsigned char *)&value, &size) == ERROR_SUCCESS)
		{
			if (type == REG_DWORD)
			{
				return true;
			}
		}
		return false;
	}
	
};

#endif