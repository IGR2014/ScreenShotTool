#include <windows.h>
#include <fstream>
#include <GL/gl.h>


#include "HookFunction.hpp"
#include "GLScreenshoter.hpp"


// C-tor
GLScreenshoter::GLScreenshoter() : endFunction(NULL),
								   endHook(new funcHook()) {

	wcscpy_s(logPath, MAX_PATH, dllPath);
	wcscat_s(logPath, MAX_PATH, L"\\Logs\\GLLog.txt");

	wcscpy_s(screenPath, MAX_PATH, dllPath);
	wcscat_s(screenPath, MAX_PATH, L"\\Screenshots\\GLScreenshot.jpg");

	globalGLScreenshoter = this;

}

// Hook DX function
BOOL DX9Screenshoter::hook() {

	HINSTANCE hOpenGL = GetModuleHandle(L"opengl32.dll");

	if (hOpenGL == NULL) {

		return FALSE;
	
	}

	HRESULT h;

	HMODULE hGL = LoadLibrary(L"opengl32");

	if (hGL == NULL) {

		return FALSE;
	
	}

#ifdef _WIN64

	endFunction = (OPENGLEND)endHook->install(,
											  (LPBYTE)hookedGLEnd,
											  19);
	VirtualProtect(endFunction,
				   19,
				   PAGE_EXECUTE_READWRITE,
				   &previousProtection);

#else

	endFunction = (OPENGLEND)endHook->install(,
											  (LPBYTE)hookedGLEnd,
											  5);
	VirtualProtect(endFunction,
				   5,
				   PAGE_EXECUTE_READWRITE,
				   &previousProtection);

#endif

	FreeModule(hGL);

	return TRUE;

}
