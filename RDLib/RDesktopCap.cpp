#include "StdAfx.h"
#include ".\rdesktopcap.h"

LRESULT CALLBACK RDesktopCap::WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	RDesktopCap * const _this = (RDesktopCap * const) GetWindowLong(hWnd, GWL_USERDATA);
	switch (iMsg)
	{
		case WM_DISPLAYCHANGE:
			_this->m_bDisplayChange = true;
			return 0;

		case WM_SYSCOLORCHANGE:
		case WM_PALETTECHANGED:
			_this->m_bPalleteChange = true;
			return 0;
		default:
			return DefWindowProc(hWnd, iMsg, wParam, lParam);
	}
}