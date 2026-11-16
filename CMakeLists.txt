cmake_minimum_required(VERSION 3.24)
project(GUARD VERSION 0.1.0 LANGUAGES CXX)

add_library(guard_core INTERFACE)
target_compile_features(guard_core INTERFACE cxx_std_17)
target_include_directories(guard_core INTERFACE "${PROJECT_SOURCE_DIR}/core/include")
add_subdirectory(host)

option(GUARD_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
if(GUARD_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "GUARD_SANITIZERS requires GCC or Clang")
  endif()
  target_compile_options(guard_core INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(guard_core INTERFACE -fsanitize=address,undefined)
endif()

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
    URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(doctest)
  add_executable(guard_core_tests core/tests/smoke.cpp)
  target_link_libraries(guard_core_tests PRIVATE guard_core doctest::doctest)
  add_test(NAME guard_core_smoke COMMAND guard_core_tests)
endif()
