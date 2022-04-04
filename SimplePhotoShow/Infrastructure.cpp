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

//define a Windows timer 
#define NEXT_IMAGE_TIMER_ID 1 

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
	bool GetRegistryValue(HKEY key, LPCTSTR name, std::wstring &value)
	{
		DWORD dwtype = 0;
		DWORD dsize = 0;
		int sizeOfChar = sizeof(wchar_t);
		if (RegQueryValueEx(key, name, NULL, &dwtype, NULL, &dsize) == ERROR_SUCCESS) {
			if (dwtype == REG_SZ || dwtype == REG_EXPAND_SZ) {
				value.resize(dsize / sizeOfChar);
				if (RegQueryValueEx(key, name, NULL, NULL, (LPBYTE)value.data(), &dsize) == ERROR_SUCCESS) {
					// the result includes last null byte
					value.resize(value.size() - 1);
					return true;
				}
			}
		}
		return false;
	}

	bool GetRegistryValue(HKEY key, LPCTSTR name, int32_t &value)
	{
		DWORD dwtype = 0;
		DWORD dsize = sizeof(value);
		return RegQueryValueEx(key, name, NULL, &dwtype, (LPBYTE)&value, &dsize) == ERROR_SUCCESS && dwtype == REG_DWORD;
	}

	bool GetRegistryValue(HKEY key, LPCTSTR name, bool &value)
	{
		int32_t intValue;
		if (GetRegistryValue(key, name, intValue)) {
			value = intValue != 0;
			return true;
		}
		return false;
	}

	bool SetRegistryValue(HKEY key, LPCTSTR name, const std::wstring &value)
	{
		return RegSetValueEx(key, name, 0, REG_SZ, (LPBYTE)value.c_str(), (value.size() + 1) * sizeof(wchar_t)) == ERROR_SUCCESS;
	}

	bool SetRegistryValue(HKEY key, LPCTSTR name, int32_t value)
	{
		return RegSetValueEx(key, name, 0, REG_DWORD, (LPBYTE)&value, sizeof(int32_t)) == ERROR_SUCCESS;
	}

	bool SetRegistryValue(HKEY key, LPCTSTR name, bool value)
	{
		return SetRegistryValue(key, name, value ? 1 : 0);
	}


	void GetConfig(std::wstring &folders, int &interval, bool &shuffle)
	{
		HKEY key;
		if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\SimpleSlideShow", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
			GetRegistryValue(key, L"Folders", folders);
			GetRegistryValue(key, L"Interval", interval);
			GetRegistryValue(key, L"Shuffle", shuffle);
			RegCloseKey(key);
		}
	}

	void SetConfig(const std::wstring &folders, int interval, bool shuffle)
	{
		HKEY key;
		if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\SimpleSlideShow", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
			SetRegistryValue(key, L"Folders", folders);
			SetRegistryValue(key, L"Interval", interval);
			SetRegistryValue(key, L"Shuffle", shuffle);
			RegCloseKey(key);
		}

	}

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

	bool s_shuffleImages;
	std::vector<std::wstring> s_imageFileList;
	std::map<HMONITOR, std::shared_ptr<PhotoShow>> s_photoShows;
	LONG s_offsetLeft;
	LONG s_offsetTop;

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

	BOOL CALLBACK LoadNextImages(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
	{
		OffsetRect(lprcMonitor, -s_offsetLeft, -s_offsetTop);

		DEBUG_LOG("LoadNexImages hMonitor: " << (int)hMonitor << " Rect: " << lprcMonitor->left << ' ' << lprcMonitor->top << ' ' << lprcMonitor->right << ' ' << lprcMonitor->bottom);
		LoadImageParam* param = (LoadImageParam*)dwData;
		HWND hWnd = param->hWnd;
		if (param->waitIndex <= 0) {
			std::shared_ptr<PhotoShow>& photoShow = s_photoShows[hMonitor];
			if (photoShow == nullptr) {
				RECT rect;
				GetClientRect(hWnd, &rect);
				DEBUG_LOG("GetClientRect: " << rect.left << ' ' << rect.top << ' ' << rect.right << ' ' << rect.bottom);
				// shuffle again to avoid same image sequence on different monitors
				if (s_shuffleImages) {
					std::shuffle(s_imageFileList.begin(), s_imageFileList.end(), std::random_device());
				}
				photoShow.reset(new PhotoShow(rect.right, rect.bottom, D2D1::RectF((FLOAT)lprcMonitor->left, (FLOAT)lprcMonitor->top, (FLOAT)lprcMonitor->right, (FLOAT)lprcMonitor->bottom), s_imageFileList), RefCntDeleter());
			}
			photoShow->LoadNextImage(hWnd);
			return param->waitIndex < 0 ? TRUE : FALSE;
		}
		param->waitIndex -= 1;
		return TRUE;
	}

	BOOL CALLBACK CallOnPaint(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
	{
		DEBUG_LOG("CallOnPaint hMonitor: " << (int)hMonitor << " Rect: " << lprcMonitor->left << ' ' << lprcMonitor->top << ' ' << lprcMonitor->right << ' ' << lprcMonitor->bottom);
		std::shared_ptr<PhotoShow>& photoShow = s_photoShows[hMonitor];
		if (photoShow != nullptr)
		{
			RECT r;
			photoShow->GetRect(&r);
			if (IntersectRect(&r, &r, lprcMonitor) == TRUE)
			{
				photoShow->OnPaint((HWND)dwData);
			}
		}
		return TRUE;
	}

	static int loadInterval = 10;
	static int currentIndex = 0;

	void StartSlideShow(HWND hWnd)
	{
		std::wstring folders;
		GetConfig(folders, loadInterval, s_shuffleImages);

		s_imageFileList = buildFileList(folders, false);

		EnumDisplayMonitors(nullptr, nullptr, &GetOffsets, 0);

		LoadImageParam param;
		param.hWnd = hWnd;
		param.waitIndex = -1;
		EnumDisplayMonitors(nullptr, nullptr, &LoadNextImages, (LPARAM)&param);

#ifdef _DEBUG
		SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, 100, NULL);
#else
		SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, loadInterval * 1000, NULL);
#endif
	}

	void StopSlideShow(HWND hWnd)
	{
		s_photoShows.clear();
		KillTimer(hWnd, NEXT_IMAGE_TIMER_ID);
	}

	void OnTimerTimeout(HWND hWnd, WPARAM wParam)
	{
		if (wParam == NEXT_IMAGE_TIMER_ID) {
#ifdef _DEBUG
			static bool sentToBack = false;
			if (!sentToBack) {
				SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, loadInterval * 1000, NULL);
				sentToBack = true;
			}
			else
#endif
			{
				if (currentIndex <= 0)
				{
					currentIndex = s_photoShows.size();
				}
				LoadImageParam param;
				param.hWnd = hWnd;
				param.waitIndex = --currentIndex;
				EnumDisplayMonitors(nullptr, nullptr, &LoadNextImages, (LPARAM)&param);
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
			StartSlideShow(hWnd);
			return S_OK;
		case WM_DESTROY:
			StopSlideShow(hWnd);
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
			std::wstring folders;
			int interval = 10;
			bool shuffle = true;
			GetConfig(folders, interval, shuffle);

			SetDlgItemText(hDlg, IDC_FOLDERS, folders.c_str());
			CheckDlgButton(hDlg, IDC_SHUFFLE, shuffle ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemInt(hDlg, IDC_DELAY, interval, false);
	
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
				std::wstring folders = GetDlgEditTextValue(hDlg, IDC_FOLDERS);
				bool shuffle = IsDlgButtonChecked(hDlg, IDC_SHUFFLE) == BST_CHECKED;
				int interval = GetDlgItemInt(hDlg, IDC_DELAY, NULL, false);

				SetConfig(folders, interval, shuffle);

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
void ShowFullscreen(HWND hwnd, bool fullscreen)
{
	DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
	if (fullscreen) {
		MONITORINFO mi = { sizeof(mi) };
		if (GetWindowPlacement(hwnd, &g_wpPrev) && GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else {
		SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hwnd, &g_wpPrev);
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

static bool isRunning = false;

LRESULT CALLBACK
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
					EnumDisplayMonitors(ps.hdc, &ps.rcPaint, &CallOnPaint, (LPARAM)hWnd);
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
					StopSlideShow(hWnd);
					ShowFullscreen(hWnd, false);
					InvalidateRect(hWnd, nullptr, true);
					isRunning = false;
				}
			}
			else if (wParam == VK_RETURN && GetKeyState(VK_CONTROL) != 0) {
				if (!isRunning) {
					ShowFullscreen(hWnd, true);
					StartSlideShow(hWnd);
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
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
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
