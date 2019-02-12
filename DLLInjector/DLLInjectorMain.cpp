#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// Timer
#define WINDOW_TIMER			1

#define ID_OPEN_FOLDER_BUTTON	1000

#include <windows.h>
#include <TlHelp32.h>

#include "DLLInjector.hpp"

// Program title
static			WCHAR*	programTitle			= L"DLLInjector v0.1";

// Exe name label text
static			WCHAR*	exeNameLabel			= L"Name:";

// Exe PID label text
static			WCHAR*	exePIDLabel				= L"Process ID:";

// Screenshots folder
static			WCHAR*	screenshotsFolder		= L"\\Screenshots\\";

// x86 DLL name
static			WCHAR*	x86DLLName				= L"\\hookX86.dll";
// x64 DLL name
static			WCHAR*	x64DLLName				= L"\\hookX64.dll";

// DLL hooked flag
static			BOOL	processHooked			= FALSE;
// Hooked process number
static			DWORD	processHookedNumber		= -1;
// Hooked process ID
static			DWORD	processHookedID			= 0;

// Count of processes to search
static const	DWORD	processCount			= 8;
// Processes to search
static const	WCHAR*	processNames[8]			= {L"dx11.exe",
												   L"dx9.exe",
												   L"dx11 x64.exe",
												   L"dx9 x64.exe",
												   L"dota2.exe",
												   L"Hearthstone.exe",
												   L"League of Legends.exe",
												   L"WorldOfTanks.exe"};

// Screenshots folder path
static			WCHAR	screenshotsFolderPath[MAX_PATH];


// Set privileige for current process
BOOL setDebugPrivilege(HANDLE hToken, LPCTSTR szPrivName, BOOL fEnable) {

	// token privileiges struct
	TOKEN_PRIVILEGES tp;
	// Ste privilaiges count to 1
	tp.PrivilegeCount = 1;

	// Lookup for privileige value
	if (!LookupPrivilegeValue(NULL, szPrivName, &tp.Privileges[0].Luid)) {

		// Return with error
		return FALSE;

	}

	// Set privileige enable or disable
	tp.Privileges[0].Attributes = fEnable ? SE_PRIVILEGE_ENABLED : 0;

	// Adjust token privileiges
	if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL)) {

		// Return with error
		return FALSE;

	}

	// Return success
	return TRUE;

}

// Check if process is x64
BOOL isX64Process(DWORD processID) {

	// Return result
	BOOL result = FALSE;
	// Pointer to IsWow64Process() function in kernel32.dll
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
	// Create pointer to fucntion
	LPFN_ISWOW64PROCESS fnIsWow64Process;

	// Open process to get handle
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS |
								  PROCESS_VM_OPERATION |
								  PROCESS_VM_READ |
								  PROCESS_VM_WRITE,
								  FALSE,
								  processID);

	// check if process handle is correct
	if (hProcess == INVALID_HANDLE_VALUE) {
	
		// Error
		return FALSE;
	
	}

	// Get pointer to function
	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(L"kernel32"),
														   "IsWow64Process");
	// system info structure
	SYSTEM_INFO sysInfo;
	// Get native system info to check architecture
	GetNativeSystemInfo(&sysInfo);
	// If architecture is x64
	if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 ||
		sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {

		// If IsWow64Process() exists
		if (fnIsWow64Process != NULL) {

			// If result is not false
			// x86 on x86 and x64 on x64 are false
			// And x86 on x674 is true
			if (fnIsWow64Process(hProcess, &result) != FALSE) {

				// Cleanup
				CloseHandle(hProcess);

				// Return function result compared to false
				// If false then app is x64 on x64 or x86 on x86
				// And if true than app is x86 on x64
				return (result == FALSE);

			}

		}
			
	}

	// Cleanup
	CloseHandle(hProcess);

	// Return error
	return FALSE;

}

// Get process ID
BOOL getProcessID(LPCWSTR* processNames,
				  const DWORD &numberOfNames,
				  DWORD &processNumber,
				  DWORD &processID) {
	
	// Process entry struct
	PROCESSENTRY32 processEntry = {0};

	// Create process snapshot
	HANDLE snapHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	// If unable to create process snapshot
	if (snapHandle == INVALID_HANDLE_VALUE) {
		
		// Return error
		return FALSE;
	
	}
	
	// Start looking for process
	processEntry.dwSize = sizeof(PROCESSENTRY32);
	Process32First(snapHandle, &processEntry);
	
	// Search process
	while (Process32Next(snapHandle, &processEntry)) {

		// Check all names
		for (DWORD i = 0; i < numberOfNames; ++i) {

			// If process found
			if (wcscmp(processEntry.szExeFile, processNames[i]) == 0) {
				
				// Close handle
				CloseHandle(snapHandle);
				// Set return values
				processNumber = i;
				processID = processEntry.th32ProcessID;
				// Success
				return TRUE;
			
			}

		}

	}

	// Close handle
	CloseHandle(snapHandle);
	
	// No any process found
	processNumber = -1;
	processID = 0;

	// Return error
	return FALSE;

}

