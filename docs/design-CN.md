# dperf设计原理
--by jianzhang peng

## 背景
最开始，dperf是设计来给四层负载均衡器(L4LB)做性能测试的。用一个通用物理服务器，一些合适的网卡，dperf就可以产生相当大的压力，既可以满足我的日常研发需求，也方便我向客户展示我们L4LB的性能。
L4LB的性能通常很高。大部分L4LB是基于报文转发实现的，在内部有一个会话表，用来跟踪连接状态, 不过L4LB不会去分析应用层的数据。所以L4LB可以达到数百万的每秒新建连接数，几十亿的并发，数百万Gbps的吞吐。

本文讲述针对四层负载均衡器的性能测试，dperf是怎么设计的, 考虑到UDP比较简单，本文主要针对TCP。

## 核心需求
- 极高性能的TCP协议栈
- 支持HTTP协议，方便用社区的各种工具进行测试
- 能够在一台物理服务器上模拟出HTTP客户端与HTTP服务器(用最少的资源，完成测试)

## 一些合理的假设
- 测试在一个内部网络进行，IP地址可以随意设置
- dperf可以接入三层交换机，dperf不需要支持路由功能
- HTTP的请求、响应长度不能超过一个MTU，必须在一个报文内(不运行真实业务，用于测试是可以接收的)
- HTTP的报文格式是正确的，dperf对HTTP报文做合法性校验（这是性能测试，不是功能测试）

## 使用场景
1. 两个dperf互打，用于测试网卡、CPU、交换机、虚拟网络等设施的性能

dperf(client)------------------------dperf(server)

2. 单作为客户端使用

dperf(client)------------------------Server(eg nginx)

3. 单作为服务器使用

Client(eg ab/curl)-------------------dperf(server)

3. 作为客户端、服务器，测试中间设备

dperf(client)---------DUT(L4LB)------dperf(server)

注意：上面省略了交换机。

## 设计要点
### 多线程与网卡分流（FDIR）
dperf是一个多线程程序，可以使用多个网卡端口，但是每个线程只使用1个RX和1个TX。
dperf使用网卡的分流特性（FDIR。
每个dperf server线程绑定一个监听IP，使用报文的目的IP分流。
每个dperf client线程只请求一个目的IP，使用报文的源IP分流。

### 极小socket
1个socket（1条连接）只占用64字节，可以放在一个cache line中，10亿连接只需要6.2GB内存，所以很容易达到几十亿并发连接数。

### 地址与socket查找
与商业测试仪类似，dperf client与dperf server要求使用连续的地址范围。

Clients and servers use a continuous range of IP addresses. Each network interface takes up one range of IP addresses, the number of IPs used by dperf(server) equals the number of threads, dperf(client) can use more IPs (eg 100), the server's listening port is also a continuous range of ports, and the lower two bytes of the client's IP address is the index for lookup and needs to be unique. Since the IP addresses of the client and server are determined beforehand, all sockets are allocated at dperf startup to form a table, the lower 2 bytes of client port, server port, and client IP can be used to locate the socket immediately.

### 定制的TCP协议栈
关于dperf的TCP协议栈有一个有趣的故事。
我曾计划使用FreeBSD的协议栈。由于之前对FreeBSD TCP协议栈有相当长的调研积累，所以我信心满满的使用它，还报名参加了一个内部活动。
不幸的是，主办方要求在2周内提交代码，我想再给我两月也完成不了，何况2周。
于是我决定裁剪功能，把所有不需要的功能全部砍掉，于是我不得不思考一个问题：对四层负载均衡器的压力测试仪来说，什么样的TCP协议栈是可以接收的？

对这个TCP协议栈，这是我思考的结果：
1. 这个协议栈只用来做四层负载均衡器的性能测试
2. 这个协议栈只适用于重复的"请求-响应"模式的协议，如HTTP
3. 应用层的请求/响应的长度文可以要求不超过1个MTU
4. 只需要实现停等协议，不需要支持SACK，拥塞控制算法
5. 需要支持丢包重传
6. 支持长连接、短连接

### 定时器
早期，我设计了通用的时间轮定时器，一个时间轮定时器需要消耗32字节，如果使用两个定时器就需要64个字节，这对要支持几十亿并发连接数的测试仪来说内存消耗太高，另外频繁的函数调用也很消耗性能。
考虑到，报文重传是较少的（如果出现重传，你应该降低压力重新测试），我借鉴了FreeBSD的timewait定时器，把所有相同的任务放在一个按时间排序的链表中。

### 零拷贝，无接收/发送缓存，零校验和
零拷贝。除了启动阶段，dperf不拷贝任何数据：包括以太网头部/IP头部/传输层头部/payload，因为这些内容几乎都是固定不变的
无接收缓存。因为dperf更本不会关心payload内容，这些内容只不过是报文的填充物而已, 基本上只关心收到了多少字节。
无发送缓存。普通协议栈需要发送缓存，只有收到对方的确认，才能释放这些缓存，否则需要重传；另外还需要考虑对方的确认的序列号可能是在任意未知。dperf发送数据都是一样的，是否要在socket
零校验和。通常我们会利用网卡功，卸载报文的校验和，但是我们要计算伪头部; 对同一个连接来说，报文类型是固定，dperf已经算好了尾部头校验和，整个过程没有校验和计算。

### HTTP协议实现
dperf服务器非常傻，它收到任何1数据包（第一个字符是G, GET的开始），就认为是完整的请求，就把固定的响应发送出去。
dperf客户端也非常傻，它收到任何一个数据报文，如果第10个字符是'2'（假设"HTTP/1.1 200 OK")，就认为是成功的响应。

### Inline函数
dperf大量使用inline来避免函数调用开销。

## 其他用途
根据dperf的特点，我们发现它除了可以对L4LB进行测试外，还可：
- 其他基于四层转发的网关进行测试，如链路负载均衡、防火墙等
- 对云上虚拟机的网络性能进行测试
- 对网卡性能、CPU的网络报文处理能力进行测试
- 作为高性能的HTTP客户端或HTTP服务器用于压测场景

我们要把四层负载均衡移植到各种CPU，我们用dperf来测评各种CPU的性能
When we ported L4LB to multiple CPUs, we used dperf to test the CPU's performance by sending traffic to each other on two servers of these CPUs. we found that some processors were suitable for network packet processing, while others were not as good as I expected. dperf can be used as a CPU benchmark for packets processing.
