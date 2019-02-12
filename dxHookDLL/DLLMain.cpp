#include <windows.h>
#include <Shlwapi.h>
#include <fstream>
#include <d3dx9.h>
#include <d3dx11.h>


#include "DX9Screenshoter.hpp"
#include "DX11Screenshoter.hpp"


#pragma comment(lib, "Shlwapi.lib")


typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;


std::wfstream*	logFile;


static	WCHAR	dllPath[MAX_PATH];
		WCHAR	screenPath[MAX_PATH];
		WCHAR	logFilePath[MAX_PATH];


static	BOOL	runSearch = TRUE;


BOOL __stdcall DllMain(HINSTANCE	hInst,
					   DWORD		ul_reason_for_call,
					   LPVOID		lpReserved) {

	HANDLE hHookDX9Thread	= INVALID_HANDLE_VALUE;
	HANDLE hHookDX11Thread	= INVALID_HANDLE_VALUE;

	switch (ul_reason_for_call) {

		case DLL_PROCESS_ATTACH: {

			GetModuleFileNameW(hInst, dllPath, MAX_PATH);
			PathRemoveFileSpecW(dllPath);

			DWORD dwVersion = GetVersion();
			DWORD dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
			DWORD dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));
	
			DWORD dwBuild = 0;
			if (dwVersion < 0x80000000) {

				dwBuild = (DWORD)(HIWORD(dwVersion));

			}

			wcscpy_s(logFilePath, MAX_PATH, dllPath);
			wcscat_s(logFilePath, MAX_PATH, L"\\Logs\\Log.txt");

			wcscpy_s(screenPath, MAX_PATH, dllPath);
			wcscat_s(screenPath, MAX_PATH, L"\\Screenshots\\");

			logFile = new std::wfstream();
			logFile->open(logFilePath, std::ios::out);
			*logFile << (WCHAR)0xEF << (WCHAR)0xBB << (WCHAR)0xBF;

			logFile->flush();
			*logFile << L"======================================\n";
			*logFile << L"OS Windows version:\t";
			*logFile << dwMajorVersion;
			*logFile << L".";
			*logFile << dwMinorVersion;
			*logFile << L" (build ";
			*logFile << dwBuild;
			*logFile << L")\n";

			BOOL bIsWow64 = FALSE;
			fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(L"kernel32"), "IsWow64Process");
			if(NULL != fnIsWow64Process) {

				if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64)) {

					*logFile << L"Application bitness: UNKNOWN!\n\n";

				} else {

					*logFile << L"Application bitness:\t";
					*logFile << ((bIsWow64 == TRUE) ? L"x86 (32)" : L"x64");
					*logFile << L"\n\n";

				}

			}

			*logFile << L"DLL path:\t\t";
			//*logFile << dllPath;
			*logFile << std::endl;
			*logFile << std::endl;

			WCHAR buffer[MAX_PATH];
			GetModuleFileNameW(GetModuleHandle(NULL), buffer, MAX_PATH);

			*logFile << L"Hook target:\t\t";
			//*logFile << buffer;
			*logFile << std::endl;
			*logFile << L"======================================\n";
			logFile->flush();

			hHookDX9Thread = CreateThread(NULL,
										  0,
										  &HookDX9,
										  &runSearch,
										  0,
										  NULL);

			hHookDX11Thread = CreateThread(NULL,
										   0,
										   &HookDX11,
										   &runSearch,
										   0,
										   NULL);

		} break;
		case DLL_PROCESS_DETACH: {

			CloseHandle(hHookDX9Thread);
			CloseHandle(hHookDX11Thread);
			runSearch = FALSE;

		} break;

	}

	return TRUE;

}
