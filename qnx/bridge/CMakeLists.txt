# qnx/bridge (B5, #42): guard-bridge sends C8 telemetry from the shared-memory rings on TCP 7700.
#   cmake -S qnx/bridge -B build/qnx-bridge -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake_minimum_required(VERSION 3.24)
project(guard_bridge LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common qnx-common)

add_executable(guard-bridge src/main.cpp)
target_link_libraries(guard-bridge PRIVATE guard_qnx_common socket)
target_compile_options(guard-bridge PRIVATE -Wall -Wextra)