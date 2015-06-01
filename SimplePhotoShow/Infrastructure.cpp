#include "PhotoShow.h"
#include <scrnsave.h>
#include <commctrl.h>
#include <shlobj.h>
#include <memory>
#include <chrono>
#include <regex>
#include <algorithm>

#include "resource.h"

//define a Windows timer 
#define NEXT_IMAGE_TIMER_ID 1 
#define NEXT_ANIM_FRAME_TIMER_ID 2

#define ANIMATION_LENGTH 1000
#define ANIMATION_PRECISION 33	// frame per second

namespace
{
	bool GetRegistryValue(HKEY key, LPCTSTR name, std::wstring &value)
	{
		DWORD dwtype = 0;
		DWORD dsize = 0;
		int sizeOfChar = sizeof(wchar_t);
		if (RegQueryValueEx(key, name, NULL, &dwtype, NULL, &dsize) == ERROR_SUCCESS)
		{
			if (dwtype == REG_SZ || dwtype == REG_EXPAND_SZ)
			{
				value.resize(dsize / sizeOfChar);
				if (RegQueryValueEx(key, name, NULL, NULL, (LPBYTE)value.data(), &dsize) == ERROR_SUCCESS)
				{
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
		if (GetRegistryValue(key, name, intValue))
		{
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
		if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\SimpleSlideShow", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
		{
			GetRegistryValue(key, L"Folders", folders);
			GetRegistryValue(key, L"Interval", interval);
			GetRegistryValue(key, L"Shuffle", shuffle);
			RegCloseKey(key);
		}
	}

	void SetConfig(const std::wstring &folders, int interval, bool shuffle)
	{
		HKEY key;
		if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\SimpleSlideShow", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS)
		{
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
}

LRESULT WINAPI
ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static std::shared_ptr<PhotoShow> photoShow;
	static std::chrono::steady_clock::time_point animStart;
	static int loadInterval = 10;

	switch (message)
	{
		case WM_CREATE:
		{
			// get window dimensions
			RECT rect;
			GetClientRect(hWnd, &rect);

			std::wstring folders;
			bool shuffle;
			GetConfig(folders, loadInterval, shuffle);

			std::vector<std::wstring> folderList;
			if (!folders.empty())
			{
				std::copy(std::wsregex_token_iterator(folders.begin(), folders.end(), std::wregex(L"\\r\\n"), -1),
					std::wsregex_token_iterator(),
					std::back_inserter(folderList));
			}

			photoShow = std::make_shared<PhotoShow>(rect.right - rect.left, rect.bottom - rect.top, folderList, shuffle);
			photoShow->LoadNextImage(hWnd);

			//set timer to tick every 30 ms
#ifdef _DEBUG
			SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, 100, NULL);
#else
			SetTimer(hWnd, NEXT_ANIM_FRAME_TIMER_ID, 1000 / ANIMATION_PRECISION, NULL);
			SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, loadInterval * 1000, NULL);
#endif
			return S_OK;
		}
		case WM_DESTROY:
		{
			photoShow.reset();
			KillTimer(hWnd, NEXT_IMAGE_TIMER_ID);
			KillTimer(hWnd, NEXT_ANIM_FRAME_TIMER_ID);
			break;
		}
		case WM_PAINT:
			photoShow->OnPaint(hWnd);
			return S_OK;
		case WM_TIMER:
		{
			if (wParam == NEXT_IMAGE_TIMER_ID)
			{
#ifdef _DEBUG
				static bool sentToBack = false;
				if (!sentToBack) {
					SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, loadInterval * 1000, NULL);
					SetTimer(hWnd, NEXT_ANIM_FRAME_TIMER_ID, 1000 / ANIMATION_PRECISION, NULL);
					sentToBack = true;
				} else
#endif
				if (photoShow->LoadNextImage(hWnd))
				{
					animStart = std::chrono::steady_clock::now();
					SetTimer(hWnd, NEXT_ANIM_FRAME_TIMER_ID, 1000 / ANIMATION_PRECISION, NULL);
				}
			}
			else if (wParam == NEXT_ANIM_FRAME_TIMER_ID)
			{
				auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - animStart).count();
				if (duration >= ANIMATION_LENGTH)
				{
					KillTimer(hWnd, NEXT_ANIM_FRAME_TIMER_ID);
					photoShow->EndAnimation(hWnd);
				}
				else
				{
					photoShow->NextAnimationFrame(hWnd, duration / float(ANIMATION_LENGTH));
				}
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


#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version = '6.0.0.0' processorArchitecture = '*' publicKeyToken = '6595b64144ccf1df' language = '*'\"")
