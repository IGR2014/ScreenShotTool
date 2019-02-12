#include <windows.h>
#include <fstream>
#include <d3dx9.h>
#include <dxerr.h>


#pragma comment(lib, "dxerr.lib")


#include "HookFunction.hpp"
#include "DX9Screenshoter.hpp"


//	Global DX9Screenshoter pointer
static	DX9Screenshoter*	global9Screenshoter			= NULL;
//	Global screenshot name
static	WCHAR				screenshotName[MAX_PATH];


DWORD __stdcall writeScreenshot(LPVOID lpParam) {

	
	IDirect3DSurface9* surface = (IDirect3DSurface9*)lpParam;

	
	HRESULT h;

	
	h = D3DXSaveSurfaceToFile(screenshotName,
							  D3DXIFF_JPG,
							  surface,
							  NULL,
							  NULL);
	
	if (FAILED(h)) {
	
		*logFile << L"\tERROR:\tUnable to save screenshot:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";
		
	}

	
	surface->Release();

	*logFile << L"\tScreenshot:\t\tOK\n";
	*logFile << L"======================================\n";

	return 0;

}


////////////////////////////////////////////////////////////////
//
//	IDirect3DDevice9::EndScene() replace function
//
HRESULT __stdcall hookedEndScene9(IDirect3DDevice9*	d3dDevice) {

	logFile->flush();
	*logFile << L"======================================\n";
	*logFile << L"Start taking DirectX 9 screenshot...\n";

	// Error value
	HRESULT h;

	// Surfaces
	IDirect3DSurface9* surface		= NULL;
	IDirect3DSurface9* renderTarget	= NULL;
	// Display mode
	D3DDISPLAYMODE d3ddm;

	*logFile << L"Getting display mode...\n";

	// Get display mode
	h = d3dDevice->GetDisplayMode(0, &d3ddm);
	// Check if success
	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to get display mode:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";

	}

	*logFile << L"\tDisplay mode:\t\tOK\n";
	*logFile << L"Getting render target...\n";

	// Get render target
	h = d3dDevice->GetRenderTarget(0, &renderTarget);
	// Check if success
	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to get render target:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";
		
	}

	*logFile << L"\tRender target:\t\tOK\n";
	*logFile << L"Creating texture...\n";

	// Create surface from screen
	h = d3dDevice->CreateOffscreenPlainSurface(d3ddm.Width,
											   d3ddm.Height,
											   d3ddm.Format,
											   D3DPOOL_SYSTEMMEM,
											   &surface,
											   NULL);
	// Check if success
	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to create texture:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";
		
	}

	*logFile << L"\tTexture:\t\tOK\n";
	*logFile << L"Getting buffer...\n";

	// Get back buffer data
	h = d3dDevice->GetBackBuffer(0,
								 0,
								 D3DBACKBUFFER_TYPE_MONO,
								 &surface);
	// Check if success
	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to get buffer:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";

	}

	*logFile << L"\tBuffer:\t\t\tOK\n";
	*logFile << L"Saving screenshot...\n";

	// Form screenshot name
	wcscpy_s(screenshotName, MAX_PATH, screenPath);
	wcscat_s(screenshotName, MAX_PATH, L"DX9Screenshot.jpg");

	CreateThread(NULL,
				 0,
				 (LPTHREAD_START_ROUTINE)writeScreenshot,
				 surface,
				 NULL,
				 NULL);

	// Save screenshot to file
	/*h = D3DXSaveSurfaceToFile(screenshotName,
							  D3DXIFF_JPG,
							  surface,
							  NULL,
							  NULL);
	// Check if success
	if (FAILED(h)) {
	
		*logFile << L"\tERROR:\tUnable to save screenshot:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";
		
	}

	// Release render target
	renderTarget->Release();
	// Release surface
	surface->Release();*/

	// Release render target
	renderTarget->Release();

	// Release hook
	global9Screenshoter->unHook();

	*logFile << L"\tScreenshot:\t\tOK\n";
	*logFile << L"======================================\n";
	logFile->flush();

	// Call original function
	return d3dDevice->EndScene();

}


