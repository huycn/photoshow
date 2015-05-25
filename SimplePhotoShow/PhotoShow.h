#pragma once
#include <windows.h>
#include <wincodec.h>
#include <d2d1.h>
#include <vector>
#include <string>

class PhotoShow
{
public:
	PhotoShow(int screenWidth, int screenHeight);
	~PhotoShow();

	void OnPaint(HWND hWnd);
	bool LoadNextImage(HWND hWnd);

private:
	int m_screenWidth;
	int m_screenHeight;

	IWICImagingFactory		*m_wicFactory;
	ID2D1Factory			*m_d2dFactory;
	ID2D1HwndRenderTarget	*m_renderTarget;

	ID2D1Bitmap				*m_d2dBitmap;
	IWICFormatConverter		*m_bitmapConverter;

	HRESULT LocateNextImage(LPWSTR pszFileName);
	HRESULT CreateDeviceResources(HWND hWnd);

	std::vector<std::wstring> m_fileList;
	size_t m_currentFileIndex;
};