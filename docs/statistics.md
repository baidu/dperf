# statistics description

### seconds
The number of seconds since the start of the program.

### cpuUsage
The CPU usage of each dperf worker thread.

### pktRx
The number of packets received per second.

### pktTx
The number of packets sent per second.

### bitsRx
The bits received per second.

### bitsTx
Bits sent per second.

### dropTx
The number of tables lost per second due to a large number of packets being sent at one time and some packets are not being sent.

### udpRx
The number of UDP messages received per second.

### udpTx
The number of UDP messages sent per second.

### tcpRx
The number of TCP messages received per second.

### tcpTx
The number of TCP messages received per second.

### tosRx
The number of IP messages received per second with TOS equal to that in the configuration.

### arpRx
The number of arp packets received per second.

### arpTx
The number of arp packets sent per second.

### icmpRx
The number of icmp packets received per second, including icmpv6 packets.

### icmpTx
The number of icmp packets sent per second, including icmpv6 packets.

### otherRx
The number of packets of unknown type received per second.

### badRx
The number of error packets received per second, and the reason for the error:
-checksum error
-ipv4 header length is not 20

### synRx
The number of TCP SYN packets received per second.

### synTx
The number of TCP SYN packets sent per second.

### finRx
The number of TCP FIN packets received per second.

### finTx
The number of TCP FIN packets sent per second.

### rstRx
The number of TCP RST packets received per second.

### rstTx
The number of TCP RST packets sent per second.

### synRt
The number of TCP SYN packets retransmitted per second. Retransmission means packet loss.

### finRt
The number of TCP FIN packets retransmitted per second. Retransmission means packet loss.

### ackRt
The number of TCP ACK packets retransmitted per second. Retransmission means packet loss.

### pushRt
The number of TCP PUSH packets retransmitted per second. Retransmission means packet loss.

### tcpDrop
The number of TCP packets dropped per second. The reasons include:
-Wrong destination port
-Could not find connection
-Wrong TCP status
-Wrong TCP sequence number
-Wrong TCP Flag
-The server runs the UDP protocol and receives TCP packets

### skOpen
The number of sockets opened per second is the number of new connections per second (CPS).

### skClose
The number of sockets closed per second.

### skCon
The number of sockets currently open, that is, the number of concurrent connections.

### skErr
The number of concurrent connections with errors per second, and the reason for the error:
-Retransmit more than 3 times

### rtt(us)
The average rtt of the first packet, the unit is us.

### httpGet
-The number of HTTP GET requests sent by the client per second
-The number of HTTP GET requests received by the server per second

### http2XX
-The number of HTTP 2XX responses received by the client per second
-The number of HTTP 2XX responses sent by the server per second

### httpErr
The number of HTTP packet errors received per second, and the reason for the error:
-The response message received by the client is not HTTP 2XX
-The request received by the server is not an HTTP GET

### udpRt
The number of retransmissions of UDP packets per second.

### udpDrop
The number of lost UDP packets per second.

### ierrors
The cumulative number of network card receiving errors.

### oerrors
The cumulative number of network card sending errors.

### imissed
The cumulative number of packet loss caused by the hardware of the network card due to insufficient receiving buffer.
