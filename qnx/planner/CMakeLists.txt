# qnx/planner (B3, #41): guard-planner wraps guard::Planner (rule backend) from core/.
#   cmake -S qnx/planner -B build/qnx-planner -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake_minimum_required(VERSION 3.24)
project(guard_planner LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common qnx-common)
# Stream A's core library from the repo root; only the targets we link get built.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. guard-root EXCLUDE_FROM_ALL)

add_executable(guard-planner src/main.cpp)
target_link_libraries(guard-planner PRIVATE guard_core guard_qnx_common)
target_compile_options(guard-planner PRIVATE -Wall -Wextra)

# On-target check against the image's guard-sim, guard-gate and guard-supervisor.
add_executable(guard_planner_check tests/planner_check.cpp)
target_link_libraries(guard_planner_check PRIVATE guard_core guard_qnx_common)
target_compile_options(guard_planner_check PRIVATE -Wall -Wextra)