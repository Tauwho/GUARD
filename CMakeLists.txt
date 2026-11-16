cmake_minimum_required(VERSION 3.24)
project(GUARD VERSION 0.1.0 LANGUAGES CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GUARD_GENERATED "${PROJECT_BINARY_DIR}/generated/guard")
file(GLOB GUARD_FIXTURES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/contracts/fixtures/messages/*.json")
add_custom_command(
  OUTPUT "${GUARD_GENERATED}/reason_codes.hpp" "${GUARD_GENERATED}/contracts_fixtures.cpp"
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/generate_contracts.py"
          "${PROJECT_SOURCE_DIR}/contracts" "${GUARD_GENERATED}"
  DEPENDS tools/generate_contracts.py contracts/reason_codes.yaml ${GUARD_FIXTURES})
add_library(guard_core STATIC core/src/codec/validation.cpp core/src/config/config.cpp core/src/config/loader.cpp
               core/src/plant/geometry.cpp core/src/plant/plant.cpp core/src/fsm/gate_fsm.cpp
               core/src/safety/stopping.cpp core/src/safety/supervisor.cpp core/src/safety/recheck.cpp
               "${GUARD_GENERATED}/reason_codes.hpp")
target_compile_features(guard_core PUBLIC cxx_std_17)
target_include_directories(guard_core PUBLIC "${PROJECT_SOURCE_DIR}/core/include"
                                            "${PROJECT_BINARY_DIR}/generated")
add_subdirectory(host)

option(GUARD_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
if(GUARD_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "GUARD_SANITIZERS requires GCC or Clang")
  endif()
  target_compile_options(guard_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(guard_core PUBLIC -fsanitize=address,undefined)
endif()

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
  endif()
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
    URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(doctest)
  add_executable(guard_core_tests core/tests/smoke.cpp core/tests/contracts_edges.cpp
                 core/tests/plant_geometry.cpp core/tests/plant_dynamics.cpp core/tests/gate_fsm_rows.cpp
                 core/tests/gate_fsm_integration.cpp core/tests/supervisor_checks.cpp
                 core/tests/supervisor_recheck.cpp core/tests/contracts_config.cpp
                 "${GUARD_GENERATED}/contracts_fixtures.cpp")
  target_link_libraries(guard_core_tests PRIVATE guard_core doctest::doctest)
  add_test(NAME guard_core_smoke COMMAND guard_core_tests)
endif()
