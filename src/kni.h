/*
 * Copyright (c) 2022-2022 Baidu.com, Inc. All Rights Reserved.
 * Copyright (c) 2022-2024 Jianzhang Peng. All Rights Reserved.
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

#ifndef __KNI_H
#define __KNI_H

#include <rte_mbuf.h>

struct config;
struct work_space;
int kni_start(struct config *cfg);
void kni_stop(struct config *cfg);
void kni_recv(struct work_space *ws, struct rte_mbuf *m);
void kni_send(struct work_space *ws);
void kni_broadcast(struct work_space *ws, struct rte_mbuf *m);
int kni_link_up(struct config *cfg);

#define KNI_NAME_DEFAULT "dperf"
#endif
