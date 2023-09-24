/*
 * Copyright (c) 2022-2023 Jianzhang Peng. All Rights Reserved.
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
 * Author: Jianzhang Peng (pengjianzhang@gmail.com)
 */

#include "trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <execinfo.h>

#define TRACE_NUM   32
void trace_callstack(void)
{
    int i = 0;
    int num = 0;
    char **info = NULL;
    char *str = NULL;
    void *array[TRACE_NUM];

    num = backtrace(array, TRACE_NUM);
    info = (char**)backtrace_symbols(array, num);

    for (i = 1; i < num - 4; i++) {
        str = info[i];
        if(strstr(str, "()") != NULL) {
            continue;
        }
        printf("%s\n", str);
    }

    if (info) {
        free(info);
    }
}
