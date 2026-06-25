#
# CMake toolchain for building bstone for Apple tvOS (device, arm64).
#
# Targets Apple TV 4K (gen 1 / A10X and gen 2 / A12) — both arm64 — with a
# minimum deployment target of tvOS 26.0, built against whatever tvOS SDK is
# installed (deployment target != SDK version).
#
# Use with the Xcode generator:
#
#   cmake -B build-tvos -G Xcode \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/tvos.toolchain.cmake
#
# then open build-tvos/bstone_solution.xcodeproj, set your Team, and Run.
#
# For the tvOS Simulator instead of a device, override the sysroot:
#   -DCMAKE_OSX_SYSROOT=appletvsimulator
#

set(CMAKE_SYSTEM_NAME tvOS)
set(CMAKE_SYSTEM_PROCESSOR arm64)

set(CMAKE_OSX_SYSROOT appletvos CACHE STRING "tvOS SDK (appletvos / appletvsimulator)")
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "tvOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET 26.0 CACHE STRING "Minimum tvOS deployment target")

# The bundled SDL2 is the only viable SDL on tvOS (cross-compiled, Metal-backed).
set(BSTONE_INTERNAL_SDL2 ON CACHE BOOL "Use bundled SDL2 (required for tvOS)" FORCE)

# Host-built codegen tools (bin2c/glapigen) are not part of the default build;
# search the host for programs, the tvOS sysroot for libraries/headers.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
