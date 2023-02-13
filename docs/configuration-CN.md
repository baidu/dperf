# 配置手册

## cc
设置客户端的并发连接数总目标。
在设置'cc'后，客户端会自动启用keepalive功能。
如果目标值设置很高，我们需要增大请求的发送间隔来降低网络带宽。

Example:
- cc 100m
- cc 1.5m
- cc 2k
- cc 100

## change_dip
当开启'flood'后，可以使用'change_ip'来修改发送报文的目的IP。
'change_ip'可以配置多行，最多支持65536个IP地址。这些IP地址平均分配到每个线程。
dperf在发送每个TCP/UDP报文时，修改报文的目的IP; dperf采用轮询方式从地址池中获取IP。
'IPAddress'是起始地址，'Step'表示下一个IP的间隔，'Number'表示IP总数。

Example:
- change_ip 192.168.1.1 64 100
- change_dip 2001:6:6:241::1 1 20

## client
设置客户端IP范围：
- 'IPAddrStart': 起始地址
- 'IPAddrNumber': 地址总数 1-254

dperf使用IP地址(包含IPv4、IPv6)的最后两个字节标识一个客户端地址。
一个'client'的地址范围里，只允许最后一个字节变动。
在client模式下，'client'数量需要与'port'的数量一致，并且一一对应。
表示这个网口使用什么客户端地址池作为建连接的源地址。

在server模式下, 'client'数量没有要求，可以比'port'多，也可以比'port'少。
表示server只接收哪些客户端的连接，dperf server不接受来自未知地址的连接请求。

Example:

    接口地址与Client地址在同一网段
    port        0000:1b:00.0    6.6.1.2   6.6.1.1
    client      6.6.1.2         100

    接口地址与Client地址不在同一网段
    port        0000:1b:00.0    6.6.10.2   6.6.10.1
    port        0000:1b:00.1    6.6.20.2   6.6.20.1
    client      6.6.30.2        100
    client      6.6.40.2        100

## client_hop
client在创建新连接时同时改变IP与端口。

## cps
'cps'是dperf客户端的新建连接数总目标，是所有worker线程共享的。
dperf向所有worker线程平均分配目标，所以推荐设置'cps'为线程数的整数倍。
在慢启动阶段，新建连接数会逐步增大。

Example:
- cps 10m
- cps 1.5m
- cps 2k
- cps 100

## cpu
设置dperf运行在哪些CPU上。dperf是一个多线程程序。它在每个CPU上起一个线程，每个线程只使用一个RX队列和一个TX队列。
如果你有多个网口，dperf根据网口的先后顺序, 给网口平均分配CPU。
在多NUMA系统上，需要注意，CPU与网口需要在同一个NUMA上，这是DPDK的要求。

Example:
- cpu 0-3 12-15
- cpu 0 1 2 3 4 5

## daemon
在daemon模式下，dperf的统计数据写入日志文件('/var/log/dperf/dperf-ctl-[server|client].log'), 否则打印到屏幕上

## duration
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

## flood
支持TCP/UDP协议，不需要网卡支持FDIR。
如果开启flood，dperf client仅发连接的第一个报文包；对于TCP协议, dperf发送SYN报文。

## http_host
设置HTTP请求的Host头部。

## http_path
设置HTTP请求的PATH。



## jumbo
开启巨帧。开启巨帧后，pakcet_size可以设置到9724。
注：packet_size不能超过网络环境的MTU。

## keepalive
'keepalive'表示允许一条连接发送多个请求、响应。

对于dperf server：
- 当'keepalve'没有开启，dperf server通过在响应报文中置上FIN, 以最快的速度关闭连接。（想一下，调用POSIX socket API的程序能做到这一点吗？）
- 在开启'keepalive'后，dperf server它会等客户端的FIN/RST或者一段时间之后才会关闭连接。
- dperf server的连接空闲超时为重传超时（约8秒）加上'timeout'。
- dperf server上不需要配置'num'。

对于dperf client:
- dperf client每隔'interval'时间发送一个请求。
- dperf client在发送完'num'个请求后关闭连接。

