# ABOUTME: Unit tests for the pure parts of the native layer (no game, no window), built separately from the runtime.
# ABOUTME: tools\test_native.bat configures in native\tests\build, builds and runs them.
cmake_minimum_required(VERSION 3.24)
project(wotm_native_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(wotm_native_test wotm_native_test.cpp)
target_include_directories(wotm_native_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
