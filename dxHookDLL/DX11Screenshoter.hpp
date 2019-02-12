#pragma once


extern std::wfstream*	logFile;


extern	WCHAR			logFilePath[MAX_PATH];
extern	WCHAR			screenPath[MAX_PATH];


///////////////////////////////////////////////////////
//
//	IDXGISwapChain::Present()
//
typedef HRESULT (__stdcall *PRESENT11)(IDXGISwapChain*,
									   UINT,
									   UINT); 


//////////////////////////////////////
//
//	HookDirectX11()
//
DWORD __stdcall HookDirectX11(LPVOID);


//////////////////////////////////////
//
//	Forward declaration of hook class
//
class funcHook;


////////////////////////////////
//
//	DirectX 9 screenshoter class
//
class DX11Screenshoter {

	private:

		// ID3D11Device pointer
		ID3D11Device*			d3d11Device;
		// ID3D11DeviceContext pointer
		ID3D11DeviceContext*	d3d11Context;
		// IDXGISwapChain pointer
		IDXGISwapChain*			d3d11SwapChain;

		// Pointer to IDirect3DDevice9::Present()
		PRESENT11				present11Function;
		// Pointer to IDirect3DDevice9 (vTable)
		DWORD_PTR*				d3d11DeviceVTable;
		// IDirect3DDevice9::Present() function hook
		funcHook*				present11Hook;


	public:

		// C-tor
		DX11Screenshoter();

		// Look for DX functions
		BOOL		search();
		// Hook DX function
		BOOL		hook();
		// Unhook DX functions
		BOOL		unHook();

		// D-tor
		~DX11Screenshoter();

};


////////////////////////////////////////////////////////
//
//	LookForDX11() function to search DX11 in application
//
DWORD LookForDX11(BOOL*, DX11Screenshoter*);


///////////////////////////////////////////
//
//	HookDX11() thread function to hook DX11
//
DWORD __stdcall HookDX11(LPVOID);