// Window callback function
LRESULT CALLBACK MainWinProc(HWND, UINT, WPARAM, LPARAM);

// Win main function
int WINAPI WinMain(HINSTANCE hInst,
				   HINSTANCE unused,
				   LPSTR lpCmdLine,
				   int nShowCmd) {

	// Window class
	WNDCLASS wc;
	wc.style			= 0;
	wc.lpfnWndProc		= MainWinProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hInst;
	wc.hIcon			= LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor			= NULL;
	wc.hbrBackground	= (HBRUSH)(COLOR_WINDOW);
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= programTitle;

	// Register window class
	if (!RegisterClass(&wc)) {
		
		// Return error
		return FALSE;
	
	}

	// Create a window
	HWND hWnd = CreateWindow(programTitle,
							 programTitle,
							 WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX,
							 CW_USEDEFAULT,
							 CW_USEDEFAULT,
							 400,
							 180,
							 NULL,
							 NULL,
							 hInst,
							 NULL);

	// check if window exists
	if (!hWnd) {
		
		// Return error
		return FALSE;

	}

	// Show window
	ShowWindow(hWnd, nShowCmd);
	// and redraw it
	UpdateWindow(hWnd);

	// Message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	
	}
	
	return (int)msg.wParam;

}

HWND g_HWND = NULL;
// Search window handle by process ID
BOOL CALLBACK EnumWindowsProcMy(HWND hWnd, LPARAM lParam) {

    DWORD lpdwProcessId;
    GetWindowThreadProcessId(hWnd, &lpdwProcessId);
    if(lpdwProcessId == lParam) {

        g_HWND = hWnd;
        return FALSE;
	
	}
	
	return TRUE;

}

static		HWND			pathEdit		= NULL;
static		HWND			openButton		= NULL;
static		HWND			exeName			= NULL;
static		HWND			exePID			= NULL;


static		PAINTSTRUCT		ps;


static		DLLInjector		injector;


