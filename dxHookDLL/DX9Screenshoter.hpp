////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Direct3D 9 screenshoter (IDirect3DDevice9::EndScene hook)
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// Windows
#include <windows.h>
#include <d3d9.h>
#include <wrl/client.h>
// C++
#include <atomic>
#include <cstdint>
// ScreenShotTool hook
#include "HookFunction.hpp"


// ScreenShotTool hook code zone
namespace sst {


	/// @brief IDirect3DDevice9::EndScene() signature
	using EndScene9 = ::HRESULT (__stdcall*)(::IDirect3DDevice9*);


	/// @brief Captures a Direct3D 9 frame by hooking IDirect3DDevice9::EndScene()
	class DX9Screenshoter final {

		Microsoft::WRL::ComPtr<::IDirect3D9>	   mD3D {};	///< Dummy Direct3D 9 object
		Microsoft::WRL::ComPtr<::IDirect3DDevice9> mDevice {};	///< Dummy device (owns the vtable we read)
		std::uintptr_t*			   mVTable {nullptr}; ///< Device vtable
		funcHook				   mHook {};	///< EndScene() trampoline


	public:

		/// @brief Constructor (registers this as the active instance)
		DX9Screenshoter() noexcept;

		// Non-copyable, non-movable (registered via a global raw pointer)
		DX9Screenshoter(const DX9Screenshoter&)			= delete;
		DX9Screenshoter(DX9Screenshoter&&)			= delete;
		auto operator=(const DX9Screenshoter&) -> DX9Screenshoter&	= delete;
		auto operator=(DX9Screenshoter&&) -> DX9Screenshoter&	= delete;

		/// @brief Destructor
		~DX9Screenshoter() noexcept;

		/// @brief Locate the device vtable in the host process
		/// @return true if Direct3D 9 was found and the vtable captured
		[[nodiscard]]
		auto search() noexcept -> bool;
		/// @brief Install the EndScene() hook
		/// @return true if the hook was installed
		[[nodiscard]]
		auto hook() noexcept -> bool;
		/// @brief Remove the EndScene() hook
		/// @return true if a hook was removed
		auto unHook() noexcept -> bool;

	};


	/// @brief Hook thread: finds Direct3D 9, then captures on hotkey
	/// @param param Pointer to the shared "keep running" flag (std::atomic<bool>)
	/// @return Thread exit code
	auto __stdcall HookDX9(::LPVOID param) noexcept -> ::DWORD;


} // namespace sst
