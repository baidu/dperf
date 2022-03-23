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

在daemon模式下，dperf的统计数据写入日志文件('/var/log/dperf/dperf-ctl-[server|client].log'), 否则打印到屏幕上。

## keepalive
- syntax: keepalive
- default: -
- required: no
- mode: server

'keepalive'表示允许一条连接发送多个请求、响应。

dperf作为服务器运行时(server模式)，需要明确开启'keepalive'。
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

这是DPDK参数"--socket"的映射，dperf直接把这个参数传递给DPDK。
使用"--socket_mem"， 我们可以在同一个主机上同时运行dperf client和dperf server。
对于较小的压力要求，我们就可以在同一个主机上搭建压力测试环境, 而不需要两个主机。

Example:
- socket_mem  40960,0
- socket_mem  0,40960

注意: 单位是MB。

Reference:

[Linux-specific EAL parameters](http://doc.dpdk.org/guides/linux_gsg/linux_eal_parameters.html#linux-specific-eal-parameters)

[Multi-process Support](http://doc.dpdk.org/guides/prog_guide/multi_proc_support.html)

## port
- syntax: port PCI|BOND IPAddress Gateway [GatewayMAC]
- default: -
- required: yes
- mode: client, server

配置dperf使用的网口。通过配置多条'port'，dperf就可以使用多个口。
作为DPDK程序，dperf需要从操作系统接管这些网口。在启动dperf之前，你需要使用DPDK脚本'dpdk-devbind.py'绑定驱动（Mellanox网卡除外）。
- PCI: 网口的PIC号，使用'dpdk-devbind.py -s'可查看PCI；
- BOND: 格式为bondMode:Policy(PIC0,PCI1,...), Mode取值为[0-4], Policy为[0-2];
- IPAddress: 给网口指定一个IP地址，用于与'Gateway'互连；
- Gateway: 网关的IP地址。dperf没有路由能力，它只会把报文发给网关，ARP、NS、ND报文除外;
- Gateway-MAC: 可选，网关的MAC地址。

Example:
- port bond4:2(0000:81:10.0,0000:81:10.1) 10.235.20.12 10.235.20.1 00:00:64:01:01:01
- port 0000:03:00.1 6.6.215.4 6.6.215.1

Reference:

[binding-and-unbinding-network-ports](http://doc.dpdk.org/guides/linux_gsg/linux_drivers.html#binding-and-unbinding-network-ports-to-from-the-kernel-modules)

[Link Bonding Poll Mode Driver Library](https://doc.dpdk.org/guides/prog_guide/link_bonding_poll_mode_drv_lib.html)

## duration
- syntax: duration Time
- default: duration 100s
- required: yes
- mode: client, server

设置dperf的运行时长，超时后，dperf会自动优雅退出，并输出统计结果。

dperf启动后，有一个慢启动过程，新建连接数会缓慢增加。
在退出时，dperf也有一个缓冲时间，以便dperf优雅退出。
你还可以通过SIGINT信号，让dperf提前结束。

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

'cps'是dperf客户端的新建连接数总目标，是所有worker线程共享的。
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
在设置'cc'后，客户端会自动启用keepalive功能。
如果目标值设置很高，我们需要增大请求的发送间隔来降低网络带宽。

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

设置同一请求内两个请求之间的时间间隔。只有在设置'cc'后才生效。

Example:
- keepalive_request_interval 1ms
- keepalive_request_interval 1s
- keepalive_request_interval 60s

## keepalive_request_num Number(0-32767)
- syntax: keepalive_request_num Number
- default: 0
- required: no
- mode: client

在一个长连接内，发送多少个请求后再关闭连接。只有在设置'cc'后才生效。
0表示无限。

## launch_num
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
在client模式下，'client'配置的数目要与'port'一致，并且是一一对应的。
表示这个网口使用什么客户端地址池作为建连的源地址。

在server模式下, 'client'地址范围的数量没有要求，可以比'port'多，也可以比'port'少。
表示server只接收哪些客户端的连接，dperf server不接受来自未知地址的连接请求。

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
- syntax: payload_size Number(>=1)
- default: -
- required: no
- mode: client, server

设置请求与响应的大小，单位是字节。
对于tcp协议，如果payload_size小于70，dperf会强制为70，这是最小的HTTP报文长度。
如果要设置更小的数据报文，请使用packet_size。

## packet_size
- syntax: pakcet_size Number(0-1514)
- default: -
- required: no
- mode: client, server

设置数据报文大小，包括以太网头部，不包括4字节FCS。使用packet_size可以设置最小包，最大包。

## jumbo
- syntax: jumbo
- default: -
- required: no
- mode: client, server

开启巨帧。开启巨帧后，pakcet_size可以设置到9724。
注：packet_size不能超过网络环境的MTU。

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

## slow_start
- syntax: slow_start Seconds(10-600)
- default: slow_start 30
- required: no
- mode: client

客户端在慢启动时间内逐步增加每秒新建连接数。

## vxlan
- syntax: vxlan vni innerSMAC innerDMAC localVtepIPAddr Number remoteVtepIPAddr Number
- default: -
- required: no
- mode: client, server

每个'port'可以设置一个'vxlan'。'innerSMAC'是内层报文的源MAC地址，'innerDMAC'是内层报文的目的地址。
'localVtepIPAddr'是local vtep的起始地址，每个网卡队列需要一个local vtep地址，用于分流。
'remoteVtepIPAddr'是remote vtep的起始地址。Number是vtep地址的数量。

## kni
- syntax: kni [ifName]
- default: -
- required: no
- mode: client, server

开启kni接口。我们给每个'port'创建一个kni接口。常见的接口名称是vEth/vnic等，默认名称是dperf。
kni接口的IP地址与路由需要手动配置，建议为kni接口分配独立的IP。
当只开启了1个CPU是，kni接口IP可以是流量IP；
如果使用了多个CPU，kni接口IP可以使用server的第一个流量IP。

## tos
- syntax: tos Number(0x00-0xff or 0-255)
- default: 0
- required: no
- mode: client, server

设置IPv4头部的tos或者IPv6头部的traffic class，可以用16进制或者10进制。
注意：tos对kni接口发出的报文不生效。
