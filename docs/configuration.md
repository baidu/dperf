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

In daemon mode, dperf statistics are written to the log file ('/var/log/dperf/dperf-ctl.log'), otherwise output to stdout.

## keepalive
- syntax: keepalive
- default: - 
- required: no
- mode: server

Keepalive means that multiple requests and responses can be sent in one connection.
'keepalive' needs to be explicitly enabled on the dperf server.
After 'keepalive' is enabled, the dperf server will not actively close the connection, it will wait for the client's FIN or RST.
When 'keepalive' is not enabled, the dperf server closes the connection at the fastest speed in the world by directly setting the FIN Flag at the response packet.

There is no need to enable 'keepalive' for the dperf client. When the dperf client finds 'cc' in the configuration file, 'keepalive' will be enable automatically.
 
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
- syntax: port PCI IPAddress Gateway [GatewayMAC]
- default: -
- required: yes
- mode: client, server

Configure the network interface port used by dperf. 
If you want to use multiple network interface ports, you only need to configure multiple 'port's. 
As a DPDK program, dperf will take over these network interface ports from the operating system. 
Before starting dperf, you need to use the DPDK script 'dpdk-devbind.py' for driver binding (except for Mellanox network interfaces).
- PCI: The PCI number of the network interface port, use 'dpdk-devbind.py -s' to get it from the system.
- IPAddress: This IP  Address is used to interconnect with the 'Gateway'
- Gateway: Gateway's IP address. dperf has no routing capability. It will send all packets to the gateway, except for ARP, NS, and ND.
- Gateway-MAC: the MAC address of the 'Gateway', which can be omitted.

Reference:
[binding-and-unbinding-network-ports](http://doc.dpdk.org/guides/linux_gsg/linux_drivers.html#binding-and-unbinding-network-ports-to-from-the-kernel-modules)

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

## synflood
- syntax: synflood
- default: -
- required: no
- mode: client

If this flag is enabled, the dperf client will only send SYN packets and will not establish connections.

## keepalive_request_interval
- syntax: keepalive_request_interval Time
- default: keepalive_request_interval 1s
- required: no
- mode: client

Set the interval between two requests in the a connection. It only takes effect after setting 'cc'.

Example:
- keepalive_request_interval 1ms
- keepalive_request_interval 1s
- keepalive_request_interval 60s

## keepalive_request_num Number
- syntax: keepalive_request_num Number
- default: -
- required: no
- mode: client

How many requests are sent in the a connection before closing the connection. It only takes effect after setting 'cc'.

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

Set the listening IP range of the server. The number of 'port' must be the same as the number of 'server', and each worker thread needs an independent listening IP.

## listen
- syntax: listen Port Number
- default: listen 80 1
- required: yes

Set the port ranges that the server listens to, and the client sends packets to these port ranges.

Note: dperf will allocate all sockets at startup. Please do not set a large port range.

## payload_size
- syntax: payload_size Number(1-1400)
- default: payload_size 1
- required: no
- mode: client, server

Set the size of the request or response in bytes.
If 'Number' is less than the minimum value of dperf, the minimum value is used.
You can use 1 to represent the minimum value.

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

## slow_start
- syntax: slow_start Seconds(10-600)
- default: slow_start 30
- required: no
- mode: client

The client gradually increases the number of new connections per second during the slow start time.
