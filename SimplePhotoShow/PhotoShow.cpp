#include "PhotoShow.h"
#include "D2D1Util.h"
#include "ImageUtil.h"

#include <strsafe.h>
#include <cstring>
#include <algorithm>

//#define WITH_DEBUG_LOG

#ifdef WITH_DEBUG_LOG
#include <fstream>
static std::ofstream logFile("R:/photoshow_photoshow.txt");
#define DEBUG_LOG(x) logFile << x << std::endl
#else
#define DEBUG_LOG(x)
#endif

const float DEFAULT_DPI = 96.f;   // Default DPI that maps image resolution directly to screen resolution
const float BACKGROUND_DARKEN = 0.6f;

#define ANIMATION_LENGTH 1000
#define ANIMATION_PRECISION 33	// frame per second

template <typename T>
inline void SafeRelease(T *&p)
{
	if (nullptr != p) {
		p->Release();
		p = nullptr;
	}
}

namespace {
	FLOAT peekaboo(std::random_device &randomizer, const std::vector<FLOAT> &pos, std::vector<FLOAT> &weight, FLOAT currentLength)
	{
		FLOAT window = pos.back();

		if (currentLength >= window) {
			return 0.f;
		}

		FLOAT halfSize = currentLength / 2;

		int startIndex = 0;
		for (; startIndex < int(pos.size()); ++startIndex) {
			if (pos[startIndex] >= halfSize) {
				--startIndex;
				break;
			}
		}

		int endIndex = pos.size() - 1;
		for (; endIndex >= 0; --endIndex) {
			if (window - pos[endIndex] >= halfSize) {
				endIndex += 2;
				break;
			}
		}

		if (endIndex >= 0 && startIndex < endIndex) {
			FLOAT center = static_cast<FLOAT>(std::piecewise_linear_distribution<>(pos.begin() + startIndex, pos.begin() + endIndex, weight.begin() + startIndex)(randomizer));
			FLOAT left = std::max(0.0f, std::min(center - halfSize, window - currentLength));
			FLOAT right = left + currentLength;
			for (size_t i = 0; i < pos.size(); ++i) {
				if (pos[i] >= left && pos[i] <= right) {
					weight[i] = 1.f;
				}
				else {
					weight[i] += 2.f;
				}
			}
			return left;
		}
		return std::uniform_real_distribution<FLOAT>(0, window - currentLength)(randomizer);
	}

	void initWeightRange(std::vector<FLOAT> &weightPos, std::vector<FLOAT> &weightValue, unsigned rangeCount, FLOAT length)
	{
		weightPos.reserve(rangeCount+1);
		FLOAT rangeWidth = length / rangeCount;
		FLOAT pos = 0;
		while (weightPos.size() < rangeCount) {
			weightPos.push_back(pos);
			pos = pos + rangeWidth;
		}
		weightPos.push_back(length);
		weightValue.resize(weightPos.size());
		std::fill(weightValue.begin(), weightValue.end(), 1.0f);
	}

}

int PhotoShow::s_instanceCount = 0;
int PhotoShow::s_virtualWidth = 0;
int PhotoShow::s_virtualHeight = 0;
IWICImagingFactory* PhotoShow::s_wicFactory = nullptr;
ID2D1Factory* PhotoShow::s_d2dFactory = nullptr;
ID2D1HwndRenderTarget* PhotoShow::s_renderTarget = nullptr;

PhotoShow::PhotoShow(int virtualScreenWidth, int virtualScreenHeight, const D2D1_RECT_F &screenRect, const std::vector<std::wstring> &imageList)
	: m_screenRect(screenRect),
	m_animProgress(0),
	m_backgroundTarget(nullptr),
	m_d2dBitmap(nullptr),
	m_bitmapConverter(nullptr),
	m_bitmapRect(),
	m_fileList(imageList),
	m_currentFileIndex(0),
	m_renderTarget(nullptr)
{
	HRESULT hr = S_OK;

	if (s_instanceCount++ == 0) {
		s_virtualWidth = virtualScreenWidth;
		s_virtualHeight = virtualScreenHeight;

		hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		// Create WIC factory
		if (SUCCEEDED(hr)) {
			hr = CoCreateInstance(
				CLSID_WICImagingFactory,
				nullptr,
				CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&s_wicFactory)
				);
		}

		if (SUCCEEDED(hr)) {
			// Create D2D factory
			hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &s_d2dFactory);
		}


	}

	int numberOfRange = 20;
	initWeightRange(m_weightPosX, m_weightValueX, numberOfRange, screenRect.right - screenRect.left);
	initWeightRange(m_weightPosY, m_weightValueY, numberOfRange, screenRect.bottom - screenRect.top);
}

