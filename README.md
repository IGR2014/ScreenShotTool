# ScreenShotTool

In-game screenshot tool for **Direct3D 9**, **Direct3D 11** and **OpenGL**
applications. A hook DLL is injected into the target process; on **F10** or
**PrintScreen** it captures the current frame and writes it next to the DLL
under `Screenshots/`, logging progress to `Logs/Log.txt`.

## Components

| Component        | Kind | Description                                                                 |
|------------------|------|-----------------------------------------------------------------------------|
| `hookX86/X64`    | DLL  | Hooks `IDirect3DDevice9::EndScene`, `IDXGISwapChain::Present` and `SwapBuffers`, then saves the frame via WIC. |
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

## Notes

This is a legitimate frame-capture tool using the same `Present`/`EndScene`
hooking technique as overlays such as Steam, Discord and OBS. Image saving
uses the Windows Imaging Component (WIC); the legacy D3DX helpers it replaced
are no longer part of any current Windows SDK.
