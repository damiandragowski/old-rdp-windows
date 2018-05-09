// Hooks.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "Hook.h"



#pragma data_seg("Shared")
UINT  UpdateMsg = 0;
HHOOK hCallWndHook = 0;
HHOOK hGetMsgHook = 0;
HHOOK hDialogMsgHook = 0;
DWORD thread_id = 0;
#pragma data_seg()

#pragma comment(linker,"/SECTION:Shared,RWS") 

bool HookEnable = false;
bool prf_use_GetUpdateRect = false;
bool prf_use_Timer = true;
bool prf_use_KeyPress = true;
bool prf_use_LButtonUp = true;
bool prf_use_MButtonUp = false;
bool prf_use_RButtonUp = false;
bool prf_use_Deferral = false;

HINSTANCE hInstance = NULL;

const UINT DEFERRED_UPDATE = RegisterWindowMessage("Hooks.Deferred.UpdateMessage");


BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			hInstance = ( HINSTANCE ) hModule;
			// init settings
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			// save settings
			break;
    }
    return TRUE;
}

inline bool GetAbsoluteClientRect(HWND hwnd, RECT *rect)
{
	POINT topleft;
	topleft.x = 0;
	topleft.y = 0;

	if (!GetClientRect(hwnd, rect))
		return false;

	if (!ClientToScreen(hwnd, &topleft))
		return false;

	rect->left += topleft.x;
	rect->top += topleft.y;
	rect->right += topleft.x;
	rect->bottom += topleft.y;

	return true;
}


inline void SendUpdateRect(short x, short y, short x2, short y2)
{
	WPARAM vwParam;
	LPARAM vlParam;

	vwParam = MAKELONG(x, y);
	vlParam = MAKELONG(x2, y2);


	if (!PostThreadMessage(thread_id, UpdateMsg, vwParam, vlParam))
		thread_id = 0;
}


inline void SendWindowRect(HWND hWnd)
{
	RECT wrect;

	if (IsWindowVisible(hWnd) && GetWindowRect(hWnd, &wrect))
		SendUpdateRect((short) wrect.left, (short) wrect.top, (short) wrect.right, (short) wrect.bottom);
}


inline void SendDeferredUpdateRect(HWND hWnd, short x, short y, short x2, short y2)
{
	WPARAM vwParam;
	LPARAM vlParam;

	vwParam = MAKELONG(x, y);
	vlParam = MAKELONG(x2, y2);

	if (prf_use_Deferral) {
		PostMessage(hWnd, DEFERRED_UPDATE, vwParam, vlParam);
	} else {
		if (!PostThreadMessage(thread_id, UpdateMsg, vwParam, vlParam))
			thread_id = 0;
	}
}

inline void SendDeferredWindowRect(HWND hWnd)
{
	RECT wrect;

	if (IsWindowVisible(hWnd) && GetWindowRect(hWnd, &wrect))
		SendDeferredUpdateRect(hWnd, (short) wrect.left, (short) wrect.top, (short) wrect.right, (short) wrect.bottom);
}

inline void SendDeferredBorderRect(HWND hWnd)
{
	RECT wrect;
	RECT crect;

	if (IsWindowVisible(hWnd) && GetWindowRect(hWnd, &wrect))
		if (GetAbsoluteClientRect(hWnd, &crect))
		{
			SendDeferredUpdateRect(hWnd, (short) wrect.left, (short) wrect.top, (short) wrect.right, (short) crect.top);
			SendDeferredUpdateRect(hWnd, (short) wrect.left, (short) wrect.top, (short) crect.left, (short) wrect.bottom);
			SendDeferredUpdateRect(hWnd, (short) wrect.left, (short) crect.bottom, (short) wrect.right, (short) wrect.bottom);
			SendDeferredUpdateRect(hWnd, (short) crect.right, (short) wrect.top, (short) wrect.right, (short) wrect.bottom);
		}
}


