#include "PhotoShow.h"
#include <scrnsave.h>
#include <commctrl.h>
#include <shlobj.h>
#include <memory>
#include <regex>
#include <algorithm>
#include <random>
#include <map>

#include "resource.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "Configuration.h"

#include <dwmapi.h>

//define a Windows timer 
#define NEXT_IMAGE_ALL_DISPLAYS_TIMER_ID 1 
#define NEXT_IMAGE_SINGLE_WINDOW_TIMER_ID 2

//#define WITH_DEBUG_LOG

#ifdef WITH_DEBUG_LOG
#include <fstream>
static std::ofstream logFile("R:/photoshow_infrastructure.txt");
#define DEBUG_LOG(x) logFile << x << std::endl
#else
#define DEBUG_LOG(x)
#endif

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version = '6.0.0.0' processorArchitecture = '*' publicKeyToken = '6595b64144ccf1df' language = '*'\"")

namespace
{
	std::wstring GetDlgEditTextValue(HWND hDlg, int itemId)
	{
		std::wstring text;
		HWND handle = GetDlgItem(hDlg, itemId);
		int len = SendMessage(handle, WM_GETTEXTLENGTH, 0, 0);
		text.resize(len + 1);
		len = SendMessage(handle, WM_GETTEXT, text.size(), (LPARAM)text.data());
		text.resize(len);
		return text;
	}

	std::vector<std::wstring> buildFileList(const std::wstring &foldersStr, bool shuffle)
	{
		std::vector<std::wstring> folders;
		if (!foldersStr.empty()) {
			std::wregex re(L"\\r\\n");
			std::copy(std::wsregex_token_iterator(foldersStr.begin(), foldersStr.end(), re, -1),
				std::wsregex_token_iterator(),
				std::back_inserter(folders));
		}

		std::vector<std::wstring> result;
		for (const std::wstring &dirPath : folders) {
			std::vector<std::wstring> fileNameList;
			ListFilesInDirectory(dirPath, fileNameList, [](const std::wstring &name) {
				return EndsWith(name, std::wstring(L".jpg")) ||
					EndsWith(name, std::wstring(L".jpeg")) ||
					EndsWith(name, std::wstring(L".png"));
			});
			for (const std::wstring &fileName : fileNameList) {
				result.push_back(dirPath + L"\\" + fileName);
			}
		}
		if (shuffle) {
			std::shuffle(result.begin(), result.end(), std::random_device());
		}
		return result;
	}

	bool s_shuffleImages = true;
	std::vector<std::wstring> s_imageFileList;
	std::map<LPVOID, std::shared_ptr<PhotoShow>> s_photoShows;
	LONG s_offsetLeft = 0;
	LONG s_offsetTop = 0;

	struct LoadImageParam
	{
		HWND hWnd;
		int waitIndex;
	};