PhotoShow::~PhotoShow()
{
	SafeRelease(m_d2dBitmap);
	SafeRelease(m_bitmapConverter);
	SafeRelease(m_backgroundTarget);

	if (--s_instanceCount == 0) {
		SafeRelease(s_renderTarget);
		CoUninitialize();
	}
}

HRESULT
PhotoShow::LocateNextImage(LPWSTR pszFileName)
{
	if (m_currentFileIndex >= m_fileList.size()) {
		m_currentFileIndex = 0;
	}
	if (m_currentFileIndex < m_fileList.size()) {
		StringCchCopy(pszFileName, MAX_PATH, m_fileList[m_currentFileIndex].c_str());
		++m_currentFileIndex;
		return S_OK;
	}
	return -1;
}

bool
PhotoShow::LoadNextImage(HWND hWnd)
{
	WCHAR szFileName[MAX_PATH];
	HRESULT hr = LocateNextImage(szFileName);

	if (SUCCEEDED(hr)) {
		IWICBitmapDecoder *pDecoder = nullptr;
		hr = s_wicFactory->CreateDecoderFromFilename(
			szFileName,                      // Image to be decoded
			nullptr,                         // Do not prefer a particular vendor
			GENERIC_READ,                    // Desired read access to the file
			WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
			&pDecoder                        // Pointer to the decoder
			);

		IWICBitmapFrameDecode *pFrame = nullptr;
		if (SUCCEEDED(hr)) {
			hr = pDecoder->GetFrame(0, &pFrame);
		}
		if (SUCCEEDED(hr)) {
			SafeRelease(m_bitmapConverter);
			hr = s_wicFactory->CreateFormatConverter(&m_bitmapConverter);
			if (SUCCEEDED(hr)) {
				hr = m_bitmapConverter->Initialize(
					pFrame,                          // Input bitmap to convert
					GUID_WICPixelFormat32bppPBGRA,   // Destination pixel format
					WICBitmapDitherTypeNone,         // Specified dither pattern
					nullptr,                         // Specify a particular palette 
					0.f,                             // Alpha threshold
					WICBitmapPaletteTypeCustom       // Palette translation type
					);
			}
		}
		if (SUCCEEDED(hr)) {
			hr = CreateDeviceResources(hWnd);
		}

		auto screenWidth = m_screenRect.right - m_screenRect.left;
		auto screenHeight = m_screenRect.bottom - m_screenRect.top;

		if (m_backgroundTarget == nullptr) {
			hr = m_renderTarget->CreateCompatibleRenderTarget(D2D1::SizeF(screenWidth, screenHeight), &m_backgroundTarget);
		}

		if (SUCCEEDED(hr) && m_d2dBitmap != nullptr) {
			// render current bitmap to background before load the next
			m_backgroundTarget->BeginDraw();
			m_backgroundTarget->SetTransform(D2D1::Matrix3x2F::Identity());

			ID2D1SolidColorBrush *brush = nullptr;
			m_backgroundTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, BACKGROUND_DARKEN), &brush);
			if (brush != nullptr) {
				m_backgroundTarget->FillRectangle(D2D1::RectF(0.f, 0.f, screenWidth, screenHeight), brush);
				brush->Release();
			}

			m_backgroundTarget->DrawBitmap(m_d2dBitmap, m_bitmapRect);
			m_backgroundTarget->EndDraw();
		}

		if (SUCCEEDED(hr)) {
			// Need to release the previous D2DBitmap if there is one
			SafeRelease(m_d2dBitmap);
			hr = m_renderTarget->CreateBitmapFromWicBitmap(m_bitmapConverter, nullptr, &m_d2dBitmap);
		}

		if (SUCCEEDED(hr)) {
			auto rtSize = m_d2dBitmap->GetSize();
			auto imgWidth = rtSize.width;
			auto imgHeight = rtSize.height;
			if (imgWidth > screenWidth || imgHeight > screenHeight)
			{
				std::tie(imgWidth, imgHeight) = ScaleToFit(imgWidth, imgHeight, screenWidth, screenHeight); // m_renderTarget->GetSize();
			}

			auto newX = peekaboo(m_randomizer, m_weightPosX, m_weightValueX, imgWidth);
			auto newY = peekaboo(m_randomizer, m_weightPosY, m_weightValueY, imgHeight);
			m_bitmapRect = D2D1::RectF(float(newX), float(newY), float(newX + imgWidth), float(newY + imgHeight));

			m_animProgress = 0.0f;
			Invalidate(hWnd);
			ref();
			m_animStart = std::chrono::steady_clock::now();
			SetTimer(hWnd, UINT_PTR(this), 1000 / ANIMATION_PRECISION, &NextAnimationFrame);
		}

		SafeRelease(pDecoder);
		SafeRelease(pFrame);
	}
	return SUCCEEDED(hr);
}

