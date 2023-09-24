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

#include "http_parse.h"
#include "socket.h"
#include "mbuf.h"
#include "http.h"
#include <stdlib.h>

#ifdef HTTP_PARSE
#define STRING_SIZE(str)  (sizeof(str) - 1)

#define CONTENT_LENGTH_NAME "Content-Length:"
#define CONTENT_LENGTH_SIZE STRING_SIZE(CONTENT_LENGTH_NAME)

#define TRANSFER_ENCODING_NAME "Transfer-Encoding:"
#define TRANSFER_ENCODING_SIZE STRING_SIZE(TRANSFER_ENCODING_NAME)

#define CONNECTION_NAME "Connection:"
#define CONNECTION_SIZE STRING_SIZE(CONNECTION_NAME)

static inline int http_header_match(const uint8_t *start, int name_len, int name_size,
    uint8_t f0, uint8_t f1, uint8_t l0, uint8_t l1)
{
    uint8_t first = 0;
    uint8_t last = 0;

    if (name_len == name_size) {
        first = start[0];
        last = start[name_size - 2];
        /* use the first and the last letter to identify 'content-length'*/
        if (((first == f0) || (first == f1)) && ((last == l0) || (last == l1))) {
            return 1;
        }
    }

    return 0;
}

static inline int http_parse_header_line(struct socket *sk, const uint8_t *start, int name_len, int line_len)
{
    int i = 0;
    long content_length = 0;

    if (http_header_match(start, name_len, CONTENT_LENGTH_SIZE, 'C', 'c', 'h', 'H')) {
        content_length = atol((const char *)(start + CONTENT_LENGTH_SIZE));
        if (content_length < 0) {
            return -1;
        }

        if ((sk->http_flags & HTTP_F_TRANSFER_ENCODING) == 0) {
            sk->http_length = content_length;
            sk->http_flags |= HTTP_F_CONTENT_LENGTH;
        } else {
            return -1;
        }
    } else if (http_header_match(start, name_len, TRANSFER_ENCODING_SIZE, 'T', 't', 'g', 'G')) {
        /* find 'chunked' in 'gzip/deflate/chunked' */
        for (i = name_len; i < line_len; i++) {
            /* 'k' stands for 'chunked' */
            if ((start[i] == 'k') || (start[i] == 'K')) {
                if ((sk->http_flags & HTTP_F_CONTENT_LENGTH) == 0) {
                    sk->http_flags |= HTTP_F_TRANSFER_ENCODING;
                    break;
                } else {
                    return -1;
                }
            }
        }
    } else if (http_header_match(start, name_len, CONNECTION_SIZE, 'C', 'c', 'n', 'N')) {
        if (line_len < (name_len + (int)STRING_SIZE("keep-alive") + 1)) {
            sk->http_flags |= HTTP_F_CLOSE;
            sk->keepalive = 0;
        }
    }

    return 0;
}

/*
 * headers must be in an mbuf
 * */
static int http_parse_headers(struct socket *sk, const uint8_t *data, int data_len)
{
    const uint8_t *p = NULL;
    const uint8_t *start = NULL;
    const uint8_t *end = NULL;
    uint8_t c = 0;
    int line_len = 0;
    int name_len = 0;

    start = data;
    p = data;
    end = data + data_len;
    while (p < end) {
        c = *p;
        p++;
        line_len++;
        if ((c == ':') && (name_len == 0)) {
            name_len = line_len;
        } else if (c == '\r') {
            continue;
        } else if (c == '\n') {
            if (sk->http_parse_state == HTTP_HEADER_BEGIN) {
                if (http_parse_header_line(sk, start, name_len, line_len) < 0) {
                    return -1;
                }
                line_len = 0;
                name_len = 0;
                start = p;
                sk->http_parse_state = HTTP_HEADER_LINE_END;
            } else {
            /* end of header */
                sk->http_parse_state = HTTP_HEADER_DONE;
                if (sk->http_flags == 0) {
                    sk->http_flags = HTTP_F_CONTENT_LENGTH_AUTO | HTTP_F_CLOSE;
                    sk->http_length = -1;
                    sk->keepalive = 0;
                }
                break;
            }
        } else if (sk->http_parse_state != HTTP_HEADER_BEGIN) {
            sk->http_parse_state = HTTP_HEADER_BEGIN;
        }
    }

    return p - data;
}

/*
 * https://datatracker.ietf.org/doc/html/rfc7230
 * */
