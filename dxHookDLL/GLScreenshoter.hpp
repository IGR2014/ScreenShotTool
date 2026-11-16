////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		OpenGL screenshoter (SwapBuffers hook + glReadPixels)
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// Windows
#include <windows.h>
// C++
#include <atomic>
// ScreenShotTool hook
#include "HookFunction.hpp"


// ScreenShotTool hook code zone
namespace sst {


	/// @brief SwapBuffers() / wglSwapBuffers() signature
	using SwapBuffersFn = ::BOOL (__stdcall*)(::HDC);


	/// @brief Captures an OpenGL frame by hooking SwapBuffers()
	///
	/// Rewritten from the original non-functional stub: glReadPixels runs on
	/// the host's render thread inside the SwapBuffers detour, where a current
	/// GL context exists, then the frame is saved through WIC.
	class GLScreenshoter final {

		::LPBYTE  mTarget {nullptr}; ///< SwapBuffers entry point
		funcHook  mHook {};		///< SwapBuffers trampoline


	public:

		/// @brief Constructor (registers this as the active instance)
		GLScreenshoter() noexcept;

		// Non-copyable, non-movable (registered via a global raw pointer)
		GLScreenshoter(const GLScreenshoter&)			= delete;
		GLScreenshoter(GLScreenshoter&&)			= delete;
		auto operator=(const GLScreenshoter&) -> GLScreenshoter&	= delete;
		auto operator=(GLScreenshoter&&) -> GLScreenshoter&	= delete;

		/// @brief Destructor
		~GLScreenshoter() noexcept;

		/// @brief Locate SwapBuffers in the host process
		/// @return true if OpenGL was found and SwapBuffers resolved
		[[nodiscard]]
		auto search() noexcept -> bool;
		/// @brief Install the SwapBuffers hook
		/// @return true if the hook was installed
		[[nodiscard]]
		auto hook() noexcept -> bool;
		/// @brief Remove the SwapBuffers hook
		/// @return true if a hook was removed
		auto unHook() noexcept -> bool;

	};


	/// @brief Hook thread: finds OpenGL, then captures on hotkey
	/// @param param Pointer to the shared "keep running" flag (std::atomic<bool>)
	/// @return Thread exit code
	auto __stdcall HookOpenGL(::LPVOID param) noexcept -> ::DWORD;


} // namespace sst
