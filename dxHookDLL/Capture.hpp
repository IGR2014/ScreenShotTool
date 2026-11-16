////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Shared frame-capture driver (concept, hotkey, thread loop)
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
#include <concepts>
#include <memory>


// ScreenShotTool hook code zone
namespace sst {


	// Poll interval while waiting for a graphics API to appear (ms)
	constexpr ::DWORD SEARCH_POLL_MS {500UL};
	// Poll interval while watching for the capture hotkey (ms)
	constexpr ::DWORD HOTKEY_POLL_MS {32UL};


	/// @brief Anything that can locate its API and install a one-shot capture hook
	///
	/// The three screenshoters (Direct3D 9, Direct3D 11, OpenGL) share this
	/// shape, so a single thread driver can run any of them.
	/// @tparam T Candidate capture type
	template<class T>
	concept FrameCapture = std::default_initializable<T> && requires(T capturer) {
		{ capturer.search() } -> std::same_as<bool>;	// Locate the API in the host process
		{ capturer.hook()   } -> std::same_as<bool>;	// Arm a one-shot capture of the next frame
		{ capturer.hooked() } -> std::same_as<bool>;	// Whether a capture is already armed
	};


	/// @brief Rising-edge detector for the capture hotkey
	///
	/// ::GetAsyncKeyState() stays set for as long as the key is held, so a bare
	/// poll fires a burst of captures per press. This reports only the moment
	/// the key goes down, giving exactly one capture per keystroke.
	class HotkeyEdge final {

		bool mWasDown {false}; ///< Whether the hotkey was down at the previous poll


	public:

		/// @brief Poll the hotkey and report a fresh press
		/// @return true only on the transition from released to pressed
		[[nodiscard]]
		auto pressed() noexcept -> bool {
			// F10 or Print Screen arms a capture
			const auto down {(0 != ::GetAsyncKeyState(VK_F10)) || (0 != ::GetAsyncKeyState(VK_SNAPSHOT))};
			const auto edge {down && !mWasDown};
			mWasDown = down;
			return edge;
		}

	};


	/// @brief Capture-thread body: wait for the API, then capture on each hotkey press
	/// @tparam T Capture type satisfying @ref FrameCapture
	/// @param keepRunning Shared flag cleared on DLL detach to stop the loop
	/// @return Thread exit code
	template<FrameCapture T>
	auto runCaptureThread(std::atomic<bool>& keepRunning) noexcept -> ::DWORD {

		const auto capturer {std::make_unique<T>()};

		// Wait for the graphics API to show up in the host process
		while (keepRunning.load() && !capturer->search()) {
			::Sleep(SEARCH_POLL_MS);
		}

		// Arm a capture on each fresh hotkey press (ignored while one is pending)
		HotkeyEdge hotkey {};
		while (keepRunning.load()) {
			if (hotkey.pressed() && !capturer->hooked()) {
				static_cast<void>(capturer->hook());
			}
			::Sleep(HOTKEY_POLL_MS);
		}

		return 0UL;
	}


} // namespace sst
