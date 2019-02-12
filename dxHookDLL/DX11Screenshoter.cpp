#include <windows.h>
#include <fstream>
#include <d3dx11.h>
#include <DxErr.h>


#pragma comment(lib, "dxerr.lib")


#include "HookFunction.hpp"
#include "DX11Screenshoter.hpp"


//	Global DX11Screenshoter pointer
static	DX11Screenshoter*	global11Screenshoter		= NULL;
//	Global screenshot name
static	WCHAR				screenshotName[MAX_PATH];


////////////////////////////////////////////////////////////////
//
//	IDirect3DDevice9::EndScene() replace function
//
HRESULT __stdcall hookedPresent11(IDXGISwapChain* pSwapChain,
								  UINT SyncInterval,
								  UINT Flags) {

	logFile->flush();
	*logFile << L"======================================\n";
	*logFile << L"Start taking DirectX 11 screenshot...\n";

	// Error value
	HRESULT h;

	// ID3D11Device pointer (device)
	ID3D11Device*			d3d11Device				= NULL;
	// ID3D11DeviceContext pointer (device context)
	ID3D11DeviceContext*	d3d11Context			= NULL;
	// ID3D11Texture2D pointer (back buffer)
	ID3D11Texture2D*		pBuffer					= NULL;
	// ID3D11Texture2D pointer (screenshot)
	ID3D11Texture2D*		pBackBufferStaging		= NULL;

	*logFile << L"Getting swapchain buffer...\n";

	// Get back buffer from swap chain
	h = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBuffer);

	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to get buffer:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";
		
	}

	*logFile << L"\tBuffer:\t\t\tOK\n";
	*logFile << L"Getting device...\n";

	// Get device from swap chain
	h = pSwapChain->GetDevice(__uuidof(ID3D11Device), (LPVOID*)&d3d11Device);

	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to get device:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";
		
	}

	*logFile << L"\tDevice:\t\t\tOK\n";
	*logFile << L"Creating texture...\n";

	// Get immediate context from swapchain
	d3d11Device->GetImmediateContext(&d3d11Context);

	// Back buffer params
	D3D11_TEXTURE2D_DESC td;
	pBuffer->GetDesc(&td);
	td.Usage			= D3D11_USAGE_STAGING;
	td.BindFlags		= 0;
	td.CPUAccessFlags	= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	td.Format			= DXGI_FORMAT_R8G8B8A8_UNORM;

	// Create texture for screenshot
	h = d3d11Device->CreateTexture2D(&td, NULL, &pBackBufferStaging);

	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to create texture:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";

	}

	*logFile << L"\tTexture:\t\tOK\n";

	// If MSAA texture
	if (td.SampleDesc.Count > 1) {

		*logFile << L"WOW MSAA texture.\n\tHandling...\n";

		// Set texture params to usual
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;

		// Create new texture
		ID3D11Texture2D* pTemp = NULL;
		h = d3d11Device->CreateTexture2D(&td, 0, &pTemp);

		// Check supported format
		UINT support = 0;
		h = d3d11Device->CheckFormatSupport(td.Format, &support);

		// Copy all samples to texture
		for(UINT item = 0; item < td.ArraySize; ++item) {

			for(UINT level = 0; level < td.MipLevels; ++level) {

				// Resolve copying
				UINT index = D3D11CalcSubresource(level, item, td.MipLevels);
				d3d11Context->ResolveSubresource(pTemp, index, pBuffer, index, td.Format);

			}

		}

		// Copy temp texture to screenshot texture
		d3d11Context->CopyResource(pBackBufferStaging, pTemp);

	} else {

		// Copy back buffer texture to screenshot texture
		d3d11Context->CopyResource(pBackBufferStaging, pBuffer);

	}

	*logFile << L"Saving screenshot...\n";

	wcscpy_s(screenshotName, MAX_PATH, screenPath);
	wcscat_s(screenshotName, MAX_PATH, L"DX11Screenshot.jpg");

	// Save texture to file
	h = D3DX11SaveTextureToFile(d3d11Context,
								pBackBufferStaging,
								D3DX11_IFF_JPG,
								screenshotName);
		
	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to take screenshot:\n";
		*logFile << L"\t" << DXGetErrorString(h) << L"\n";
		*logFile << L"\t" << DXGetErrorDescription(h) << L"\n";
		
	}

	// Release screenshot texture
	pBackBufferStaging->Release();
	// Release back buffer
	pBuffer->Release();

	// Release hook
	global11Screenshoter->unHook();

	*logFile << L"\tScreenshot:\t\tOK\n";
	*logFile << L"======================================\n";
	logFile->flush();

	// Call original function
	return pSwapChain->Present(SyncInterval, Flags);

}


