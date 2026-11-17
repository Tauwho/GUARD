# qnx/sim (B3, #38): guard-sim wraps guard::Plant and guard::World from core/.
#   cmake -S qnx/sim -B build/qnx-sim -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake_minimum_required(VERSION 3.24)
project(guard_sim LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common qnx-common)
# Stream A's core library from the repo root; only the targets we link get built.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. guard-root EXCLUDE_FROM_ALL)

add_executable(guard-sim src/main.cpp)
target_link_libraries(guard-sim PRIVATE guard_core guard_qnx_common)
target_compile_options(guard-sim PRIVATE -Wall -Wextra)

# On-target check: plays gate, supervisor, planner and operator against guard-sim.
add_executable(guard_sim_check tests/sim_check.cpp)
target_link_libraries(guard_sim_check PRIVATE guard_core guard_qnx_common)
target_compile_options(guard_sim_check PRIVATE -Wall -Wextra)
