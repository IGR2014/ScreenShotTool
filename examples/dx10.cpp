// Direct3D 10 test window for the screenshot hook.
//
// Usage: dx10_<arch>.exe [--ver=10.0|10.1] [--driver=warp|reference]
//   --ver     10.1 (default) uses D3D10CreateDeviceAndSwapChain1 and walks
//             feature levels 10.1 -> 9.1; 10.0 uses the original D3D10 API
//   --driver  driver type (default: hardware, WARP fallback)

#include "ExampleCommon.h"

#include <array>
#include <d3d10_1.h>
#include <dxgi.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

	using example::UniqueComPtr;

	enum class Dx10Version {
		V10_0,
		V10_1
	};

	struct Options {
		Dx10Version	  version = Dx10Version::V10_1;
		D3D10_DRIVER_TYPE driver  = D3D10_DRIVER_TYPE_HARDWARE;
	};

	struct AppState {
		Options				     opts {};
		UniqueComPtr<ID3D10Device>	     device;
		UniqueComPtr<IDXGISwapChain>	     swap_chain;
		UniqueComPtr<ID3D10RenderTargetView> rtv;
		example::Size			     size;
	};

	struct LevelInfo {
		D3D10_FEATURE_LEVEL1 level;
		const wchar_t*	     label;
	};

	constexpr std::array LEVELS_10_1 {
		LevelInfo {D3D10_FEATURE_LEVEL_10_1, L"10.1"},
		LevelInfo {D3D10_FEATURE_LEVEL_10_0, L"10.0"},
		LevelInfo {D3D10_FEATURE_LEVEL_9_3, L"9.3"},
		LevelInfo {D3D10_FEATURE_LEVEL_9_2, L"9.2"},
		LevelInfo {D3D10_FEATURE_LEVEL_9_1, L"9.1"},
	};

	Options ParseArgs(const wchar_t* cmd_line) {
		Options		  opts {};
		std::wstring_view sv(cmd_line ? cmd_line : L"");

		if (sv.find(L"--ver=10.0") != std::wstring_view::npos) {
			opts.version = Dx10Version::V10_0;
		}
		if (sv.find(L"--driver=warp") != std::wstring_view::npos) {
			opts.driver = D3D10_DRIVER_TYPE_WARP;
		} else if (sv.find(L"--driver=reference") != std::wstring_view::npos) {
			opts.driver = D3D10_DRIVER_TYPE_REFERENCE;
		}
		return opts;
	}

	bool CreateRenderTarget(AppState &state) {
		state.rtv.reset();

		ID3D10Texture2D* back_buffer_raw = nullptr;
		if (FAILED(state.swap_chain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&back_buffer_raw)))) {
			return false;
		}
		UniqueComPtr<ID3D10Texture2D> back_buffer(back_buffer_raw);

		ID3D10RenderTargetView* rtv_raw = nullptr;
		if (FAILED(state.device->CreateRenderTargetView(back_buffer.get(), nullptr, &rtv_raw))) {
			return false;
		}
		state.rtv.reset(rtv_raw);

		ID3D10RenderTargetView* rtv_ptr = state.rtv.get();
		state.device->OMSetRenderTargets(1, &rtv_ptr, nullptr);

		D3D10_VIEWPORT vp {};
		vp.Width    = state.size.width;
		vp.Height   = state.size.height;
		vp.MaxDepth = 1.0f;
		state.device->RSSetViewports(1, &vp);
		return true;
	}

	/// @brief Try to create a D3D10.1 device at the given level; returns true on success
	bool TryCreate10_1(AppState &state, const DXGI_SWAP_CHAIN_DESC &scd, D3D10_DRIVER_TYPE driver, UINT flags,
			   D3D10_FEATURE_LEVEL1 level) {
		auto		desc	= scd;
		IDXGISwapChain* sc_raw	= nullptr;
		ID3D10Device1*	dev_raw = nullptr;
		const HRESULT	hr	= D3D10CreateDeviceAndSwapChain1(
			  nullptr, driver, nullptr, flags, level, D3D10_1_SDK_VERSION, &desc, &sc_raw, &dev_raw
		  );
		state.swap_chain.reset(sc_raw);
		state.device.reset(dev_raw); // ID3D10Device1 derives from ID3D10Device
		return SUCCEEDED(hr) && state.device && state.swap_chain;
	}

	/// @brief Try to create a plain D3D10.0 device; returns true on success
	bool TryCreate10_0(AppState &state, const DXGI_SWAP_CHAIN_DESC &scd, D3D10_DRIVER_TYPE driver, UINT flags) {
		auto		desc	= scd;
		IDXGISwapChain* sc_raw	= nullptr;
		ID3D10Device*	dev_raw = nullptr;
		const HRESULT	hr	= D3D10CreateDeviceAndSwapChain(
			  nullptr, driver, nullptr, flags, D3D10_SDK_VERSION, &desc, &sc_raw, &dev_raw
		  );
		state.swap_chain.reset(sc_raw);
		state.device.reset(dev_raw);
		return SUCCEEDED(hr) && state.device && state.swap_chain;
	}

	bool InitDx10(HWND hwnd, AppState &state) {
		state.size = example::ClientSize(hwnd);

		DXGI_SWAP_CHAIN_DESC scd {};
		scd.BufferCount			       = 1;
		scd.BufferDesc.Width		       = state.size.width;
		scd.BufferDesc.Height		       = state.size.height;
		scd.BufferDesc.Format		       = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.RefreshRate.Numerator   = 60;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferUsage			       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.OutputWindow		       = hwnd;
		scd.SampleDesc.Count		       = 1;
		scd.Windowed			       = TRUE;
		scd.SwapEffect			       = DXGI_SWAP_EFFECT_DISCARD;

		// Debug layer if available (needs the Graphics Tools optional feature), else plain device
		std::vector<UINT> flag_sets;
#ifdef _DEBUG
		flag_sets.push_back(D3D10_CREATE_DEVICE_DEBUG);
#endif
		flag_sets.push_back(0);

		std::vector<D3D10_DRIVER_TYPE> drivers {state.opts.driver};
		if (state.opts.driver == D3D10_DRIVER_TYPE_HARDWARE) {
			drivers.push_back(D3D10_DRIVER_TYPE_WARP);
		}

		for (const auto driver : drivers) {
			const wchar_t* suffix = driver == D3D10_DRIVER_TYPE_WARP	? L", WARP]"
						: driver == D3D10_DRIVER_TYPE_REFERENCE ? L", REF]"
											: L"]";
			for (const auto flags : flag_sets) {
				if (state.opts.version == Dx10Version::V10_1) {
					for (const auto &lvl : LEVELS_10_1) {
						if (TryCreate10_1(state, scd, driver, flags, lvl.level)) {
							SetWindowTextW(hwnd, (std::wstring(L"DirectX 10.1 Test [Feature Level: ") + lvl.label + suffix).c_str());
							return CreateRenderTarget(state);
						}
					}
				} else if (TryCreate10_0(state, scd, driver, flags)) {
					SetWindowTextW(hwnd, (std::wstring(L"DirectX 10.0 Test [Feature Level: 10.0") + suffix).c_str());
					return CreateRenderTarget(state);
				}
			}
		}
		return false;
	}

	void ResizeDx10(HWND, AppState &state, UINT w, UINT h) {
		state.size = {w, h};
		// All back-buffer references (including the bound RTV) must be released before ResizeBuffers
		state.device->OMSetRenderTargets(0, nullptr, nullptr);
		state.rtv.reset();
		if (SUCCEEDED(state.swap_chain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0))) {
			CreateRenderTarget(state);
		}
	}

	void RenderFrame(HWND, AppState &state) {
		if (!state.rtv) {
			return;
		}
		constexpr std::array<float, 4> blue = {0.0f, 0.2f, 0.4f, 1.0f};
		state.device->ClearRenderTargetView(state.rtv.get(), blue.data());
		state.swap_chain->Present(1, 0);
	}

	void ShutdownDx10(AppState &state) {
		if (state.device) {
			state.device->ClearState();
		}
		state.rtv.reset();
		state.swap_chain.reset();
		state.device.reset();
	}

} // namespace

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, PWSTR cmd_line, int ncmdshow) {
	AppState state {};
	state.opts = ParseArgs(cmd_line);
	return example::RunExample<AppState>(
		hinstance, ncmdshow, L"SSTExampleDX10", L"DirectX 10 Test Window", state,
		{InitDx10, ResizeDx10, RenderFrame, ShutdownDx10}
	);
}
