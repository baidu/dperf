# dperf Design

## Background
dperf was first designed to perform performance testing for the four-layer load balancer (L4LB). The goal is to use a general-purpose physical server with some suitable network cards, dperf can generate considerable pressure, which is convenient for R&D personnel to carry out tests, and it is also convenient to demonstrate the performance of our L4LB in the customer environment.

The performance of L4LB is usually very high. Mainstream L4LB is implemented based on IP packet forwarding. It tracks the connection status through the session table, and then modifies and forwards IP packets; and L4LB does not analyze application layer data; therefore, L4LB can reach millions of new connections per second. One billion concurrency, millions of Gbps throughput.

The performance of L4LB is so high that it is very difficult to test its performance. This article explains how dperf solves this problem. Since UDP is relatively simple, this article only explains it from the perspective of TCP.

## Core needs
- Extremely high-performance TCP protocol stack, performance in all aspects exceeds L4LB;
- Support HTTP protocol, it is convenient to use various tools to test the compatibility and correctness of the protocol stack;
- Able to simulate HTTP client and HTTP server on a physical server, complete the test with minimal resources.

## Reasonable assumptions
- The load test is carried out on the internal network, and the IP address can be set in the entire section;
- dperf can be connected to a three-layer switch without supporting routing function;
- we can test small and large packets, without supporting long HTTP messages (such as 16K HTTP requests, 1MB HTTP responses, etc.), and the HTTP message length is limited to 1 MTU;
- Do not pursue the transmission speed of a single connection, and enhance the overall test pressure through mechanisms such as large concurrency and long connections;
- In the performance test, the HTTP message format is legal, and dperf does not need to do a complete verification of the legality of the HTTP message;
- During the test, the content of the HTTP message sent by dperf is fixed in length and the content is unchanged.

## Design Essentials
### Multi threads and Flow Director
Dperf is a multi-threaded program, each thread uses only 1 RX and 1 TX. dperf uses the diversion feature (FDIR) of the network card. Each dperf server thread is bound to a listening IP, and the destination IP of the message used on the dperf server is shunted. Each dperf client thread only requests one destination IP, and the dperf client uses the source IP of the packet to divert. After shunting, there is no shared resources between threads and no lock competition. Theoretically, the performance can be linearly expanded. In fact, due to the sharing of physical resources such as the CPU execution unit, Cache, bus, and network card, it is inevitable that they will interfere with each other and cannot achieve 100%. 100 linear.

For NICs that do not support the FDIR feature. dperf uses L3/L3L4 RSS splits. dperf uses a symmetric hash key, and the client will select a specific IP, so that the packets in the TX/RX direction of the same connection all fall on the same > thread.
For network cards that do not support setting RSS. dperf uses the default RSS offloading algorithm. The first sending queue connected to the connection may not be the receiving queue, and dperf uses the first response message to correct it.
The socket table is shared on the dperf server, and the message hash to any thread can be processed normally. The performance of RSS is not much different from that of FDIR.

### Very small socket
1 socket (1 connection) only occupies 64 bytes and can be placed in a cache line. 1 billion connections only require 6.2GB of memory, so dperf can easily reach billions of concurrent connections.

However, a connection of the general TCP protocol stack may occupy 16K resources, and the size of a session record of the four-layer load balancing is several hundred bytes. Therefore, it is more than enough to use dperf to pressure test the number of concurrent connections of the four-layer load balancing.

### Address and socket lookup
dperf requires the use of continuous client addresses, continuous listening addresses, and continuous listening ports. The source address and destination address used by dperf must all be written in the configuration file. When dperf starts, according to the combination of address and port, the socket is applied for at once to form a 3-dimensional array lookup table. In order to reduce memory consumption, the client address uses the lowest two bytes to index, the server address is not indexed, each thread is associated with a server IP, and the listening port is indexed by a serial number. dperf does not use a hash algorithm to find the socket, it only needs to access the array, and it only takes a few instructions.

### Customized TCP Stack
I originally planned to use the FreeBSD protocol stack, but due to some reasons, I need to complete the demo within 2 weeks. So I needed to reduce the workload and difficulty. I had to abandon the FreeBSD protocol stack and redesign a TCP protocol stack. Then the question became: What kind of TCP protocol stack is for the stress tester of the four-layer load balancer? Is it acceptable?

This is the result of my thinking:

