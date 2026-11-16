# ScreenShotTool

[![CI](https://github.com/IGR2014/ScreenShotTool/actions/workflows/ci.yml/badge.svg)](https://github.com/IGR2014/ScreenShotTool/actions/workflows/ci.yml)

In-game screenshot tool for **Direct3D 9**, **Direct3D 10**, **Direct3D 11** and **OpenGL**
applications. A hook DLL is injected into the target process; on **F10** or
**PrintScreen** it captures the current frame and writes it next to the DLL
under `Screenshots/`, logging progress to `Logs/Log.txt`.

## Components

| Component        | Kind | Description                                                                 |
|------------------|------|-----------------------------------------------------------------------------|
| `hookX86/X64`    | DLL  | Hooks `IDirect3DDevice9::EndScene`, `IDXGISwapChain::Present` (Direct3D 10 and 11) and `SwapBuffers`, then saves the frame via WIC. |
| `DLLInjector`    | GUI  | Watches for known target processes and injects the matching hook DLL.       |
| `x64Patcher`     | CLI  | 64-bit helper the 32-bit GUI launches to inject into 64-bit targets.        |
| `common`         | lib  | Shared DLL injector and Win32 RAII helpers.                                 |

## Building

Requires a C++20 compiler (MSVC 2022 or MinGW-w64 GCC 13+) and CMake 3.21+.

```sh
# 64-bit (Visual Studio)
cmake -S . -B build -A x64
cmake --build build --config Release

# 32-bit
cmake -S . -B build32 -A Win32
cmake --build build32 --config Release
```

The hook DLL is named `hookX64.dll` or `hookX86.dll` after the target
bitness, which is what `DLLInjector` looks for at runtime. Ship both the
32- and 64-bit hook DLLs and `x64Patcher.exe` alongside the injector.

A MinGW-w64 cross build (used by CI on Linux) is also supported:

```sh
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake
cmake --build build
```

## Testing

A small dependency-free test suite (CTest) covers the pure-logic pieces —
`InlineHook` NOP padding, the `Common` RAII helpers, the `FrameCapture`
concept, `x64Patcher` argument parsing — and a WIC JPEG-encode integration
test. Tests are built by default at the top level:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Disable with `-DSST_BUILD_TESTS=OFF`.

## Releases

Pushing a `v*` tag runs the release workflow, which builds both
architectures and publishes a `ScreenShotTool-<tag>.zip` containing the
32-bit injector, the 64-bit patcher and both hook DLLs:

```sh
git tag v0.2.0
git push origin v0.2.0
```

## Notes

This is a legitimate frame-capture tool using the same `Present`/`EndScene`
hooking technique as overlays such as Steam, Discord and OBS. Image saving
uses the Windows Imaging Component (WIC); the legacy D3DX helpers it replaced
are no longer part of any current Windows SDK.
