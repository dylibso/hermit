# Copyright (C) 2019 Intel Corporation.  All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required (VERSION 3.14)

include(CheckPIESupported)

project (hermit)

set (CMAKE_VERBOSE_MAKEFILE OFF)

set (WAMR_BUILD_PLATFORM "cosmopolitan")

# Reset default linker flags
set (CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set (CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

set (CMAKE_C_STANDARD 99)
set (CMAKE_CXX_STANDARD 17)

# Set WAMR_BUILD_TARGET, currently values supported:
# "X86_64", "AMD_64", "X86_32", "AARCH64[sub]", "ARM[sub]", "THUMB[sub]",
# "MIPS", "XTENSA", "RISCV64[sub]", "RISCV32[sub]"
if (NOT DEFINED WAMR_BUILD_TARGET)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
    set (WAMR_BUILD_TARGET "AARCH64")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    set (WAMR_BUILD_TARGET "RISCV64")
  elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # Build as X86_64 by default in 64-bit platform
    set (WAMR_BUILD_TARGET "X86_64")
  elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    # Build as X86_32 by default in 32-bit platform
    set (WAMR_BUILD_TARGET "X86_32")
  else ()
    message(SEND_ERROR "Unsupported build target platform!")
  endif ()
endif ()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()

#lower case needed to identify the right hermitfile-parser's "Hermitfile.$CMAKE_BUILD_TYPE_LOWER" file 
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

if (NOT DEFINED WAMR_BUILD_INTERP)
  # Enable Interpreter by default
  set (WAMR_BUILD_INTERP 1)
endif ()

if (NOT DEFINED WAMR_BUILD_AOT)
  # Enable AOT by default.
  set (WAMR_BUILD_AOT 1)
endif ()

if (NOT DEFINED WAMR_BUILD_JIT)
  # Disable JIT by default.
  set (WAMR_BUILD_JIT 0)
endif ()

if (NOT DEFINED WAMR_BUILD_FAST_JIT)
  # Disable Fast JIT by default
  set (WAMR_BUILD_FAST_JIT 0)
endif ()

if (NOT DEFINED WAMR_BUILD_LIBC_BUILTIN)
  # Enable libc builtin support by default
  set (WAMR_BUILD_LIBC_BUILTIN 1)
endif ()

if (NOT DEFINED WAMR_BUILD_LIBC_WASI)
  # Enable libc wasi support by default
  set (WAMR_BUILD_LIBC_WASI 1)
endif ()

if (NOT DEFINED WAMR_BUILD_FAST_INTERP)
  # Enable fast interpreter
  set (WAMR_BUILD_FAST_INTERP 1)
endif ()

if (NOT DEFINED WAMR_BUILD_MULTI_MODULE)
  # Disable multiple modules by default
  set (WAMR_BUILD_MULTI_MODULE 0)
endif ()

if (NOT DEFINED WAMR_BUILD_LIB_PTHREAD)
  # Disable pthread library by default
  set (WAMR_BUILD_LIB_PTHREAD 0)
endif ()

if (NOT DEFINED WAMR_BUILD_LIB_WASI_THREADS)
  # Disable wasi threads library by default
  set (WAMR_BUILD_LIB_WASI_THREADS 0)
endif()


if (NOT DEFINED WAMR_BUILD_MINI_LOADER)
  # Disable wasm mini loader by default
  set (WAMR_BUILD_MINI_LOADER 0)
endif ()

if (NOT DEFINED WAMR_BUILD_SIMD)
  # Enable SIMD by default
  set (WAMR_BUILD_SIMD 1)
endif ()

if (NOT DEFINED WAMR_BUILD_REF_TYPES)
  # Disable reference types by default
  set (WAMR_BUILD_REF_TYPES 0)
endif ()

if (NOT DEFINED WAMR_BUILD_DEBUG_INTERP)
  # Disable Debug feature by default
  set (WAMR_BUILD_DEBUG_INTERP 0)
endif ()

if (WAMR_BUILD_DEBUG_INTERP EQUAL 1)
  set (WAMR_BUILD_FAST_INTERP 0)
  set (WAMR_BUILD_MINI_LOADER 0)
  set (WAMR_BUILD_SIMD 0)
endif ()

set (WAMR_DISABLE_STACK_HW_BOUND_CHECK 1)
set (WAMR_BUILD_AOT 0)
set (WAMR_DISABLE_WRITE_GS_BASE 1)

set (WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wasm-micro-runtime)

include (${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

check_pie_supported()
add_library(vmlib ${WAMR_RUNTIME_LIB_SOURCE})
set_target_properties (vmlib PROPERTIES POSITION_INDEPENDENT_CODE ON)

set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Wshadow")
# set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion -Wsign-conversion")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Wno-unused")

if (WAMR_BUILD_TARGET MATCHES "X86_.*" OR WAMR_BUILD_TARGET STREQUAL "AMD_64")
  if (NOT (CMAKE_C_COMPILER MATCHES ".*clang.*" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mindirect-branch-register")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mindirect-branch-register")
    # UNDEFINED BEHAVIOR, refer to https://en.cppreference.com/w/cpp/language/ub
  endif ()
endif ()

# The following flags are to enhance security, but it may impact performance,
# we disable them by default.
#if (WAMR_BUILD_TARGET MATCHES "X86_.*" OR WAMR_BUILD_TARGET STREQUAL "AMD_64")
#  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftrapv -D_FORTIFY_SOURCE=2")
#endif ()
#set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong --param ssp-buffer-size=4")
#set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack,-z,relro,-z,now")

include (${SHARED_DIR}/utils/uncommon/shared_uncommon.cmake)

set(CMAKE_EXECUTABLE_SUFFIX ".com")

add_executable (hermit-base src/hermit-base.c src/wamr.c ${UNCOMMON_SHARED_SOURCE})
set_target_properties (hermit-base PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries (hermit-base vmlib ${LLVM_AVAILABLE_LIBS} ${UV_A_LIBS} ${WASI_NN_LIBS} -lm -ldl -lpthread)

add_subdirectory(hermit-cli)

add_custom_command(OUTPUT "hermit.com" COMMAND
  wasmtime run --env "PWD=${CMAKE_CURRENT_BINARY_DIR}" --env "EXE_NAME=hermit-base.com" --dir / "${cliwasmpath}" -f "${CMAKE_CURRENT_SOURCE_DIR}/hermit-cli/Hermitfile.${CMAKE_BUILD_TYPE_LOWER}" -o hermit.com
  DEPENDS hermit-base cli
  VERBATIM)
add_custom_target(hermit ALL DEPENDS "hermit.com")
add_custom_command(
  TARGET hermit
  POST_BUILD
  COMMAND chmod
  ARGS +x "hermit.com"
)

add_custom_command(OUTPUT "cowsay.hermit.com" COMMAND
  ./hermit.com -f ${CMAKE_CURRENT_SOURCE_DIR}/src/cowsay/Hermitfile -o "cowsay.hermit.com"
  DEPENDS hermit
  VERBATIM)
add_custom_target(cowsay-hermit ALL DEPENDS "cowsay.hermit.com")
add_custom_command(
  TARGET cowsay-hermit
  POST_BUILD
  COMMAND chmod
  ARGS +x "cowsay.hermit.com"
)

add_custom_command(OUTPUT "count_vowels.hermit.com" COMMAND
  ./hermit.com -f ${CMAKE_CURRENT_SOURCE_DIR}/src/count_vowels/Hermitfile -o "count_vowels.hermit.com"
  DEPENDS hermit
  VERBATIM)
add_custom_target(count_vowels-hermit ALL DEPENDS "count_vowels.hermit.com")
add_custom_command(
  TARGET count_vowels-hermit
  POST_BUILD
  COMMAND chmod
  ARGS +x "count_vowels.hermit.com"
)

add_custom_command(OUTPUT "cat.hermit.com" COMMAND
  ./hermit.com -f ${CMAKE_CURRENT_SOURCE_DIR}/src/cat/Hermitfile -o "cat.hermit.com"
  DEPENDS hermit
  VERBATIM)
add_custom_target(cat-hermit ALL DEPENDS "cat.hermit.com")
add_custom_command(
  TARGET cat-hermit
  POST_BUILD
  COMMAND chmod
  ARGS +x "cat.hermit.com"
)
