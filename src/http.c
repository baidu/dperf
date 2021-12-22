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

#include "http.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "version.h"

#define HTTP_RSP_FORMAT         \
"HTTP/1.1 200 OK\r\n"           \
"Server: dperf/"VERSION"\r\n"   \
"Content-Type: text/plain\r\n"  \
"Content-Length: %d\r\n"        \
"Connection: keep-alive\r\n"    \
"\r\n"                          \
"%s"

#define HTTP_REQ_FORMAT             \
"GET /%s HTTP/1.1\r\n"              \
"User-Agent: dperf/"VERSION"\r\n"   \
"Host: localhost\r\n"               \
"Accept: */*\r\n"                   \
"\r\n"

static char http_rsp[HTTP_BUF_SIZE];
static char http_req[HTTP_BUF_SIZE];
static const char *http_rsp_body_default = "hello dperf!\r\n";
static const char *http_req_path_default = "";

const char *http_get_request(void)
{
    return http_req;
}

const char *http_get_response(void)
{
    return http_rsp;
}

static int http_set_payload_buf(char *dest, const char *data, bool server)
{
    if (server) {
        return sprintf(dest, HTTP_RSP_FORMAT, (int)strlen(data), data);
    } else {
        return sprintf(dest, HTTP_REQ_FORMAT, data);
    }
}

static void http_set_payload_common(char *dest, const char *default_data, int payload_size, bool server)
{
    int pad = 0;
    int total = 0;
    int min_size = 0;
    char buf[HTTP_BUF_SIZE] = {0};

    if (payload_size == 0) {
        http_set_payload_buf(dest, default_data, server);
        return;
    }

    if (server) {
        min_size = strlen(HTTP_RSP_FORMAT);
    } else {
        min_size = strlen(HTTP_REQ_FORMAT);
    }

    if (payload_size <= min_size) {
        pad = 1;
    } else {
        pad = payload_size - min_size + 2;
    }

    for (; pad >= 1; pad--) {
        memset(buf, 'a', pad);
        buf[pad] = 0;
        total = http_set_payload_buf(dest, buf, server);
        if (total <= PAYLOAD_SIZE_MAX) {
            break;
        }
    }
}

void http_set_payload(int payload_size)
{
    http_set_payload_common(http_rsp, http_rsp_body_default, payload_size, true);
    http_set_payload_common(http_req, http_req_path_default, payload_size, false);
}
