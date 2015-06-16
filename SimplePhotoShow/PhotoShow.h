#pragma once
#include <windows.h>
#include <wincodec.h>
#include <d2d1.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>

#include "RefCnt.h"

class PhotoShow : public RefCnt<PhotoShow>
{
public:
	PhotoShow(int virtualScreenWidth, int virtualScreenHeight, const D2D1_RECT_F &screenRect, const std::vector<std::wstring> &imageList);
	~PhotoShow();

	void OnPaint(HWND hWnd);
	bool LoadNextImage(HWND hWnd);
	void GetRect(LPRECT outRect);

private:
	D2D1_RECT_F m_screenRect;

	FLOAT m_animProgress;
	std::vector<FLOAT> m_weightPosX;
	std::vector<FLOAT> m_weightValueX;
	std::vector<FLOAT> m_weightPosY;
	std::vector<FLOAT> m_weightValueY;

	ID2D1BitmapRenderTarget *m_backgroundTarget;
	ID2D1Bitmap				*m_d2dBitmap;
	IWICFormatConverter		*m_bitmapConverter;

	D2D1_RECT_F             m_bitmapRect;	// relative to m_screenRect

	std::vector<std::wstring> m_fileList;
	size_t m_currentFileIndex;
	std::random_device m_randomizer;

	std::chrono::steady_clock::time_point m_animStart;

	static int                   s_instanceCount;
	static int                   s_virtualWidth;
	static int                   s_virtualHeight;
	static IWICImagingFactory    *s_wicFactory;
	static ID2D1Factory	         *s_d2dFactory;
	static ID2D1HwndRenderTarget *s_renderTarget;

	ID2D1HwndRenderTarget *m_renderTarget;

	HRESULT LocateNextImage(LPWSTR pszFileName);
	HRESULT CreateDeviceResources(HWND hWnd);
	void Invalidate(HWND hWnd);
	void OnRenderTargetReset();

	static void CALLBACK NextAnimationFrame(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
};