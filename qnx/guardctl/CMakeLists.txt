# qnx/guardctl (B5, #42): the operator CLI, contracts/guardctl.md (C14).
#   cmake -S qnx/guardctl -B build/qnx-guardctl -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake_minimum_required(VERSION 3.24)
project(guard_guardctl LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common qnx-common)
# Stream A's core library from the repo root (message codecs only).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. guard-root EXCLUDE_FROM_ALL)

add_executable(guardctl src/main.cpp)
target_link_libraries(guardctl PRIVATE guard_core guard_qnx_common)
target_compile_options(guardctl PRIVATE -Wall -Wextra)