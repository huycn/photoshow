#pragma once
#include <windows.h>
#include <wincodec.h>
#include <d2d1.h>
#include <vector>
#include <string>
#include <random>

class PhotoShow
{
public:
	PhotoShow(int screenWidth, int screenHeight);
	~PhotoShow();

	void OnPaint(HWND hWnd);
	bool LoadNextImage(HWND hWnd);
	void NextAnimationFrame(HWND hWnd, float progress);
	void EndAnimation(HWND hWnd);

private:
	int m_screenWidth;
	int m_screenHeight;
	float m_animProgress;

	IWICImagingFactory		*m_wicFactory;
	ID2D1Factory			*m_d2dFactory;
	ID2D1HwndRenderTarget	*m_renderTarget;
	ID2D1BitmapRenderTarget *m_backgroundTarget;

	ID2D1Bitmap				*m_d2dBitmap;
	IWICFormatConverter		*m_bitmapConverter;

	D2D1_RECT_F             m_bitmapRect;

	std::vector<std::wstring> m_fileList;
	size_t m_currentFileIndex;
	std::random_device m_randomizer;

	HRESULT LocateNextImage(LPWSTR pszFileName);
	HRESULT CreateDeviceResources(HWND hWnd);
};