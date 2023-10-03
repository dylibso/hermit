/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * Copyright (C) 2023 Dylibso.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bh_platform.h"
#include "bh_read_file.h"
#include "wasm_export.h"

#if BH_HAS_DLFCN
#include <dlfcn.h>
#endif

static const void *
app_instance_main(wasm_module_inst_t module_inst, int app_argc, char *app_argv[])
{
    const char *exception;

    wasm_application_execute_main(module_inst, app_argc, app_argv);
    if ((exception = wasm_runtime_get_exception(module_inst)))
        printf("%s\n", exception);
    return exception;
}

static const void *
app_instance_func(wasm_module_inst_t module_inst, int app_argc, char *app_argv[], const char *func_name)
{
    wasm_application_execute_func(module_inst, func_name, app_argc - 1,
                                  app_argv + 1);
    /* The result of wasm function or exception info was output inside
       wasm_application_execute_func(), here we don't output them again. */
    return wasm_runtime_get_exception(module_inst);
}

#if WASM_ENABLE_LIBC_WASI != 0
bool validate_env_str(const char *env)
{
    const char *p = env;
    int key_len = 0;

    while (*p != '\0' && *p != '=')
    {
        key_len++;
        p++;
    }

    if (*p != '=' || key_len == 0)
        return false;

    return true;
}
#endif

#if BH_HAS_DLFCN
typedef uint32 (*get_native_lib_func)(char **p_module_name,
                                      NativeSymbol **p_native_symbols);

static uint32
load_and_register_native_libs(const char **native_lib_list,
                              uint32 native_lib_count,
                              void **native_handle_list)
{
    uint32 i, native_handle_count = 0, n_native_symbols;
    NativeSymbol *native_symbols;
    char *module_name;
    void *handle;

    for (i = 0; i < native_lib_count; i++)
    {
        /* open the native library */
        if (!(handle = dlopen(native_lib_list[i], RTLD_NOW | RTLD_GLOBAL)) && !(handle = dlopen(native_lib_list[i], RTLD_LAZY)))
        {
            LOG_WARNING("warning: failed to load native library %s",
                        native_lib_list[i]);
            continue;
        }

        /* lookup get_native_lib func */
        get_native_lib_func get_native_lib = dlsym(handle, "get_native_lib");
        if (!get_native_lib)
        {
            LOG_WARNING("warning: failed to lookup `get_native_lib` function "
                        "from native lib %s",
                        native_lib_list[i]);
            dlclose(handle);
            continue;
        }

        n_native_symbols = get_native_lib(&module_name, &native_symbols);

        /* register native symbols */
        if (!(n_native_symbols > 0 && module_name && native_symbols && wasm_runtime_register_natives(module_name, native_symbols, n_native_symbols)))
        {
            LOG_WARNING("warning: failed to register native lib %s",
                        native_lib_list[i]);
            dlclose(handle);
            continue;
        }

        native_handle_list[native_handle_count++] = handle;
    }

    return native_handle_count;
}

static void
unregister_and_unload_native_libs(uint32 native_lib_count,
                                  void **native_handle_list)
{
    uint32 i, n_native_symbols;
    NativeSymbol *native_symbols;
    char *module_name;
    void *handle;

    for (i = 0; i < native_lib_count; i++)
    {
        handle = native_handle_list[i];

        /* lookup get_native_lib func */
        get_native_lib_func get_native_lib = dlsym(handle, "get_native_lib");
        if (!get_native_lib)
        {
            LOG_WARNING("warning: failed to lookup `get_native_lib` function "
                        "from native lib %p",
                        handle);
            continue;
        }

        n_native_symbols = get_native_lib(&module_name, &native_symbols);
        if (n_native_symbols == 0 || module_name == NULL || native_symbols == NULL)
        {
            LOG_WARNING("warning: get_native_lib returned different values for "
                        "native lib %p",
                        handle);
            continue;
        }

        /* unregister native symbols */
        if (!wasm_runtime_unregister_natives(module_name, native_symbols))
        {
            LOG_WARNING("warning: failed to unregister native lib %p", handle);
            continue;
        }

        dlclose(handle);
    }
}
#endif /* BH_HAS_DLFCN */

#if WASM_ENABLE_MULTI_MODULE != 0
static char *
handle_module_path(const char *module_path)
{
    /* next character after = */
    return (strchr(module_path, '=')) + 1;
}

static char *module_search_path = ".";

static bool
module_reader_callback(const char *module_name, uint8 **p_buffer,
                       uint32 *p_size)
{
    const char *format = "%s/%s.wasm";
    int sz = strlen(module_search_path) + strlen("/") + strlen(module_name) + strlen(".wasm") + 1;
    char *wasm_file_name = BH_MALLOC(sz);
    if (!wasm_file_name)
    {
        return false;
    }

    snprintf(wasm_file_name, sz, format, module_search_path, module_name);

    *p_buffer = (uint8_t *)bh_read_file_to_buffer(wasm_file_name, p_size);

    wasm_runtime_free(wasm_file_name);
    return *p_buffer != NULL;
}

