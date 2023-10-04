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

/* clang-format off */
static int
print_help(const char *prog_name)
{
    fprintf(stderr, "usage: %s [args...]\n", prog_name);
    fprintf(stderr, "%s must have hermit.json and main.wasm embedded in its zip filesystem\n", prog_name);
    return 1;
}
/* clang-format on */

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

// similar to C++ vector reserve
static bool reserve(const char ***list, uint32_t *list_capacity, const uint32_t new_capacity)
{
    if (*list_capacity >= new_capacity)
        return true;
    const char **new_list = realloc(*list, new_capacity * sizeof(const char *));
    if (new_list == NULL)
    {
        fprintf(stderr, "%s: realloc failed %u -> %u\n", __func__, *list_capacity, new_capacity);
        return false;
    }
    *list = new_list;
    *list_capacity = new_capacity;
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

int main(int argc, char *argv[])
{
    defer_free struct json_value_s *json = NULL;
    {
        defer_free char *json_bytes = NULL;
        int size;
        {
            static const char *hermit_json_path = "/zip/hermit.json";
            defer_close FILE *json_file = fopen(hermit_json_path, "rb");
            if (json_file == NULL)
            {
                fprintf(stderr, "error opening %s\n", hermit_json_path);
                return 1;
            }
            if (fseek(json_file, 0, SEEK_END) != 0)
            {
                fprintf(stderr, "error seeking on %s\n", hermit_json_path);
                return 1;
            }
            size = ftell(json_file);
            if (size < 0)
            {
                fprintf(stderr, "error ftell on %s\n", hermit_json_path);
                return 1;
            }
            rewind(json_file);
            json_bytes = malloc(size);
            if (json_bytes == NULL)
            {
                fprintf(stderr, "error malloc(%d)\n", size);
                return 1;
            }
            const int fread_status = fread(json_bytes, size, 1, json_file);
            if (fread_status != 1)
            {
                fprintf(stderr, "error fread on %s\n", hermit_json_path);
                return 1;
            }
        }
        json = json_parse(json_bytes, size);
    }
    if (json == NULL)
    {
        fprintf(stderr, "error parsing json\n");
        return 1;
    }
    if (json->type != json_type_object)
    {
        fprintf(stderr, "error json should consist of an object\n");
        return 1;
    }

    const char **dir_list = NULL;
    uint32_t dir_list_max = 0;
    uint32_t dir_list_size = 0;
    const char **env_list = NULL;
    uint32_t env_list_max = 0;
    uint32_t env_list_size = 0;
    const char *func_name = NULL;

    const struct json_object_s *object = json->payload;
    typedef enum
    {
        HC_UNKNOWN = -1,
        HC_MAP,
        HC_ENV_PWD_IS_HOST_CWD,
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
        {"ENV", json_type_array, HC_ENV},
        {"NET", json_type_array, HC_NET},
        {"ARGV", json_type_array, HC_ARGV},
        {"ENTRYPOINT", json_type_string, HC_ENTRYPOINT}};
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
                    free(json);
                    fprintf(stderr, "%s: expected %s got %s!\n", items[i].key, get_json_type_name(items[i].type), get_json_type_name(item->value->type));
                    return print_help(argv[0]);
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
            if (!reserve(&dir_list, &dir_list_max, dir_list_size + value->length))
            {
                fprintf(stderr, "MAP: reserve failed\n");
                return 1;
            }
            for (const struct json_array_element_s *aitem = value->start; aitem != NULL; aitem = aitem->next)
            {
                if (aitem->value->type != json_type_string)
                {
                    free(json);
                    fprintf(stderr, "MAP must be an array of strings\n");
                    return print_help(argv[0]);
                }
                const struct json_string_s *string = aitem->value->payload;
                char *dir_item = memdup(string->string, string->string_size + 1);
                if (!dir_item)
                {
                    fprintf(stderr, "MAP: malloc failed\n");
                    return 1;
                }
                dir_list[dir_list_size++] = dir_item;
            }
            break;
        }
        case HC_ENV_PWD_IS_HOST_CWD:
        {
            if (!reserve(&env_list, &env_list_max, env_list_size + 1))
            {
                fprintf(stderr, "ENV_PWD_IS_HOST_CWD: reserve failed\n");
                return 1;
            }
            char *wd = getcwd(NULL, 0);
            static const char pwd_prefix[] = "PWD=";
            const size_t wd_len = strlen(wd);
            const size_t pwd_size = sizeof(pwd_prefix) + wd_len;
            char *pwd = malloc(pwd_size);
            if (!pwd)
            {
                fprintf(stderr, "ENV_PWD_IS_HOST_CWD: malloc failed\n");
                return 1;
            }
            memcpy(mempcpy(pwd, pwd_prefix, sizeof(pwd_prefix) - 1), wd, wd_len + 1);
            free(wd);
            env_list[env_list_size++] = pwd;
            break;
        }
        case HC_ENV:
        {
            const struct json_array_s *value = item->value->payload;
            if (!reserve(&env_list, &env_list_max, env_list_size + value->length + 1))
            {
                fprintf(stderr, "ENV: reserve failed\n");
                return 1;
            }
            for (const struct json_array_element_s *aitem = value->start; aitem != NULL; aitem = aitem->next)
            {
                if (aitem->value->type != json_type_string)
                {
                    free(json);
                    fprintf(stderr, "ENV must be an array of strings\n");
                    return print_help(argv[0]);
                }
                const struct json_string_s *string = aitem->value->payload;
                if (!validate_env_str(string->string))
                {
                    fprintf(stderr, "ENV: parse env string failed: expect \"key=value\", "
                                    "got \"%s\"\n",
                            string->string);
                    return print_help(argv[0]);
                }
                char *env_item = memdup(string->string, string->string_size + 1);
                if (!env_item)
                {
                    fprintf(stderr, "ENV: memdup failed\n");
                    return 1;
                }
                env_list[env_list_size++] = env_item;
            }
            break;
        }
        case HC_ENTRYPOINT:
        {
            const struct json_string_s *value = item->value->payload;
            func_name = memdup(value->string, value->string_size + 1);
            if (!func_name)
            {
                fprintf(stderr, "ENTRYPOINT: memdup failed\n");
                return 1;
            }
            break;
        }
        case HC_UNKNOWN:
        case HC_NET:
        case HC_ARGV:
            break;
        }
        fprintf(stderr, "hermit_loader: %s key: %.*s\n", ((config_index != HC_UNKNOWN) ? "found" : "unknown"), (int)name->string_size, name->string);
    }
    free(json);

    // setup args
    int app_argc = argc >= 1 ? argc : 1;
    char **app_argv = malloc((app_argc + 1) * sizeof(char *));
    app_argv[0] = "/zip/main.wasm";
    memcpy(&app_argv[1], &argv[1], sizeof(char *) * (argc - 1));
    app_argv[app_argc] = NULL;
    const char *wasm_file = app_argv[0];

    // WAMR backend using wasm_runtime_api
    return wamr(wasm_file, app_argc, app_argv, dir_list, dir_list_size, env_list, env_list_size, func_name);
}