'keepalive'使用场景：
- 并发测试：设置较大的并发连接数（cc 10m），较大的interval，如60s。
- 带宽测试：设置较大的packet_size（如 1500），较小的interval（如1ms）。
- 单向PPS测试：设置flood，较小packet_size（如64），较小的并发连接数cc（如3000），较小的interval（如1ms或10us）。
- 双向PPS测试：设置较小packet_size（如64），较小的并发连接数cc（如3000），较小的interval（如1ms或10us）。

## kni
开启kni接口。我们给每个'port'创建一个kni接口。常见的接口名称是vEth/vnic等，默认名称是dperf。
kni接口的IP地址与路由需要手动配置, 只能配置为'port'的IP。

## launch_num
dperf客户端一次发起的新建连接数。
用较少的'launch_num'可以让报文发送更加平顺, 避免突发报文给接收方的网卡造成丢包。

## listen
设置dperf的监听端口范围。dperf客户端向这些端口发送请求。
注意：dperf在启动时会把所有的socket分配好，所以不要配置太大的端口范围。

## lport_range
设置客户端的本地端口范围。在Google Cloud上建议设置本地端口范围为[1000-65535]。

## mode
设置dperf的运行模式。dperf既可以作为客户端单独运行，也可以作为服务器单独运行.

## mss
设置tcp MSS。

## payload_size
设置请求与响应的大小，单位是字节。
对于tcp协议，如果payload_size小于70，dperf会强制为70，这是最小的HTTP报文长度。
如果要设置更小的数据报文，请使用packet_size。

## payload_random
设置payload为随机的字符('a'-'z')，默认为全'a'。

## packet_size
设置数据报文大小，包括以太网头部，不包括4字节FCS。使用packet_size可以设置最小包，最大包。

## port
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

## protocol
TCP或者UDP协议或者HTTP协议。不论是TCP还是UDP协议，dperf客户端都是发送HTTP请求，dperf Server回复HTTP响应。
当前，只有client模式才能配置http协议，可以用来测试nginx。

## quiet
关闭每秒输出统计数据。

## rss
使用网卡RSS分流。没有FDIR特性的网卡需要开启多队列/多线程时需要开启此开关。
- l3: 使用基于IP地址的对称哈希算法分流，要求网卡支持修改RSS配置, 这是默认选项
- l3l4: 使用l3l4对称哈希算法分流, 要求网卡支持修改RSS配置
- auto: 使用网卡默认算法分流, 要求一个port只能配置一个server IP
- mq_rx_rss: 使用DPDK参数'RTE_ETH_MQ_RX_RSS'开启网卡的RSS特性, 这是默认参数
- mq_rx_none: 不使用DPDK参数'RTE_ETH_MQ_RX_RSS'去配置网卡，部分网卡不允许配置，只能用在auto模式

注意：'vxlan'暂时还不能与'rss'同时使用。

## server
设置dperf server的监听地址范围。'port'的数量必须要与'server'的数量一致。
默认要求server IP地址数目与CPU数目相等，即每个worker线程分配一个。
当开启'rss'后，允许server IP地址数目少于CPU数量。

## socket_mem
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

## slow_start
客户端在慢启动时间内逐步增加每秒新建连接数。

## tcp_rst
设置dperf对请求未打开的TCP端口的SYN报文是否回复rst。



## tx_burst
使用DPDK API发送报文时，单次最大发送报文数。
较小的值，可以让报文发送的更加平顺，可以有效避免接收方丢包，但是增加了dperf的CPU消耗。

## tos
设置IPv4头部的tos或者IPv6头部的traffic class，可以用16进制或者10进制。
注意：tos对kni接口发出的报文不生效。

## vxlan
每个'port'可以设置一个'vxlan'。'innerSMAC'是内层报文的源MAC地址，'innerDMAC'是内层报文的目的地址。
'localVtepIPAddr'是local vtep的起始地址，每个网卡队列需要一个local vtep地址，用于分流。
'remoteVtepIPAddr'是remote vtep的起始地址。Number是vtep地址的数量。

