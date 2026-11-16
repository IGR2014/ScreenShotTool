# CMake version
cmake_minimum_required(VERSION 3.21.0)

# Project
project(
	ScreenShotTool
	VERSION		0.2.0
	DESCRIPTION	"DirectX 9/11 and OpenGL in-game screenshot tool"
	LANGUAGES	CXX
)

# Windows only (the tool hooks Direct3D / OpenGL via the Win32 API)
if(NOT WIN32)
	message(FATAL_ERROR "ScreenShotTool targets Windows only.")
endif()

# C++20 standard
set(CMAKE_CXX_STANDARD			20)
# Required C++20
set(CMAKE_CXX_STANDARD_REQUIRED		True)
# No compiler extensions
set(CMAKE_CXX_EXTENSIONS		OFF)
# Export compile commands (clangd / tooling)
set(CMAKE_EXPORT_COMPILE_COMMANDS	ON)
# Hidden inline visibility
set(CMAKE_VISIBILITY_INLINES_HIDDEN	True)

# MinGW needs -municode so the Unicode entry points (wWinMain / wmain) are used
if(MINGW)
	add_compile_options(-municode)
	add_link_options(-municode)
endif()

# Hook DLL name follows the target bitness (hookX64 / hookX86)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(SST_ARCH_SUFFIX "X64")
else()
	set(SST_ARCH_SUFFIX "X86")
endif()


# Shared injector sources (used by both the GUI and the x64 helper)
add_library(
	sst_common
	STATIC
	common/DLLInjector.cpp
)
target_include_directories(
	sst_common
	PUBLIC
	${CMAKE_SOURCE_DIR}
)


# Hook DLL (injected into the target process)
add_library(
	hook
	SHARED
	dxHookDLL/DLLMain.cpp
	dxHookDLL/Log.cpp
	dxHookDLL/WICImage.cpp
	dxHookDLL/InlineHook.cpp
	dxHookDLL/DX9Screenshoter.cpp
	dxHookDLL/DX11Screenshoter.cpp
	dxHookDLL/GLScreenshoter.cpp
)
target_include_directories(
	hook
	PRIVATE
	${CMAKE_SOURCE_DIR}
)
target_link_libraries(
	hook
	PRIVATE
	d3d9
	d3d11
	opengl32
	windowscodecs
	ole32
	gdi32
	user32
)
set_target_properties(
	hook
	PROPERTIES
	OUTPUT_NAME	"hook${SST_ARCH_SUFFIX}"
	PREFIX		""
)


# Injector GUI
add_executable(
	DLLInjector
	WIN32
	DLLInjector/DLLInjectorMain.cpp
)
target_link_libraries(
	DLLInjector
	PRIVATE
	sst_common
	shell32
	user32
)


# 64-bit injection helper (launched by the 32-bit GUI for 64-bit targets)
add_executable(
	x64Patcher
	x64Patcher/x64Patcher.cpp
)
target_link_libraries(
	x64Patcher
	PRIVATE
	sst_common
	user32
)
