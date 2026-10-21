# ABOUTME: Netcode do wotlane sem codigo do jogo: biblioteca wotm_net (protocolo, lockstep, cliente), servidor wotm_server e testes.
# ABOUTME: Compila sozinho (tools\build_net.bat) ou dentro do runtime (add_subdirectory no CMake do PS2Recomp, para o wotm.exe).
cmake_minimum_required(VERSION 3.24)
project(wotm_net C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    enet
    GIT_REPOSITORY "https://github.com/lsalzman/enet.git"
    GIT_TAG "v1.3.18"
)
FetchContent_MakeAvailable(enet)
if(WIN32)
    target_link_libraries(enet PUBLIC ws2_32 winmm)
endif()

add_library(wotm_net INTERFACE)
target_include_directories(wotm_net INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}" "${enet_SOURCE_DIR}/include")
target_link_libraries(wotm_net INTERFACE enet)

add_executable(wotm_net_test wotm_net_test.cpp)
target_link_libraries(wotm_net_test PRIVATE wotm_net)
