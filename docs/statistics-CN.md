# 统计说明

### seconds
程序启动到现在的秒数。

### cpuUsage
每个dperf worker线程的CPU使用率。

### pktRx
每秒接收的报文个数。

### pktTx
每秒发送的报文个数。

### bitsRx
每秒接收到的bit。

### bitsTx
每秒发送的bit。

### dropTx
由于一次发送大量报文导致部分报文未发送引起的每秒丢表数。

### udpRx
每秒收到的UDP报文个数。

### udpTx
每秒发送的UDP报文个数。

### tcpRx
每秒收到的TCP报文个数。

### tcpTx
每秒收到的TCP报文个数。

### tosRx
每秒收到的IP报文中tos与配置中tos相等的报文个数。

### arpRx
每秒收到的arp报文数。

### arpTx
每秒发送的arp报文数。

### icmpRx
每秒收到的icmp报文数量，包含icmpv6报文。

### icmpTx
每秒发送的icmp报文数量，包含icmpv6报文。

### otherRx
每秒收到的未知类型的报文个数。

### badRx
每秒收到的错误报文数，错误原因：
- checksum错误
- ipv4头部长度不为20

### synRx
每秒接收到的TCP SYN报文个数。

### synTx
每秒发送的TCP SYN报文个数。

### finRx
每秒接收到的TCP FIN报文个数。

### finTx
每秒发送的TCP FIN报文个数。

### rstRx
每秒接收到的TCP RST报文个数。

### rstTx
每秒发送的TCP RST报文个数。

### synRt
每秒重传TCP SYN报文的个数，重传意味着丢包。

### finRt
每秒重传TCP FIN报文的个数，重传意味着丢包。

### ackRt
每秒重传TCP ACK报文的个数，重传意味着丢包。

### pushRt
每秒重传TCP PUSH报文的个数，重传意味着丢包。

### tcpDrop
每秒丢掉的TCP报文的个数，原因包括:
- 错误的目的端口
- 找不到连接
- 错误的TCP状态
- 错误的TCP序列号
- 错误的TCP Flag
- 服务器运行UDP协议，收到TCP报文

### skOpen
每秒打开的socket个数, 就是每秒新建连接数(CPS)。

### skClose
每秒关闭的socket个数。

### skCon
当前处于打开状态的socket个数，即并发连接数。

### skErr
每秒出错的并发连接数，出错原因：
- 重传超过3次

### rtt(us)
首包平均rtt，单位是us。

### httpGet
- 客户端每秒发送的HTTP GET请求个数
- 服务端每秒收到的HTTP GET请求个数

### http2XX
- 客户端每秒接收的HTTP 2XX响应个数
- 服务端每秒发送的HTTP 2XX响应个数

### httpErr
每秒收到的HTTP报文错误个数，错误原因：
- 客户端收到的响应报文不是HTTP 2XX
- 服务器收到的请求不是HTTP GET

### udpRt
每秒UDP报文的重传数。

### udpDrop
每秒UDP报文的丢包数。

### ierrors
网卡接收错误的累计数。

### oerrors
网卡发送错误的累计数。

### imissed
由于接收buffer不够，导致网卡硬件丢包的累计数。