//////////////////////////////////////////////////////////////////////
//
//	C-tor
//
DX11Screenshoter::DX11Screenshoter() : d3d11Device(NULL),
									   d3d11Context(NULL),
									   d3d11SwapChain(NULL),
									   present11Function(NULL),
									   d3d11DeviceVTable(NULL),
									   present11Hook(new funcHook()) {

	// Initialize global pointer with this instance of class
	global11Screenshoter = this;

	logFile->flush();
	*logFile << L"DirectX 11 screenshoter initialized." << std::endl;
	logFile->flush();

}

/////////////////////////////////
//
//	Look for DX functions
//
BOOL DX11Screenshoter::search() {

	logFile->flush();
	*logFile << L"======================================\n";
	*logFile << L"DirectX 11 searching started.\n";
	*logFile << L"Looking for d3d11.dll in application...\n";

	// Get d3d11.dll module handle
	HINSTANCE hD3D11 = GetModuleHandle(L"d3d11.dll");
	// Check if exists
	if (hD3D11 == NULL) {

		*logFile << L"\td3d11.dll was not found :(\n";
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;
	
	}

	// Error value
	HRESULT h;

	*logFile << L"\td3d11.dll found.\n\tTrying to search functions...\n";
	*logFile << L"Loading d3d11.dll for our purpose.\n";

	// Load d3d11.dll
	HMODULE hD3D = LoadLibrary(L"d3d11");
	// Check if exists
	if (hD3D == NULL) {

		*logFile << L"\tUnable to load d3d11.dll.\n";
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;
	
	}

	*logFile << L"\td3d11.dll loaded.\n\tSearching functions...\n";

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

	// Pointer to D3D11CreateDeviceAndSwapChain() function
	PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN d3d11CreateDeviceAndSwapchain;
	// Get pointer to D3D11CreateDeviceAndSwapChain() function
	d3d11CreateDeviceAndSwapchain = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(hD3D, "D3D11CreateDeviceAndSwapChain");

	// Set Swapchain parameters
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    DXGI_SWAP_CHAIN_DESC d3d11SwapChainDesc;
    ZeroMemory(&d3d11SwapChainDesc, sizeof(d3d11SwapChainDesc));
	d3d11SwapChainDesc.OutputWindow					= hWnd;
    d3d11SwapChainDesc.Windowed						= TRUE;
    d3d11SwapChainDesc.BufferCount					= 1;
	d3d11SwapChainDesc.SampleDesc.Count				= 1;
	d3d11SwapChainDesc.BufferDesc.ScanlineOrdering	= DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    d3d11SwapChainDesc.BufferDesc.Scaling			= DXGI_MODE_SCALING_UNSPECIFIED;
    d3d11SwapChainDesc.BufferDesc.Format			= DXGI_FORMAT_R8G8B8A8_UNORM;
    d3d11SwapChainDesc.BufferUsage					= DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d3d11SwapChainDesc.SwapEffect					= DXGI_SWAP_EFFECT_DISCARD;

	*logFile << L"Getting device...\n";

	// Create device and swapchain
    h = d3d11CreateDeviceAndSwapchain(NULL,
									  D3D_DRIVER_TYPE_HARDWARE,
									  NULL,
									  NULL,
									  &featureLevel,
									  1,
									  D3D11_SDK_VERSION,
									  &d3d11SwapChainDesc,
									  &d3d11SwapChain,
									  &d3d11Device,
									  NULL,
									  &d3d11Context);
	// Check if success
	if (FAILED(h)) {

		*logFile << L"\tERROR:\tUnable to get device:\n";
		*logFile << L"\t" << DXGetErrorString(h) << std::endl;
		*logFile << L"\t" << DXGetErrorDescription(h) << std::endl;
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;

	}

	*logFile << L"\tGetting device:\t\tOK\n";
	*logFile << L"Checking device...\n";

	// Check if device, context or swapchain is not NULL
	if (d3d11Device == NULL ||
		d3d11Context == NULL ||
		d3d11SwapChain == NULL) {


		*logFile << L"\tERROR:\tDevice is NULL.\n";
		*logFile << L"======================================\n";
		logFile->flush();

		// Return error
		return FALSE;
	
	}

	*logFile << L"\tDevice check:\t\tOK\n";

	// Get device virtual table
	d3d11DeviceVTable	= (DWORD_PTR*)d3d11SwapChain;
    d3d11DeviceVTable	= (DWORD_PTR*)d3d11DeviceVTable[0];

	// Free module
	FreeLibrary(hD3D);

	// Close and destroy window
	CloseWindow(hWnd);
	DestroyWindow(hWnd);

	*logFile << L"DirectX 11 found:\t\tOK\n";
	*logFile << L"======================================\n";
	logFile->flush();

	// Return success
	return TRUE;

}

