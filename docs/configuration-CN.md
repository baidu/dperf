# 配置手册

## mode
- syntax: mode client | server
- default: -
- required: yes
- mode: -

设置dperf的运行模式。dperf既可以作为客户端单独运行，也可以作为服务器单独运行.

## daemon
- syntax: daemon
- default: -
- required: no
- mode: client, server

在daemon模式下，dperf的统计数据写入日志文件('/var/log/dperf/dperf-ctl.log'), 否则打印到屏幕上。

## keepalive
- syntax: keepalive
- default: - 
- required: no
- mode: server

'keepalive'表示允许一条连接发送多个请求、响应。dperf作为服务器运行时(server模式)，需要明确开启'keealive'。
在开启'keepalive'后，dperf server不会主动关闭连接，它会等客户端的FIN或者RST。
当'keepalve'没有开启，dperf server通过在响应报文中置上FIN, 以最快的速度关闭连接。（想一下，调用POSIX socket API的程序能做到这一点吗？）

dperf client不需要显示设置'keepalive'。当配置了'cc'后，'keepalive'会自动打开。
 
## cpu
- syntax: cpu n0 n1 n2-n3...
- default: -
- required: yes
- mode: client, server

设置dperf运行在哪些CPU上。dperf是一个多线程程序。它在每个CPU上起一个线程，每个线程只使用一个RX队列和一个TX队列。
如果你有多个网口，dperf根据网口的先后顺序, 给网口平均分配CPU。
在多NUMA系统上，需要注意，CPU与网口需要在同一个NUMA上，这是DPDK的要求。

Example:
- cpu 0-3 12-15
- cpu 0 1 2 3 4 5

## socket_mem
- syntax: socket_mem n0,n1,n2...
- default: -
- required: no
- mode: client, server

只是DPDK参数"--socket"的映射，dperf直接把这个参数传递给DPDK。
使用"--socket_mem"， 我们可以在同一个主机上同时运行dperf client和dperf server。这样我们就可以在同一个主机上搭建压力测试环境。

Example:
- socket_mem  40960,0
- socket_mem  0,40960

注意: 单位是MB。

Reference:
[Linux-specific EAL parameters](http://doc.dpdk.org/guides/linux_gsg/linux_eal_parameters.html#linux-specific-eal-parameters)

[Multi-process Support](http://doc.dpdk.org/guides/prog_guide/multi_proc_support.html)

## port
- syntax: port PCI IPAddress Gateway [GatewayMAC]
- default: -
- required: yes
- mode: client, server

配置dpef使用的网口。通过配置多条'port'，dperf就可以使用多个口。
作为DPDK程序，dperf需要从操作系统接管这些网口。在启动dperf之前，你需要使用DPDK脚本'dpdk-devbind.py'绑定驱动（Mellanox网卡除外）。
- PCI: 网口的PIC号，使用'dpdk-devbind.py -s'可查看PCI；
- IPAddress: 给网口指定一个IP地址，用于与'Gateway'互连；
- Gateway: 网关的IP地址。dperf没有路由能力，它只会把报文发给网关，ARP、NS、ND报文除外。
- Gateway-MAC: 可选，网关的MAC地址。

Reference:
[binding-and-unbinding-network-ports](http://doc.dpdk.org/guides/linux_gsg/linux_drivers.html#binding-and-unbinding-network-ports-to-from-the-kernel-modules)

## duration
- syntax: duration Time
- default: duration 100s
- required: yes 
- mode: client, server

设置dperf的运行时长，过了时间后，dperf会自动退出。

dperf启动后，有一个慢启动过程，这是新建连接数会缓慢增加。
在退出时，dperf也有一个缓冲时间，以便dperf优雅退出。
你还可以通过SIGINT信号，让dperf提前优雅退出。

Example:
- duration 1.5d
- duration 2h
- duration 3.5m
- duration 100s
- duration 100

## cps
- syntax: cps Number
- default: -
- required: yes
- mode: client

'cps'是dperf客户端的新建连接总目标，是所有worker线程的。
dperf向所有worker线程平均分配目标，所以推荐设置'cps'为线程数的整数倍。
在慢启动阶段，新建连接数会逐步增大。

Example:
- cps 10m
- cps 1.5m
- cps 2k
- cps 100

## cc
- syntax: cc Number 
- default: -
- required: no
- mode: client

设置客户端的并发连接数总目标。
在这是'cc'后，客户端会自动启用keepalive功能。
如果目标值设置很高，我们需要增大请求的发现间隔来降低网络带宽。

Example:
- cc 100m
- cc 1.5m
- cc 2k
- cc 100

## synflood
- syntax: synflood
- default: -
- required: no
- mode: client

如果开启synflood，dperf client只发SYN包，不建连接。

## keepalive_request_interval
- syntax: keepalive_request_interval Time
- default: keepalive_request_interval 1s
- required: no
- mode: client

设置同一请求内连个请求之间的时间间隔。只有在设置'cc'后才生效。

Example:
- keepalive_request_interval 1ms
- keepalive_request_interval 1s
- keepalive_request_interval 60s

## keepalive_request_num Number
- syntax: keepalive_request_num Number
- default: -
- required: no
- mode: client

在一个长连接内，发送多少个请求后再关闭连接。只有在设置'cc'后才生效。

# launch_num
- syntax: launch_num Number
- default: launch_num 4
- required: no
- mode: client

dperf客户端一次发起的新建连接数。
用较少的'launch_num'可以让报文发送更加平顺, 避免突发报文给接收方的网卡造成丢包。

## client
- syntax: client IPAddress Number
- default: -
- required: yes 
- mode: client, server

设置客户端IP范围：
- 'IPAddress': 起始地址；
- 'number': 地址总数 1-254

dperf使用IP地址(包含IPv4、IPv6)的最后两个字节标识一个客户端地址。
一个'client'的地址范围里，只允许最后一个字节变动。
在client模式下，'client'配置的数目要与'port'一致，并且是一对一的关系。
表示这个网口使用什么客户端地址池来做连接的源地址。

在server模式下, 'client'地址范围的数量没有要求，可以比'port'多，也可以比'port'少。
表示server只接收哪些客户端的连接，dperf server不接受未指定的连接请求。

## server
- syntax: server IPAddress Number
- default: -
- required: yes 
- mode: client, server

设置dperf server的监听地址范围。'port'的数量必须要与'server'的数量一致，并且每个worker线程分配一个唯一的监听IP。

## listen
- syntax: listen Port Number
- default: listen 80 1
- required: yes

设置dperf的监听端口范围。dperf客户端向这些端口发送请求。

注意：dperf在启动时会把所有的socket分配好，所以不要配置太大的端口范围。

## payload_size
- syntax: payload_size Number(1-1400)
- default: payload_size 1
- required: no
- mode: client, server

设置请求与响应的大小，单位字节。
如果'Number'小于dperf所支持的最小值，dperf会使用最小值。所以可以用'1'表示最小值。

## mss
- syntax: mss Number
- default: mss 1460
- required: no
- mode: client, server

设置tcp MSS。

## protocol
- syntax: protocol tcp | udp
- default: protocol tcp
- required: no
- mode: client, server

TCP或者UDP协议。不论是TCP还是UDP协议，dperf客户端都是发送HTTP请求，dperf Server回复HTTP响应。

## tx_burst
- syntax: tx_burst Number(1-1024)
- default: tx_burst 8
- required: no
- mode: client, server

使用DPDK API发送报文时，单次最大发送报文数。
较小的值，可以让报文发送的更加平顺，可以有效避免接收方丢包，但是增加了dperf的CPU消耗。