inline bool ProcessMessage(UINT MessageId, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (MessageId == DEFERRED_UPDATE)
	{
		if (!PostThreadMessage(thread_id, UpdateMsg, wParam, lParam))
			thread_id = 0;
		return false;
	}

	switch (MessageId)
	{
		case WM_NCPAINT:
		case WM_NCACTIVATE:
			SendDeferredBorderRect(hWnd);
			break;
		case WM_CHAR:
		case WM_KEYUP:							
			if (prf_use_KeyPress)
				SendDeferredWindowRect(hWnd);
			break;
		case WM_LBUTTONUP:						
			if (prf_use_LButtonUp)
				SendDeferredWindowRect(hWnd);
			break;
		case WM_MBUTTONUP:						
			if (prf_use_MButtonUp)
				SendDeferredWindowRect(hWnd);
			break;
		case WM_RBUTTONUP:						
			if (prf_use_RButtonUp)
				SendDeferredWindowRect(hWnd);
			break;
		case WM_TIMER:
			if (prf_use_Timer)
				SendDeferredWindowRect(hWnd);
			break;
		case WM_HSCROLL:
		case WM_VSCROLL:
			if (((int) LOWORD(wParam) == SB_THUMBTRACK) || ((int) LOWORD(wParam) == SB_ENDSCROLL))
				SendDeferredWindowRect(hWnd);
			break;
		case WM_SYSCOLORCHANGE:
		case WM_PALETTECHANGED:
		case WM_SETTEXT:
		case WM_ENABLE:
		case BM_SETCHECK:
		case BM_SETSTATE:
		case EM_SETSEL:
			SendDeferredWindowRect(hWnd);
			break;
		case WM_PAINT:
			if (prf_use_GetUpdateRect)
			{
				HRGN region;
				region = CreateRectRgn(0, 0, 0, 0);

				if (GetUpdateRgn(hWnd, region, false) != ERROR)
				{
					int buffsize;
					UINT x;
					RGNDATA * buff;
					POINT TopLeft;

					TopLeft.x = 0;
					TopLeft.y = 0;
					if (!ClientToScreen(hWnd, &TopLeft))
						break;

					buffsize = GetRegionData(region, 0, 0);
					if (buffsize != 0)
					{
						buff = (RGNDATA *) new BYTE [buffsize];
						if (buff == NULL)
							break;

						if(GetRegionData(region, buffsize, buff))
						{
							for (x=0; x<(buff->rdh.nCount); x++)
							{
								RECT *urect = (RECT *) (((BYTE *) buff) + sizeof(RGNDATAHEADER) + (x * sizeof(RECT)));
								SendDeferredUpdateRect(
									hWnd,
									(short) (TopLeft.x + urect->left),
									(short) (TopLeft.y + urect->top),
									(short) (TopLeft.x + urect->right),
									(short) (TopLeft.y + urect->bottom)
									);
							}
						}

						delete [] buff;
					}
				}

				if (region != NULL)
					DeleteObject(region);
			}
			else
				SendDeferredWindowRect(hWnd);
			break;
		case WM_WINDOWPOSCHANGING:
			if (IsWindowVisible(hWnd))
				SendWindowRect(hWnd);
			break;
		case WM_WINDOWPOSCHANGED:
			if (IsWindowVisible(hWnd))
				SendDeferredWindowRect(hWnd);
			break;
		case WM_DESTROY:
				SendWindowRect(hWnd);
			break;
	}

	return true;
}

LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		if (thread_id)
		{
			CWPSTRUCT *cwpStruct = (CWPSTRUCT *) lParam;
			ProcessMessage(cwpStruct->message, cwpStruct->hwnd, cwpStruct->wParam, cwpStruct->lParam);
		}
	}
	return CallNextHookEx (hCallWndHook, nCode, wParam, lParam);
}

LRESULT CALLBACK GetMessageProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		if (thread_id)
		{
			MSG *msg = (MSG *) lParam;

			if (wParam & PM_REMOVE)
			{
				ProcessMessage(msg->message, msg->hwnd, msg->wParam, msg->lParam);
			}
		}
	}
	return CallNextHookEx (hGetMsgHook, nCode, wParam, lParam);
}

LRESULT CALLBACK DialogMessageProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		if (thread_id)
		{
			MSG *msg = (MSG *) lParam;

			ProcessMessage(msg->message, msg->hwnd, msg->wParam, msg->lParam);
		}
	}
	return CallNextHookEx (hGetMsgHook, nCode, wParam, lParam);
}

HOOK_API bool SetHooks(DWORD id, UINT UpdateMessage)
{
	if (!id)
		return false;

	if (thread_id)
		return false;

	hCallWndHook = SetWindowsHookEx(WH_CALLWNDPROC,	(HOOKPROC) CallWndProc,	hInstance, 0L);
	hGetMsgHook = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC) GetMessageProc, hInstance, 0L);
	hDialogMsgHook = SetWindowsHookEx(WH_SYSMSGFILTER, (HOOKPROC) DialogMessageProc, hInstance, 0L);

	if ((hCallWndHook != 0) && (hGetMsgHook != 0) && (hDialogMsgHook != 0))	{
		thread_id = id;			
		UpdateMsg = UpdateMessage;
		HookEnable = true;
		return true;
	} else {
		if (hCallWndHook != 0)
			UnhookWindowsHookEx(hCallWndHook);
		if (hGetMsgHook != 0)
			UnhookWindowsHookEx(hGetMsgHook);
		if (hDialogMsgHook != 0)
			UnhookWindowsHookEx(hDialogMsgHook);
		hCallWndHook = 0;
		hGetMsgHook = 0;
		hDialogMsgHook = 0;
		UpdateMsg = 0;
	}
	return false;
}

HOOK_API bool UnSetHooks(DWORD id)
{
	bool success = true;

	if (id != thread_id)
		return false;

	if (hCallWndHook != 0) {
		success &= (UnhookWindowsHookEx(hCallWndHook) > 0);
		hCallWndHook = 0;
	}

	if (hGetMsgHook != 0) {
		success &= (UnhookWindowsHookEx(hGetMsgHook) > 0);
		hGetMsgHook = 0;
	}

	if (hDialogMsgHook != 0) {
		success &= (UnhookWindowsHookEx(hDialogMsgHook) > 0);
		hDialogMsgHook = 0;
	}

	if (success) {
		thread_id = 0;
		HookEnable = false;
	}
	return success;
}
