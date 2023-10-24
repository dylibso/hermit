# Copyright (C) 2023 Dylibso.  All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required (VERSION 3.14)

project (hermit-cli)

set (CMAKE_VERBOSE_MAKEFILE OFF)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()

set(cliwasmpath "${CMAKE_CURRENT_SOURCE_DIR}/target/wasm32-wasi/debug/hermitfile-parser.wasm")
set(cliwasmpath ${cliwasmpath} PARENT_SCOPE)
add_custom_command(OUTPUT ${cliwasmpath} COMMAND
  cargo build --target wasm32-wasi
  VERBATIM
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
add_custom_target(cli ALL DEPENDS ${cliwasmpath})
