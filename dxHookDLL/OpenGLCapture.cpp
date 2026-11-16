////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		OpenGL frame capture implementation
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
#include <GL/gl.h>
// C++
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
// ScreenShotTool common
#include <common/Common.hpp>
// ScreenShotTool hook
#include "OpenGLCapture.hpp"
#include "Capture.hpp"
#include "Globals.hpp"
#include "Log.hpp"
#include "WICImage.hpp"


// ScreenShotTool hook code zone
namespace sst {


	// Bytes of the SwapBuffers prologue to overwrite (whole instructions, >= jump size)
#ifdef _WIN64
	constexpr std::size_t SWAP_PATCH_SIZE {16U};
#else
	constexpr std::size_t SWAP_PATCH_SIZE {5U};
#endif


	// Active instance, so the static detour can reach its owner and unhook
	constinit static OpenGLCapture* gActive {nullptr};
	// Trampoline to the original SwapBuffers(), set while the hook is installed
	constinit static SwapBuffersFn gOriginalSwapBuffers {nullptr};


	// Read the current GL framebuffer for device context @c dc and save it as JPEG
	static auto captureFrame(const ::HDC dc) noexcept -> ::HRESULT {

		// Frame size from the client area of the window behind the DC
		::RECT client {};
		if ((nullptr == dc) || (FALSE == ::GetClientRect(::WindowFromDC(dc), &client))) {
			return E_FAIL;
		}
		const auto width  {static_cast<std::uint32_t>(client.right - client.left)};
		const auto height {static_cast<std::uint32_t>(client.bottom - client.top)};
		if ((0U == width) || (0U == height)) {
			return E_FAIL;
		}

		// glReadPixels returns rows bottom-up, so read into a scratch buffer first
		const auto stride {width * 4U};
		std::vector<std::byte> bottomUp (static_cast<std::size_t>(stride) * height);
		::glReadPixels(0, 0, static_cast<::GLsizei>(width), static_cast<::GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());

		// Flip vertically into a top-down buffer for WIC
		std::vector<std::byte> topDown (bottomUp.size());
		for (std::uint32_t row {0U}; row < height; ++row) {
			const auto src {std::span {bottomUp}.subspan(static_cast<std::size_t>(height - 1U - row) * stride, stride)};
			std::ranges::copy(src, topDown.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(row) * stride));
		}

		return wic::saveJpeg(gScreenDir / L"GLScreenshot.jpg", width, height, stride, std::span<const std::byte> {topDown}, wic::pixel_order::rgba);
	}


	// SwapBuffers() replacement: capture, unhook, then swap for real
	static ::BOOL __stdcall hookedSwapBuffers(const ::HDC dc) noexcept {
		gLog.line(L"GL: taking screenshot...");
		if (const auto hr {captureFrame(dc)}; SUCCEEDED(hr)) {
			gLog.line(L"GL: screenshot OK");
		}
		// Remove the hook so the real SwapBuffers() runs without re-entering us
		if (nullptr != gActive) {
			static_cast<void>(gActive->unhook());
		}
		return (nullptr != gOriginalSwapBuffers) ? gOriginalSwapBuffers(dc) : ::SwapBuffers(dc);
	}


	// Constructor (registers this as the active instance)
	OpenGLCapture::OpenGLCapture() noexcept {
		gActive = this;
		gLog.line(L"OpenGL screenshoter initialized.");
	}

	// Locate SwapBuffers in the host process
	auto OpenGLCapture::search() noexcept -> bool {
		// Only bother once the host has loaded OpenGL
		if (nullptr == ::GetModuleHandleW(L"opengl32.dll")) {
			return false;
		}
		// SwapBuffers lives in gdi32 (wglSwapBuffers forwards to it)
		const auto gdi32 {::GetModuleHandleW(L"gdi32.dll")};
		if (nullptr == gdi32) {
			return false;
		}
		mTarget = std::bit_cast<::LPBYTE>(::GetProcAddress(gdi32, "SwapBuffers"));
		if (nullptr == mTarget) {
			return false;
		}
		gLog.line(L"OpenGL found.");
		return true;
	}

	// Install the SwapBuffers hook
	auto OpenGLCapture::hook() noexcept -> bool {
		if (nullptr == mTarget) {
			return false;
		}
		gOriginalSwapBuffers = std::bit_cast<SwapBuffersFn>(mHook.install(mTarget, std::bit_cast<::LPBYTE>(&hookedSwapBuffers), SWAP_PATCH_SIZE));
		return nullptr != gOriginalSwapBuffers;
	}

	// Remove the SwapBuffers hook
	auto OpenGLCapture::unhook() noexcept -> bool {
		return mHook.uninstall();
	}

	// Whether a one-shot capture is currently armed
	auto OpenGLCapture::hooked() const noexcept -> bool {
		return mHook.installed();
	}

	// Destructor
	OpenGLCapture::~OpenGLCapture() noexcept {
		static_cast<void>(mHook.uninstall());
		if (this == gActive) {
			gActive = nullptr;
		}
		gLog.line(L"OpenGL screenshoter done.");
	}


	// Capture thread: waits for the API, then captures on each hotkey press
	auto __stdcall openGLCaptureThread(::LPVOID param) noexcept -> ::DWORD {
		return runCaptureThread<OpenGLCapture>(*static_cast<std::atomic<bool>*>(param));
	}


} // namespace sst