static void
moudle_destroyer(uint8 *buffer, uint32 size)
{
    if (!buffer)
    {
        return;
    }

    wasm_runtime_free(buffer);
    buffer = NULL;
}
#endif /* WASM_ENABLE_MULTI_MODULE */

#if WASM_ENABLE_GLOBAL_HEAP_POOL != 0
static char global_heap_buf[WASM_GLOBAL_HEAP_SIZE] = {0};
#endif

#if WASM_ENABLE_STATIC_PGO != 0
static void
dump_pgo_prof_data(wasm_module_inst_t module_inst, const char *path)
{
    char *buf;
    uint32 len;
    FILE *file;

    if (!(len = wasm_runtime_get_pgo_prof_data_size(module_inst)))
    {
        printf("failed to get LLVM PGO profile data size\n");
        return;
    }

    if (!(buf = wasm_runtime_malloc(len)))
    {
        printf("allocate memory failed\n");
        return;
    }

    if (len != wasm_runtime_dump_pgo_prof_data_to_buf(module_inst, buf, len))
    {
        printf("failed to dump LLVM PGO profile data\n");
        wasm_runtime_free(buf);
        return;
    }

    if (!(file = fopen(path, "wb")))
    {
        printf("failed to create file %s", path);
        wasm_runtime_free(buf);
        return;
    }
    fwrite(buf, len, 1, file);
    fclose(file);

    wasm_runtime_free(buf);

    printf("LLVM raw profile file %s was generated.\n", path);
}
#endif

int wamr(const char *wasm_file, int argc, char *argv[], const char *dir_list[], const uint32_t dir_list_size, const char *env_list[], const uint32_t env_list_size, const char *func_name)
{
    int32 ret = -1;
    uint8 *wasm_file_buf = NULL;
    uint32 wasm_file_size;
    uint32 stack_size = 64 * 1024;
#if WASM_ENABLE_LIBC_WASI != 0
    uint32 heap_size = 0;
#else
    uint32 heap_size = 16 * 1024;
#endif
#if WASM_ENABLE_FAST_JIT != 0
    uint32 jit_code_cache_size = FAST_JIT_DEFAULT_CODE_CACHE_SIZE;
#endif
#if WASM_ENABLE_JIT != 0
    uint32 llvm_jit_size_level = 3;
    uint32 llvm_jit_opt_level = 3;
    uint32 segue_flags = 0;
#endif
    wasm_module_t wasm_module = NULL;
    wasm_module_inst_t wasm_module_inst = NULL;
    RunningMode running_mode = 0;
    RuntimeInitArgs init_args;
    char error_buf[128] = {0};
#if WASM_ENABLE_LOG != 0
    int log_verbose_level = 2;
#endif
    bool is_xip_file = false;
#if WASM_CONFIGUABLE_BOUNDS_CHECKS != 0
    bool disable_bounds_checks = false;
#endif
#if WASM_ENABLE_LIBC_WASI != 0
    const char *addr_pool[8] = {NULL};
    uint32 addr_pool_size = 0;
    const char *ns_lookup_pool[8] = {NULL};
    uint32 ns_lookup_pool_size = 0;
#endif
#if BH_HAS_DLFCN
    const char *native_lib_list[8] = {NULL};
    uint32 native_lib_count = 0;
    void *native_handle_list[8] = {NULL};
    uint32 native_handle_count = 0;
#endif
#if WASM_ENABLE_DEBUG_INTERP != 0
    char *ip_addr = NULL;
    int instance_port = 0;
#endif
#if WASM_ENABLE_STATIC_PGO != 0
    const char *gen_prof_file = NULL;
#endif

    memset(&init_args, 0, sizeof(RuntimeInitArgs));

    init_args.running_mode = running_mode;
#if WASM_ENABLE_GLOBAL_HEAP_POOL != 0
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);
#else
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = malloc;
    init_args.mem_alloc_option.allocator.realloc_func = realloc;
    init_args.mem_alloc_option.allocator.free_func = free;
#endif

#if WASM_ENABLE_FAST_JIT != 0
    init_args.fast_jit_code_cache_size = jit_code_cache_size;
#endif

#if WASM_ENABLE_JIT != 0
    init_args.llvm_jit_size_level = llvm_jit_size_level;
    init_args.llvm_jit_opt_level = llvm_jit_opt_level;
    init_args.segue_flags = segue_flags;
#endif

#if WASM_ENABLE_DEBUG_INTERP != 0
    init_args.instance_port = instance_port;
    if (ip_addr)
        strcpy(init_args.ip_addr, ip_addr);
#endif

    /* initialize runtime environment */
    if (!wasm_runtime_full_init(&init_args))
    {
        printf("Init runtime environment failed.\n");
        return -1;
    }

#if WASM_ENABLE_LOG != 0
    bh_log_set_verbose_level(log_verbose_level);
#endif