## wait
客户端在启动后的等待时间，之后进入慢启动阶段。


##############################################
| parameter      | syntax                                       | default | required | mode                |
|----------------|----------------------------------------------|---------|----------|-----------------    |
| cc             | cc Number                                    | -       | no       | client              |
| change_dip     | change_dip IPAddress Step Number             | -       | no       | client              |
| client         | client IPAddrStart IPAddrNumber              | -       | yes      | client, server      |
| client_hop     | client_hop                                   | -       | no       | client              |
| cps            | cps Number                                   | -       | yes      | client              |
| cpu            | cpu n0 n1 n2-n3...                           | -       | yes      | client, server      |
| daemo          | daemo                                        | -       | no       | client, server      |
| duration       | duration Time                                | 100s    | yes      | client, server      |
| flood          | flood                                        | -       | no       | client              |
| http_host      | http_host String(1-127)                      | dperf   | no       | client              |
| http_path      | http_path String(1-255)                      | /       | no       | client              |
| lport_range    | lport_range Number [Number]                  | -       | no       | client              |
| launch_num     | launch_num Number                            | 4       | no       | client              |
| jumbo          | jumbo                                        | -       | no       | client, server      |
| keepalive      | keepalive interval(timeout) [num]            | -       | no       | client, server      |
| kni            | kni [ifName]                                 | -       | no       | client, server      |
| launch_num     | launch_num Number                            | 4       | no       | client              |
| listen         | listen Port Number                           | 80 1    | yes      |                     |
| lport_range    | lport_range Number [Number]                  | 1 65535 | no       | client              |
| mode           | client/server                                | -       | yes      |  -                  |
| mss            | mss Number                                   | 1460    | no       | client, server      |
| payload_size   | payload_size Number(>=1)                     | -       | no       | client, server      |
| payload_random | payload_random                               | -       | no       | client, server      |
| packet_size    | pakcet_size Number(0-1514)                   | -       | no       | client, server      |
| port           | port PCI BOND IPAddress Gateway [GatewayMAC] | -       | yes      | client, server      |
| protocol       | protocol tcp | udp | http                    | tcp     | no       | client, server      |
| quiet          | quiet                                        | -       | no       | client, server      |
| rss            | rss [l3/l3l4/auto] [mq_rx_none mq_rx_rss]    | -       | no       | client, server      |
| server         | server IPAddrStart IPAddrNumber              | -       | yes      | client, server      |
| slow_start     | slow_start Seconds(10-600)                   | 30      | no       | client              |
| socket_mem     | socket_mem n0,n1,n2...                       | -       | no       | client, server      |
| tcp_rst        | tcp_rst Number[0-1]                          | 1       | no       | client, server      |
| tx_burst       | tx_burst Number(1-1024)                      | 8       | no       | client, server      |
| tos            | tos Number(0x00-0xff or 0-255)               | 0       | no       | client, server      |
| vxlan          | vxlan vni innerSMAC innerDMAC localVtep<br>IPAddr Number remoteVtepIPAddr Number        | -       | no       | client, server      |
| wait           | wait Seconds                                 | 3       | no       | client              |


#########################  必须配置的参数          ##########################################

| parameter | syntax                                       | default | required | mode            |
|-----------|----------------------------------------------|---------|----------|-----------------|
| cps       | cps Number                                   | -       | yes      | client          |
| client    | client IPAddrStart IPAddrNumber              | -       | yes      | client, server  |
| cpu       | cpu n0 n1 n2-n3...                           | -       | yes      | client, server  |
| duration  | duration Time                                | 100s    | yes      | client, server  |
| listen    | listen Port Number                           | 80 1    | yes      |                 |
| mode      | client / server                              | -       | yes      |     -           |
| port      | port PCI BOND IPAddress Gateway [GatewayMAC] | -       | yes      | client, server  |
| server    | server IPAddrStart IPAddrNumber              | -       | yes      | client, server  |




