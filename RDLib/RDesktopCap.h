#ifndef RDESKTOPCAP_H
#define RDESKTOPCAP_H
#include "stdafx.h"
#include "logger.h"
#include "libheaders.h"
#include "../Compression/Compression.h"
#pragma once

class RDesktopCapDX
{
private:
	IDirect3D8*			g_pD3D;
	IDirect3DDevice8*	g_pd3dDevice;
	IDirect3DSurface8*	g_pSurface;
	HDC					hBackDC;
	LPVOID				pBits;
	HBITMAP				hBackBitmap;
	BITMAPINFO			bmpInfo;
	RECT				gScreenRect;

public:
	RDesktopCapDX() { }
	~RDesktopCapDX() { Cleanup(); }
	void Cleanup()
	{
		if(g_pSurface)
		{
			g_pSurface->Release();
			g_pSurface=NULL;
		}
		if(g_pd3dDevice)
		{
			g_pd3dDevice->Release();
			g_pd3dDevice=NULL;
		}
		if(g_pD3D)
		{
			g_pD3D->Release();
			g_pD3D=NULL;
		}
	}

	int Initialize(HWND hWnd)
	{
		ZeroMemory(&bmpInfo,sizeof(BITMAPINFO));
		bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
		bmpInfo.bmiHeader.biBitCount=32;
		bmpInfo.bmiHeader.biCompression = 0;
		bmpInfo.bmiHeader.biWidth=GetSystemMetrics(SM_CXSCREEN);
		bmpInfo.bmiHeader.biHeight=GetSystemMetrics(SM_CYSCREEN);
		bmpInfo.bmiHeader.biPlanes=1;
		bmpInfo.bmiHeader.biSizeImage=abs(bmpInfo.bmiHeader.biHeight)*bmpInfo.bmiHeader.biWidth*bmpInfo.bmiHeader.biBitCount/8;

		HDC	hdc=GetDC(0);
		hBackDC=CreateCompatibleDC(hdc);
		pBits = 0;
		hBackBitmap=CreateDIBSection(hdc,&bmpInfo,DIB_RGB_COLORS,&pBits,NULL,0);
		if(FAILED(InitD3D(hWnd)))	
			return -1;
		return 0;
	}
	HRESULT	InitD3D(HWND hWnd)
	{
		D3DDISPLAYMODE	ddm;
		D3DPRESENT_PARAMETERS	d3dpp;

		if((g_pD3D=Direct3DCreate8(D3D_SDK_VERSION))==NULL)
		{
			return E_FAIL;
		}

		if(FAILED(g_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&ddm)))
		{
			return E_FAIL;
		}

		ZeroMemory(&d3dpp,sizeof(D3DPRESENT_PARAMETERS));

		d3dpp.Windowed=true;
		d3dpp.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
		d3dpp.BackBufferFormat=ddm.Format;
		d3dpp.BackBufferHeight=gScreenRect.bottom =ddm.Height;
		d3dpp.BackBufferWidth=gScreenRect.right =ddm.Width;
		d3dpp.MultiSampleType=D3DMULTISAMPLE_NONE;
		d3dpp.SwapEffect=D3DSWAPEFFECT_DISCARD;
		d3dpp.hDeviceWindow=hWnd;
		d3dpp.FullScreen_PresentationInterval=D3DPRESENT_INTERVAL_DEFAULT;
		d3dpp.FullScreen_RefreshRateInHz=D3DPRESENT_RATE_DEFAULT;

		if(FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,hWnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING ,&d3dpp,&g_pd3dDevice)))
		{
			return E_FAIL;
		}

		if(FAILED(g_pd3dDevice->CreateImageSurface(ddm.Width,ddm.Height,D3DFMT_A8R8G8B8,&g_pSurface)))
		{
			return E_FAIL;
		}

		return S_OK;
	}

	int CaptureScreen()
	{
			g_pd3dDevice->GetFrontBuffer(g_pSurface);			
			D3DLOCKED_RECT	lockedRect;
			if(FAILED(g_pSurface->LockRect(&lockedRect,&gScreenRect,D3DLOCK_NO_DIRTY_UPDATE|D3DLOCK_NOSYSLOCK|D3DLOCK_READONLY)))
			{
				return -1;
			}
			for(int i=0;i<gScreenRect.bottom;i++)
			{
				memcpy((BYTE*)pBits+(gScreenRect.bottom-i-1)*gScreenRect.right*32/8,(BYTE*)lockedRect.pBits+i*lockedRect.Pitch,gScreenRect.right*32/8);
			}
			g_pSurface->UnlockRect();
			return 0;
	}
	inline void DisplayBackBuffer(HDC hdc)
	{
		HDC dc = CreateCompatibleDC(hdc);
		HBITMAP bitmap = CreateDIBitmap(hdc, &bmpInfo.bmiHeader, CBM_INIT, pBits, &bmpInfo, DIB_RGB_COLORS);
		SelectObject(dc, bitmap);
		BitBlt(hdc, 0, 0, gScreenRect.right, gScreenRect.bottom, dc, 0 , 0, SRCCOPY);
		DeleteObject(bitmap);
		DeleteDC(dc);
	}

};


