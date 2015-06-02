#include "PhotoShow.h"
#include "D2D1Util.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "ImageUtil.h"

#include <strsafe.h>
#include <cstring>
#include <algorithm>

const float DEFAULT_DPI = 96.f;   // Default DPI that maps image resolution directly to screen resolution
const float BACKGROUND_DARKEN = 0.6f;

template <typename T>
inline void SafeRelease(T *&p)
{
	if (nullptr != p)
	{
		p->Release();
		p = nullptr;
	}
}

namespace {
	int peekaboo(std::random_device &randomizer, const std::vector<int> &pos, std::vector<int> &weight, int currentLength)
	{
		int window = pos.back();

		if (currentLength >= window)
			return 0;

		int halfSize = currentLength / 2;

		int startIndex = 0;
		for (; startIndex < int(pos.size()); ++startIndex)
		{
			if (pos[startIndex] >= halfSize)
			{
				--startIndex;
				break;
			}
		}

		int endIndex = pos.size() - 1;
		for (; endIndex >= 0; --endIndex)
		{
			if (window - pos[endIndex] >= halfSize)
			{
				endIndex += 2;
				break;
			}
		}

		if (endIndex >= 0 && startIndex < endIndex)
		{
			int center = RoundToNearest(std::piecewise_linear_distribution<>(pos.begin() + startIndex, pos.begin() + endIndex, weight.begin() + startIndex)(randomizer));
			int left = std::max(0, std::min(center - halfSize, window - currentLength));
			int right = left + currentLength;
			for (size_t i = 0; i < pos.size(); ++i)
			{
				if (pos[i] >= left && pos[i] <= right)
					weight[i] = 1;
				else
					weight[i] += 2;
			}
			return left;
		}
		return std::uniform_int_distribution<int>(0, window - currentLength)(randomizer);
	}

	void initWeightRange(std::vector<int> &weightPos, std::vector<int> &weightValue, unsigned rangeCount, int length)
	{
		weightPos.reserve(rangeCount+1);
		float rangeWidth = float(length) / rangeCount;
		int pos = 0;
		while (weightPos.size() < rangeCount)
		{
			weightPos.push_back(pos);
			pos = RoundToNearest(pos + rangeWidth);
		}
		weightPos.push_back(length);
		weightValue.resize(weightPos.size());
		std::fill(weightValue.begin(), weightValue.end(), 1);
	}

}
PhotoShow::PhotoShow(int screenWidth, int screenHeight, const std::vector<std::wstring> &folders, bool shuffle)
	: m_screenWidth(screenWidth),
	m_screenHeight(screenHeight),
	m_wicFactory(nullptr),
	m_d2dFactory(nullptr),
	m_renderTarget(nullptr),
	m_backgroundTarget(nullptr),
	m_d2dBitmap(nullptr),
	m_bitmapConverter(nullptr),
	m_currentFileIndex(0)
{
	HRESULT hr = S_OK;

	hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	// Create WIC factory
	if (SUCCEEDED(hr))
	{
		hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
			);
	}

	if (SUCCEEDED(hr))
	{
		// Create D2D factory
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
	}

	for (const std::wstring &dirPath : folders)
	{
		std::vector<std::wstring> fileNameList;
		ListFilesInDirectory(dirPath, fileNameList, [](const std::wstring &name) {
			return EndsWith(name, std::wstring(L".jpg")) ||
				EndsWith(name, std::wstring(L".jpeg")) ||
				EndsWith(name, std::wstring(L".png"));
		});
		for (const std::wstring &fileName : fileNameList)
		{
			m_fileList.push_back(dirPath + L"\\" + fileName);
		}
	}
	if (shuffle)
	{
		std::shuffle(m_fileList.begin(), m_fileList.end(), m_randomizer);
	}

	int numberOfRange = 20;
	initWeightRange(m_weightPosX, m_weightValueX, numberOfRange, screenWidth);
	initWeightRange(m_weightPosY, m_weightValueY, numberOfRange, screenHeight);
}

PhotoShow::~PhotoShow()
{
	CoUninitialize();
}

