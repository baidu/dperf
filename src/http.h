/*
 * Copyright (c) 2021-2022 Baidu.com, Inc. All Rights Reserved.
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
 * Author: Jianzhang Peng (pengjianzhang@baidu.com)
 *         Jianzhang Peng (pengjianzhang@gmail.com)
 */

#ifndef __HTTP_H
#define __HTTP_H

#include <stdint.h>
#include "work_space.h"

static inline void http_parse_response(const uint8_t *data, uint16_t len)
{
    net_stats_tcp_rsp();
    /* HTTP/1.1 200 OK */
    if ((len > 9) && data[9] == '2') {
        net_stats_http_2xx();
    } else {
        net_stats_http_error();
    }
}

static inline void http_parse_request(const uint8_t *data, uint16_t len)
{
    net_stats_tcp_req();
    /*
     * GET /xxx HTTP/1.1
     * First Char is G
     * POST /XXX HTTP/1.1
     * Second Char is O
     * */
    if (len > 18) {
        if (data[0] == 'G') {
            net_stats_http_get();
        } else if (data[1] == 'O') {
            net_stats_http_post();
        } else {
            net_stats_http_error();
        }
    } else {
        net_stats_http_error();
    }
}

#define HTTP_DATA_MIN_SIZE  85
void http_set_payload(struct config *cfg, int payload_size);
const char *http_get_request(void);
const char *http_get_response(void);

#endif
