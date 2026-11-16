////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Direct3D 11 screenshoter (IDXGISwapChain::Present hook)
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// Windows
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
// C++
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
// ScreenShotTool hook
#include "HookFunction.hpp"


// ScreenShotTool hook code zone
namespace sst {


	/// @brief IDXGISwapChain::Present() signature
	using Present11 = ::HRESULT (__stdcall*)(::IDXGISwapChain*, ::UINT, ::UINT);


	/// @brief Captures a Direct3D 11 frame by hooking IDXGISwapChain::Present()
	class DX11Screenshoter final {

		Microsoft::WRL::ComPtr<::ID3D11Device>		mDevice {};	///< Dummy device (for vtable discovery)
		Microsoft::WRL::ComPtr<::ID3D11DeviceContext>	mContext {};	///< Dummy context
		Microsoft::WRL::ComPtr<::IDXGISwapChain>		mSwapChain {};	///< Dummy swap chain (owns the vtable we read)
		std::uintptr_t*				mVTable {nullptr}; ///< Swap chain vtable
		funcHook					mHook {};	///< Present() trampoline


	public:

		/// @brief Constructor (registers this as the active instance)
		DX11Screenshoter() noexcept;

		// Non-copyable, non-movable (registered via a global raw pointer)
		DX11Screenshoter(const DX11Screenshoter&)			= delete;
		DX11Screenshoter(DX11Screenshoter&&)				= delete;
		auto operator=(const DX11Screenshoter&) -> DX11Screenshoter&	= delete;
		auto operator=(DX11Screenshoter&&) -> DX11Screenshoter&		= delete;

		/// @brief Destructor
		~DX11Screenshoter() noexcept;

		/// @brief Locate the swap chain vtable in the host process
		/// @return true if Direct3D 11 was found and the vtable captured
		[[nodiscard]]
		auto search() noexcept -> bool;
		/// @brief Install the Present() hook
		/// @return true if the hook was installed
		[[nodiscard]]
		auto hook() noexcept -> bool;
		/// @brief Remove the Present() hook
		/// @return true if a hook was removed
		auto unHook() noexcept -> bool;

	};


	/// @brief Hook thread: finds Direct3D 11, then captures on hotkey
	/// @param param Pointer to the shared "keep running" flag (std::atomic<bool>)
	/// @return Thread exit code
	auto __stdcall HookDX11(::LPVOID param) noexcept -> ::DWORD;


} // namespace sst
