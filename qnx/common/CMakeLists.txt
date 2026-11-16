# qnx/common (B-3, #37): thin QNX 8.0 wrappers. Builds only with the QNX toolchain:
#   cmake -S qnx/common -B build/qnx-common -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/qnx-x86_64.cmake
#   cmake --build build/qnx-common
# Tests run on the target (see qnx/common/README.md).
cmake_minimum_required(VERSION 3.24)
project(guard_qnx_common LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
  message(FATAL_ERROR "qnx/common needs the QNX toolchain (cmake/qnx-x86_64.cmake)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

add_library(guard_qnx_common STATIC
  src/ipc.cpp
  src/sched.cpp
  src/timer.cpp)
target_include_directories(guard_qnx_common PUBLIC include)

include(FetchContent)
FetchContent_Declare(doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
  URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(doctest)

add_executable(guard_qnx_common_tests tests/main.cpp tests/test_sched.cpp tests/test_ipc.cpp
  tests/test_timer.cpp)
target_link_libraries(guard_qnx_common_tests PRIVATE guard_qnx_common doctest::doctest)
