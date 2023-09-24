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

#ifndef __HTTP_PARSE_H
#define __HTTP_PARSE_H

#include "socket.h"

enum {
    HTTP_INIT,
    HTTP_HEADER_BEGIN,
    HTTP_HEADER_LINE_END,
    HTTP_HEADER_DONE,
    HTTP_CHUNK_SIZE,
    HTTP_CHUNK_SIZE_END,
    HTTP_CHUNK_DATA,
    HTTP_CHUNK_DATA_END,
    HTTP_CHUNK_TRAILER_BEGIN,
    HTTP_CHUNK_TRAILER,
    HTTP_CHUNK_END,
    HTTP_BODY_DONE,
    HTTP_ERROR
};

#define HTTP_F_CONTENT_LENGTH_AUTO  0x1
#define HTTP_F_CONTENT_LENGTH       0x2
#define HTTP_F_TRANSFER_ENCODING    0x4
#define HTTP_F_CLOSE                0x8

#define HTTP_PARSE_OK      0
#define HTTP_PARSE_END     1
#define HTTP_PARSE_ERR    -1

/*
 * return:
 *  0   continue
 *  1   end
 *  -1  error
 * */
int http_parse_run(struct socket *sk, const uint8_t *data, int data_len);


#endif