void
PhotoShow::NextAnimationFrame(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	PhotoShow* thiz = (PhotoShow*) idEvent;
	
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - thiz->m_animStart).count();
	if (duration >= ANIMATION_LENGTH)
	{
		KillTimer(hWnd, idEvent);
		thiz->m_animProgress = 1.f;
		thiz->Invalidate(hWnd);
		thiz->unref();
	}
	else
	{
		thiz->m_animProgress = duration / FLOAT(ANIMATION_LENGTH);
		thiz->Invalidate(hWnd);
	}
}

void
PhotoShow::GetRect(LPRECT outRect)
{
	outRect->left = RoundToNearest(m_screenRect.left);
	outRect->right = RoundToNearest(m_screenRect.right);
	outRect->top = RoundToNearest(m_screenRect.top);
	outRect->bottom = RoundToNearest(m_screenRect.bottom);
}

void
PhotoShow::Invalidate(HWND hWnd)
{
	RECT r;
	GetRect(&r);
	DEBUG_LOG("Invalidate Rect: " << r.left << ' ' << r.top << ' ' << r.right << ' ' << r.bottom);
	InvalidateRect(hWnd, &r, false);
}

void
PhotoShow::OnPaint(HWND hWnd)
{
	// Create render target if not yet created
	HRESULT hr = CreateDeviceResources(hWnd);

	if (SUCCEEDED(hr) && m_renderTarget != nullptr && m_backgroundTarget != nullptr && !(m_renderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
	{
		m_renderTarget->BeginDraw();
		m_renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

		ID2D1Bitmap *background = nullptr;
		if (SUCCEEDED(m_backgroundTarget->GetBitmap(&background)))
		{
			m_renderTarget->DrawBitmap(background, m_screenRect);
		}

		ID2D1SolidColorBrush *brush = nullptr;
		m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, BACKGROUND_DARKEN), &brush);
		if (brush != nullptr) {
			brush->SetOpacity(m_animProgress);
			m_renderTarget->FillRectangle(m_screenRect, brush);
			brush->Release();
		}

		// D2DBitmap may have been released due to device loss. 
		// If so, re-create it from the source bitmap
		if (m_bitmapConverter != nullptr && m_d2dBitmap == nullptr)
		{
			m_renderTarget->CreateBitmapFromWicBitmap(m_bitmapConverter, nullptr, &m_d2dBitmap);
		}

		// Draws an image and scales it to the current window size
		if (m_d2dBitmap != nullptr)
		{
			auto rect = m_bitmapRect;
			D2D1::OffsetRect(rect, m_screenRect.left, m_screenRect.top);
			m_renderTarget->DrawBitmap(m_d2dBitmap, rect, m_animProgress);
		}

		hr = m_renderTarget->EndDraw();

		// In case of device loss, discard D2D render target and D2DBitmap
		// They will be re-created in the next rendering pass
		if (hr == D2DERR_RECREATE_TARGET)
		{
			SafeRelease(s_renderTarget);
			m_renderTarget = nullptr;
			OnRenderTargetReset();

			// Force a re-render
			Invalidate(hWnd);
			hr = S_OK;
		}
	}
}

void
PhotoShow::OnRenderTargetReset()
{
	SafeRelease(m_d2dBitmap);
	SafeRelease(m_backgroundTarget);
}

/******************************************************************
*  This method creates resources which are bound to a particular  *
*  D2D device. It's all centralized here, in case the resources   *
*  need to be recreated in the event of D2D device loss           *
* (e.g. display change, remoting, removal of video card, etc).    *
******************************************************************/
HRESULT
PhotoShow::CreateDeviceResources(HWND hWnd)
{
	HRESULT hr = S_OK;

	if (s_renderTarget == nullptr)
	{
		auto renderTargetProperties = D2D1::RenderTargetProperties();

		// Set the DPI to be the default system DPI to allow direct mapping
		// between image pixels and desktop pixels in different system DPI settings
		renderTargetProperties.dpiX = DEFAULT_DPI;
		renderTargetProperties.dpiY = DEFAULT_DPI;

		auto size = D2D1::SizeU(s_virtualWidth, s_virtualHeight);

		hr = s_d2dFactory->CreateHwndRenderTarget(
			renderTargetProperties,
			D2D1::HwndRenderTargetProperties(hWnd, size),
			&s_renderTarget
			);
	}

	if (m_renderTarget != s_renderTarget)
	{
		OnRenderTargetReset();
		m_renderTarget = s_renderTarget;
	}

	return hr;
}
