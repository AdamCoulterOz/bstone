cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

add_subdirectory(lib/khronos)
add_subdirectory(lib/nuked_opl3)
add_subdirectory(lib/openal_soft)
add_subdirectory(lib/stb)
add_subdirectory(lib/vulkan)
add_subdirectory(lib/xbrz)

if (BSTONE_INTERNAL_SDL2)
	add_subdirectory(lib/sdl2)
endif ()

# Host-side codegen tools (bin2c/glapigen). They are not used by the default
# build and cannot run on a cross-compiled device target, so skip them on tvOS.
if (NOT CMAKE_SYSTEM_NAME STREQUAL "tvOS")
	add_subdirectory(tools)
endif ()

add_subdirectory(bstone)

if (BSTONE_TESTS)
	add_subdirectory(tests)
endif ()
