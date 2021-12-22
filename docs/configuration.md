# Configuration

## daemon
- syntax: daemon
- default: -
- required: no
Run in daemon. Statistics are output in the file '/var/log/dperf/dperf-ctl.log'.

## keepalive
- syntax: keepalive
- default: - 
- required: no
The server enables keepalive to test the bandwidth and the number of concurrent connections.

## mode
- syntax: mode client | server
- default: -
- required: yes
Set dperf running mode: client or server
 
## cpu
- syntax: cpu n0 n1 n2-n3...
- default: -
- required: yes
How many threads are running, and on which CPUs these threads are running.

    Example:
    cpu 0-3 12-15
    cpu 0 1 2 3 4 5

## socket_mem
- syntax: socket_mem n0,n1,n2...
- default: -
- required: no
Pass the "--socket MEM" parameter to dpdk so that multiple dperf instances can run on a host.

    Example:
    socket_mem  40960,0
    socket_mem  0,40960

## port
- syntax: port PCI IPAddress Gateway [Gateway-Mac]
- default: -
- required: yes
Set the network card used by dperf. You can assign multiple network cards to dpef. Dperf will take over these network cards from the operating system.

## duration
- syntax: duration Time
- default: duration 100s
- required: yes 
Set the running time of dperf. Dperf will slowly increase the pressure when it starts, and will delay for a few seconds when it exits. You can also make dperf exit through a signal (SIGINT). 

    Example:
    duration 1.5d
    duration 2h
    duration 3.5m
    duration 100s
    duration 100

## cps
- syntax: cps Number
- default: -
- required: yes
Sets the number of new connections per second(CPS) sent by clients.

    Example:
    cps 10m
    cps 1.5m
    cps 2k
    cps 100

## cc
- syntax: cc Number 
- default: -
- required: no
Set the target of the number of concurrent connections that the client needs to establish. Usually need to cooperate with 'keepalive_request_interval' and 'keepalive_request_num' sets the request interval and the number of requests for a single connection. 

    Example:
    cc 100m
    cc 1.5m
    cc 2k
    cc 100

## synflood
- syntax: synflood
- default: -
- required: no
The client only sends SYN packets.

## keepalive_request_interval Time, eg 1ms, 1s, 60s, default 1s
- syntax: keepalive_request_interval Time
- default: keepalive_request_interval 1s
- required: no
Set the interval between two requests in the same connection.

    Example:
    keepalive_request_interval 1ms
    keepalive_request_interval 1s
    keepalive_request_interval 60s

## keepalive_request_num Number
- syntax: keepalive_request_num Number
- default: -
- required: no
How many requests are sent in the same connection before closing the connection.

# launch_num
- syntax: launch_num Number
- default: launch_num 4
- required: no

How many connections are initiated by the client at a time. In case of packet loss, try to reduce the number to make the packet sending more smooth, so as to avoid the packet loss of the receiver's network card caused by the surge of packets.

## client
- syntax: client IPAddress Number
- default: -
- required: yes 
Set the IP address range of the client. 'IPAddress' is the starting address. The last byte of the address in the address range cannot exceed 254. In client mode, only one client address range can be configured for each network card interface; Dperf uses it as the source address of the connection. In the server mode, multiple connections can be configured, indicating that the server only accepts connections within these IP ranges.
Note: dperf will allocate all sockets at startup. Please do not set a large address range.

## server
- syntax: server IPAddress Number
- default: -
- required: yes 
Set the IP address range of the server. Each network card interface can only be configured with one item. The 'number' must be equal to the number of CPUs.

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

Set the size of the HTTP request, response or UDP payload in bytes. When testing CPS, it is recommended to set payload_size to 1. 1 means the smallest. Since the request or response will include HTTP headers, the real packet payload exceeds 1.

## mss
- syntax: mss Number
- default: mss 1460
- required: no
Set tcp mss option.

## protocol
- syntax: protocol tcp | udp
- default: protocol tcp
- required: no

## tx_burst
- syntax: tx_burst Number(1-1024)
- default: tx_burst 8
- required: no

Set the number of packets sent at one time. A smaller value can make the data packet sending smoother and avoid packet loss at the receiving side, but it increases the CPU consumption of dperf.
