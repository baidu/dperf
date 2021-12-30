# dperf Design

## Background
dperf is designed to test the performance of the L4 load balancer(L4LB). It can generate a lot of pressure with only one ordinary physical server installed with some appropriate network interfaces to meet my daily research and development needs, and can easily display the performance of our network products in the customer's environment.

L4LB has high performance. Most L4LBs are based on packet forwarding and typically have a session table inside that tracks the state of the connections, but does not analyze application-layer data. So a L4LB has millions of new connections, billions concurrent connections, and hundreds of gigabits of throughput.

## Core needs
- Extremely high performance TCP protocol stack
- Supports HTTP protocol for easy testing with other clients and servers
- Simulate both HTTP client and HTTP server on a physical server

## Reasonable assumptions
- Testing in an internal network where addresses can be freely specified
- dperf does not require routing, it is handled by the switch
- HTTP request or response cannot exceed one MTU in a single message
- HTTP message is correct, dperf does not need to validate HTTP message

## Usage scenario
dperf(client)------------------------dperf(server)

dperf(client)------------------------Server(eg nginx)

Client(eg ab/curl)-------------------dperf(server)

dperf(client)---------DUT(L4LB)------dperf(server)

Note: the switch is omitted.

## Design Essentials
### Multi threads and Flow Director
dperf is a multi-thread program that can use multiple network interfaces, but only one RX queue and one TX queue per thread. dperf takes advantage of the FDIR (flow director) function of the network interface. Each thread of dperf (server) binds a separate IP, and dperf (client) requests only one destination IP per thread, so that each thread can run independently.

### Very small socket
A socket consumes only 64 bytes and can be placed in a cache line, so it's easy to reach billions of concurrent connections.

### Address and socket lookup
Clients and servers use a continuous range of IP addresses. Each network interface takes up one range of IP addresses, the number of IPs used by dperf(server) equals the number of threads, dperf(client) can use more IPs (eg 100), the server's listening port is also a continuous range of ports, and the lower two bytes of the client's IP address is the index for lookup and needs to be unique. Since the IP addresses of the client and server are determined beforehand, all sockets are allocated at dperf startup to form a table, the lower 2 bytes of client port, server port, and client IP can be used to locate the socket immediately.

### Customized TCP Stack
Consider that this TCP stack is only used to send HTTP requests and HTTP responses, and requires HTTP requests and HTTP responses to be in a single packet. This can be accomplished using a TCP stack for a stop protocol, without supporting SACK, congestion control algorithms, and with just supporting timeout retransmissions.

### Timer
In earlier versions, a time-wheel timer was designed, but the memory consumption of the timer was unacceptable and function calls were expensive. In addition, considering the low number of retransmissions (which should reduce the pressure if more retransmissions occur), we only need to place the socket in a FIFO queue, similar to the timewait timer in the freebsd's TCP stack.

### Zero Copy, Zero Sockbuf, Zero Checksum
The payload and Ethernet header of the packets sent by dperf are fixed, so we do not need to copy them or cache them in the socket. Checksum calculations can also be reused.

### Inline function
Many inline functions are used to reduce function calls.

## Network Benchmark
When we ported L4LB to multiple CPUs, we used dperf to test the CPU's performance by sending traffic to each other on two servers of these CPUs. we found that some processors were suitable for network packet processing, while others were not as good as I expected. dperf can be used as a CPU benchmark for packets processing.
