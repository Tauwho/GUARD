# qnx/supervisor (B3, #40): guard-supervisor wraps guard::Supervisor from core/.
#   cmake -S qnx/supervisor -B build/qnx-supervisor -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake_minimum_required(VERSION 3.24)
project(guard_supervisor LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common qnx-common)
# Stream A's core library from the repo root; only the targets we link get built.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. guard-root EXCLUDE_FROM_ALL)

add_executable(guard-supervisor src/main.cpp)
target_link_libraries(guard-supervisor PRIVATE guard_core guard_qnx_common)
target_compile_options(guard-supervisor PRIVATE -Wall -Wextra)

# On-target check against the image's guard-sim and guard-gate.
add_executable(guard_supervisor_check tests/supervisor_check.cpp)
target_link_libraries(guard_supervisor_check PRIVATE guard_core guard_qnx_common)
target_compile_options(guard_supervisor_check PRIVATE -Wall -Wextra)