////////////////////////////////////////////////////////////////////
//
//	C-tor
//
DX9Screenshoter::DX9Screenshoter() : d3d9(NULL),
									 d3d9Device(NULL),
									 endScene9Function(NULL),
									 d3d9DeviceVTable(NULL),
									 endScene9Hook(new funcHook()) {

	// Initialize global pointer with this instance of class
	global9Screenshoter = this;

	logFile->flush();
	*logFile << L"DirectX 9 screenshoter initialized." << std::endl;
	logFile->flush();

}

////////////////////////////////
//
//	Look for DX functions
//
BOOL DX9Screenshoter::search() {

	logFile->flush();
	*logFile << L"======================================\n";
	*logFile << L"DirectX 9 searching started.\n";
	*logFile << L"Looking for d3d9.dll in application...\n";

	// Get d3d9.dll module handle
	HINSTANCE hD3D9 = GetModuleHandle(L"d3d9.dll");
	// Check if exists
	if (hD3D9 == NULL) {

		*logFile << L"\td3d9.dll was not found :(\n";
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;
	
	}

	*logFile << L"\td3d9.dll found.\n\tTrying to search functions...\n";
	*logFile << L"Loading d3d9.dll for our purpose.\n";

	// Errors value
	HRESULT h;

	// Load d3d9.dll
	HMODULE hD3D = LoadLibrary(L"d3d9");
	// check if exists
	if (hD3D == NULL) {

		*logFile << L"\tUnable to load d3d9.dll.\n";
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;
	
	}

	*logFile << L"\td3d9.dll loaded.\n\tSearching functions...\n";

	// Get pointer to Direct3DCreate9() function
	DIRECT3DCREATE9 d3dCreate9 = (DIRECT3DCREATE9)GetProcAddress(hD3D, "Direct3DCreate9");
	// Call it to create Direct3D9 object
	d3d9 = d3dCreate9(D3D_SDK_VERSION);
	// Display mode struct
	D3DDISPLAYMODE d3ddm;

	*logFile << L"Getting adapter mode...\n";

	// Get display mode for defaul adapter
	h = d3d9->GetAdapterDisplayMode(0, &d3ddm);
	// Check if success
	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to get adapter mode:\n";
		*logFile << L"\t" << DXGetErrorString(h) << std::endl;
		*logFile << L"\t" << DXGetErrorDescription(h) << std::endl;
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;

	}

	*logFile << L"\tAdapter mode:\t\tOK\n";

	// Create temporary window
	HWND hWnd = CreateWindow(L"STATIC",
							 L"",
							 WS_EX_TOPMOST | WS_POPUP,
							 0, 0,
							 0, 0,
							 NULL,
							 NULL,
							 NULL,
							 NULL);

	// D3D parameters structure
	D3DPRESENT_PARAMETERS d3dpp; 
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.hDeviceWindow				= hWnd;
	d3dpp.Windowed					= TRUE;
	d3dpp.BackBufferWidth			= d3ddm.Width;
	d3dpp.BackBufferHeight			= d3ddm.Height;
	d3dpp.BackBufferFormat			= d3ddm.Format;
	d3dpp.SwapEffect				= D3DSWAPEFFECT_DISCARD;
	d3dpp.PresentationInterval		= D3DPRESENT_INTERVAL_IMMEDIATE;

	*logFile << L"Getting device...\n";

	// Create Direct3DDevice9 object
	h = d3d9->CreateDevice(D3DADAPTER_DEFAULT,
						   D3DDEVTYPE_HAL,
						   hWnd,
						   D3DCREATE_HARDWARE_VERTEXPROCESSING |
						   D3DCREATE_NOWINDOWCHANGES,
						   &d3dpp,
						   &d3d9Device);
	// Check if success
	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to get device:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;
		
	}

	*logFile << L"\tGetting device:\t\tOK\n";
	*logFile << L"Checking device...\n";

	// Check if device is not NULL
	if (d3d9Device == NULL) {

		*logFile << L"\tERROR:\tDevice is NULL.\n";
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;
	
	}

	*logFile << L"\tDevice check:\t\tOK\n";

	// Get device virtual table
	d3d9DeviceVTable	= (DWORD_PTR*)d3d9Device;
	d3d9DeviceVTable	= (DWORD_PTR*)d3d9DeviceVTable[0];

	// Free module
	FreeModule(hD3D);

	// Close and destroy window
	CloseWindow(hWnd);
	DestroyWindow(hWnd);

	*logFile << L"DirectX 9 found:\t\tOK\n";
	*logFile << L"======================================\n";
	logFile->flush();

	// Return success
	return TRUE;

}

