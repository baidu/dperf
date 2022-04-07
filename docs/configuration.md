# Configuration

## mode
- syntax: mode client | server
- default: -
- required: yes
- mode: -

Set the running mode of dperf. dperf can be run as a client or a server.

## daemon
- syntax: daemon
- default: -
- required: no
- mode: client, server

In daemon mode, dperf statistics are written to the log file ('/var/log/dperf/dperf-ctl-[server|client].log'), otherwise output to stdout.

## quiet
- syntax: quiet
- default: -
- required: no
- mode: client, server

Turn off output statistics per second.

## keepalive
- syntax: keepalive interval(timeout) [num]
- default: -
- required: no
- mode: client, server

Keepalive means that multiple requests and responses can be sent in one connection.

For dperf server:
- When 'keepalve' is not enabled, the dperf server closes the connection at the fastest speed by setting FIN in the response message. (Think about it, can a program calling the POSIX socket API do this?)
- After enabling 'keepalive', the dperf server will wait for the client FIN/RST or a period of time before closing the connection.
- The connection idle timeout of the dperf server is the retransmission timeout (about 8 seconds) plus 'timeout'.
- 'num' does not need to be configured on the dperf server.

For dperf client:
- dperf client sends a request every 'interval' time.
- The dperf client closed the connection after sending 'num' requests.

'keepalive' usage scenarios:
- CC test: set a larger number of concurrent connections (cc 10m), a larger interval, such as 60s.
- tps test: set a larger packet_size (such as 1500) and a smaller interval (such as 1ms).
- TX PPS test: set flood, smaller packet_size (eg 64), smaller number of concurrent connections cc (eg 3000), smaller interval (eg 1ms).
- TX and RX PPS test: set a smaller packet_size (such as 64), a smaller number of concurrent connections cc (such as 3000), and a smaller interval (such as 1ms).

## cpu
- syntax: cpu n0 n1 n2-n3...
- default: -
- required: yes
- mode: client, server

Set which CPUs dperf runs on. dperf is a multithread program.
It starts a worker thread on each CPU, and each thread uses only 1 RX queue and 1 TX queue.
If you use multiple network interface ports, dperf will distribute the CPU equally according to the configuration order of the network interface port.

On a multi-NUMA system, it is necessary to note that the CPU needs to be on the same NUMA node as the network interface port, which is a requirement of DPDK.

Example:
- cpu 0-3 12-15
- cpu 0 1 2 3 4 5

## socket_mem
- syntax: socket_mem n0,n1,n2...
- default: -
- required: no
- mode: client, server

This is the DPDK "--socket" parameter, and dperf passes it to DPDK. Using'--socket_mem',
we can run a dperf client and a dperf server on the same host at the same time,
so that we can build a load test environment with one host.

Example:
- socket_mem  40960,0
- socket_mem  0,40960

Note: The unit of the parameter is MB.