	BOOL CALLBACK GetOffsets(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
	{
		s_offsetLeft = std::min(s_offsetLeft, lprcMonitor->left);
		s_offsetTop = std::min(s_offsetTop, lprcMonitor->top);
		return TRUE;
	}

	BOOL CALLBACK DoLoadNextImage(LPVOID screenId, LPRECT lpRect, LoadImageParam* lpParam)
	{
		DEBUG_LOG("LoadNexImages hMonitor: " << (int)hMonitor << " Rect: " << lpRect->left << ' ' << lpRect->top << ' ' << lpRect->right << ' ' << lpRect->bottom);
		HWND hWnd = lpParam->hWnd;
		if (lpParam->waitIndex <= 0) {
			std::shared_ptr<PhotoShow>& photoShow = s_photoShows[screenId];
			if (photoShow == nullptr) {
				RECT rect;
				GetClientRect(hWnd, &rect);
				DEBUG_LOG("GetClientRect: " << rect.left << ' ' << rect.top << ' ' << rect.right << ' ' << rect.bottom);
				// shuffle again to avoid same image sequence on different monitors
				if (s_shuffleImages) {
					std::shuffle(s_imageFileList.begin(), s_imageFileList.end(), std::random_device());
				}
				photoShow.reset(new PhotoShow(rect.right, rect.bottom, D2D1::RectF((FLOAT)lpRect->left, (FLOAT)lpRect->top, (FLOAT)lpRect->right, (FLOAT)lpRect->bottom), s_imageFileList), RefCntDeleter());
			}
			photoShow->LoadNextImage(hWnd);
			return lpParam->waitIndex < 0 ? TRUE : FALSE;
		}
		lpParam->waitIndex -= 1;
		return TRUE;
	}
	
	BOOL CALLBACK LoadNextImage(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
	{
		OffsetRect(lprcMonitor, -s_offsetLeft, -s_offsetTop);
		LoadImageParam* param = (LoadImageParam*)dwData;
		return DoLoadNextImage((LPVOID)hMonitor, lprcMonitor, param);
	}

	BOOL CALLBACK DoCallOnPaint(LPVOID screenId, LPRECT lpRect, HWND hWnd)
	{
		DEBUG_LOG("CallOnPaint hMonitor: " << (int)hMonitor << " Rect: " << lpRect->left << ' ' << lpRect->top << ' ' << lpRect->right << ' ' << lpRect->bottom);
		std::shared_ptr<PhotoShow>& photoShow = s_photoShows[screenId];
		if (photoShow != nullptr)
		{
			RECT r;
			photoShow->GetRect(&r);
			if (IntersectRect(&r, &r, lpRect) == TRUE)
			{
				photoShow->OnPaint(hWnd);
			}
		}
		return TRUE;
	}

	BOOL CALLBACK CallOnPaint(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
	{
		return DoCallOnPaint((LPVOID)hMonitor, lprcMonitor, (HWND)dwData);
	}

	static int currentIndex = 0;

	void StartSlideShow(HWND hWnd, bool isScreenSaver, const Configuration &config)
	{
		s_shuffleImages = config.shuffle;
		if (!isScreenSaver && config.transparency > 0) {
			SetWindowLong(hWnd, GWL_EXSTYLE, (GetWindowLong(hWnd, GWL_EXSTYLE) & ~(WS_EX_LAYERED | WS_EX_TRANSPARENT)) | WS_EX_LAYERED | (config.clickThrough ? WS_EX_TRANSPARENT : 0));
			if (config.clickThrough) {
				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			SetLayeredWindowAttributes(hWnd, 0, 255 - config.transparency, LWA_ALPHA);
		}

		s_imageFileList = buildFileList(config.folders, config.shuffle);

		if (isScreenSaver) {
			EnumDisplayMonitors(nullptr, nullptr, &GetOffsets, 0);
		}

		LoadImageParam param;
		param.hWnd = hWnd;
		param.waitIndex = -1;

		if (isScreenSaver) {
			EnumDisplayMonitors(nullptr, nullptr, &LoadNextImage, (LPARAM)&param);
		}
		else {
			RECT r;
			GetClientRect(hWnd, &r);
			DoLoadNextImage(0, &r, &param);
		}

		UINT_PTR timerId = isScreenSaver ? NEXT_IMAGE_ALL_DISPLAYS_TIMER_ID : NEXT_IMAGE_SINGLE_WINDOW_TIMER_ID;

#ifdef _DEBUG
		SetTimer(hWnd, timerId, 100, NULL);
#else
		SetTimer(hWnd, timerId, config.interval * 1000, NULL);
#endif
	}

	void StopSlideShow(HWND hWnd, bool isScreenSaver)
	{
		s_photoShows.clear();
		KillTimer(hWnd, isScreenSaver ? NEXT_IMAGE_ALL_DISPLAYS_TIMER_ID : NEXT_IMAGE_SINGLE_WINDOW_TIMER_ID);
		if (!isScreenSaver) {
			SetWindowLong(hWnd, GWL_EXSTYLE, (GetWindowLong(hWnd, GWL_EXSTYLE) & ~(WS_EX_LAYERED | WS_EX_TRANSPARENT)));
			SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

	void OnTimerTimeout(HWND hWnd, WPARAM wParam)
	{
		if (wParam == NEXT_IMAGE_ALL_DISPLAYS_TIMER_ID || wParam == NEXT_IMAGE_SINGLE_WINDOW_TIMER_ID) {
#ifdef _DEBUG
			static bool sentToBack = false;
			if (!sentToBack) {
				SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				SetTimer(hWnd, wParam, loadInterval * 1000, NULL);
				sentToBack = true;
			}
			else
#endif
			{
				if (currentIndex <= 0) {
					currentIndex = s_photoShows.size();
				}
				LoadImageParam param;
				param.hWnd = hWnd;
				param.waitIndex = --currentIndex;

				if (wParam == NEXT_IMAGE_ALL_DISPLAYS_TIMER_ID) {
					EnumDisplayMonitors(nullptr, nullptr, &LoadNextImage, (LPARAM)&param);
				}
				else {
					RECT r;
					GetClientRect(hWnd, &r);
					DoLoadNextImage(0, &r, &param);
				}
			}
		}
	}
}

#ifndef STANDALONE

LRESULT WINAPI
ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CREATE:
			StartSlideShow(hWnd, true, Configuration::Load());
			return S_OK;
		case WM_DESTROY:
			StopSlideShow(hWnd, true);
			break;
		case WM_TIMER:
			OnTimerTimeout(hWnd, wParam);
			return S_OK;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			if (BeginPaint(hWnd, &ps))
			{
				DEBUG_LOG("Dirty Rect: " << ps.rcPaint.left << ' ' << ps.rcPaint.top << ' ' << ps.rcPaint.right << ' ' << ps.rcPaint.bottom);
				EnumDisplayMonitors(ps.hdc, &ps.rcPaint, &CallOnPaint, (LPARAM)hWnd);
				EndPaint(hWnd, &ps);
			}
			return S_OK;
		}
#ifdef _DEBUG
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_MOUSEMOVE:
		case WM_NCACTIVATE:
		case WM_ACTIVATEAPP:
		case WM_ACTIVATE:
			return S_OK;
#endif
	}

	return DefScreenSaverProc(hWnd, message, wParam, lParam);
}
#endif // !STANDALONE

BOOL WINAPI
ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			auto config = Configuration::Load();

			SetDlgItemText(hDlg, IDC_FOLDERS, config.folders.c_str());
			CheckDlgButton(hDlg, IDC_SHUFFLE, config.shuffle ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemInt(hDlg, IDC_DELAY, config.interval, false);
			CheckDlgButton(hDlg, IDC_CLICK_THROUGH, config.clickThrough ? BST_CHECKED : BST_UNCHECKED);
			SendDlgItemMessage(hDlg, IDC_TRANSPARENCY, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(0, 255));
			SendDlgItemMessage(hDlg, IDC_TRANSPARENCY, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)config.transparency);

			return TRUE;
		}
		case WM_COMMAND:
		{
			int cmd = LOWORD(wParam);
			if (cmd == IDC_ADD)
			{
				TCHAR folder[MAX_PATH];
				BROWSEINFO bi = { 0 };
				bi.hwndOwner = hDlg;
				bi.pidlRoot = NULL;
				bi.pszDisplayName = folder;
				bi.lpszTitle = L"Choose a folder";
				bi.ulFlags = 0;
				bi.lpfn = NULL;
				bi.lParam = 0;
				LPITEMIDLIST pidlSelected = SHBrowseForFolder(&bi);
				if (pidlSelected != nullptr)
				{
					if (SHGetPathFromIDList(pidlSelected, folder))
					{
						std::wstring folders = GetDlgEditTextValue(hDlg, IDC_FOLDERS);
						if (folders.empty())
						{
							folders = folder;
						}
						else
						{
							folders += L"\r\n" + std::wstring(folder);
						}
						SetDlgItemText(hDlg, IDC_FOLDERS, folders.c_str());
					}
				}
			}
			else if (cmd == IDOK)
			{
				Configuration config;
				config.folders = GetDlgEditTextValue(hDlg, IDC_FOLDERS);
				config.shuffle = IsDlgButtonChecked(hDlg, IDC_SHUFFLE) == BST_CHECKED;
				config.interval = GetDlgItemInt(hDlg, IDC_DELAY, NULL, false);
				config.transparency = static_cast<uint8_t>(SendDlgItemMessage(hDlg, IDC_TRANSPARENCY, TBM_GETPOS, NULL, NULL));
				config.clickThrough = IsDlgButtonChecked(hDlg, IDC_CLICK_THROUGH) == BST_CHECKED;
				config.Save();

				EndDialog(hDlg, LOWORD(wParam) == IDOK);
				return TRUE;
			}
			else if (cmd == IDCANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam) == IDOK);
				return TRUE;
			}
		}
		}

	return FALSE;
}

