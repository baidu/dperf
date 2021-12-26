# dperf is a load tester for L4 load balancer[![Apache V2 License](https://img.shields.io/badge/license-Apache%20V2-blue.svg)](https://github.com/baidu/dperf/blob/main/LICENSE)
Based on DPDK, dperf can generate huge traffic with a single x86 server: tens of millions of HTTP CPS，hundreds of Gbps throughput and billions of connections. It can give detailed statistics and find every packet loss.

## Performance
### HTTP Connections per Second
|Client Cores|Server Cores|HTTP CPS|
|------------|------------|--------|
|1|1|2,101,044|
|2|2|4,000,423|
|4|4|7,010,743|
|6|6|10,027,172|

### HTTP Throughput per Second
|Client Cores|Server Cores|RX(Gbps)|TX(Gbps))|Client CPU Usage(%)|Server CPU Usage(%)|
|------------|------------|--------|---------|-------------------|-------------------|
|1|1|18|18|60|59|
|2|2|35|35|60|59|
|4|4|46|46|43|43|

### HTTP Current Connections
|Client Cores|Server Cores|Current Connections|Client CPU Usage(%)|Server CPU Usage(%)|
|------------|------------|-------------------|-------------------|-------------------|
|1|1|100,000,000|34|39|
|2|1|200,000,000|36|39|
|4|4|400,000,000|40|41|

### Client & Server Configuration
- MEM: 512GB(hugepage 100GB)
- NIC: Mellanox MT27710 25Gbps * 2
- Kernel: 4.19.90

## Statistics
dperf outputs various statistics every second.
- TPS, CPS, various PPS
- Errors of TCP/Socket/HTTP
- Packets loss/drop
- Retransmissions of TCP Flags

'''
seconds 22                 cpuUsage 52
pktRx   3,001,058          pktTx    3,001,025          bitsRx   2,272,799,040      bitsTx  1,920,657,600      dropTx  0
arpRx   0                  arpTx    0                  icmpRx   0                  icmpTx  0                  otherRx 0          badRx 0
synRx   1,000,345          synTx    1,000,330          finRx    1,000,350          finTx   1,000,350          rstRx   0          rstTx 0
synRt   0                  finRt    0                  ackRt    0                  pushRt  0                  tcpDrop 0
skOpen  1,000,330          skClose  1,000,363          skCon    230                skErr   0
httpGet 1,000,345          http2XX  1,000,350          httpErr  0
ierrors 0                  oerrors  0                  imissed  0
'''

## Getting Started
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

## Running the tests
    Test HTTP CPS as shown below.

    #run server at some host
    ./build/dperf -c test/http/server-cps.conf

    #run client at another host
    ./build/dperf -c test/http/client-cps.conf

## Contributing
dperf welcomes your contribution.

## Authors
* **Jianzhang Peng** - *Initial work* - [Jianzhang Peng](https://github.com/pengjianzhang)

## License
dperf is distributed under the [Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0).
