////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Direct3D 11 frame capture (IDXGISwapChain::Present hook)
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
#include "InlineHook.hpp"


// ScreenShotTool hook code zone
namespace sst {


	/// @brief IDXGISwapChain::Present() signature
	using Present11 = ::HRESULT (__stdcall*)(::IDXGISwapChain*, ::UINT, ::UINT);


	/// @brief Captures a Direct3D 11 frame by hooking IDXGISwapChain::Present()
	class Direct3D11Capture final {

		Microsoft::WRL::ComPtr<::ID3D11Device>		mDevice {};	///< Dummy device (for vtable discovery)
		Microsoft::WRL::ComPtr<::ID3D11DeviceContext>	mContext {};	///< Dummy context
		Microsoft::WRL::ComPtr<::IDXGISwapChain>		mSwapChain {};	///< Dummy swap chain (owns the vtable we read)
		std::uintptr_t*				mVTable {nullptr}; ///< Swap chain vtable
		InlineHook				mHook {};	///< Present() trampoline


	public:

		/// @brief Constructor (registers this as the active instance)
		Direct3D11Capture() noexcept;

		// Non-copyable, non-movable (registered via a global raw pointer)
		Direct3D11Capture(const Direct3D11Capture&)			= delete;
		Direct3D11Capture(Direct3D11Capture&&)				= delete;
		auto operator=(const Direct3D11Capture&) -> Direct3D11Capture&	= delete;
		auto operator=(Direct3D11Capture&&) -> Direct3D11Capture&		= delete;

		/// @brief Destructor
		~Direct3D11Capture() noexcept;

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
		auto unhook() noexcept -> bool;
		/// @brief Whether a one-shot capture is currently armed
		/// @return true while the hook is installed
		[[nodiscard]]
		auto hooked() const noexcept -> bool;

	};


	/// @brief Hook thread: finds Direct3D 11, then captures on hotkey
	/// @param param Pointer to the shared "keep running" flag (std::atomic<bool>)
	/// @return Thread exit code
	auto __stdcall direct3D11CaptureThread(::LPVOID param) noexcept -> ::DWORD;


} // namespace sst
