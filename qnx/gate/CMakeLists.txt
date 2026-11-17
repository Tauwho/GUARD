# qnx/gate (B2, #39): guard-gate wraps guard::Gate from core/ in a QNX resource manager.
#   cmake -S qnx/gate -B build/qnx-gate -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake_minimum_required(VERSION 3.24)
project(guard_gate LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common qnx-common)
# Stream A's core library from the repo root; only the targets we link get built.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. guard-root EXCLUDE_FROM_ALL)

add_executable(guard-gate src/main.cpp src/status_text.cpp)
target_include_directories(guard-gate PRIVATE src)
target_link_libraries(guard-gate PRIVATE guard_core guard_qnx_common)
target_compile_options(guard-gate PRIVATE -Wall -Wextra)

# On-target check: fake /dev/sim/motor plus a scripted supervisor/operator client.
add_executable(guard_gate_check tests/gate_check.cpp)
target_link_libraries(guard_gate_check PRIVATE guard_core guard_qnx_common)
target_compile_options(guard_gate_check PRIVATE -Wall -Wextra)
