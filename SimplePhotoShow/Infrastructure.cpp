#include "PhotoShow.h"
#include <scrnsave.h>
#include <memory>
#include <chrono>

//define a Windows timer 
#define NEXT_IMAGE_TIMER_ID 1 
#define NEXT_ANIM_FRAME_TIMER_ID 2

#define LOAD_INTERVAL 10
#define ANIMATION_LENGTH 1000
#define ANIMATION_PRECISION 33	// frame per second

LRESULT WINAPI
ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static std::shared_ptr<PhotoShow> photoShow;
	static std::chrono::steady_clock::time_point animStart;

	switch (message)
	{
		case WM_CREATE:
		{
			// get window dimensions
			RECT rect;
			GetClientRect(hWnd, &rect);
			photoShow = std::make_shared<PhotoShow>(rect.right - rect.left, rect.bottom - rect.top);
			photoShow->LoadNextImage(hWnd);

			//set timer to tick every 30 ms
#ifdef _DEBUG
			SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, 100, NULL);
#else
			SetTimer(hWnd, NEXT_ANIM_FRAME_TIMER_ID, 1000 / ANIMATION_PRECISION, NULL);
			SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, LOAD_INTERVAL * 1000, NULL);
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
					SetTimer(hWnd, NEXT_IMAGE_TIMER_ID, LOAD_INTERVAL * 1000, NULL);
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
