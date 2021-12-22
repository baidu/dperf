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

#ifndef __HTTP_H
#define __HTTP_H

#include <stdint.h>
#include "work_space.h"

static inline void http_parse_response(uint8_t *data, uint16_t len)
{
    /*HTTP/1.1 200 OK*/
    if ((len > 9) && data[9] == '2') {
        net_stats_http_2xx();
    } else {
        net_stats_http_error();
    }
}

static inline void http_parse_request(uint8_t *data, uint16_t len)
{
    /*
     * GET /xxx HTTP/1.1
     * First Char is G
     * */
    if ((len > 18) && data[0] == 'G') {
        net_stats_http_get();
    } else {
        net_stats_http_error();
    }
}

#define HTTP_BUF_SIZE       2048
void http_set_payload(int payload_size);
const char *http_get_request(void);
const char *http_get_response(void);

#endif
