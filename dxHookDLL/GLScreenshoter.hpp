#pragma once


extern	WCHAR		dllPath[MAX_PATH];


///////////
//
//	glEnd()
//
typedef void (*OPENGLEND)();


////////////////
//
//	HookOpenGL()
//
DWORD __stdcall HookOpenGL(LPVOID);


// Forward declaration of hook class
class funcHook;


// OpenGL screenshoter class
class GLScreenshoter {

	private:

		// Pointer to IDirect3DDevice9::Present()
		OPENGLEND		endFunction;
		// IDirect3DDevice9::Present() function hook
		funcHook*		endHook;


	public:

		// C-tor
		GLScreenshoter();

		// Hook GL function
		BOOL		hook();
		// Take screenshot
		void		takeScreenshot();
		// Unhook GL functions
		BOOL		unHook();

		// D-tor
		~GLScreenshoter();

};