//////////////////////////////
//
//	Hook DX function
//
BOOL DX9Screenshoter::hook() {

	logFile->flush();
	*logFile << L"======================================\n";
	*logFile << L"Hooking DirectX 9...\n";

	// Check if virtual table found
	if (d3d9DeviceVTable != NULL) {

		// Protection level for memory page
		DWORD previousProtection;

#ifdef _WIN64

		*logFile << L"\tx64 ";

		// Hook IDirect3DDevice9::EndScene9() function
		endScene9Function = (ENDSCENE9)endScene9Hook->install((LPBYTE)d3d9DeviceVTable[42],
															  (LPBYTE)hookedEndScene9,
															  20);
		// Allow function memory to execute
		VirtualProtect(endScene9Function,
					   20,
					   PAGE_EXECUTE_READWRITE,
					   &previousProtection);

#else

		*logFile << L"\tx86 ";

		// Hook IDirect3DDevice9::EndScene9() function
		endScene9Function = (ENDSCENE9)endScene9Hook->install((LPBYTE)d3d9DeviceVTable[42],
															  (LPBYTE)hookedEndScene9,
															  5);
		// Allow function memory to execute
		VirtualProtect(endScene9Function,
					   5,
					   PAGE_EXECUTE_READWRITE,
					   &previousProtection);

#endif

		*logFile << L"hooked:\t\tOK\n";
		*logFile << L"======================================\n";
		logFile->flush();

		// Return success
		return TRUE;

	}

	*logFile << L"\thooked:\t\t\tNOT FOUND\n";
	*logFile << L"======================================\n";
	logFile->flush();

	// Return error
	return FALSE;

}


////////////////////////////////
//
//	Unhook DX functions
//
BOOL DX9Screenshoter::unHook() {

	// Unhook IDirect3DDevice9::EndScene9() function
	return endScene9Hook->uninstall();

}


/////////////////////////////////////
//
//	D-tor
//
DX9Screenshoter::~DX9Screenshoter() {

	// If hook exists - delete hook
	if (endScene9Hook != NULL) {

		endScene9Hook->uninstall();
		delete endScene9Hook;
		endScene9Hook = NULL;

	}

	// Release D3D objects
	d3d9Device->Release();
	d3d9->Release();

	logFile->flush();
	*logFile << L"DirectX 9 screenshoter done.\n";
	logFile->flush();

}


//////////////////////////////////////////////////////
//
//	LookForDX9() function to search DX9 in application
//
DWORD LookForDX9(BOOL* runSearch,
				 DX9Screenshoter* screenshoter9) {

	// While search is active
	while (*runSearch == TRUE) {

		// Check if DX9 found
		*runSearch = !screenshoter9->search();
		Sleep(500);

	}

	// Stop search
	*runSearch = FALSE;

	return 0;

}

/////////////////////////////////////////
//
//	HookDX9() thread function to hook DX9
//
DWORD __stdcall HookDX9(LPVOID lpParam) {

	// Search flag
	LPBOOL search = static_cast<LPBOOL>(lpParam);

	// Create screenshoter
	DX9Screenshoter* screenshoter9 = new DX9Screenshoter();

	// Search DX 9
	LookForDX9(search, screenshoter9);

	while (true) {

		// If key was pressed
		if (GetAsyncKeyState(VK_F10) ||
			GetAsyncKeyState(VK_SNAPSHOT)) {

			logFile->flush();
			*logFile << L"F10 or PrintScreen was pressed.\n\tScreenshoting dx9...\n";
			logFile->flush();

			// Hook DX9
			screenshoter9->hook();

		}
		
		Sleep(200);

	}

	// Release screenshoter
	delete screenshoter9;

	ExitThread(0);

}
