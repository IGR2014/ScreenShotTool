// Direct3D 11 test window for the screenshot hook.
//
// Usage: dx11_<arch>.exe [--fl=9.1|9.2|9.3|10.0|10.1|11.0|11.1] [--driver=warp]
//   --fl      request a single feature level (default: best available)
//   --driver  force the WARP software rasterizer (default: hardware, WARP fallback)

#include "ExampleCommon.h"

#include <array>
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

	using example::UniqueComPtr;

	struct FeatureLevelInfo {
		std::wstring_view arg;
		D3D_FEATURE_LEVEL level;
		const wchar_t*	  label;
	};

	constexpr std::array FEATURE_LEVELS {
		FeatureLevelInfo {L"11.1", D3D_FEATURE_LEVEL_11_1, L"11.1"},
		FeatureLevelInfo {L"11.0", D3D_FEATURE_LEVEL_11_0, L"11.0"},
		FeatureLevelInfo {L"10.1", D3D_FEATURE_LEVEL_10_1, L"10.1"},
		FeatureLevelInfo {L"10.0", D3D_FEATURE_LEVEL_10_0, L"10.0"},
		FeatureLevelInfo {L"9.3", D3D_FEATURE_LEVEL_9_3, L"9.3"},
		FeatureLevelInfo {L"9.2", D3D_FEATURE_LEVEL_9_2, L"9.2"},
		FeatureLevelInfo {L"9.1", D3D_FEATURE_LEVEL_9_1, L"9.1"},
	};

	struct Options {
		std::vector<D3D_FEATURE_LEVEL> levels;
		bool			       force_warp = false;
	};

	struct AppState {
		Options				     opts;
		UniqueComPtr<ID3D11Device>	     device;
		UniqueComPtr<ID3D11DeviceContext>    context;
		UniqueComPtr<IDXGISwapChain>	     swap_chain;
		UniqueComPtr<ID3D11RenderTargetView> rtv;
		example::Size			     size;
	};

	Options ParseArgs(const wchar_t* cmd_line) {
		Options		  opts;
		std::wstring_view sv(cmd_line ? cmd_line : L"");

		if (const auto pos = sv.find(L"--fl="); pos != std::wstring_view::npos) {
			auto value = sv.substr(pos + 5);
			value	   = value.substr(0, value.find(L' '));
			for (const auto &fl : FEATURE_LEVELS) {
				if (value == fl.arg) {
					opts.levels = {fl.level};
				}
			}
		}
		if (opts.levels.empty()) {
			for (const auto &fl : FEATURE_LEVELS) {
				opts.levels.push_back(fl.level);
			}
		}

		opts.force_warp = sv.find(L"--driver=warp") != std::wstring_view::npos;
		return opts;
	}

	const wchar_t* FeatureLevelLabel(D3D_FEATURE_LEVEL level) {
		for (const auto &fl : FEATURE_LEVELS) {
			if (fl.level == level) {
				return fl.label;
			}
		}
		return L"Unknown";
	}

	bool CreateRenderTarget(AppState &state) {
		state.rtv.reset();

		ID3D11Texture2D* back_buffer_raw = nullptr;
		if (FAILED(state.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer_raw)))) {
			return false;
		}
		UniqueComPtr<ID3D11Texture2D> back_buffer(back_buffer_raw);

		ID3D11RenderTargetView* rtv_raw = nullptr;
		if (FAILED(state.device->CreateRenderTargetView(back_buffer.get(), nullptr, &rtv_raw))) {
			return false;
		}
		state.rtv.reset(rtv_raw);

		ID3D11RenderTargetView* rtv_ptr = state.rtv.get();
		state.context->OMSetRenderTargets(1, &rtv_ptr, nullptr);

		D3D11_VIEWPORT vp {};
		vp.Width    = static_cast<float>(state.size.width);
		vp.Height   = static_cast<float>(state.size.height);
		vp.MaxDepth = 1.0f;
		state.context->RSSetViewports(1, &vp);
		return true;
	}

	HRESULT CreateDevice(AppState &state, const DXGI_SWAP_CHAIN_DESC &scd, D3D_DRIVER_TYPE driver, UINT flags,
			     D3D_FEATURE_LEVEL &out_level) {
		IDXGISwapChain*	     sc_raw  = nullptr;
		ID3D11Device*	     dev_raw = nullptr;
		ID3D11DeviceContext* ctx_raw = nullptr;
		auto		     desc    = scd;

		const HRESULT hr = D3D11CreateDeviceAndSwapChain(
			nullptr, driver, nullptr, flags,
			state.opts.levels.data(), static_cast<UINT>(state.opts.levels.size()),
			D3D11_SDK_VERSION, &desc, &sc_raw, &dev_raw, &out_level, &ctx_raw
		);
		state.swap_chain.reset(sc_raw);
		state.device.reset(dev_raw);
		state.context.reset(ctx_raw);
		return hr;
	}

	bool InitDx11(HWND hwnd, AppState &state) {
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
		flag_sets.push_back(D3D11_CREATE_DEVICE_DEBUG);
#endif
		flag_sets.push_back(0);

		std::vector<D3D_DRIVER_TYPE> drivers;
		if (!state.opts.force_warp) {
			drivers.push_back(D3D_DRIVER_TYPE_HARDWARE);
		}
		drivers.push_back(D3D_DRIVER_TYPE_WARP);

		D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
		HRESULT		  hr	= E_FAIL;
		D3D_DRIVER_TYPE	  used	= D3D_DRIVER_TYPE_UNKNOWN;
		for (const auto driver : drivers) {
			for (const auto flags : flag_sets) {
				hr = CreateDevice(state, scd, driver, flags, level);
				if (SUCCEEDED(hr)) {
					used = driver;
					break;
				}
			}
			if (SUCCEEDED(hr)) {
				break;
			}
		}
		if (FAILED(hr) || !state.device || !state.context || !state.swap_chain) {
			return false;
		}

		std::wstring title = L"DirectX 11 Test [Feature Level: ";
		title += FeatureLevelLabel(level);
		title += used == D3D_DRIVER_TYPE_WARP ? L", WARP]" : L"]";
		SetWindowTextW(hwnd, title.c_str());

		return CreateRenderTarget(state);
	}

	void ResizeDx11(HWND, AppState &state, UINT w, UINT h) {
		state.size = {w, h};
		// All back-buffer references must be released before ResizeBuffers
		state.context->OMSetRenderTargets(0, nullptr, nullptr);
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
		state.context->ClearRenderTargetView(state.rtv.get(), blue.data());
		state.swap_chain->Present(1, 0);
	}

	void ShutdownDx11(AppState &state) {
		if (state.context) {
			state.context->ClearState();
		}
		state.rtv.reset();
		state.swap_chain.reset();
		state.context.reset();
		state.device.reset();
	}

} // namespace

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, PWSTR cmd_line, int ncmdshow) {
	AppState state {};
	state.opts = ParseArgs(cmd_line);
	return example::RunExample<AppState>(
		hinstance, ncmdshow, L"SSTExampleDX11", L"DirectX 11 Test Window", state,
		{InitDx11, ResizeDx11, RenderFrame, ShutdownDx11}
	);
}
