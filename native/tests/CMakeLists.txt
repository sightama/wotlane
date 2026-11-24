# ABOUTME: Unit tests for the pure parts of the native layer (no game, no window), built separately from the runtime.
# ABOUTME: tools\test_native.bat configures in native\tests\build, builds and runs them.
cmake_minimum_required(VERSION 3.24)
project(wotm_native_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(wotm_native_test wotm_native_test.cpp ../wotm_detmath.cpp ../wotm_json.cpp ../wotm_stagemap.cpp)
target_include_directories(wotm_native_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
# the native VU0 arithmetic (wotm_vu0_native.cpp, which the test includes) without PS2Recomp: vu0_stub stands in for its VU1State
add_executable(wotm_vu0_test wotm_vu0_test.cpp)
target_include_directories(wotm_vu0_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_SOURCE_DIR}/../runtime"
                           "${CMAKE_CURRENT_SOURCE_DIR}/vu0_stub")
set(test_targets wotm_native_test wotm_vu0_test)

# the SSE intrinsics the translated game's VU0 code uses, as the runtime gets them: sse2neon on ARM (the tag PS2Recomp's CMake
# fetches, with its definitions: SSE2NEON_PRECISE_MINMAX makes min/max pick the x86 operand on +-0 and NaN), Emscripten's SSE
# emulation in the browser (tools/build_web.sh's flags), AVX2 on x86 for the VU0 code's doubles (as the runtime builds it)
set(target_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)   # an x86_64 build on an Apple Silicon Mac (Rosetta) runs the x86 intrinsics
    set(target_arch "${CMAKE_OSX_ARCHITECTURES}")
endif()
if(target_arch MATCHES "arm64|aarch64|ARM64" AND NOT EMSCRIPTEN)
    include(FetchContent)
    FetchContent_Declare(sse2neon GIT_REPOSITORY https://github.com/DLTcollab/sse2neon.git GIT_TAG v1.9.1 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(sse2neon)
endif()

foreach(t IN LISTS test_targets)
    # the same float rules as the runtime's strict sources: every operation rounded on its own (wotm_detmath, wotm_json,
    # wotm_stagemap and the VU0 arithmetic's bits depend on it)
    if(MSVC)
        target_compile_options(${t} PRIVATE /fp:precise)
    else()
        target_compile_options(${t} PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
    if(target_arch MATCHES "arm64|aarch64|ARM64" AND NOT EMSCRIPTEN)
        target_include_directories(${t} PRIVATE "${sse2neon_SOURCE_DIR}")
        target_compile_definitions(${t} PRIVATE USE_SSE2NEON SSE2NEON_PRECISE_MINMAX=1)
    elseif(EMSCRIPTEN)
        target_compile_options(${t} PRIVATE -msimd128 -msse4.1)
        target_link_options(${t} PRIVATE -sALLOW_MEMORY_GROWTH=1)   # the zip tests hold a 64 MB guest RAM, as the runtime does
    endif()
endforeach()
if(NOT EMSCRIPTEN AND NOT target_arch MATCHES "arm64|aarch64|ARM64")
    if(MSVC)
        target_compile_options(wotm_vu0_test PRIVATE /arch:AVX2)
    else()
        target_compile_options(wotm_vu0_test PRIVATE -mavx2)
    endif()
endif()
