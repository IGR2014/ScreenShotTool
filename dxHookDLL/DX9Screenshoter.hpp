#pragma once


extern std::wfstream*	logFile;


extern	WCHAR			logFilePath[MAX_PATH];
extern	WCHAR			screenPath[MAX_PATH];


////////////////////////////////////////////////////////
//
//	Direct3DCreate9()
//
typedef IDirect3D9* (__stdcall *DIRECT3DCREATE9)(DWORD);


////////////////////////////////////////////////////////
//
//	IDirect3DDevice9::Present()
//
typedef HRESULT (__stdcall *PRESENT9)(IDirect3DDevice9*,
									  const RECT*,
									  const RECT*,
									  HWND,
									  LPVOID);

//////////////////////////////////////////////////////////
//
//	IDirect3DDevice9::EndScene()
//
typedef HRESULT (__stdcall *ENDSCENE9)(IDirect3DDevice9*);

////////////////////////////////////////////////////////////
//
//	IDirect3DDevice9::Reset()
//
typedef HRESULT (__stdcall *RESET9)(IDirect3DDevice9*,
									D3DPRESENT_PARAMETERS*);


/////////////////////////////////////
//
//	HookDirectX9()
//
DWORD __stdcall HookDirectX9(LPVOID);


/////////////////////////////////////
//
//	Forward declaration of hook class
//
class funcHook;


/////////////////////////////////
//
//	DirectX 9 screenshoter class
//
class DX9Screenshoter {

	private:

		// IDirect3D9 pointer
		LPDIRECT3D9 d3d9;
		// IDIrect3DDevice9 pointer
		LPDIRECT3DDEVICE9 d3d9Device;

		// Pointer to IDirect3DDevice9::EndScene()
		ENDSCENE9			endScene9Function;
		// Pointer to IDirect3DDevice9 (vTable)
		DWORD_PTR*			d3d9DeviceVTable;
		// IDirect3DDevice9::EndScene() function hook
		funcHook*			endScene9Hook;


	public:

		// C-tor
		DX9Screenshoter();

		// Look for DX functions
		BOOL		search();
		// Hook DX function
		BOOL		hook();
		// Unhook DX functions
		BOOL		unHook();

		// D-tor
		~DX9Screenshoter();

};


//////////////////////////////////////////////////////
//
//	LookForDX9() function to search DX9 in application
//
DWORD LookForDX9(BOOL*, DX9Screenshoter*);


/////////////////////////////////////////
//
//	HookDX9() thread function to hook DX9
//
DWORD __stdcall HookDX9(LPVOID);
