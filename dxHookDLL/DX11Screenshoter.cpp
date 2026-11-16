////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Direct3D 11 screenshoter implementation
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
#include <d3d11.h>
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
#include "DX11Screenshoter.hpp"
#include "Globals.hpp"
#include "Log.hpp"
#include "WICImage.hpp"


// ScreenShotTool hook code zone
namespace sst {


	using Microsoft::WRL::ComPtr;


	// IDXGISwapChain::Present() sits at vtable slot 8
	constexpr std::size_t PRESENT_VTABLE_INDEX {8U};
	// Bytes of the Present() prologue to overwrite (whole instructions, >= jump size)
#ifdef _WIN64
	constexpr std::size_t PRESENT_PATCH_SIZE {19U};
#else
	constexpr std::size_t PRESENT_PATCH_SIZE {5U};
#endif


	// Active instance, so the static detour can reach its owner and unhook
	constinit static DX11Screenshoter* gActive {nullptr};
	// Trampoline to the original Present(), set while the hook is installed
	constinit static Present11 gOriginalPresent {nullptr};


	// Save the swap chain's current back buffer to a JPEG file
	static auto captureBackBuffer(::IDXGISwapChain* const swapChain) noexcept -> ::HRESULT {

		// Back buffer texture
		ComPtr<::ID3D11Texture2D> backBuffer {};
		auto hr {swapChain->GetBuffer(0U, IID_PPV_ARGS(&backBuffer))};
		if (FAILED(hr)) {
			gLog(L"DX11: GetBuffer failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		// Owning device and its immediate context
		ComPtr<::ID3D11Device> device {};
		backBuffer->GetDevice(&device);
		ComPtr<::ID3D11DeviceContext> context {};
		device->GetImmediateContext(&context);

		// Describe a CPU-readable staging copy of the back buffer
		::D3D11_TEXTURE2D_DESC desc {};
		backBuffer->GetDesc(&desc);
		const auto width  {desc.Width};
		const auto height {desc.Height};
		const auto multisampled {desc.SampleDesc.Count > 1U};

		::D3D11_TEXTURE2D_DESC stagingDesc {desc};
		stagingDesc.Usage	   = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags	   = 0U;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.MiscFlags	   = 0U;
		stagingDesc.SampleDesc	   = ::DXGI_SAMPLE_DESC {.Count = 1U, .Quality = 0U};

		ComPtr<::ID3D11Texture2D> staging {};
		hr = device->CreateTexture2D(&stagingDesc, nullptr, &staging);
		if (FAILED(hr)) {
			gLog(L"DX11: CreateTexture2D(staging) failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		// Resolve multisampling into the staging copy, or plain-copy otherwise
		if (multisampled) {
			::D3D11_TEXTURE2D_DESC resolveDesc {stagingDesc};
			resolveDesc.Usage	  = D3D11_USAGE_DEFAULT;
			resolveDesc.CPUAccessFlags = 0U;
			ComPtr<::ID3D11Texture2D> resolved {};
			hr = device->CreateTexture2D(&resolveDesc, nullptr, &resolved);
			if (FAILED(hr)) {
				return hr;
			}
			context->ResolveSubresource(resolved.Get(), 0U, backBuffer.Get(), 0U, desc.Format);
			context->CopyResource(staging.Get(), resolved.Get());
		} else {
			context->CopyResource(staging.Get(), backBuffer.Get());
		}

		// Map the staging texture and hand its pixels to WIC
		::D3D11_MAPPED_SUBRESOURCE mapped {};
		hr = context->Map(staging.Get(), 0U, D3D11_MAP_READ, 0U, &mapped);
		if (FAILED(hr)) {
			gLog(L"DX11: Map failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return hr;
		}

		const auto* const pixels {std::bit_cast<const std::byte*>(mapped.pData)};
		const auto	  byteCount {static_cast<std::size_t>(mapped.RowPitch) * height};
		hr = wic::saveJpeg(gScreenDir / L"DX11Screenshot.jpg", width, height, mapped.RowPitch, std::span {pixels, byteCount}, wic::pixel_order::rgba);

		context->Unmap(staging.Get(), 0U);
		return hr;
	}


	// IDXGISwapChain::Present() replacement: capture, unhook, then present for real
	static ::HRESULT __stdcall hookedPresent11(::IDXGISwapChain* const swapChain, const ::UINT syncInterval, const ::UINT flags) noexcept {
		gLog.line(L"DX11: taking screenshot...");
		if (const auto hr {captureBackBuffer(swapChain)}; SUCCEEDED(hr)) {
			gLog.line(L"DX11: screenshot OK");
		}
		// Remove the hook so the real Present() runs without re-entering us
		if (nullptr != gActive) {
			static_cast<void>(gActive->unHook());
		}
		return swapChain->Present(syncInterval, flags);
	}


	// Constructor (registers this as the active instance)
	DX11Screenshoter::DX11Screenshoter() noexcept {
		gActive = this;
		gLog.line(L"DirectX 11 screenshoter initialized.");
	}

	// Locate the swap chain vtable in the host process
	auto DX11Screenshoter::search() noexcept -> bool {

		// Only bother if the host actually loaded Direct3D 11
		if (nullptr == ::GetModuleHandleW(L"d3d11.dll")) {
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

		auto featureLevel {D3D_FEATURE_LEVEL_11_0};
		const auto hr {::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0U, &featureLevel, 1U, D3D11_SDK_VERSION, &swapDesc, &mSwapChain, &mDevice, nullptr, &mContext)};
		if (FAILED(hr) || !mSwapChain) {
			gLog(L"DX11: device creation failed (0x{:08X})", static_cast<std::uint32_t>(hr));
			return false;
		}

		// First machine word of a COM object is its vtable pointer
		mVTable = *std::bit_cast<std::uintptr_t**>(mSwapChain.Get());
		gLog.line(L"DirectX 11 found.");
		return true;
	}

	// Install the Present() hook
	auto DX11Screenshoter::hook() noexcept -> bool {
		if (nullptr == mVTable) {
			return false;
		}
		gOriginalPresent = std::bit_cast<Present11>(mHook.install(std::bit_cast<::LPBYTE>(mVTable[PRESENT_VTABLE_INDEX]), std::bit_cast<::LPBYTE>(&hookedPresent11), PRESENT_PATCH_SIZE));
		return nullptr != gOriginalPresent;
	}

	// Remove the Present() hook
	auto DX11Screenshoter::unHook() noexcept -> bool {
		return mHook.uninstall();
	}

	// Destructor
	DX11Screenshoter::~DX11Screenshoter() noexcept {
		static_cast<void>(mHook.uninstall());
		if (this == gActive) {
			gActive = nullptr;
		}
		gLog.line(L"DirectX 11 screenshoter done.");
	}


	// Hook thread: finds Direct3D 11, then captures on hotkey
	auto __stdcall HookDX11(::LPVOID param) noexcept -> ::DWORD {

		auto* const keepRunning {static_cast<std::atomic<bool>*>(param)};
		const auto screenshoter {std::make_unique<DX11Screenshoter>()};

		// Poll for Direct3D 11 to appear in the host process
		while (keepRunning->load() && !screenshoter->search()) {
			::Sleep(500UL);
		}

		// Capture on F10 / PrintScreen until asked to stop
		while (keepRunning->load()) {
			if ((0 != ::GetAsyncKeyState(VK_F10)) || (0 != ::GetAsyncKeyState(VK_SNAPSHOT))) {
				gLog.line(L"DX11: hotkey pressed, hooking Present...");
				static_cast<void>(screenshoter->hook());
			}
			::Sleep(200UL);
		}

		return 0UL;
	}


} // namespace sst
