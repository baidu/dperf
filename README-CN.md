# dperf [![Apache V2 License](https://img.shields.io/badge/license-Apache%20V2-blue.svg)](https://github.com/baidu/dperf/blob/main/LICENSE) <a href="https://hellogithub.com/repository/67958cc5d1f44a6a84f3544e3c007e5f" target="_blank"><img src="https://abroad.hellogithub.com/v1/widgets/recommend.svg?rid=67958cc5d1f44a6a84f3544e3c007e5f&claim_uid=Thc9mJByaKSbdng&theme=small" alt="Featured｜HelloGitHub" /></a>

[English](README.md) | 中文

dperf 是一个100Gbps的网络性能与压力测试软件。

## 优点
- 性能强大：
  - 基于 DPDK，使用一台普通 x86 服务器就可以产生巨大的流量：千万级的 HTTP 每秒新建连接数，数百Gbps的带宽，几十亿的并发连接数
- 统计信息详细：
  - 能够输出详细的统计信息，并且识别每一个丢包
- 使用场景丰富：
  - 可用于对四层负载均衡等四层网关进行性能压力测试、长稳测试
  - 可用于对云上虚拟机的网络性能进行测试
  - 可用于对网卡性能、CPU的网络报文处理能力进行测试
  - 压测场景下，可作为高性能的HTTP Server或HTTP Client单独使用

## 性能
### HTTP每秒新建连接数
|Client Cores|Server Cores|HTTP CPS|
|------------|------------|--------|
|1|1|210,1044|
|2|2|400,0423|
|4|4|701,0743|
|6|6|1002,7172|

### HTTP吞吐
|Client Cores|Server Cores|RX(Gbps)|TX(Gbps)|Client CPU Usage(%)|Server CPU Usage(%)|
|------------|------------|--------|--------|-------------------|-------------------|
|1|1|18|18|60|59|
|2|2|35|35|60|59|
|4|4|46|46|43|43|

### HTTP并发连接数
|Client Cores|Server Cores|Current Connections|Client CPU Usage(%)|Server CPU Usage(%)|
|------------|------------|-------------------|-------------------|-------------------|
|1|1|1,0000,0000|34|39|
|2|2|2,0000,0000|36|39|
|4|4|4,0000,0000|40|41|

### UDP TX PPS
|Client Cores|TX MPPS|Client CPU Usage(%)|
|------------|-------|-------------------|
|1|15.96|95|
|2|29.95|95|
|4|34.92|67|
|6|35.92|54|
|8|37.12|22|

注意：本测试基于单张25Gbps Mellanox CX4

### 测试环境配置（客户端、服务器）
dperf 的以上性能数据，基于下面的配置测试得到：

- 内存: 512GB(大页 100GB)
- 网卡: Mellanox MT27710 25Gbps * 2
- 内核: 4.19.90

## 统计数据
dperf 每秒输出多种统计数据：
- TPS, CPS,  各种维度的PPS
- TCP/Socket/HTTP级别的错误数
- 丢包数
- 按照TCP Flag分类的报文重传数

```
seconds 22                 cpuUsage 52
pktRx   3,001,058          pktTx    3,001,025          bitsRx   2,272,799,040      bitsTx  1,920,657,600      dropTx  0
arpRx   0                  arpTx    0                  icmpRx   0                  icmpTx  0                  otherRx 0          badRx 0
synRx   1,000,345          synTx    1,000,330          finRx    1,000,350          finTx   1,000,350          rstRx   0          rstTx 0
synRt   0                  finRt    0                  ackRt    0                  pushRt  0                  tcpDrop 0
skOpen  1,000,330          skClose  1,000,363          skCon    230                skErr   0
httpGet 1,000,345          http2XX  1,000,350          httpErr  0
ierrors 0                  oerrors  0                  imissed  0
```

## 开始使用
### 设置大页
    #参考如下参数编辑 '/boot/grub2/grub.cfg'，然后重启OS
    linux16 /vmlinuz-... nopku transparent_hugepage=never default_hugepagesz=1G hugepagesz=1G hugepages=8

### 编译DPDK
    #编辑'config/common_base'打开PMD开关
    #Mellanox CX4/CX5 requires 'CONFIG_RTE_LIBRTE_MLX5_PMD=y'
    #HNS3 requires 'CONFIG_RTE_LIBRTE_HNS3_PMD=y'
    #VMXNET3 requires 'CONFIG_RTE_LIBRTE_VMXNET3_PMD=y'

    TARGET=x86_64-native-linuxapp-gcc #or arm64-armv8a-linuxapp-gcc
    cd /root/dpdk/dpdk-stable-19.11.10
    make install T=$TARGET -j16

### 编译dperf
    cd dperf
    make -j8 RTE_SDK=/root/dpdk/dpdk-stable-19.11.10 RTE_TARGET=$TARGET

### 绑定网卡
    #Mellanox网卡跳过此步
    #假设PCI号是0000:1b:00.0

    modprobe uio
    modprobe uio_pci_generic
    /root/dpdk/dpdk-stable-19.11.10/usertools/dpdk-devbind.py -b uio_pci_generic 0000:1b:00.0

### 启动dperf server
    #dperf server监听6.6.241.27:80, 网关是6.6.241.1
    ./build/dperf -c test/http/server-cps.conf

### 从客户端发送请求
    #客户端IP必须要在配置文件的'client'范围内
    ping 6.6.241.27
    curl http://6.6.241.27/

## 运行测试
下面的例子运行一个HTTP CPS压力测试。

    #在server端运行dperf
    ./build/dperf -c test/http/server-cps.conf

    #以另一台机器作为client端，运行dperf
    ./build/dperf -c test/http/client-cps.conf

## 文档
请访问[https://dperf.org/](https://dperf.org/).

## 限制
 - dperf 要求HTTP消息在一个数据包中，所以不适合7层负载均衡的测试。
 - dperf 要求独占使用网络接口。
 - dperf 没有路由功能。建议配合三层交换机搭建测试环境。

## 贡献
dperf 欢迎大家贡献。详情请参阅[贡献指南](CONTRIBUTING.md)。

## 专利
 - 彭建章(2024). 网络设备的测试方法及其装置. CN114205274B. 2024-06-11.

## 作者
* [Jianzhang Peng](https://github.com/pengjianzhang), 中国科学技术大学计算机专业博士，曾在华为和百度担任主任工程师一职，参与了7层负载均衡与4层负载均衡系统的开发。在百度工作期间，他开发了dperf项目。即使从百度离职后，他仍然致力于维护dperf项目。目前，他在新加坡工作，专注于研究和实现加密货币的高频量化交易的低时延网络系统。

## 许可
dperf基于 [Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0) 许可证。
