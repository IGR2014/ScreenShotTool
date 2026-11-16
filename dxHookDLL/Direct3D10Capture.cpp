////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Direct3D 10 frame capture implementation
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
#include <d3d10.h>
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
#include "Capture.hpp"
#include "Direct3D10Capture.hpp"
#include "Globals.hpp"
#include "Log.hpp"
#include "WICImage.hpp"


// ScreenShotTool hook code zone
namespace sst {


	using Microsoft::WRL::ComPtr;


	// IDXGISwapChain::Present() sits at vtable slot 8
	constexpr std::size_t PRESENT10_VTABLE_INDEX {8U};
	// Bytes of the Present() prologue to overwrite (whole instructions, >= jump size)
#ifdef _WIN64
	constexpr std::size_t PRESENT10_PATCH_SIZE {19U};
#else
	constexpr std::size_t PRESENT10_PATCH_SIZE {5U};
#endif


	// Active instance, so the static detour can reach its owner and unhook
	constinit static Direct3D10Capture* gActive {nullptr};
	// Trampoline to the original Present(), set while the hook is installed
	constinit static Present10 gOriginalPresent {nullptr};


	// Save the swap chain's current back buffer to a JPEG file
	static auto captureBackBuffer(::IDXGISwapChain* const swapChain) noexcept -> ::HRESULT {

		// Back buffer texture
		ComPtr<::ID3D10Texture2D> backBuffer {};
		auto hr {swapChain->GetBuffer(0U, IID_PPV_ARGS(&backBuffer))};
		if (FAILED(hr)) {
			gLog(L"DX10: GetBuffer failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		// Owning device (Direct3D 10 drives the context through the device itself)
		ComPtr<::ID3D10Device> device {};
		backBuffer->GetDevice(&device);

		// Describe a CPU-readable staging copy of the back buffer
		::D3D10_TEXTURE2D_DESC desc {};
		backBuffer->GetDesc(&desc);
		const auto width  {desc.Width};
		const auto height {desc.Height};
		const auto multisampled {desc.SampleDesc.Count > 1U};

		::D3D10_TEXTURE2D_DESC stagingDesc {desc};
		stagingDesc.Usage	   = D3D10_USAGE_STAGING;
		stagingDesc.BindFlags	   = 0U;
		stagingDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
		stagingDesc.MiscFlags	   = 0U;
		stagingDesc.SampleDesc	   = ::DXGI_SAMPLE_DESC {.Count = 1U, .Quality = 0U};

		ComPtr<::ID3D10Texture2D> staging {};
		hr = device->CreateTexture2D(&stagingDesc, nullptr, &staging);
		if (FAILED(hr)) {
			gLog(L"DX10: CreateTexture2D(staging) failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		// Resolve multisampling into the staging copy, or plain-copy otherwise
		if (multisampled) {
			::D3D10_TEXTURE2D_DESC resolveDesc {stagingDesc};
			resolveDesc.Usage	   = D3D10_USAGE_DEFAULT;
			resolveDesc.CPUAccessFlags = 0U;
			ComPtr<::ID3D10Texture2D> resolved {};
			hr = device->CreateTexture2D(&resolveDesc, nullptr, &resolved);
			if (FAILED(hr)) {
				return hr;
			}
			device->ResolveSubresource(resolved.Get(), 0U, backBuffer.Get(), 0U, desc.Format);
			device->CopyResource(staging.Get(), resolved.Get());
		} else {
			device->CopyResource(staging.Get(), backBuffer.Get());
		}

		// Map the staging texture and hand its pixels to WIC
		::D3D10_MAPPED_TEXTURE2D mapped {};
		hr = staging->Map(0U, D3D10_MAP_READ, 0U, &mapped);
		if (FAILED(hr)) {
			gLog(L"DX10: Map failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		const auto* const pixels {std::bit_cast<const std::byte*>(mapped.pData)};
		const auto	  byteCount {static_cast<std::size_t>(mapped.RowPitch) * height};
		hr = wic::saveJpeg(gScreenDir / L"DX10Screenshot.jpg", width, height, mapped.RowPitch, std::span {pixels, byteCount}, wic::pixel_order::rgba);

		staging->Unmap(0U);
		return hr;
	}


	// IDXGISwapChain::Present() replacement: capture, unhook, then present for real
	static ::HRESULT __stdcall hookedPresent10(::IDXGISwapChain* const swapChain, const ::UINT syncInterval, const ::UINT flags) noexcept {
		gLog.line(L"DX10: taking screenshot...");
		if (const auto hr {captureBackBuffer(swapChain)}; SUCCEEDED(hr)) {
			gLog.line(L"DX10: screenshot OK");
		}
		// Remove the hook so the real Present() runs without re-entering us
		if (nullptr != gActive) {
			static_cast<void>(gActive->unhook());
		}
		return swapChain->Present(syncInterval, flags);
	}


	// Constructor (registers this as the active instance)
	Direct3D10Capture::Direct3D10Capture() noexcept {
		gActive = this;
		gLog.line(L"DirectX 10 screenshoter initialized.");
	}

	// Locate the swap chain vtable in the host process
	auto Direct3D10Capture::search() noexcept -> bool {

		// Only bother if the host actually loaded Direct3D 10
		if (nullptr == ::GetModuleHandleW(L"d3d10.dll")) {
			return false;
		}

		// A tiny hidden window to back a throwaway swap chain
		::HWND window {::CreateWindowExW(0UL, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, nullptr, nullptr)};
		if (nullptr == window) {
			return false;
		}
		const scope_exit destroyWindow {[window]() noexcept {
			::DestroyWindow(window);
		}};

		::DXGI_SWAP_CHAIN_DESC swapDesc {};
		swapDesc.BufferCount	   = 1U;
		swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapDesc.BufferUsage	   = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.OutputWindow	   = window;
		swapDesc.SampleDesc.Count  = 1U;
		swapDesc.Windowed	   = TRUE;
		swapDesc.SwapEffect	   = DXGI_SWAP_EFFECT_DISCARD;

		const auto hr {::D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0U, D3D10_SDK_VERSION, &swapDesc, &mSwapChain, &mDevice)};
		if (FAILED(hr) || !mSwapChain) {
			gLog(L"DX10: device creation failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return false;
		}

		// First machine word of a COM object is its vtable pointer
		mVTable = *std::bit_cast<std::uintptr_t**>(mSwapChain.Get());
		gLog.line(L"DirectX 10 found.");
		return true;
	}

	// Install the Present() hook
	auto Direct3D10Capture::hook() noexcept -> bool {
		if (nullptr == mVTable) {
			return false;
		}
		gOriginalPresent = std::bit_cast<Present10>(mHook.install(std::bit_cast<::LPBYTE>(mVTable[PRESENT10_VTABLE_INDEX]), std::bit_cast<::LPBYTE>(&hookedPresent10), PRESENT10_PATCH_SIZE));
		return nullptr != gOriginalPresent;
	}

	// Remove the Present() hook
	auto Direct3D10Capture::unhook() noexcept -> bool {
		return mHook.uninstall();
	}

	// Whether a one-shot capture is currently armed
	auto Direct3D10Capture::hooked() const noexcept -> bool {
		return mHook.installed();
	}

	// Destructor
	Direct3D10Capture::~Direct3D10Capture() noexcept {
		static_cast<void>(mHook.uninstall());
		if (this == gActive) {
			gActive = nullptr;
		}
		gLog.line(L"DirectX 10 screenshoter done.");
	}


	// Capture thread: waits for the API, then captures on each hotkey press
	auto __stdcall direct3D10CaptureThread(::LPVOID param) noexcept -> ::DWORD {
		return runCaptureThread<Direct3D10Capture>(*static_cast<std::atomic<bool>*>(param));
	}


} // namespace sst
