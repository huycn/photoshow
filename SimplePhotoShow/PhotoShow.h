#pragma once
#include <windows.h>
#include <wincodec.h>
#include <d2d1.h>

class PhotoShow
{
public:
	PhotoShow(int screenWidth, int screenHeight);
	~PhotoShow();

	void OnPaint(HWND hWnd);
	void LoadNextImage(HWND hWnd);

private:
	int m_screenWidth;
	int m_screenHeight;

	IWICImagingFactory		*m_wicFactory;
	ID2D1Factory			*m_d2dFactory;
	ID2D1HwndRenderTarget	*m_renderTarget;

	ID2D1Bitmap				*m_d2dBitmap;
	IWICFormatConverter		*m_convertedSrcBitmap;

	HRESULT LocateNextImage(LPWSTR pszFileName, DWORD cchFileName);
	HRESULT CreateDeviceResources(HWND hWnd);
};