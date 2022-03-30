/*
 * Copyright (c) 2022 Baidu.com, Inc. All Rights Reserved.
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

#ifndef __RSS_H
#define __RSS_H

#include <stdbool.h>
#include <rte_ethdev.h>

struct socket;
struct work_space;
int rss_config_port(struct rte_eth_conf *conf, struct rte_eth_dev_info *dev_info);
bool rss_check_socket(struct work_space *ws, struct socket *sk);
void rss_init(void);

#endif
