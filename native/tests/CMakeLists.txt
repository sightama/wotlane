# ABOUTME: Unit tests for the pure parts of the native layer (no game, no window), built separately from the runtime.
# ABOUTME: tools\test_native.bat configures in native\tests\build, builds and runs them.
cmake_minimum_required(VERSION 3.24)
project(wotm_native_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(wotm_native_test wotm_native_test.cpp ../wotm_detmath.cpp ../wotm_json.cpp ../wotm_stagemap.cpp)
target_include_directories(wotm_native_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
# the same float rules as the runtime's strict sources: every operation rounded on its own (wotm_detmath, wotm_json and wotm_stagemap bits depend on it)
if(MSVC)
    target_compile_options(wotm_native_test PRIVATE /fp:precise)
else()
    target_compile_options(wotm_native_test PRIVATE -ffp-contract=off -fno-fast-math)
endif()
# the SSE intrinsics the translated game's VU0 code uses, as the runtime gets them: sse2neon on ARM (the tag PS2Recomp's CMake
# fetches, with its definitions: SSE2NEON_PRECISE_MINMAX makes min/max pick the x86 operand on +-0 and NaN), Emscripten's SSE
# emulation in the browser (tools/build_web.sh's flags)
set(target_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)   # an x86_64 build on an Apple Silicon Mac (Rosetta) runs the x86 intrinsics
    set(target_arch "${CMAKE_OSX_ARCHITECTURES}")
endif()
if(target_arch MATCHES "arm64|aarch64|ARM64" AND NOT EMSCRIPTEN)
    include(FetchContent)
    FetchContent_Declare(sse2neon GIT_REPOSITORY https://github.com/DLTcollab/sse2neon.git GIT_TAG v1.9.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(sse2neon)
    target_include_directories(wotm_native_test PRIVATE "${sse2neon_SOURCE_DIR}")
    target_compile_definitions(wotm_native_test PRIVATE USE_SSE2NEON SSE2NEON_PRECISE_MINMAX=1)
elseif(EMSCRIPTEN)
    target_compile_options(wotm_native_test PRIVATE -msimd128 -msse4.1)
    target_link_options(wotm_native_test PRIVATE -sALLOW_MEMORY_GROWTH=1)   # the zip tests hold a 64 MB guest RAM, as the runtime does
elseif(NOT MSVC)
    target_compile_options(wotm_native_test PRIVATE -msse4.1)   # x86: wotm_rtz_lanes.hpp's SSE4.1 intrinsics, which the browser build also uses
endif()
