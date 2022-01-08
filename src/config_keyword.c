/*
 * Copyright (c) 2021 Baidu.com, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Jianzhang Peng (pengjianzhang@baidu.com)
 */

#include "config_keyword.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct config_keyword *config_keyword_lookup(const struct config_keyword *keywords, const char *name)
{
    int i = 0;

    while (keywords[i].name != NULL) {
        if (strcmp(keywords[i].name, name) == 0) {
            return &keywords[i];
        }
        i++;
    }

    return NULL;
}

static char *config_skip_space(char *str)
{
    char *s = str;

    while ((s != NULL) && (*s != 0)) {
        if (isspace(*s)) {
            s++;
            continue;
        } else {
            return s;
        }
    }

    return NULL;
}

static char *config_next_space(char *str)
{
    char *s = str;

    while ((s != NULL) && (*s != 0)) {
        if (isspace(*s)) {
            break;
        }
        s++;
    }

    return s;
}

static int config_get_string(char *in, char **out, char **next)
{
    char *start = NULL;
    char *end = NULL;

    start = config_skip_space(in);
    if (start == NULL) {
        return 0;
    }

    end = config_next_space(start);
    if (start == end) {
        return 0;
    }

    *out = start;
    if (*end != 0) {
        *end = 0;
        end++;
    }

    *next = end;
    return end - start;
}

static int config_keyword_check_input(const char *line)
{
    const char *p = line;

    while (p && *p) {
        if (!(isascii(*p) && ((isprint(*p) || isspace(*p))))) {
            return -1;
        }
        p++;
    }

    return 0;
}

static int config_keyword_parse_line(char *line, char **argv, int argv_size)
{
    int argc = 0;
    char *str = NULL;
    char *arg = NULL;

    if (config_keyword_check_input(line) < 0) {
        return -1;
    }

    str = line;
    for (argc = 0; argc < argv_size; argc++) {
        if (config_get_string(str, &arg, &str) == 0) {
            break;
        }

        argv[argc] = arg;
        if ((argc == 0) && (*arg == '#')) {
            return 0;
        }
    }

    return argc;
}

static int config_keyword_call(const struct config_keyword *keywords, int argc, char **argv, void *data, int line_num)
{
    const struct config_keyword *keyword = NULL;

    keyword = config_keyword_lookup(keywords, argv[0]);
    if (keyword == NULL) {
        printf("line %d: unknown config keyword(\"%s\")\n", line_num, argv[0]);
        return -1;
    }

    if (keyword->handler) {
        if (keyword->handler(argc, argv, data) != 0) {
            printf("line %d: error\n", line_num);
            return -1;
        }
    }

    return 0;
}

int config_keyword_parse(const char *file_path, const struct config_keyword *keywords, void *data)
{
    int ret = 0;
    int argc = 0;
    int line_num = 0;
    FILE *fp = NULL;
    char *argv[CONFIG_ARG_NUM_MAX];
    char config_line[CONFIG_LINE_MAX];

    fp = fopen(file_path, "r");
    if (fp == NULL) {
        printf("config file open error: %s\n", file_path);
        return -1;
    }

    while (!feof(fp)) {
        line_num++;
        if (fgets(config_line, CONFIG_LINE_MAX - 1, fp) == NULL) {
            break;
        }

        argc = config_keyword_parse_line(config_line, argv, CONFIG_ARG_NUM_MAX);
        if (argc == 0) {
            continue;
        } else if (argc == -1) {
            ret = -1;
            break;
        }

        ret = config_keyword_call(keywords, argc, argv, data, line_num);
        if (ret < 0) {
            break;
        }
    }

    fclose(fp);
    return ret;
}

void config_keyword_help(const struct config_keyword *keywords)
{
    const struct config_keyword *keyword = keywords;

    while (keyword && (keyword->name != NULL)) {
        if (keyword->help) {
            printf("%s %s\n", keyword->name, keyword->help);
        } else {
            printf("%s\n", keyword->name);
        }
        keyword++;
    }
}