BOOL WINAPI
RegisterDialogClasses(HANDLE hInst)
{
	return TRUE;
}

#ifdef STANDALONE

WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };
void ShowFullscreen(HWND hwnd, bool fullscreen, bool clickThrough)
{
	DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
	if (fullscreen) {
		MONITORINFO mi = { sizeof(mi) };
		if (GetWindowPlacement(hwnd, &g_wpPrev) && GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			if (clickThrough) {
				SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_CAPTION);
			} else {
				SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			}
			SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else if ((dwStyle & WS_OVERLAPPEDWINDOW) != WS_OVERLAPPEDWINDOW) {
		SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hwnd, &g_wpPrev);
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

static bool isRunning = false;

static LRESULT CALLBACK
WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_TIMER:
			OnTimerTimeout(hWnd, wParam);
			return S_OK;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			if (BeginPaint(hWnd, &ps))
			{
				if (isRunning) {
					DEBUG_LOG("Dirty Rect: " << ps.rcPaint.left << ' ' << ps.rcPaint.top << ' ' << ps.rcPaint.right << ' ' << ps.rcPaint.bottom);
					DoCallOnPaint(0, &ps.rcPaint, hWnd);
				}
				else {
					FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
				}
				EndPaint(hWnd, &ps);
			}
			return S_OK;
		}
		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE) {
				if (isRunning) {
					StopSlideShow(hWnd, false);
					ShowFullscreen(hWnd, false, false);
					InvalidateRect(hWnd, nullptr, true);
					isRunning = false;
				}
			}
			else if (wParam == VK_F8) {
				DialogBox(nullptr, MAKEINTRESOURCE(DLG_SCRNSAVECONFIGURE), hWnd, &ScreenSaverConfigureDialog);
			}
			else if (wParam == VK_RETURN && GetAsyncKeyState(VK_CONTROL) < 0) {
				if (!isRunning) {
					auto config = Configuration::Load();
					if (GetAsyncKeyState(VK_SHIFT) >= 0) {
						ShowFullscreen(hWnd, true, config.transparency > 0 && config.clickThrough);
					}
					StartSlideShow(hWnd, false, config);
					isRunning = true;
				}
			}
			return S_OK;
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static const TCHAR* windowClassName = TEXT("SimplePhotoShow Class");

int WINAPI
wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
	WNDCLASSEX wc;
	memset(&wc, 0, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = &WindowProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszClassName = windowClassName;
	wc.hIcon = (HICON)LoadImage(wc.hInstance, MAKEINTRESOURCE(ID_APP), IMAGE_ICON, 32, 32, LR_SHARED);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, TEXT("Window Registration Failed"), TEXT("ERROR!"), MB_OK | MB_ICONERROR);
		return -1;
	}

	HWND hwnd = CreateWindowEx(0, windowClassName, TEXT("Simple PhotoShow"),
		WS_OVERLAPPEDWINDOW,            // Window style
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, // Size and position
		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		NULL        // Additional application data
	);

	if (hwnd == NULL) {
		return 0;
	}

	BOOL USE_DARK_MODE = true;
	DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE, sizeof(USE_DARK_MODE));

	ShowWindow(hwnd, nCmdShow);

	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

#endif
