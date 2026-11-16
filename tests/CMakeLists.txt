# Unit / integration tests (dependency-free harness + CTest)

# Common include root (lets tests include <common/...>, <dxHookDLL/...>, <x64Patcher/...>)
set(SST_TEST_INCLUDE ${CMAKE_SOURCE_DIR})


# Pure-logic tests: InlineHook NOP padding
add_executable(
	test_InlineHook
	test_InlineHook.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/InlineHook.cpp
)

# RAII helpers + FrameCapture concept (needs the capture TUs for the concept check)
add_executable(
	test_Common
	test_Common.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/InlineHook.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/Direct3D9Capture.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/Direct3D10Capture.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/Direct3D11Capture.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/OpenGLCapture.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/Globals.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/Log.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/WICImage.cpp
)
target_link_libraries(
	test_Common
	PRIVATE
	d3d9 d3d10 d3d11 opengl32 windowscodecs ole32 gdi32 user32
)

# Command-line parsing
add_executable(
	test_ArgParse
	test_ArgParse.cpp
)

# WIC JPEG encoder integration test
add_executable(
	test_WICImage
	test_WICImage.cpp
	${CMAKE_SOURCE_DIR}/dxHookDLL/WICImage.cpp
)
target_link_libraries(
	test_WICImage
	PRIVATE
	windowscodecs ole32
)


# Shared include dirs and registration with CTest
foreach(test IN ITEMS test_InlineHook test_Common test_ArgParse test_WICImage)
	target_include_directories(${test} PRIVATE ${SST_TEST_INCLUDE} ${CMAKE_CURRENT_SOURCE_DIR})
	add_test(NAME ${test} COMMAND ${test})
endforeach()
