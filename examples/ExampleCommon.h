#pragma once

// Shared boilerplate for the Direct3D example windows: COM ownership,
// window creation and the render loop. Each example only implements
// its API-specific init / resize / render.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <memory>
#include <windows.h>

namespace example {

	/// @brief Releases a COM interface when the owning pointer dies
	struct ComDeleter {
		template<typename T>
		void operator()(T* ptr) const noexcept {
			if (ptr) {
				ptr->Release();
			}
		}
	};

	template<typename T>
	using UniqueComPtr = std::unique_ptr<T, ComDeleter>;

	/// @brief Client area size of a window, clamped to at least 1x1
	struct Size {
		UINT width  = 1;
		UINT height = 1;
	};

	[[nodiscard]] inline Size ClientSize(HWND hwnd) noexcept {
		RECT rect {};
		GetClientRect(hwnd, &rect);
		const auto w = rect.right - rect.left;
		const auto h = rect.bottom - rect.top;
		return {w > 0 ? static_cast<UINT>(w) : 1U, h > 0 ? static_cast<UINT>(h) : 1U};
	}

	/// @brief Callbacks an example provides to RunExample
	template<typename State>
	struct Callbacks {
		bool (*init)(HWND, State &);		 ///< Create device / swap chain; false aborts
		void (*resize)(HWND, State &, UINT, UINT); ///< Client area resized (non-zero size)
		void (*render)(HWND, State &);		 ///< Draw and present one frame
		void (*shutdown)(State &);		 ///< Release device objects (window still alive)
	};

	namespace detail {

		template<typename State>
		struct Context {
			State*		 state;
			Callbacks<State> cb;
			bool		 ready = false; ///< Set once init succeeded
		};

		template<typename State>
		LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			auto* ctx = reinterpret_cast<Context<State>*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
			switch (msg) {
				case WM_CREATE: {
					auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
					SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
					return 0;
				}
				case WM_SIZE:
					if (ctx && ctx->ready && wparam != SIZE_MINIMIZED && LOWORD(lparam) && HIWORD(lparam)) {
						ctx->cb.resize(hwnd, *ctx->state, LOWORD(lparam), HIWORD(lparam));
					}
					return 0;
				case WM_KEYDOWN:
					if (wparam == VK_ESCAPE) {
						DestroyWindow(hwnd);
					}
					return 0;
				case WM_DESTROY:
					if (ctx && ctx->ready) {
						ctx->ready = false;
						ctx->cb.shutdown(*ctx->state);
					}
					PostQuitMessage(0);
					return 0;
				default:
					return DefWindowProcW(hwnd, msg, wparam, lparam);
			}
		}

	} // namespace detail

	/// @brief Create a 1280x720 (client area) window, initialise the example and run its render loop
	/// @return Process exit code
	template<typename State>
	int RunExample(HINSTANCE hinstance, int ncmdshow, const wchar_t* class_name, const wchar_t* title,
		       State &state, const Callbacks<State> &cb) {
		WNDCLASSEXW wc {};
		wc.cbSize	 = sizeof(wc);
		wc.style	 = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc	 = detail::WndProc<State>;
		wc.hInstance	 = hinstance;
		wc.hCursor	 = LoadCursorW(nullptr, IDC_ARROW);
		wc.lpszClassName = class_name;
		if (!RegisterClassExW(&wc)) {
			return 1;
		}

		// Size the window so the client area (= back buffer) is exactly 1280x720
		RECT rect {0, 0, 1280, 720};
		AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

		detail::Context<State> ctx {&state, cb};
		HWND		       hwnd = CreateWindowExW(
			      0, class_name, title, WS_OVERLAPPEDWINDOW,
			      CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
			      nullptr, nullptr, hinstance, &ctx
		      );
		if (!hwnd) {
			return 1;
		}

		if (!cb.init(hwnd, state)) {
			cb.shutdown(state);
			MessageBoxW(hwnd, L"Failed to initialize the requested Direct3D mode.", title, MB_ICONERROR);
			DestroyWindow(hwnd);
			return 1;
		}
		ctx.ready = true;

		ShowWindow(hwnd, ncmdshow);
		UpdateWindow(hwnd);

		MSG msg {};
		while (msg.message != WM_QUIT) {
			if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			} else if (ctx.ready) {
				cb.render(hwnd, state);
			}
		}
		return static_cast<int>(msg.wParam);
	}

} // namespace example
