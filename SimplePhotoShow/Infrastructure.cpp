#include "PhotoShow.h"
#include <scrnsave.h>
#include <memory>

//define a Windows timer 
#define TIMER_ID 1 


LRESULT WINAPI
ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static std::shared_ptr<PhotoShow> photoShow;

	RECT rect;
	switch (message) {
	case WM_CREATE:
		// get window dimensions
		GetClientRect(hWnd, &rect);
		photoShow = std::make_shared<PhotoShow>(rect.right - rect.left, rect.bottom - rect.top);
		photoShow->LoadNextImage(hWnd);
		//get configuration from registry
		//GetConfig();

		//set timer to tick every 30 ms
		SetTimer(hWnd, TIMER_ID, 100, NULL);
		return 0;
	case WM_DESTROY:
		photoShow.reset();
		KillTimer(hWnd, TIMER_ID);
		return 0;
	case WM_TIMER:
		photoShow->OnPaint(hWnd);
		return 0;
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
