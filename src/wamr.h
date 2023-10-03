/*
 * Copyright (C) 2023 Dylibso.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once
#include <stdint.h>

int wamr(const char *wasm_file, int argc, char *argv[], const char *dir_list[], const uint32_t dir_list_size, const char *env_list[], const uint32_t env_list_size, const char *func_name);

bool validate_env_str(const char *env);
