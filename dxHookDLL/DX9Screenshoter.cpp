////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Direct3D 9 screenshoter implementation
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
#include <d3d9.h>
#include <wrl/client.h>
// C++
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
// ScreenShotTool common
#include <common/Common.hpp>
// ScreenShotTool hook
#include "DX9Screenshoter.hpp"
#include "Globals.hpp"
#include "Log.hpp"
#include "WICImage.hpp"


// ScreenShotTool hook code zone
namespace sst {


	using Microsoft::WRL::ComPtr;


	// IDirect3DDevice9::EndScene() sits at vtable slot 42
	constexpr std::size_t ENDSCENE_VTABLE_INDEX {42U};
	// Bytes of the EndScene() prologue to overwrite (whole instructions, >= jump size)
#ifdef _WIN64
	constexpr std::size_t ENDSCENE_PATCH_SIZE {20U};
#else
	constexpr std::size_t ENDSCENE_PATCH_SIZE {5U};
#endif


	// Active instance, so the static detour can reach its owner and unhook
	constinit static DX9Screenshoter* gActive {nullptr};
	// Trampoline to the original EndScene(), set while the hook is installed
	constinit static EndScene9 gOriginalEndScene {nullptr};


	// Save the device's current back buffer to a JPEG file
	static auto captureBackBuffer(::IDirect3DDevice9* const device) noexcept -> ::HRESULT {

		// Back buffer surface
		ComPtr<::IDirect3DSurface9> backBuffer {};
		auto hr {device->GetBackBuffer(0U, 0U, D3DBACKBUFFER_TYPE_MONO, &backBuffer)};
		if (FAILED(hr)) {
			gLog(L"DX9: GetBackBuffer failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		::D3DSURFACE_DESC desc {};
		backBuffer->GetDesc(&desc);

		// System-memory surface we can lock and read on the CPU
		ComPtr<::IDirect3DSurface9> staging {};
		hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &staging, nullptr);
		if (FAILED(hr)) {
			gLog(L"DX9: CreateOffscreenPlainSurface failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		// Pull the GPU back buffer into the lockable surface
		hr = device->GetRenderTargetData(backBuffer.Get(), staging.Get());
		if (FAILED(hr)) {
			gLog(L"DX9: GetRenderTargetData failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		// Lock, encode via WIC, unlock
		::D3DLOCKED_RECT locked {};
		hr = staging->LockRect(&locked, nullptr, D3DLOCK_READONLY);
		if (FAILED(hr)) {
			gLog(L"DX9: LockRect failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		const auto stride {static_cast<std::uint32_t>(locked.Pitch)};
		const auto* const pixels {std::bit_cast<const std::byte*>(locked.pBits)};
		const auto	  byteCount {static_cast<std::size_t>(stride) * desc.Height};
		hr = wic::saveJpeg(gScreenDir / L"DX9Screenshot.jpg", desc.Width, desc.Height, stride, std::span {pixels, byteCount}, wic::pixel_order::bgra);

		static_cast<void>(staging->UnlockRect());
		return hr;
	}


	// IDirect3DDevice9::EndScene() replacement: capture, unhook, then end scene for real
	static ::HRESULT __stdcall hookedEndScene9(::IDirect3DDevice9* const device) noexcept {
		gLog.line(L"DX9: taking screenshot...");
		if (const auto hr {captureBackBuffer(device)}; SUCCEEDED(hr)) {
			gLog.line(L"DX9: screenshot OK");
		}
		// Remove the hook so the real EndScene() runs without re-entering us
		if (nullptr != gActive) {
			static_cast<void>(gActive->unHook());
		}
		return device->EndScene();
	}


	// Constructor (registers this as the active instance)
	DX9Screenshoter::DX9Screenshoter() noexcept {
		gActive = this;
		gLog.line(L"DirectX 9 screenshoter initialized.");
	}

	// Locate the device vtable in the host process
	auto DX9Screenshoter::search() noexcept -> bool {

		// Only bother if the host actually loaded Direct3D 9
		if (nullptr == ::GetModuleHandleW(L"d3d9.dll")) {
			return false;
		}

		mD3D.Attach(::Direct3DCreate9(D3D_SDK_VERSION));
		if (!mD3D) {
			return false;
		}

		::D3DDISPLAYMODE mode {};
		if (FAILED(mD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode))) {
			return false;
		}

		// A tiny hidden window to back a throwaway device
		::HWND window {::CreateWindowExW(0UL, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, nullptr, nullptr)};
		if (nullptr == window) {
			return false;
		}
		const scope_exit destroyWindow {[window]() noexcept {
			::DestroyWindow(window);
		}};

		::D3DPRESENT_PARAMETERS params {};
		params.Windowed		    = TRUE;
		params.SwapEffect	    = D3DSWAPEFFECT_DISCARD;
		params.BackBufferWidth	    = mode.Width;
		params.BackBufferHeight	    = mode.Height;
		params.BackBufferFormat	    = mode.Format;
		params.hDeviceWindow	    = window;
		params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

		const auto hr {mD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES, &params, &mDevice)};
		if (FAILED(hr) || !mDevice) {
			gLog(L"DX9: device creation failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return false;
		}

		// First machine word of a COM object is its vtable pointer
		mVTable = *std::bit_cast<std::uintptr_t**>(mDevice.Get());
		gLog.line(L"DirectX 9 found.");
		return true;
	}

	// Install the EndScene() hook
	auto DX9Screenshoter::hook() noexcept -> bool {
		if (nullptr == mVTable) {
			return false;
		}
		gOriginalEndScene = std::bit_cast<EndScene9>(mHook.install(std::bit_cast<::LPBYTE>(mVTable[ENDSCENE_VTABLE_INDEX]), std::bit_cast<::LPBYTE>(&hookedEndScene9), ENDSCENE_PATCH_SIZE));
		return nullptr != gOriginalEndScene;
	}

	// Remove the EndScene() hook
	auto DX9Screenshoter::unHook() noexcept -> bool {
		return mHook.uninstall();
	}

	// Destructor
	DX9Screenshoter::~DX9Screenshoter() noexcept {
		static_cast<void>(mHook.uninstall());
		if (this == gActive) {
			gActive = nullptr;
		}
		gLog.line(L"DirectX 9 screenshoter done.");
	}


	// Hook thread: finds Direct3D 9, then captures on hotkey
	auto __stdcall HookDX9(::LPVOID param) noexcept -> ::DWORD {

		auto* const keepRunning {static_cast<std::atomic<bool>*>(param)};
		const auto screenshoter {std::make_unique<DX9Screenshoter>()};

		// Poll for Direct3D 9 to appear in the host process
		while (keepRunning->load() && !screenshoter->search()) {
			::Sleep(500UL);
		}

		// Capture on F10 / PrintScreen until asked to stop
		while (keepRunning->load()) {
			if ((0 != ::GetAsyncKeyState(VK_F10)) || (0 != ::GetAsyncKeyState(VK_SNAPSHOT))) {
				gLog.line(L"DX9: hotkey pressed, hooking EndScene...");
				static_cast<void>(screenshoter->hook());
			}
			::Sleep(200UL);
		}

		return 0UL;
	}


} // namespace sst
