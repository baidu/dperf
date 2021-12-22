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

#ifndef __CONFIG_KEYWORD_H
#define __CONFIG_KEYWORD_H

/*
 * A Linux configuration style parser
 * ----------------------------------
 * keyword0 arg1 arg2 arg3 ..
 * keyword1 arg1 arg2 arg3 ..
 * keyword2 arg1 arg2 arg3 ..
 * */

struct config_keyword {
    const char *name;
    int (*handler)(int argc, char *argv[], void *data);
    const char *help;   /* manual */
};

#define CONFIG_ARG_NUM_MAX    32
#define CONFIG_LINE_MAX       2048

int config_keyword_parse(const char *file_path, const struct config_keyword *keywords, void *data);
void config_keyword_help(const struct config_keyword *keywords);

#endif