HRESULT
PhotoShow::LocateNextImage(LPWSTR pszFileName)
{
	if (m_currentFileIndex >= m_fileList.size())
		m_currentFileIndex = 0;
	
	if (m_currentFileIndex < m_fileList.size())
	{
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

	// Step 1: Create the open dialog box and locate the image file
	if (SUCCEEDED(hr))
	{
		// Step 2: Decode the source image

		// Create a decoder
		IWICBitmapDecoder *pDecoder = nullptr;

		hr = m_wicFactory->CreateDecoderFromFilename(
			szFileName,                      // Image to be decoded
			nullptr,                         // Do not prefer a particular vendor
			GENERIC_READ,                    // Desired read access to the file
			WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
			&pDecoder                        // Pointer to the decoder
			);

		// Retrieve the first frame of the image from the decoder
		IWICBitmapFrameDecode *pFrame = nullptr;

		if (SUCCEEDED(hr))
		{
			hr = pDecoder->GetFrame(0, &pFrame);
		}

		//Step 3: Format convert the frame to 32bppPBGRA
		if (SUCCEEDED(hr))
		{
			SafeRelease(m_bitmapConverter);
			hr = m_wicFactory->CreateFormatConverter(&m_bitmapConverter);
		}

		if (SUCCEEDED(hr))
		{
			hr = m_bitmapConverter->Initialize(
				pFrame,                          // Input bitmap to convert
				GUID_WICPixelFormat32bppPBGRA,   // Destination pixel format
				WICBitmapDitherTypeNone,         // Specified dither pattern
				nullptr,                         // Specify a particular palette 
				0.f,                             // Alpha threshold
				WICBitmapPaletteTypeCustom       // Palette translation type
				);
		}

		//Step 4: Create render target and D2D bitmap from IWICBitmapSource
		if (SUCCEEDED(hr))
		{
			hr = CreateDeviceResources(hWnd);
		}

		if (SUCCEEDED(hr) && m_d2dBitmap != nullptr)
		{
			// render current bitmap to background before load the next
			m_backgroundTarget->BeginDraw();
			m_backgroundTarget->SetTransform(D2D1::Matrix3x2F::Identity());

			ID2D1SolidColorBrush *brush;
			m_backgroundTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, BACKGROUND_DARKEN), &brush);
			m_backgroundTarget->FillRectangle(D2D1::RectF(0.0f, 0.0f, float(m_screenWidth), float(m_screenHeight)), brush);
			brush->Release();

			m_backgroundTarget->DrawBitmap(m_d2dBitmap, m_bitmapRect);
			m_backgroundTarget->EndDraw();
		}

		if (SUCCEEDED(hr))
		{
			// Need to release the previous D2DBitmap if there is one
			SafeRelease(m_d2dBitmap);
			hr = m_renderTarget->CreateBitmapFromWicBitmap(m_bitmapConverter, nullptr, &m_d2dBitmap);
		}

		if (SUCCEEDED(hr))
		{
			auto rtSize = m_d2dBitmap->GetSize();
			int imgWidth = RoundToNearest(rtSize.width);
			int imgHeight = RoundToNearest(rtSize.height);
			if (imgWidth > m_screenWidth || imgHeight > m_screenHeight)
			{
				std::tie(imgWidth, imgHeight) = ScaleToFit(imgWidth, imgHeight, m_screenWidth, m_screenHeight); // m_renderTarget->GetSize();
			}

			int newX = peekaboo(m_randomizer, m_weightPosX, m_weightValueX, imgWidth);
			int newY = peekaboo(m_randomizer, m_weightPosY, m_weightValueY, imgHeight);
			m_bitmapRect = D2D1::RectF(float(newX), float(newY), float(newX + imgWidth), float(newY + imgHeight));

			m_animProgress = 0.0f;

			InvalidateRect(hWnd, nullptr, false);
		}

		SafeRelease(pDecoder);
		SafeRelease(pFrame);
	}
	return SUCCEEDED(hr);
}

void
PhotoShow::NextAnimationFrame(HWND hWnd, float progress)
{
	m_animProgress = progress;
	InvalidateRect(hWnd, nullptr, false);
}

void
PhotoShow::EndAnimation(HWND hWnd)
{
	m_animProgress = 1.0f;
	InvalidateRect(hWnd, nullptr, false);
}


void
PhotoShow::OnPaint(HWND hWnd)
{
	PAINTSTRUCT ps;

	if (BeginPaint(hWnd, &ps))
	{
		// Create render target if not yet created
		HRESULT hr = CreateDeviceResources(hWnd);

		if (SUCCEEDED(hr) && !(m_renderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
		{
			m_renderTarget->BeginDraw();
			m_renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

			ID2D1Bitmap *background = nullptr;
			if (SUCCEEDED(m_backgroundTarget->GetBitmap(&background)))
			{
				m_renderTarget->DrawBitmap(background, D2D1::RectF(0.0f, 0.0f, float(m_screenWidth), float(m_screenHeight)));
			}

			ID2D1SolidColorBrush *brush;
			m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, BACKGROUND_DARKEN), &brush);
			brush->SetOpacity(m_animProgress);
			m_renderTarget->FillRectangle(D2D1::RectF(0.0f, 0.0f, float(m_screenWidth), float(m_screenHeight)), brush);
			brush->Release();

			// D2DBitmap may have been released due to device loss. 
			// If so, re-create it from the source bitmap
			if (m_bitmapConverter != nullptr && m_d2dBitmap == nullptr)
			{
				m_renderTarget->CreateBitmapFromWicBitmap(m_bitmapConverter, nullptr, &m_d2dBitmap);
			}

			// Draws an image and scales it to the current window size
			if (m_d2dBitmap != nullptr)
			{
				m_renderTarget->DrawBitmap(m_d2dBitmap, m_bitmapRect, m_animProgress);
			}

			hr = m_renderTarget->EndDraw();

			// In case of device loss, discard D2D render target and D2DBitmap
			// They will be re-created in the next rendering pass
			if (hr == D2DERR_RECREATE_TARGET)
			{
				SafeRelease(m_d2dBitmap);
				SafeRelease(m_backgroundTarget);
				SafeRelease(m_renderTarget);
				
				// Force a re-render
				hr = InvalidateRect(hWnd, nullptr, TRUE) ? S_OK : E_FAIL;
			}
		}

		EndPaint(hWnd, &ps);
	}
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

	if (m_renderTarget == nullptr)
	{
		auto renderTargetProperties = D2D1::RenderTargetProperties();

		// Set the DPI to be the default system DPI to allow direct mapping
		// between image pixels and desktop pixels in different system DPI settings
		renderTargetProperties.dpiX = DEFAULT_DPI;
		renderTargetProperties.dpiY = DEFAULT_DPI;

		auto size = D2D1::SizeU(m_screenWidth, m_screenHeight);

		hr = m_d2dFactory->CreateHwndRenderTarget(
			renderTargetProperties,
			D2D1::HwndRenderTargetProperties(hWnd, size),
			&m_renderTarget
			);

		if (m_renderTarget != nullptr)
		{
			m_renderTarget->CreateCompatibleRenderTarget(&m_backgroundTarget);
		}
	}

	return hr;
}
