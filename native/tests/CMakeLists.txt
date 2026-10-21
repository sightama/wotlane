# ABOUTME: Testes de unidade das partes puras da camada nativa (sem jogo nem janela), compilados a parte do runtime.
# ABOUTME: tools\test_native.bat configura em native\tests\build, compila e roda.
cmake_minimum_required(VERSION 3.24)
project(wotm_native_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(wotm_native_test wotm_native_test.cpp)
target_include_directories(wotm_native_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