Reference:
[Linux-specific EAL parameters](http://doc.dpdk.org/guides/linux_gsg/linux_eal_parameters.html#linux-specific-eal-parameters)

[Multi-process Support](http://doc.dpdk.org/guides/prog_guide/multi_proc_support.html)

## port
- syntax: port PCI|BOND IPAddress Gateway [GatewayMAC]
- default: -
- required: yes
- mode: client, server

Configure the network interface port used by dperf.
If you want to use multiple network interface ports, you only need to configure multiple 'port's.
As a DPDK program, dperf will take over these network interface ports from the operating system.
Before starting dperf, you need to use the DPDK script 'dpdk-devbind.py' for driver binding (except for Mellanox network interfaces).
- PCI: The PCI number of the network interface port, use 'dpdk-devbind.py -s' to get it from the system.
- BOND: The format is bondMode:Policy(PIC0,PCI1,...), Mode is [0-4], Policy is [0-2].
- IPAddress: This IP  Address is used to interconnect with the 'Gateway'.
- Gateway: Gateway IP address. dperf has no routing capability. It will send all packets to the gateway, except for ARP, NS, and ND.
- Gateway-MAC: the MAC address of the 'Gateway', which can be omitted.

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

Set the running time of dperf, after this time dperf will exit.

After dperf is started, there is a slow start phase, and the CPS will gradually increase.
There is also a short buffer time when exiting. You can also use the signal 'SIGINT' to make dperf exit gracefully immediately.

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

This is the total target for the number of new connections per second for all worker threads.
dperf evenly distributes the total target to each worker thread, so it is recommended to set'cps' to an integer multiple of the number of worker threads.
In the slow start phase, CPS will gradually increase.

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

Set the total target for the number of concurrent connections on the client side.
When 'cc' is set, the client will enable 'keepalive'.
When the target value is high, we need to increase the time interval between two requests to reduce network bandwidth usage.

Example:
- cc 100m
- cc 1.5m
- cc 2k
- cc 100

## flood
- syntax: flood
- default: -
- required: no
- mode: client

Support TCP/UDP protocol, no need for network card to support FDIR.
If flood is enabled, the dperf client only sends the first packet of the connection; for the TCP protocol, dperf sends a SYN packet. 

## launch_num
- syntax: launch_num Number
- default: launch_num 4
- required: no
- mode: client

How many connections are initiated by the client at a time.
In case of packet loss, try to reduce the number to make the packet sending more smooth,
so as to avoid the packet loss of the receiver's network card caused by the surge of packets.

## client
- syntax: client IPAddress Number
- default: -
- required: yes 
- mode: client, server

Set the client's IP address range.
- 'IPAddress': starting address.
- 'number': number of addresses, 1-254

dperf uses the last two bytes to uniquely identify an IPv4 or IPv6 address.
In a 'client' configuration, only the last byte of the address is allowed to be variable.

In the client mode, the number of 'client' must be equal to the number of 'port', which has a one-to-one correspondence. 
Indicates that the 'port' uses the address pool of the 'client' as the source addresses of the connections.

In the server mode, the number of 'client' can be greater or less than the number of 'port'.
Indicates that the server only accepts connections from these clients, and does not accept connections from unspecified clients.

## server
- syntax: server IPAddress Number
- default: -
- required: yes 
- mode: client, server

Set the listening IP range of the server. The number of 'port' must be the same as the number of 'server'.
By default, the number of server IP addresses is required to be equal to the number of CPUs, that is, one for each worker thread.
When 'rss' is enabled, the number of server IP addresses is allowed to be less than the number of CPUs.

## listen
- syntax: listen Port Number
- default: listen 80 1
- required: yes

Set the port ranges that the server listens to, and the client sends packets to these port ranges.

Note: dperf will allocate all sockets at startup. Please do not set a large port range.

## payload_size
- syntax: payload_size Number(>=1)
- default: -
- required: no
- mode: client, server

Set the size of the request and response, in bytes.
For tcp protocol, if payload_size is less than 70, dperf will be forced to 70, which is the minimum HTTP packet length.
If you want to set smaller data packets, use packet_size. 

## packet_size
- syntax: pakcet_size Number(0-1514)
- default: -
- required: no
- mode: client, server

Sets the data packet size, including the Ethernet header, excluding the 4-byte FCS. Use packet_size to set the minimum and maximum packets. 

## jumbo
- syntax: jumbo
- default: -
- required: no
- mode: client, server

Enable jumbo frames. After enabling jumbo frames, pakcet_size can be set to 9724.
Note: packet_size cannot exceed the MTU of the network environment. 

## mss
- syntax: mss Number
- default: mss 1460
- required: no
- mode: client, server

Set tcp MSS option.

## protocol
- syntax: protocol tcp | udp
- default: protocol tcp
- required: no
- mode: client, server

TCP or UDP protocol. Regardless of the TCP or UDP protocol, the dperf client sends an HTTP Request, and the dperf server sends an HTTP Response.

## tx_burst
- syntax: tx_burst Number(1-1024)
- default: tx_burst 8
- required: no
- mode: client, server

Use the DPDK API to send packets, the maximum number of packets sent at a time.
A smaller value can make the packets sending smoother and avoid packet loss at the receiving side, but it increases the CPU consumption of dperf.

## wait
- syntax: wait Seconds
- default: wait 3
- required: no
- mode: client

The time the client waits after startup before entering the slow-start phase.

## slow_start
- syntax: slow_start Seconds(10-600)
- default: slow_start 30
- required: no
- mode: client

The client gradually increases the number of new connections per second during the slow start time.

## vxlan
- syntax: vxlan vni innerSMAC innerDMAC localVtepIPAddr Number remoteVtepIPAddr Number
- default: -
- required: no
- mode: client, server

One 'vxlan' can be set per 'port'. 'innerSMAC' is the source MAC address of the inner packet, and 'innerDMAC' is the destination address of the inner packet.
'localVtepIPAddr' is the starting address of local vtep. Each NIC queue needs a local vtep address for traffic classification.
'remoteVtepIPAddr' is the starting address of the remote vtep. Number is the number of vtep addresses.

## kni
- syntax: kni [ifName]
- default: -
- required: no
- mode: client, server

Enable the kni interface. We create a kni interface for each 'port'. Common interface Names are vEth/vnic, etc. The default name is dperf.
The IP address and routing of the kni interface need to be manually configured.
It is recommended to assign a separate IP to the kni interface.
When only one CPU is enabled, the kni interface IP can be the traffic IP;
If multiple CPUs are used, the kni interface IP can use the first traffic IP of the server.

## tos
- syntax: tos Number(0x00-0xff or 0-255)
- default: 0
- required: no
- mode: client, server

Set the tos of the IPv4 header or the traffic class of the IPv6 header, which can be in hexadecimal or decimal.
Note: tos does not take effect on the packets sent by the kni interface.

## rss
- syntax: rss
- default: -
- required: no
- mode: client, server

Use the network interface L3 RSS distribution. When a network card without FDIR wants to use multi-queue/multi-threading, rss needs to be enabled.
Note: 'vxlan' cannot be used simultaneously with 'rss' yet. 

## tcp_rst
- syntax: tcp_rst Number[0-1]
- default: 1
- required: no
- mode: client, server

Set whether dperf replies rst to SYN packets requesting unopened TCP ports. 
