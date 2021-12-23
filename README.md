# Dperf: 10M HTTP CPS Load tester[![Apache V2 License](https://img.shields.io/badge/license-Apache%20V2-blue.svg)]
Dperf is a DPDK based high performance L4L7 load tester.
It can generate huge traffic with a single x86 server: tens of millions of HTTP CPS，hundreds of Gbps throughput and billions of connections.
It can also be a CPU benchmark for packets processing.

## Main Features
- http/udp CPS(connections per second) test
- http/udp CC(current connections) test
- http/udp TPS(throughput per second) test
- multiple client/server network subnets
- multiple network interfaces
- ipv4/ipv6
- X86_64/ARM64
- dpdk versions: dpdk-17.11, dpdk-18.11, dpdk-19.11, dpdk-20.11

## Performance
### HTTP Connection per Second
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
Dperf welcomes your contribution.

## Authors
* **Jianzhang Peng** - *Initial work* - [Jianzhang Peng](https://github.com/pengjianzhang)

## License
Dperf is distributed under the [Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0).
