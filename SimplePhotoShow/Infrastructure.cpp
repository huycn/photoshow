#include <windows.h>
#include <scrnsave.h>
#include <gl/GL.h>
#include <gl/GLU.h>

//globals used by the function below to hold the screen size
int Width;
int Height;

//define a Windows timer 
#define TIMER_ID 1 

static void InitGL(HWND hWnd, HDC & hDC, HGLRC & hRC)
{
	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof pfd);
	pfd.nSize = sizeof pfd;
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;

	hDC = GetDC(hWnd);

	int i = ChoosePixelFormat(hDC, &pfd);
	SetPixelFormat(hDC, i, &pfd);

	hRC = wglCreateContext(hDC);
	wglMakeCurrent(hDC, hRC);
}

static void CloseGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);
	ReleaseDC(hWnd, hDC);
}

static void SetupAnimation(int Width, int Height)
{
	//window resizing stuff
	glViewport(0, 0, (GLsizei)Width, (GLsizei)Height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(-300, 300, -240, 240, 25, 75);
	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();
	gluLookAt(0.0, 0.0, 50.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
	//camera xyz, the xyz to look at, and the up vector (+y is up)

	//background
	glClearColor(0.0, 0.0, 0.0, 0.0); //0.0s is black


	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);

	//no need to initialize any objects
	//but this is where I'd do it

	glColor3f(0.1, 1.0, 0.3); //green

}

static GLfloat spin = 0;   //a global to keep track of the square's spinning

static void OnTimer(HDC hDC) //increment and display
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	spin = spin + 1;

	glPushMatrix();
	glRotatef(spin, 0.0, 0.0, 1.0);

	glPushMatrix();

	glTranslatef(150, 0, 0);

	//if (bTumble)
	//	glRotatef(spin * -3.0, 0.0, 0.0, 1.0);
	//else
		glRotatef(spin * -1.0, 0.0, 0.0, 1.0);

	//draw the square (rotated to be a diamond)

	float xvals[] = { -30.0, 0.0, 30.0, 0.0 };
	float yvals[] = { 0.0, -30.0, 0.0, 30.0 };

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBegin(GL_POLYGON);
	for (int i = 0; i < 4; i++)
		glVertex2f(xvals[i], yvals[i]);
	glEnd();

	glPopMatrix();

	glFlush();
	SwapBuffers(hDC);
	glPopMatrix();
}

static void CleanupAnimation()
{
	//didn't create any objects, so no need to clean them up
}

LRESULT WINAPI
ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HDC hDC;
	static HGLRC hRC;
	static RECT rect;

	switch (message) {

	case WM_CREATE:
		// get window dimensions
		GetClientRect(hWnd, &rect);
		Width = rect.right;
		Height = rect.bottom;

		//get configuration from registry
		//GetConfig();

		// setup OpenGL, then animation
		InitGL(hWnd, hDC, hRC);
		SetupAnimation(Width, Height);

		//set timer to tick every 30 ms
		SetTimer(hWnd, TIMER_ID, 30, NULL);
		return 0;

	case WM_DESTROY:
		KillTimer(hWnd, TIMER_ID);
		CleanupAnimation();
		CloseGL(hWnd, hDC, hRC);
		return 0;

	case WM_TIMER:
		OnTimer(hDC);       //animate!      
		return 0;
	}

	return DefScreenSaverProc(hWnd, message, wParam, lParam);
}

BOOL WINAPI
ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{

	//InitCommonControls();  
	//would need this for slider bars or other common controls

	HWND aCheck;

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
