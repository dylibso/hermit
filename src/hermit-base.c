/*
 * Copyright (C) 2023 Dylibso.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "wamr.h"

// cosmopolitan libc internal function
char *GetProgramExecutableName(void);

static const char *get_json_type_name(const json_type_t t)
{
#define X(name)       \
    case name:        \
        return #name; \
        break;
    switch (t)
    {
        X(json_type_string)
        X(json_type_number)
        X(json_type_object)
        X(json_type_array)
        X(json_type_true)
        X(json_type_false)
        X(json_type_null)
    default:
        return "UNKNOWN_TYPE";
    }
#undef X
}

typedef struct
{
    char **arr;
    uint32_t max;
    uint32_t size;
} list;

// similar to C++ vector reserve
static bool list_reserve(list *l, const uint32_t new_capacity)
{
    if (l->max >= new_capacity)
        return true;
    char **new_list = realloc(l->arr, new_capacity * sizeof(const char *));
    if (new_list == NULL)
    {
        fprintf(stderr, "%s: realloc failed %u -> %u\n", __func__, l->max, new_capacity);
        return false;
    }
    l->arr = new_list;
    l->max = new_capacity;
    return true;
}

// strdup for mem
static void *memdup(const void *src, const size_t size)
{
    void *dest = malloc(size);
    if (dest)
    {
        memcpy(dest, src, size);
    }
    return dest;
}

#define defer(fn) __attribute__((cleanup(fn)))

void cleanup_free(void *p)
{
    void **tofree = (void **)p;
    if (*tofree == NULL)
    {
        return;
    }
    free(*tofree);
}
#define defer_free defer(cleanup_free)

void cleanup_close(FILE **fp)
{
    if (*fp == NULL)
    {
        return;
    }
    fclose(*fp);
}
#define defer_close defer(cleanup_close)

void cleanup_list(list *l)
{
    if (l->arr)
    {
        for (uint32_t i = 0; i < l->size; i++)
        {
            free(l->arr[i]);
        }
        free(l->arr);
    }
}
#define defer_list defer(cleanup_list)

static struct json_value_s *load_json_file(const char *json_path)
{
    defer_free char *json_bytes = NULL;
    int size;
    {
        defer_close FILE *json_file = fopen(json_path, "rb");
        if (json_file == NULL)
        {
            fprintf(stderr, "error opening %s\n", json_path);
            return NULL;
        }
        if (fseek(json_file, 0, SEEK_END) != 0)
        {
            fprintf(stderr, "error seeking on %s\n", json_path);
            return NULL;
        }
        size = ftell(json_file);
        if (size < 0)
        {
            fprintf(stderr, "error ftell on %s\n", json_path);
            return NULL;
        }
        rewind(json_file);
        json_bytes = malloc(size);
        if (json_bytes == NULL)
        {
            fprintf(stderr, "error malloc(%d)\n", size);
            return NULL;
        }
        const int fread_status = fread(json_bytes, size, 1, json_file);
        if (fread_status != 1)
        {
            fprintf(stderr, "error fread on %s\n", json_path);
            return NULL;
        }
    }
    return json_parse(json_bytes, size);
}

static bool load_hermit_config(const char *hermit_json_path, list *dir_list, list *env_list, char **func_name)
{
    const bool debug = getenv("HERMIT_DEBUG_BASE") != NULL;
    defer_free struct json_value_s *json = load_json_file(hermit_json_path);
    if (json == NULL)
    {
        fprintf(stderr, "error parsing json\n");
        return false;
    }
    if (json->type != json_type_object)
    {
        fprintf(stderr, "error json should consist of an object\n");
        return false;
    }

    typedef enum
    {
        HC_UNKNOWN = -1,
        HC_MAP,
        HC_ENV_PWD_IS_HOST_CWD,
        HC_ENV_EXE_NAME_IS_HOST_EXE_NAME,
        HC_NET,
        HC_ARGV,
        HC_ENV,
        HC_ENTRYPOINT
    } hermit_config_index;
    typedef struct
    {
        const char *key;
        json_type_t type;
        hermit_config_index index;
    } hermit_config_item;
    static const hermit_config_item items[] = {
        {"MAP", json_type_array, HC_MAP},
        {"ENV_PWD_IS_HOST_CWD",
         json_type_true,
         HC_ENV_PWD_IS_HOST_CWD},
        {"ENV_EXE_NAME_IS_HOST_EXE_NAME", json_type_true, HC_ENV_EXE_NAME_IS_HOST_EXE_NAME},
        {"ENV", json_type_array, HC_ENV},
        {"NET", json_type_array, HC_NET},
        {"ARGV", json_type_array, HC_ARGV},
        {"ENTRYPOINT", json_type_string, HC_ENTRYPOINT}};
    const struct json_object_s *object = json->payload;
    for (const struct json_object_element_s *item = object->start; item != NULL;
         item = item->next)
    {
        const struct json_string_s *name = item->name;
        hermit_config_index config_index = HC_UNKNOWN;
        for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++)
        {
            if (strcmp(items[i].key, name->string) == 0)
            {
                if (items[i].type != item->value->type)
                {
                    fprintf(stderr, "%s: expected %s got %s!\n", items[i].key, get_json_type_name(items[i].type), get_json_type_name(item->value->type));
                    return false;
                }
                config_index = items[i].index;
                break;
            }
        }
        switch (config_index)
        {
        case HC_MAP:
        {
            const struct json_array_s *value = item->value->payload;
            if (!list_reserve(dir_list, dir_list->size + value->length))
            {
                fprintf(stderr, "MAP: list_reserve failed\n");
                return false;
            }
            for (const struct json_array_element_s *aitem = value->start; aitem != NULL; aitem = aitem->next)
            {
                if (aitem->value->type != json_type_string)
                {
                    fprintf(stderr, "MAP must be an array of strings\n");
                    return false;
                }
                const struct json_string_s *string = aitem->value->payload;
                char *dir_item = memdup(string->string, string->string_size + 1);
                if (!dir_item)
                {
                    fprintf(stderr, "MAP: malloc failed\n");
                    return false;
                }
                dir_list->arr[dir_list->size++] = dir_item;
            }
            break;
        }
        case HC_ENV_PWD_IS_HOST_CWD:
        {
            if (!list_reserve(env_list, env_list->size + 1))
            {
                fprintf(stderr, "ENV_PWD_IS_HOST_CWD: list_reserve failed\n");
                return false;
            }
            defer_free char *wd = getcwd(NULL, 0);
            static const char pwd_prefix[] = "PWD=";
            const size_t wd_len = strlen(wd);
            const size_t pwd_size = sizeof(pwd_prefix) + wd_len;
            char *pwd = malloc(pwd_size);
            if (!pwd)
            {
                fprintf(stderr, "ENV_PWD_IS_HOST_CWD: malloc failed\n");
                return false;
            }
            memcpy(mempcpy(pwd, pwd_prefix, sizeof(pwd_prefix) - 1), wd, wd_len + 1);
            env_list->arr[env_list->size++] = pwd;
            break;
        }
        case HC_ENV_EXE_NAME_IS_HOST_EXE_NAME:
        {
            if (!list_reserve(env_list, env_list->size + 1))
            {
                fprintf(stderr, "ENV_EXE_NAME_IS_HOST_EXE_NAME: list_reserve failed\n");
                return false;
            }
            const char *exe = GetProgramExecutableName();
            static const char exename_prefix[] = "EXE_NAME=";
            const size_t exe_len = strlen(exe);
            const size_t exename_size = sizeof(exename_prefix) + exe_len;
            char *exename = malloc(exename_size);
            if (!exename)
            {
                fprintf(stderr, "ENV_PWD_IS_HOST_CWD: malloc failed\n");
                return false;
            }
            memcpy(mempcpy(exename, exename_prefix, sizeof(exename_prefix) - 1), exe, exe_len + 1);
            env_list->arr[env_list->size++] = exename;
            break;
        }
        case HC_ENV:
        {
            const struct json_array_s *value = item->value->payload;
            if (!list_reserve(env_list, env_list->size + value->length + 1))
            {
                fprintf(stderr, "ENV: list_reserve failed\n");
                return false;
            }
            for (const struct json_array_element_s *aitem = value->start; aitem != NULL; aitem = aitem->next)
            {
                if (aitem->value->type != json_type_string)
                {
                    fprintf(stderr, "ENV must be an array of strings\n");
                    return false;
                }
                const struct json_string_s *string = aitem->value->payload;
                if (!validate_env_str(string->string))
                {
                    fprintf(stderr, "ENV: parse env string failed: expect \"key=value\", "
                                    "got \"%s\"\n",
                            string->string);
                    return false;
                }
                char *env_item = memdup(string->string, string->string_size + 1);
                if (!env_item)
                {
                    fprintf(stderr, "ENV: memdup failed\n");
                    return false;
                }
                env_list->arr[env_list->size++] = env_item;
            }
            break;
        }
        case HC_ENTRYPOINT:
        {
            const struct json_string_s *value = item->value->payload;
            if (value->string_size == 0)
            {
                break;
            }
            *func_name = memdup(value->string, value->string_size + 1);
            if (!(*func_name))
            {
                fprintf(stderr, "ENTRYPOINT: memdup failed\n");
                return false;
            }
            break;
        }
        case HC_UNKNOWN:
        case HC_NET:
        case HC_ARGV:
            break;
        }
        if (debug)
        {
            fprintf(stderr, "hermit-base: %s key: %.*s\n", ((config_index != HC_UNKNOWN) ? "found" : "unknown"), (int)name->string_size, name->string);
        }
    }
    return true;
}

int main(int argc, char *argv[])
{
    // load config
    defer_list list dir_list = {0};
    defer_list list env_list = {0};
    defer_free char *func_name = NULL;
    if (!load_hermit_config("/zip/hermit.json", &dir_list, &env_list, &func_name))
    {
        return 1;
    }

    // setup args
    int app_argc = argc >= 1 ? argc : 1;
    defer_free char **app_argv = malloc((app_argc + 1) * sizeof(char *));
    app_argv[0] = "/zip/main.wasm";
    memcpy(&app_argv[1], &argv[1], sizeof(char *) * (argc - 1));
    app_argv[app_argc] = NULL;
    const char *wasm_file = app_argv[0];

    // WAMR backend using wasm_runtime_api
    return wamr(wasm_file, app_argc, app_argv, dir_list.arr, dir_list.size, env_list.arr, env_list.size, func_name);
}