- The protocol stack is only used for performance testing of the four-layer load balancer;
- There is no need to support congestion control and SACK; only need to implement stop-and-wait protocol and retransmit over time. The client sends a request message and waits for a message from the server to respond, but the response is still one message. If the timeout expires, and the other party has not received the confirmation, it will retransmit. If the retransmission is unsuccessful for many times, the connection will be considered as a failure;
- There is no need to support the event mechanism, the application layer processing is performed immediately after receiving the message, socket and application layer processing are deeply integrated into the TCP protocol stack;
- There is no need to support TCP keepalive timer, there is no idle connection for a long time, short connection does not need TCP keepalive timer, long connection will send requests regularly, and TCP keepalive timer is not required;
- No timewait timer is required. The closed connection allows quick recycling;
- Since dperf only sends 1 message at a time and receives 1 message at a time, we believe that dperf's sending window and receiving window are sufficient, and it will not happen or support that the message is partially confirmed.

### Timer
dperf once used the time wheel timer. One time wheel timer consumes 32 bytes, and one socket uses two timers, which consumes 64 bytes. This is the memory for a tester that supports billions of concurrent connections. The consumption is a bit high, and the frequent function callback overhead is unacceptable. I removed the time wheel timer and borrowed FreeBSD's timewait timer to put all the same tasks in a linked list sorted by time.

### Zero Copy, Zero Sockbuf, Zero Checksum
- Zero copy. Except for the startup phase, dperf does not copy any data, including Ethernet header/IP header/transport layer header/payload, because these contents are almost fixed.
- Zero receive buffer. dperf does not care about the content of the payload. These contents are nothing more than the filler of the message. dperf only cares about how many bytes are received.
- Zero sending buffer. The ordinary protocol stack needs to send buffers, and only after receiving the confirmation from the other party, can these buffers be released, otherwise it is retransmitted; in addition, it needs to be considered that the sequence number of the other party's confirmation may be anywhere in the buffer, not necessarily confirming a message At the end of; the management of the sending buffer is a more complicated task. The data sent is the same, so dperf does not need a socket-level sending buffer, just a global message pool.
- Zero checksum. Usually we will use the network card function to offload the checksum of the message, but we have to calculate the pseudo header; for the same connection, the message type is fixed, dperf has already calculated the tail and head checksum, the whole process does not Checksum calculation.

### HTTP protocol implementation
The dperf server is very stupid. It receives any 1 data packet (the first character is G, the beginning of GET), it considers it to be a complete request, and sends a fixed response.

The dperf client supports HTTP and has the following reasonable requirements:
- The message complies with the HTTP protocol specification
- TCP packets cannot overlap
- The following headers cannot cross packets: connection, content-length, transfer-encoding

### VXLAN
Considering that many NICs do not have VXLAN inner-layer packet checksum offloading and offloading capabilities, dperf does not use these advanced functions to support to more NICs.
- classification. dperf uses the destination IP of the outer packet to classify traffic. Each queue is a vtep.
- ipv4/ipv6. The outer layer only supports IPV4, and the inner layer supports both IPV4 and IPV6.
- addressing. The MAC address of inner-layer packets needs to be specified in the configuration. Dynamic addressing of inner-layer packets is not supported.
- Checksum. The outer packets use hardware offload, and the inner packets use incremental checksum correction.

### KNI
KNI is required in the following scenarios:
- need to use dynamic routing protocol.
- Missing management port.

Implementation principle:
- dperf forwards all the packets that do not hit the socket to the kni interface.
- dperf receives a packet from the kni every 1ms, so the kni of dperf is only suitable for transmitting a small number of packets.
- The processing thread of the kni of each port is the processing thread of the first queue of this port.

### Bond
Since dperf supports multiple network cards, normally, we don't use bond; sometimes, we need to use bond mode.
To support Mode 4, dperf sends lldp packets every 100ms.
dperf requires that the network interfaces in the same bond belong to the same NUMA.
Note that the PF of many network interfaces support Mode 4, and the VF dose not support Mode 4.

### Other optimizations
- dperf uses inline extensively to avoid function calls;
- Socket memory is allocated from large pages to avoid missing page tables;
- Batch release of messages;
- Sending with buffering, once sending is unsuccessful, and sending again next time, there will be no packet loss immediately.

## Other uses
According to the characteristics of dperf, we found that in addition to testing L4LB, it can also:

- Test other gateways based on four-layer forwarding, such as link load balancing, firewalls, etc.
- Test the network performance of virtual machines on the cloud
- Test the performance of the network card and the network packet processing capability of the CPU
- As a high-performance HTTP client or HTTP server for stress testing scenarios