// Window callback function
LRESULT CALLBACK MainWinProc(HWND hWnd,
							 UINT msg,
							 WPARAM wParam,
							 LPARAM lParam) {

	switch (msg) {

		case WM_CREATE: {

			g_HWND = hWnd;

			// Set timer for 1 sec
			SetTimer(hWnd, WINDOW_TIMER, 1000, NULL);

			// Get current process token
			HANDLE hCurrentProc = GetCurrentProcess();
			HANDLE token;
			OpenProcessToken(hCurrentProc,
							 TOKEN_QUERY |
							 TOKEN_ADJUST_PRIVILEGES,
							 &token);

			// Set debug privilegies for current process
			if (!setDebugPrivilege(token, SE_DEBUG_NAME, TRUE)) {
			
				// Error
				MessageBox(hWnd,
						   L"Невозможно установить привелегии для процесса!",
						   L"Критическая ошибка! :(",
						   MB_OK);
				PostQuitMessage(0);
			
			}

			// Create .EXE pID label
			exePID = CreateWindow(L"STATIC",
								  exePIDLabel,
								  WS_CHILD |
								  WS_VISIBLE |
								  WS_TABSTOP,
								  84, 15,
								  300, 20,
								  hWnd, 
								  NULL,
								  (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
								  NULL);

			// Create .EXE name label
			exeName = CreateWindow(L"STATIC",
								   exeNameLabel,
								   WS_CHILD |
								   WS_VISIBLE |
								   WS_TABSTOP,
								   84, 45,
								   300, 20,
								   hWnd,
								   NULL,
								   (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
								   NULL);

			// Create path edit
			pathEdit = CreateWindow(L"EDIT",
									L"",
									WS_CHILD |
									WS_VISIBLE |
									WS_BORDER |
									ES_READONLY |
									ES_AUTOHSCROLL,
									10, 84,
									370, 20,
									hWnd,
									NULL,
									(HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
									NULL);
			// Get current directory
			GetCurrentDirectory(MAX_PATH, screenshotsFolderPath);
			wcscat_s(screenshotsFolderPath, MAX_PATH, screenshotsFolder);
			// Set text to path edit
			SetWindowText(pathEdit, screenshotsFolderPath); 

			// create edit button
			openButton = CreateWindow(L"BUTTON",
									  L"Открыть",
									  WS_TABSTOP |
									  WS_VISIBLE |
									  WS_CHILD,
									  280, 114,
									  100, 30,
									  hWnd,
									  (HMENU)ID_OPEN_FOLDER_BUTTON,
									  (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
									  NULL);

		} break;
		case WM_TIMER: {
		
			// If timer ticked
			if (wParam == WINDOW_TIMER) {
			
				BOOL getProcess = FALSE;

				if (processHooked == FALSE) {

					getProcess = getProcessID(processNames,
											  processCount,
											  processHookedNumber,
											  processHookedID);

					if (getProcess == TRUE) {

						processHooked = TRUE;

						WCHAR tempBuf[MAX_PATH];
						WCHAR tempBuf2[MAX_PATH];
						wcscpy_s(tempBuf, MAX_PATH, exeNameLabel);
						wcscat_s(tempBuf, MAX_PATH, L"\t\t");
						wcscat_s(tempBuf, MAX_PATH, processNames[processHookedNumber]);
						SetWindowText(exeName, tempBuf);
						wcscpy_s(tempBuf, MAX_PATH, exePIDLabel);
						wcscat_s(tempBuf, MAX_PATH, L"\t");
						wsprintf(tempBuf2, L"0x%04X (%d)", processHookedID, processHookedID);
						wcscat_s(tempBuf, MAX_PATH, tempBuf2);
						SetWindowText(exePID, tempBuf);

						EnumWindows(EnumWindowsProcMy, processHookedID);
						InvalidateRect(hWnd, NULL, TRUE);

						WCHAR currentPath[MAX_PATH];
						GetCurrentDirectory(MAX_PATH, currentPath);

						if (isX64Process(processHookedID) == TRUE) {
						
							wcscat_s(currentPath, MAX_PATH, x64DLLName);

							WCHAR buf[2048];
							WCHAR buf2[32];
							wcscpy_s(buf, 2048, L"-d ");
							wcscat_s(buf, 2048, currentPath);
							wcscat_s(buf, 2048, L" -f ");
							_itow_s(processHookedID, buf2, 2048, 10);
							wcscat_s(buf, 2048, buf2);

							ShellExecuteW(NULL,
										  NULL,
										  L"x64Patcher.exe",
										  buf,
										  NULL,
										  SW_HIDE);

						} else {
						
							wcscat_s(currentPath, MAX_PATH, x86DLLName);

							if (!injector.setProcessByID(processHookedID)) {
							
								MessageBox(hWnd, L"Процесс не существует!", L"Ошибка! :(", MB_OK);
								break;

							}

							if (!injector.setDLLByName(currentPath)) {

								MessageBox(hWnd, L"DLL не найдена!", L"Ошибка! :(", MB_OK);
								break;
								
							}

							if (!injector.inject()) {
								
								MessageBox(hWnd, L"Ошибка внедрения!", L"Ошибка! :(", MB_OK);
								break;
								
							}

						}

						WCHAR newWindowTitle[MAX_PATH];
						wcscpy_s(newWindowTitle, 17, programTitle);
						wcscat_s(newWindowTitle, MAX_PATH, L" [");
						wcscat_s(newWindowTitle,
								 MAX_PATH,
								 processNames[processHookedNumber]);
						wcscat_s(newWindowTitle, MAX_PATH, L"]");
						SetWindowText(hWnd, newWindowTitle);

					}

				} else {

					DWORD procNum	= -1;
					DWORD procID	= 0;

					getProcess = getProcessID(&processNames[processHookedNumber],
												 1,
												 procNum,
												 procID);

					if (procID != processHookedID) {

						processHooked = FALSE;

						processHookedNumber		= -1;
						processHookedID			= 0;

					
					}

					if (processHooked == FALSE) {
					
						g_HWND = hWnd;
						InvalidateRect(hWnd, NULL, TRUE);

						SetWindowText(hWnd, programTitle);

						SetWindowText(exeName, exeNameLabel);
						SetWindowText(exePID, exePIDLabel);
					
					}

				}
			
			}

		} break;
		case WM_PAINT: {

			HICON hIcon = (HICON)GetClassLongPtr(g_HWND, GCLP_HICON);
			HICON hSmallIcon = (HICON)GetClassLongPtr(g_HWND, GCLP_HICONSM);
			SetClassLongPtr(hWnd, GCLP_HICONSM, (LONG)hSmallIcon);
			HDC hdc = BeginPaint(hWnd, &ps);
			DrawIconEx(hdc, 10, 10, hIcon, 64, 64, 0, NULL, DI_NORMAL);
			EndPaint(hWnd, &ps);

		} break;
		case WM_COMMAND: {

			if (wParam == ID_OPEN_FOLDER_BUTTON) {
			
				ShellExecuteW(NULL,
							  NULL,
							  L"explorer.exe",
							  screenshotsFolderPath,
							  NULL,
							  SW_SHOWNORMAL);
			
			}

		} break;
		case WM_DESTROY: {

			KillTimer(hWnd, WINDOW_TIMER);
			/*if (!injector.eject()) {
			
				MessageBox(hWnd, L"Невозможно отключить DLL!", L"Критическая ошибка!", MB_OK);
			
			}*/
			PostQuitMessage(0);

		} break;
		default: {

		} break;
	
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);

}