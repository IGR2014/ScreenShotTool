// Direct3D 9 test window for the screenshot hook.
//
// Usage: dx9_<arch>.exe [--mode=d3d9|d3d9ex] [--driver=ref] [--vsync=0]
//   --mode    IDirect3D9 (default) or IDirect3D9Ex with a FLIPEX swap chain
//   --driver  use the reference rasterizer (needs the DirectX SDK runtime)
//   --vsync   0 presents immediately (default: vsync on)

#include "ExampleCommon.h"

#include <d3d9.h>
#include <string_view>

namespace {

	using example::UniqueComPtr;

	enum class Dx9Mode {
		Standard, ///< IDirect3D9
		Ex	  ///< IDirect3D9Ex
	};

	struct Options {
		Dx9Mode	   mode	    = Dx9Mode::Standard;
		D3DDEVTYPE dev_type = D3DDEVTYPE_HAL;
		bool	   vsync    = true;
	};

	struct AppState {
		Options				 opts {};
		UniqueComPtr<IDirect3D9>	 d3d;
		UniqueComPtr<IDirect3D9Ex>	 d3d9ex;
		UniqueComPtr<IDirect3DDevice9>	 device;
		UniqueComPtr<IDirect3DDevice9Ex> device9ex;
		D3DPRESENT_PARAMETERS		 pp {};
		bool				 device_lost = false;
	};

	Options ParseArgs(const wchar_t* cmd_line) {
		Options		  opts {};
		std::wstring_view sv(cmd_line ? cmd_line : L"");

		if (sv.find(L"--mode=d3d9ex") != std::wstring_view::npos) {
			opts.mode = Dx9Mode::Ex;
		}
		if (sv.find(L"--driver=ref") != std::wstring_view::npos) {
			opts.dev_type = D3DDEVTYPE_REF;
		}
		if (sv.find(L"--vsync=0") != std::wstring_view::npos) {
			opts.vsync = false;
		}
		return opts;
	}

	D3DPRESENT_PARAMETERS BuildPresentParams(HWND hwnd, const Options &opts, example::Size size) {
		D3DPRESENT_PARAMETERS pp {};
		pp.Windowed		= TRUE;
		pp.SwapEffect		= opts.mode == Dx9Mode::Ex ? D3DSWAPEFFECT_FLIPEX : D3DSWAPEFFECT_DISCARD;
		pp.BackBufferFormat	= D3DFMT_UNKNOWN;
		pp.BackBufferWidth	= size.width;
		pp.BackBufferHeight	= size.height;
		pp.BackBufferCount	= opts.mode == Dx9Mode::Ex ? 2 : 1;
		pp.hDeviceWindow	= hwnd;
		pp.PresentationInterval = opts.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
		return pp;
	}

	bool InitDx9(HWND hwnd, AppState &state) {
		state.pp = BuildPresentParams(hwnd, state.opts, example::ClientSize(hwnd));

		// Hardware vertex processing first, software as a fallback for weak / virtual adapters
		constexpr DWORD vertex_modes[] = {D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DCREATE_SOFTWARE_VERTEXPROCESSING};

		if (state.opts.mode == Dx9Mode::Ex) {
			IDirect3D9Ex* ex_raw = nullptr;
			if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &ex_raw))) {
				return false;
			}
			state.d3d9ex.reset(ex_raw);

			for (const auto vp : vertex_modes) {
				IDirect3DDevice9Ex* dev_raw = nullptr;
				if (SUCCEEDED(state.d3d9ex->CreateDeviceEx(D3DADAPTER_DEFAULT, state.opts.dev_type, hwnd,
									   vp | D3DCREATE_MULTITHREADED, &state.pp, nullptr, &dev_raw))) {
					state.device9ex.reset(dev_raw);
					break;
				}
			}
			if (!state.device9ex) {
				return false;
			}

			// Keep a base-interface reference so the render path is shared with plain D3D9
			IDirect3DDevice9* base_dev = state.device9ex.get();
			base_dev->AddRef();
			state.device.reset(base_dev);
			SetWindowTextW(hwnd, L"DirectX 9 Test [Mode: D3D9Ex]");
			return true;
		}

		state.d3d.reset(Direct3DCreate9(D3D_SDK_VERSION));
		if (!state.d3d) {
			return false;
		}

		for (const auto vp : vertex_modes) {
			IDirect3DDevice9* dev_raw = nullptr;
			if (SUCCEEDED(state.d3d->CreateDevice(D3DADAPTER_DEFAULT, state.opts.dev_type, hwnd, vp, &state.pp, &dev_raw))) {
				state.device.reset(dev_raw);
				break;
			}
		}
		if (!state.device) {
			return false;
		}
		SetWindowTextW(hwnd, L"DirectX 9 Test [Mode: Standard D3D9]");
		return true;
	}

	void ResetDx9(HWND hwnd, AppState &state, example::Size size) {
		state.pp	  = BuildPresentParams(hwnd, state.opts, size);
		const HRESULT hr  = state.device9ex ? state.device9ex->ResetEx(&state.pp, nullptr) : state.device->Reset(&state.pp);
		state.device_lost = FAILED(hr);
	}

	void ResizeDx9(HWND hwnd, AppState &state, UINT w, UINT h) {
		if (w != state.pp.BackBufferWidth || h != state.pp.BackBufferHeight || state.device_lost) {
			ResetDx9(hwnd, state, {w, h});
		}
	}

	void RenderFrame(HWND hwnd, AppState &state) {
		if (state.device_lost) {
			// D3D9Ex devices are never "lost" in the classic sense, but a failed ResetEx is retried here too
			const HRESULT coop = state.device9ex ? D3DERR_DEVICENOTRESET : state.device->TestCooperativeLevel();
			if (coop == D3DERR_DEVICENOTRESET) {
				ResetDx9(hwnd, state, example::ClientSize(hwnd));
			}
			if (state.device_lost) {
				Sleep(10);
				return;
			}
		}

		state.device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 40, 100), 1.0f, 0);
		if (SUCCEEDED(state.device->BeginScene())) {
			state.device->EndScene();
		}

		const HRESULT hr = state.device9ex ? state.device9ex->PresentEx(nullptr, nullptr, nullptr, nullptr, 0)
						   : state.device->Present(nullptr, nullptr, nullptr, nullptr);
		if (hr == D3DERR_DEVICELOST) {
			state.device_lost = true;
		}
	}

	void ShutdownDx9(AppState &state) {
		state.device.reset();
		state.device9ex.reset();
		state.d3d.reset();
		state.d3d9ex.reset();
	}

} // namespace

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, PWSTR cmd_line, int ncmdshow) {
	AppState state {};
	state.opts = ParseArgs(cmd_line);
	return example::RunExample<AppState>(
		hinstance, ncmdshow, L"SSTExampleDX9", L"DirectX 9 Test Window", state,
		{InitDx9, ResizeDx9, RenderFrame, ShutdownDx9}
	);
}
