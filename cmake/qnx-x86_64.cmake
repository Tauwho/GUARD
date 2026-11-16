# CMake toolchain for QNX SDP 8.0, x86_64 target (QSTI on QEMU).
#
# Source qnxsdp-env.sh (Linux/WSL2) or qnxsdp-env.bat (Windows) first, then:
#   cmake --preset qnx-x86_64 && cmake --build --preset qnx-x86_64
# See docs/setup.md section 8. Nothing from the SDP is copied into the repo.

set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0.0)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

foreach(guard_var QNX_HOST QNX_TARGET)
  if("$ENV{${guard_var}}" STREQUAL "")
    message(FATAL_ERROR
      "${guard_var} is not set. Source qnxsdp-env first (docs/setup.md section 4).")
  endif()
  file(TO_CMAKE_PATH "$ENV{${guard_var}}" ${guard_var})
endforeach()

if(CMAKE_HOST_WIN32)
  set(guard_exe ".exe")
else()
  set(guard_exe "")
endif()

# qcc selects the target with -V<variant>; CMake passes the *_COMPILER_TARGET value.
set(CMAKE_C_COMPILER "${QNX_HOST}/usr/bin/qcc${guard_exe}")
set(CMAKE_C_COMPILER_TARGET gcc_ntox86_64)
set(CMAKE_CXX_COMPILER "${QNX_HOST}/usr/bin/q++${guard_exe}")
set(CMAKE_CXX_COMPILER_TARGET gcc_ntox86_64)

# Find target libraries and headers only in the SDP target tree; host programs on the host.
set(CMAKE_FIND_ROOT_PATH "${QNX_TARGET}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