static inline int http_parse_chunk(struct socket *sk, const uint8_t *data, int data_len)
{
    uint8_t c = 0;
    int len = 0;
    const uint8_t *end = data + data_len;
    const uint8_t *p = data;

retry:
    switch (sk->http_parse_state) {
    case HTTP_HEADER_DONE:
        sk->http_parse_state = HTTP_CHUNK_SIZE;
        /* fall through */
    case HTTP_CHUNK_SIZE:
        while (p < end) {
            c = *p;
            p++;
            if ((c >= '0') && (c <= '9')) {
                sk->http_length = (sk->http_length << 4) + c - '0';
            } else if ((c >= 'a') && (c <= 'f')) {
                sk->http_length = (sk->http_length << 4) + c - 'a' + 10;
            } else {
                sk->http_parse_state = HTTP_CHUNK_SIZE_END;
                break;
            }
        }
        /* fall through */
    case HTTP_CHUNK_SIZE_END:
        while (p < end) {
            c = *p;
            p++;
            if (c == '\n') {
                if (sk->http_length > 0) {
                    sk->http_parse_state = HTTP_CHUNK_DATA;
                    break;
                } else {
                    sk->http_parse_state = HTTP_CHUNK_TRAILER_BEGIN;
                    goto trailer_begin;
                }
            }
            /* skip chunk ext */
        }
        /* fall through */
    case HTTP_CHUNK_DATA:
        /* fall through */
        if (p < end) {
            len = end - p;
            if (sk->http_length >= len) {
                sk->http_length -= len;
                return HTTP_PARSE_OK;
            }

            p += sk->http_length;
            sk->http_length = 0;
            c = *p;
            p++;
            if (c == '\r') {
                sk->http_parse_state = HTTP_CHUNK_DATA_END;
            } else {
                return HTTP_PARSE_ERR;
            }
        } else {
            return HTTP_PARSE_OK;
        }
        /* fall through */
    case HTTP_CHUNK_DATA_END:
        if (p < end) {
            c = *p;
            p++;
            if (c == '\n') {
                sk->http_parse_state = HTTP_CHUNK_SIZE;
                goto retry;
            } else {
                return HTTP_PARSE_ERR;
            }
        } else {
            return HTTP_PARSE_OK;
        }
        /* fall through */
    case HTTP_CHUNK_TRAILER_BEGIN:
trailer_begin:
        if (p < end) {
            c = *p;
            p++;
            if (c == '\r') {
                sk->http_parse_state = HTTP_CHUNK_END;
                goto chunk_end;
            } else {
                sk->http_parse_state = HTTP_CHUNK_TRAILER;
            }
        } else {
            return HTTP_PARSE_OK;
        }
        /* fall through */
    case HTTP_CHUNK_TRAILER:
        while (p < end) {
            c = *p;
            p++;
            if (c == '\n') {
                sk->http_parse_state = HTTP_CHUNK_TRAILER_BEGIN;
                goto trailer_begin;
            }
        }
        return HTTP_PARSE_OK;

    case HTTP_CHUNK_END:
chunk_end:
        if (p < end) {
            c = *p;
            p++;
            if (c == '\n') {
                sk->http_parse_state = HTTP_BODY_DONE;
                return HTTP_PARSE_END;
            } else {
                return HTTP_PARSE_ERR;
            }
        } else {
            return HTTP_PARSE_OK;
        }
    default:
        return HTTP_PARSE_ERR;
    }
}

static inline int http_parse_body(struct socket *sk, const uint8_t *data, int data_len)
{
    if (sk->http_flags & HTTP_F_CONTENT_LENGTH) {
        if (data_len < sk->http_length) {
            sk->http_length -= data_len;
            return HTTP_PARSE_OK;
        } else if (data_len == sk->http_length) {
            sk->http_length = 0;
            sk->http_parse_state = HTTP_BODY_DONE;
            return HTTP_PARSE_END;
        } else {
            return HTTP_PARSE_ERR;
            return -1;
        }
    } else if (sk->http_flags & HTTP_F_TRANSFER_ENCODING) {
        return http_parse_chunk(sk, data, data_len);
    } else {
        return HTTP_PARSE_OK;
    }
}

int http_parse_run(struct socket *sk, const uint8_t *data, int data_len)
{
    int len = 0;

    if (sk->http_parse_state == HTTP_INIT) {
        http_parse_response(data, data_len);
        sk->http_parse_state = HTTP_HEADER_BEGIN;
    }

    if (sk->http_parse_state < HTTP_HEADER_DONE) {
        len = http_parse_headers(sk, data, data_len);
        if (len < 0) {
            return HTTP_PARSE_ERR;
        }

        data += len;
        data_len -= len;
    }

    if (data_len >= 0) {
        return http_parse_body(sk, data, data_len);
    } else {
        return HTTP_PARSE_OK;
    }
}
#endif
