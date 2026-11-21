# Direct3D test windows used to exercise the hook DLL.
# Output names carry the target bitness (e.g. dx11_x64.exe) and match the
# DLLInjector's TARGET_PROCESSES list.

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(SST_EXAMPLE_ARCH_SUFFIX "_x64")
else()
	set(SST_EXAMPLE_ARCH_SUFFIX "_x86")
endif()

# sst_add_example(<name> <libs...>) - builds <name>.cpp as a GUI executable
function(sst_add_example name)
	add_executable(${name} WIN32 ${name}.cpp ExampleCommon.h)
	set_target_properties(${name} PROPERTIES OUTPUT_NAME "${name}${SST_EXAMPLE_ARCH_SUFFIX}")
	target_link_libraries(${name} PRIVATE user32 ${ARGN})
endfunction()

sst_add_example(dx9  d3d9)
sst_add_example(dx10 d3d10 d3d10_1 dxgi)
sst_add_example(dx11 d3d11 dxgi)