///////////////////////////////
//
//	Hook DX function
//
BOOL DX11Screenshoter::hook() {

	logFile->flush();
	*logFile << L"======================================\n";
	*logFile << L"Hooking DirectX 11...\n";

	// Check if virtual table found
	if (d3d11DeviceVTable != NULL) {

		// Protection level for memory page
		DWORD previousProtection;

#ifdef _WIN64

		*logFile << L"\tx64 ";

		// Hook IDXGISwapChain::Present() function
		present11Function = (PRESENT11)present11Hook->install((LPBYTE)d3d11DeviceVTable[8],
															  (LPBYTE)hookedPresent11,
															  19);
		// Allow function memory to execute
		VirtualProtect(present11Function,
					   19,
					   PAGE_EXECUTE_READWRITE,
					   &previousProtection);

#else

		*logFile << L"\tx86 ";

		// Hook IDXGISwapChain::Present() function
		present11Function = (PRESENT11)present11Hook->install((LPBYTE)d3d11DeviceVTable[8],
															  (LPBYTE)hookedPresent11,
															  5);
		// Allow function memory to execute
		VirtualProtect(present11Function,
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

/////////////////////////////////
//
//	Unhook DX functions
//
BOOL DX11Screenshoter::unHook() {

	// Unhook IDXGISwapChain::Present() function
	return present11Hook->uninstall();

}

///////////////////////////////////////
//
//	D-tor
//
DX11Screenshoter::~DX11Screenshoter() {

	// If hook exists - delete hook
	if (present11Hook != NULL) {

		present11Hook->uninstall();
		delete present11Hook;
		present11Hook = NULL;

	}

	// Release all D3D objects
	d3d11Device->Release();
	d3d11Context->Release();
	d3d11SwapChain->Release();

	logFile->flush();
	*logFile << L"DirectX 11 screenshoter done.\n";
	logFile->flush();

}


////////////////////////////////////////////////////////
//
//	LookForDX11() function to search DX11 in application
//
DWORD LookForDX11(BOOL* runSearch,
				  DX11Screenshoter* screenshoter11) {

	// While search is active
	while (*runSearch == TRUE) {

		// Check if DX11 found
		*runSearch = !screenshoter11->search();
		Sleep(500);

	}

	// Stop search
	*runSearch = FALSE;

	return 0;

}

///////////////////////////////////////////
//
//	HookDX11() thread function to hook DX11
//
DWORD __stdcall HookDX11(LPVOID lpParam) {

	// Search flag
	LPBOOL search = static_cast<LPBOOL>(lpParam);

	// Create screenshoter
	DX11Screenshoter* screenshoter11 = new DX11Screenshoter();

	// Search DX 11
	LookForDX11(search, screenshoter11);

	while (true) {

		// If key was pressed
		if (GetAsyncKeyState(VK_F10) ||
			GetAsyncKeyState(VK_SNAPSHOT)) {

			logFile->flush();
			*logFile << L"F10 or PrintScreen was pressed.\n\tScreenshoting dx11...\n";
			logFile->flush();

			// Hook DX11
			screenshoter11->hook();

		}
		
		Sleep(200);

	}

	// Release screenshoter
	delete screenshoter11;

	ExitThread(0);

}

