#include "PhotoShow.h"
#include <scrnsave.h>
#include <memory>

//define a Windows timer 
#define TIMER_ID 1 


LRESULT WINAPI
ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static std::shared_ptr<PhotoShow> photoShow;

	switch (message)
	{
		case WM_CREATE:
		{
			// get window dimensions
			RECT rect;
			GetClientRect(hWnd, &rect);
			photoShow = std::make_shared<PhotoShow>(rect.right - rect.left, rect.bottom - rect.top);
			//get configuration from registry
			//GetConfig();
			//long style;
			//style = GetWindowLong(hWnd, GWL_EXSTYLE);
			//style &= ~WS_EX_TOPMOST;
			//SetWindowLong(hWnd, GWL_EXSTYLE, style);

			//style = GetWindowLong(hWnd, GWL_STYLE);
			//style &= ~WS_POPUP;
			//style |= WS_OVERLAPPEDWINDOW;
			//SetWindowLong(hWnd, GWL_STYLE, style);

			//set timer to tick every 30 ms
			SetTimer(hWnd, TIMER_ID, 3000, NULL);
			return S_OK;
		}
		case WM_DESTROY:
		{
			photoShow.reset();
			KillTimer(hWnd, TIMER_ID);
			break;
		}
		case WM_PAINT:
			photoShow->OnPaint(hWnd);
			return S_OK;
		case WM_TIMER:
		{
#ifdef _DEBUG
			static bool sentToBack = false;
			if (!sentToBack) {
				SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				sentToBack = true;
			}
#endif
			if (photoShow->LoadNextImage(hWnd))
			{
				RECT r;
				GetClientRect(hWnd, &r);
				InvalidateRect(hWnd, &r, false);
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

	//InitCommonControls();  
	//would need this for slider bars or other common controls

	//HWND aCheck;

	switch (message)
	{

	case WM_INITDIALOG:
		LoadString(hMainInstance, IDS_DESCRIPTION, szAppName, 40);

		//GetConfig();

		//aCheck = GetDlgItem(hDlg, IDC_TUMBLE);
		//SendMessage(aCheck, BM_SETCHECK, bTumble ? BST_CHECKED : BST_UNCHECKED, 0);

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{

		//case IDC_TUMBLE:
		//	bTumble = (IsDlgButtonChecked(hDlg, IDC_TUMBLE) == BST_CHECKED);
		//	return TRUE;

			//cases for other controls would go here

		case IDOK:
		//	WriteConfig(hDlg);      //get info from controls
			EndDialog(hDlg, LOWORD(wParam) == IDOK);
			return TRUE;

		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam) == IDOK);
			return TRUE;
		}

	}

	return FALSE;
}

BOOL WINAPI
RegisterDialogClasses(HANDLE hInst)
{
	return TRUE;
}
