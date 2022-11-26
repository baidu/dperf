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

/*
 * Don't Change HTTP1.1 Request/Response Format.
 * Keep the minimum request and response packet sizes the same.
 * Mimimal HTTP/1.1 Packet Size is 128 Bytes.
 * */

#define HTTP_REQ_FORMAT         \
    "GET %s HTTP/1.1\r\n"       \
    "User-Agent: dperf\r\n"     \
    "Host: %s\r\n"              \
    "Accept: */*\r\n"           \
    "P: aa\r\n"                 \
    "\r\n"

#define HTTP_RSP_FORMAT         \
    "HTTP/1.1 200 OK\n"         \
    "Serv:dperf\n"              \
    "Content-Length:%4d\n"      \
    "Connection:keep-alive\n"   \
    "\n"                        \
    "%s"

static char http_rsp[MBUF_DATA_SIZE];
static char http_req[MBUF_DATA_SIZE];
static const char *http_rsp_body_default = "hello dperf!\r\n";

const char *http_get_request(void)
{
    return http_req;
}

const char *http_get_response(void)
{
    return http_rsp;
}

static void http_set_payload_client(struct config *cfg, char *dest, int len, int payload_size)
{
    int pad = 0;
    char buf[MBUF_DATA_SIZE] = {0};

    if (payload_size <= 0) {
        snprintf(dest, len, HTTP_REQ_FORMAT, cfg->http_path, cfg->http_host);
    } else if (payload_size < HTTP_DATA_MIN_SIZE) {
        config_set_payload(cfg, dest, payload_size, 1);
    } else {
        pad = payload_size - HTTP_DATA_MIN_SIZE;
        if (pad > 0) {
            config_set_payload(cfg, buf, pad, 0);
        }
        buf[0] = '/';
        snprintf(dest, len, HTTP_REQ_FORMAT, buf, cfg->http_host);
    }
}

static void http_set_payload_server(struct config *cfg, char *dest, int len, int payload_size)
{
    int pad = 0;
    char buf[MBUF_DATA_SIZE] = {0};
    const char *data = NULL;

    if (payload_size <= 0) {
        data = http_rsp_body_default;
        snprintf(dest, len, HTTP_RSP_FORMAT, (int)strlen(data), data);
    } else if (payload_size < HTTP_DATA_MIN_SIZE) {
        config_set_payload(cfg, dest, payload_size, 1);
    } else {
        pad = payload_size - HTTP_DATA_MIN_SIZE;
        if (pad > 0) {
            config_set_payload(cfg, buf, pad, 1);
        }
        snprintf(dest, len, HTTP_RSP_FORMAT, (int)strlen(buf), buf);
    }
}

void http_set_payload(struct config *cfg, int payload_size)
{
    http_set_payload_server(cfg, http_rsp, MBUF_DATA_SIZE, payload_size);
    http_set_payload_client(cfg, http_req, MBUF_DATA_SIZE, payload_size);
}