struct BitmapInfo {
	BITMAPINFO		bminfo;
	RGBQUAD			Colors[256];
};

class RDesktopCap
{
private:
	BitmapInfo		BI;
	int				wigth;
	int				height;
	HBITMAP			hBackBufferBitmap;
	HDC				hBackBuferDC;
	HDC				hDesktopDC;
	BYTE		*   BackBuffer;
	BYTE		*   SendBuffer;
	BYTE		*   CompressBuffer;
	HWND			m_hWnd;
	HINSTANCE		m_hInstance;
	bool			m_bFirst;
	int				BytesPerPixel;
public:
	bool			m_bDisplayChange;
	bool			m_bPalleteChange;
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
	RDesktopCap(void):wigth(0), height(0), hBackBuferDC(0), hBackBufferBitmap(0), hDesktopDC(0)
		,m_bDisplayChange(true), m_bPalleteChange(true), m_bFirst(true)
	{
		memset(&BI.bminfo, 0, sizeof(BITMAPINFO));
		BackBuffer = 0;
		SendBuffer = 0;
		CompressBuffer = 0;
	}
	BitmapInfo&  getBitmapInfo() { return BI; }

	BYTE * getCompressedBuffer(Rect & rect, int & len)
	{
		unsigned long l = (int)((double)BI.bminfo.bmiHeader.biSizeImage*1.015 + 1);
		BYTE * temp = getBuffer(rect, len);
		compress((unsigned char *)CompressBuffer, &l, (unsigned char *)temp, len);
		len = l;
		return CompressBuffer;
	}

	BYTE * getBuffer(Rect & rect, int & len)
	{
		int dx = rect.x2 - rect.x1;
		int dy = rect.y2 - rect.y1;
		for (int i=0; i < dy; ++i)
			memcpy((SendBuffer+dx*i*BytesPerPixel), (BackBuffer+ ((rect.y1+i)*wigth+rect.x1)*BytesPerPixel), sizeof(BYTE)*BytesPerPixel*dx);
		len = dx * dy * BytesPerPixel;
		return SendBuffer;
	}

	inline int Initialize(HINSTANCE hInstance)
	{
		if ( m_bFirst ) {
			m_bFirst = false;
			m_hInstance = hInstance;
			WNDCLASSEX	m_cWndClass;
			static TCHAR		*	szDesktopName = TEXT("DesktopWindow");

			m_cWndClass.cbSize			= sizeof(m_cWndClass);
			m_cWndClass.style			= 0;
			m_cWndClass.lpfnWndProc		= RDesktopCap::WndProc;
			m_cWndClass.cbClsExtra		= 0;
			m_cWndClass.cbWndExtra		= 0;
			m_cWndClass.hInstance		= hInstance;
			m_cWndClass.hIcon			= 0;
			m_cWndClass.hCursor			= 0;
			m_cWndClass.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
			m_cWndClass.lpszMenuName	= (const char *) 0;
			m_cWndClass.lpszClassName	= szDesktopName;
			m_cWndClass.hIconSm			= 0;

			if ( RegisterClassEx(&m_cWndClass) == 0 ) {
				logger.AddString(TEXT("Registering class error\n"));
				return GetLastError();
			}

			m_hWnd = CreateWindow(szDesktopName, szDesktopName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 	CW_USEDEFAULT,	400, 400, 0, 0, hInstance, 0);
			if ( m_hWnd == 0 ) {
				logger.AddString(TEXT("Create window error\n"));
				return GetLastError();
			}
			SetWindowLong(m_hWnd, GWL_USERDATA, (long) this);	
		}
		wigth = GetSystemMetrics(SM_CXSCREEN);
		height = GetSystemMetrics(SM_CYSCREEN);
		hDesktopDC = GetDC(0);
		if ( (hBackBuferDC = CreateCompatibleDC(hDesktopDC)) == 0 )
			return GetLastError();
		if ( (hBackBufferBitmap=CreateCompatibleBitmap(hDesktopDC, wigth, height)) == 0 ) {
			DeleteObject(hBackBuferDC);
			return GetLastError();
		}
		SelectObject(hBackBuferDC, hBackBufferBitmap);
		GdiFlush();
		if ( BitBlt(hBackBuferDC, 0, 0, wigth, height, hDesktopDC, 0, 0, SRCCOPY) == FALSE ) {
			DeleteObject(hBackBuferDC);
			DeleteObject(hBackBufferBitmap);
			return GetLastError();
		}

		BI.bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		BI.bminfo.bmiHeader.biBitCount = 0;
		GetDIBits(hBackBuferDC, hBackBufferBitmap, 0, 1, 0, &BI.bminfo, DIB_RGB_COLORS);
		GetDIBits(hBackBuferDC, hBackBufferBitmap, 0, 1, 0, &BI.bminfo, DIB_RGB_COLORS);
		BytesPerPixel = BI.bminfo.bmiHeader.biBitCount / 8;
		BackBuffer = (BYTE *)malloc(BI.bminfo.bmiHeader.biSizeImage);
		SendBuffer = (BYTE *)malloc((int)((double)BI.bminfo.bmiHeader.biSizeImage*1.015 + 1));
		CompressBuffer = (BYTE *)malloc((int)((double)BI.bminfo.bmiHeader.biSizeImage*1.015 + 1));
		if ( BackBuffer == 0 ) {
			DeleteObject(hBackBuferDC);
			DeleteObject(hBackBufferBitmap);
			return ERROR_OUTOFMEMORY;
		}
		return ERROR_SUCCESS;
	}

