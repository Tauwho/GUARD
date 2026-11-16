# qnx/hog (B-10, #44): guard-hog, a SCHED_FIFO busy loop for fault F2. Test image only.
# Builds only with the QNX toolchain:
#   cmake -S qnx/hog -B build/qnx-hog -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake_minimum_required(VERSION 3.24)
project(guard_hog LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common qnx-common)

add_library(guard_hog_lib STATIC src/hog.cpp)
target_include_directories(guard_hog_lib PUBLIC include)
target_link_libraries(guard_hog_lib PUBLIC guard_qnx_common)

add_executable(guard-hog src/main.cpp)
target_link_libraries(guard-hog PRIVATE guard_hog_lib)

add_executable(guard_hog_tests tests/test_hog.cpp)
target_link_libraries(guard_hog_tests PRIVATE guard_hog_lib doctest::doctest)
