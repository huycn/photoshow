#include "PhotoShow.h"
#include <cstring>

const float DEFAULT_DPI = 96.f;   // Default DPI that maps image resolution directly to screen resoltuion

template <typename T>
inline void SafeRelease(T *&p)
{
	if (nullptr != p)
	{
		p->Release();
		p = nullptr;
	}
}

PhotoShow::PhotoShow(int screenWidth, int screenHeight)
	: m_screenWidth(screenWidth),
	m_screenHeight(screenHeight),
	m_wicFactory(nullptr),
	m_d2dFactory(nullptr),
	m_renderTarget(nullptr),
	m_d2dBitmap(nullptr),
	m_convertedSrcBitmap(nullptr)
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
}

PhotoShow::~PhotoShow()
{
	CoUninitialize();
}

HRESULT
PhotoShow::LocateNextImage(LPWSTR pszFileName, DWORD cchFileName)
{
	wcscpy(pszFileName, L"D:/huyc/Desktop/Anja-Nejarri.jpg");
	return S_OK;
}

void PhotoShow::LoadNextImage(HWND hWnd)
{
	WCHAR szFileName[MAX_PATH];
	HRESULT hr = LocateNextImage(szFileName, ARRAYSIZE(szFileName));

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
			SafeRelease(m_convertedSrcBitmap);
			hr = m_wicFactory->CreateFormatConverter(&m_convertedSrcBitmap);
		}

		if (SUCCEEDED(hr))
		{
			hr = m_convertedSrcBitmap->Initialize(
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

		if (SUCCEEDED(hr))
		{
			// Need to release the previous D2DBitmap if there is one
			SafeRelease(m_d2dBitmap);
			hr = m_renderTarget->CreateBitmapFromWicBitmap(m_convertedSrcBitmap, nullptr, &m_d2dBitmap);
		}

		SafeRelease(pDecoder);
		SafeRelease(pFrame);
	}
}

void
PhotoShow::OnPaint(HWND hWnd)
{
	HRESULT hr = S_OK;
	PAINTSTRUCT ps;

	if (BeginPaint(hWnd, &ps))
	{
		// Create render target if not yet created
		hr = CreateDeviceResources(hWnd);

		if (SUCCEEDED(hr) && !(m_renderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
		{
			m_renderTarget->BeginDraw();

			m_renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

			// Clear the background
			m_renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

			auto rtSize = m_renderTarget->GetSize();

			// Create a rectangle with size of current window
			auto rectangle = D2D1::RectF(0.0f, 0.0f, rtSize.width, rtSize.height);

			// D2DBitmap may have been released due to device loss. 
			// If so, re-create it from the source bitmap
			if (m_convertedSrcBitmap != nullptr && m_d2dBitmap == nullptr)
			{
				m_renderTarget->CreateBitmapFromWicBitmap(m_convertedSrcBitmap, nullptr, &m_d2dBitmap);
			}

			// Draws an image and scales it to the current window size
			if (m_d2dBitmap)
			{
				m_renderTarget->DrawBitmap(m_d2dBitmap, rectangle);
			}

			hr = m_renderTarget->EndDraw();

			// In case of device loss, discard D2D render target and D2DBitmap
			// They will be re-created in the next rendering pass
			if (hr == D2DERR_RECREATE_TARGET)
			{
				SafeRelease(m_d2dBitmap);
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
	}

	return hr;
}