	inline void MakeGreyScale()
	{
		for ( int i = 0; i < wigth; ++i)
			for ( int j = 0; j < height; ++j) {
				DWORD d = (GetRValue(((COLORREF*)BackBuffer)[i*height+j]) + GetGValue(((COLORREF*)BackBuffer)[i*height+j]) +
						GetBValue(((COLORREF*)BackBuffer)[i*height+j]))/3;
				((COLORREF*)BackBuffer)[i*height+j] = RGB( d,d,d );
			}		
	}

	inline BYTE * getBackBuffer() const { return BackBuffer; }
	inline long getBackBufferSize() { return BI.bminfo.bmiHeader.biSizeImage; }

	inline int CaptureScreen()
	{
		if ( m_bDisplayChange || m_bPalleteChange ) {
			CleanUp();
			return Initialize(m_hInstance);
		} else {
			GdiFlush();
			if ( BitBlt(hBackBuferDC, 0, 0, wigth, height, hDesktopDC, 0, 0, SRCCOPY) == FALSE )
				return GetLastError();
			if ( GetDIBits(hBackBuferDC, hBackBufferBitmap, 0, BI.bminfo.bmiHeader.biHeight, BackBuffer, &BI.bminfo, DIB_RGB_COLORS) == 0 )
				return GetLastError();
		}
		return ERROR_SUCCESS;
	}

	inline int DisplayBuffer(HDC hdc, BYTE * buffer, int x, int y, int Width, int Height)
	{
		HDC dc = CreateCompatibleDC(hdc);
		if ( dc == 0 )
			return GetLastError();
		HBITMAP bitmap = CreateDIBitmap(hdc, &(BI.bminfo.bmiHeader), CBM_INIT, buffer, &BI.bminfo, DIB_RGB_COLORS);
		if ( bitmap == 0 )
			return GetLastError();
		SelectObject(dc, bitmap);
		if ( BitBlt(hdc, x, y, Width, Height, dc, 0 , 0, SRCCOPY) == FALSE )
			return GetLastError();
		if ( DeleteObject(bitmap) != 0 )
			return GetLastError();
		if ( DeleteDC(dc) != 0 )
			return GetLastError();
		return ERROR_SUCCESS;
	}

	inline int DisplayBackBuffer(HDC hdc)
	{
		HDC dc = CreateCompatibleDC(hdc);
		if ( dc == 0 )
			return GetLastError();
		HBITMAP bitmap = CreateDIBitmap(hdc, &(BI.bminfo.bmiHeader), CBM_INIT, BackBuffer, &BI.bminfo, DIB_RGB_COLORS);
		if ( bitmap == 0 )
			return GetLastError();
		SelectObject(dc, bitmap);
		if ( BitBlt(hdc, 0, 0, wigth, height, dc, 0 , 0, SRCCOPY) == FALSE )
			return GetLastError();
		if ( DeleteObject(bitmap) != 0 )
			return GetLastError();
		if ( DeleteDC(dc) != 0 )
			return GetLastError();
		return ERROR_SUCCESS;
	}
	
	inline void CleanUp()
	{
		DeleteDC(hBackBuferDC);
		DeleteObject(hBackBufferBitmap);
		free(BackBuffer);
		free(SendBuffer);
		free(CompressBuffer);
	}

	~RDesktopCap(void)
	{
		if ( BackBuffer != 0 )
			CleanUp();
		DestroyWindow(m_hWnd);
	}
};




#endif
