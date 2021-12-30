# dperf is a network load tester for cloud [![Apache V2 License](https://img.shields.io/badge/license-Apache%20V2-blue.svg)](https://github.com/baidu/dperf/blob/main/LICENSE)

[English](README.md) | 中文

基于DPDK，使用一台普通x86服务器，dperf可以产生巨大的流量：千万级的HTTP每秒新建连接数，数百Gbps的带宽，几十亿的并发连接数。dperf能够输出详细的统计信息，并且识别每一个丢包。

## 性能
### HTTP每秒新建连接数
|Client Cores|Server Cores|HTTP CPS|
|------------|------------|--------|
|1|1|2,101,044|
|2|2|4,000,423|
|4|4|7,010,743|
|6|6|10,027,172|

### HTTP吞吐
|Client Cores|Server Cores|RX(Gbps)|TX(Gbps))|Client CPU Usage(%)|Server CPU Usage(%)|
|------------|------------|--------|---------|-------------------|-------------------|
|1|1|18|18|60|59|
|2|2|35|35|60|59|
|4|4|46|46|43|43|

### HTTP并发连接数
|Client Cores|Server Cores|Current Connections|Client CPU Usage(%)|Server CPU Usage(%)|
|------------|------------|-------------------|-------------------|-------------------|
|1|1|100,000,000|34|39|
|2|1|200,000,000|36|39|
|4|4|400,000,000|40|41|

### 测试环境配置（客户端、服务器）
- 内存: 512GB(大页 100GB)
- 网卡: Mellanox MT27710 25Gbps * 2
- 内核: 4.19.90

## 统计数据
dperf每秒输出多种统计数据。
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
    #set hugepages
    #edit '/boot/grub2/grub.cfg' like this, and reboot the OS
    #linux16 /vmlinuz-... nopku transparent_hugepage=never default_hugepagesz=1G hugepagesz=1G hugepages=8

    #download & build dpdk
    #download and unpack dpdk
    TARGET=x86_64-native-linuxapp-gcc
    #TARGET=arm64-armv8a-linuxapp-gcc
    cd /root/dpdk/dpdk-stable-19.11.10
    make install T=$TARGET -j16

    #build dperf
    cd dperf
    make -j8 RTE_SDK=/root/dpdk/dpdk-stable-19.11.10 RTE_TARGET=$TARGET

    #bind NIC
    modprobe uio
    insmod /root/dpdk/dpdk-stable-19.11.10/$TARGET/kmod/igb_uio.ko

    #Suppose your PCI number is 0000:1b:00.0
    /root/dpdk/dpdk-stable-19.11.10/usertools/dpdk-devbind.py -b igb_uio 0000:1b:00.0

    #run dperf server
    #dperf server bind at 6.6.241.27:80,  gateway is 6.6.241.1
    ./build/dperf -c test/http/server-cps.conf

    #send request to dperf server at client
    curl http://6.6.241.27/

## 运行测试
    运行一个HTTP CPS测试。

    #run server at some host
    ./build/dperf -c test/http/server-cps.conf

    #run client at another host
    ./build/dperf -c test/http/client-cps.conf

## 文档
 - [配置手册](docs/configuration-CN.md)
 - [设计原理](docs/design-CN.md)

## 贡献
dperf欢迎大家贡献。

## 作者 
* **Jianzhang Peng** - *初试工作* - [Jianzhang Peng](https://github.com/pengjianzhang)

## 许可
dperf基于[Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0)许可证.
