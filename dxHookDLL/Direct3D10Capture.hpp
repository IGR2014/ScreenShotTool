////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Direct3D 10 frame capture (IDXGISwapChain::Present hook)
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// Windows
#include <windows.h>
#include <d3d10_1.h>
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
	using Present10 = ::HRESULT (__stdcall*)(::IDXGISwapChain*, ::UINT, ::UINT);


	/// @brief Captures a Direct3D 10 frame by hooking IDXGISwapChain::Present()
	///
	/// Direct3D 10 and 11 both present through DXGI, so this mirrors the
	/// Direct3D 11 capture but drives an ID3D10Device (which carries the
	/// device context methods itself, with no separate immediate context).
	class Direct3D10Capture final {

		Microsoft::WRL::ComPtr<::ID3D10Device>	mDevice {};	///< Dummy device (for vtable discovery)
		Microsoft::WRL::ComPtr<::IDXGISwapChain>	mSwapChain {};	///< Dummy swap chain (owns the vtable we read)
		std::uintptr_t*				mVTable {nullptr}; ///< Swap chain vtable
		InlineHook				mHook {};	///< Present() trampoline


	public:

		/// @brief Constructor (registers this as the active instance)
		Direct3D10Capture() noexcept;

		// Non-copyable, non-movable (registered via a global raw pointer)
		Direct3D10Capture(const Direct3D10Capture&)			= delete;
		Direct3D10Capture(Direct3D10Capture&&)				= delete;
		auto operator=(const Direct3D10Capture&) -> Direct3D10Capture&	= delete;
		auto operator=(Direct3D10Capture&&) -> Direct3D10Capture&	= delete;

		/// @brief Destructor
		~Direct3D10Capture() noexcept;

		/// @brief Locate the swap chain vtable in the host process
		/// @return true if Direct3D 10 was found and the vtable captured
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


	/// @brief Capture thread: waits for Direct3D 10, then captures on each hotkey press
	/// @param param Pointer to the shared "keep running" flag (std::atomic<bool>)
	/// @return Thread exit code
	auto __stdcall direct3D10CaptureThread(::LPVOID param) noexcept -> ::DWORD;


} // namespace sst