#if BH_HAS_DLFCN
    native_handle_count = load_and_register_native_libs(
        native_lib_list, native_lib_count, native_handle_list);
#endif

    /* load WASM byte buffer from WASM bin file */
    if (!(wasm_file_buf =
              (uint8 *)bh_read_file_to_buffer(wasm_file, &wasm_file_size)))
        goto fail1;

#if WASM_ENABLE_AOT != 0
    if (wasm_runtime_is_xip_file(wasm_file_buf, wasm_file_size))
    {
        uint8 *wasm_file_mapped;
        int map_prot = MMAP_PROT_READ | MMAP_PROT_WRITE | MMAP_PROT_EXEC;
        int map_flags = MMAP_MAP_32BIT;

        if (!(wasm_file_mapped =
                  os_mmap(NULL, (uint32)wasm_file_size, map_prot, map_flags)))
        {
            printf("mmap memory failed\n");
            wasm_runtime_free(wasm_file_buf);
            goto fail1;
        }

        bh_memcpy_s(wasm_file_mapped, wasm_file_size, wasm_file_buf,
                    wasm_file_size);
        wasm_runtime_free(wasm_file_buf);
        wasm_file_buf = wasm_file_mapped;
        is_xip_file = true;
    }
#endif

#if WASM_ENABLE_MULTI_MODULE != 0
    wasm_runtime_set_module_reader(module_reader_callback, moudle_destroyer);
#endif

    /* load WASM module */
    if (!(wasm_module = wasm_runtime_load(wasm_file_buf, wasm_file_size,
                                          error_buf, sizeof(error_buf))))
    {
        printf("%s\n", error_buf);
        goto fail2;
    }

#if WASM_ENABLE_LIBC_WASI != 0
    wasm_runtime_set_wasi_args(wasm_module, dir_list, dir_list_size, NULL, 0,
                               env_list, env_list_size, argv, argc);

    wasm_runtime_set_wasi_addr_pool(wasm_module, addr_pool, addr_pool_size);
    wasm_runtime_set_wasi_ns_lookup_pool(wasm_module, ns_lookup_pool,
                                         ns_lookup_pool_size);
#endif

    /* instantiate the module */
    if (!(wasm_module_inst =
              wasm_runtime_instantiate(wasm_module, stack_size, heap_size,
                                       error_buf, sizeof(error_buf))))
    {
        printf("%s\n", error_buf);
        goto fail3;
    }

#if WASM_CONFIGUABLE_BOUNDS_CHECKS != 0
    if (disable_bounds_checks)
    {
        wasm_runtime_set_bounds_checks(wasm_module_inst, false);
    }
#endif

#if WASM_ENABLE_DEBUG_INTERP != 0
    if (ip_addr != NULL)
    {
        wasm_exec_env_t exec_env =
            wasm_runtime_get_exec_env_singleton(wasm_module_inst);
        uint32_t debug_port;
        if (exec_env == NULL)
        {
            printf("%s\n", wasm_runtime_get_exception(wasm_module_inst));
            goto fail4;
        }
        debug_port = wasm_runtime_start_debug_instance(exec_env);
        if (debug_port == 0)
        {
            printf("Failed to start debug instance\n");
            goto fail4;
        }
    }
#endif

    ret = 0;
    if (func_name)
    {
        if (app_instance_func(wasm_module_inst, argc, argv, func_name))
        {
            /* got an exception */
            ret = 1;
        }
    }
    else
    {
        if (app_instance_main(wasm_module_inst, argc, argv))
        {
            /* got an exception */
            ret = 1;
        }
    }

#if WASM_ENABLE_LIBC_WASI != 0
    if (ret == 0)
    {
        /* wait for threads to finish and propagate wasi exit code. */
        ret = wasm_runtime_get_wasi_exit_code(wasm_module_inst);
        if (wasm_runtime_get_exception(wasm_module_inst))
        {
            /* got an exception in spawned thread */
            ret = 1;
        }
    }
#endif

#if WASM_ENABLE_STATIC_PGO != 0 && WASM_ENABLE_AOT != 0
    if (get_package_type(wasm_file_buf, wasm_file_size) == Wasm_Module_AoT && gen_prof_file)
        dump_pgo_prof_data(wasm_module_inst, gen_prof_file);
#endif

#if WASM_ENABLE_DEBUG_INTERP != 0
fail4:
#endif
    /* destroy the module instance */
    wasm_runtime_deinstantiate(wasm_module_inst);

fail3:
    /* unload the module */
    wasm_runtime_unload(wasm_module);

fail2:
    /* free the file buffer */
    if (!is_xip_file)
        wasm_runtime_free(wasm_file_buf);
    else
        os_munmap(wasm_file_buf, wasm_file_size);

fail1:
#if BH_HAS_DLFCN
    /* unload the native libraries */
    unregister_and_unload_native_libs(native_handle_count, native_handle_list);
#endif

    /* destroy runtime environment */
    wasm_runtime_destroy();

    return ret;
}
