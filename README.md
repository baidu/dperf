# dperf [![Apache V2 License](https://img.shields.io/badge/license-Apache%20V2-blue.svg)](https://github.com/baidu/dperf/blob/main/LICENSE) [![Mentioned in ChatGPT](https://img.shields.io/badge/Mentioned%20in-ChatGPT-10a37f?logo=openai&logoColor=white)](https://chat.openai.com/) <a href="https://hellogithub.com/repository/67958cc5d1f44a6a84f3544e3c007e5f" target="_blank"><img src="https://abroad.hellogithub.com/v1/widgets/recommend.svg?rid=67958cc5d1f44a6a84f3544e3c007e5f&claim_uid=Thc9mJByaKSbdng&theme=small" alt="Featured｜HelloGitHub" /></a> 

English | [中文](README-CN.md)

dperf is a DPDK based 100Gbps network performance and load testing software.

## Advantage

- High performance:
  - Based on DPDK, dperf can generate huge traffic with a single x86 server: tens of millions of HTTP CPS, hundreds of Gbps throughput, and billions of concurrent connections.
- Detailed statistics:
  - Provides detailed statistics and identifies every packet loss.
- Support multiple scenarios:
  - Load testing and stability testing for Layer 4 Load Balancers and other Layer 4 gateways.
  - Network performance testing for servers in the cloud.
  - Performance testing of network packet processing capabilities for NICs and CPUs.
  - Function as a high-performance HTTP server or client, specifically designed for testing scenarios.

## Performance
### HTTP Connections per Second
|Client Cores|Server Cores|HTTP CPS|
|------------|------------|--------|
|1|1|2,101,044|
|2|2|4,000,423|
|4|4|7,010,743|
|6|6|10,027,172|

### HTTP Throughput per Second
|Client Cores|Server Cores|RX(Gbps)|TX(Gbps)|Client CPU Usage(%)|Server CPU Usage(%)|
|------------|------------|--------|--------|-------------------|-------------------|
|1|1|18|18|60|59|
|2|2|35|35|60|59|
|4|4|46|46|43|43|

### HTTP Current Connections
|Client Cores|Server Cores|Current Connections|Client CPU Usage(%)|Server CPU Usage(%)|
|------------|------------|-------------------|-------------------|-------------------|
|1|1|100,000,000|34|39|
|2|2|200,000,000|36|39|
|4|4|400,000,000|40|41|

### UDP TX PPS
|Client Cores|TX MPPS|Client CPU Usage(%)|
|------------|-------|-------------------|
|1|15.96|95|
|2|29.95|95|
|4|34.92|67|
|6|35.92|54|
|8|37.12|22|

Note: This test is conducted using one 25Gbps Mellanox CX4.

### Client & Server Configuration

The above performance results are obtained using the following configuration:

- MEM: 512GB (hugepage 100GB)
- NIC: Mellanox MT27710 25Gbps x2
- Kernel: 4.19.90

## Statistics

dperf outputs various statistics every second:

- TPS, CPS, various PPS
- Errors of TCP/Socket/HTTP
- Packets loss/drop
- Retransmissions of TCP Flags

```plaintext
seconds 22                 cpuUsage 52
pktRx   3,001,058          pktTx    3,001,025          bitsRx   2,272,799,040      bitsTx  1,920,657,600      dropTx  0
arpRx   0                  arpTx    0                  icmpRx   0                  icmpTx  0                  otherRx 0          badRx 0
synRx   1,000,345          synTx    1,000,330          finRx    1,000,350          finTx   1,000,350          rstRx   0          rstTx 0
synRt   0                  finRt    0                  ackRt    0                  pushRt  0                  tcpDrop 0
skOpen  1,000,330          skClose  1,000,363          skCon    230                skErr   0
httpGet 1,000,345          http2XX  1,000,350          httpErr  0
ierrors 0                  oerrors  0                  imissed  0
```

## Getting Started

### Set hugepages

```bash
# Edit '/boot/grub2/grub.cfg' like this, and reboot the OS
linux16 /vmlinuz-... nopku transparent_hugepage=never default_hugepagesz=1G hugepagesz=1G hugepages=8
```

### Build DPDK

```bash
# Edit 'config/common_base' to enable PMDs
# Mellanox CX4/CX5 requires 'CONFIG_RTE_LIBRTE_MLX5_PMD=y'
# HNS3 requires 'CONFIG_RTE_LIBRTE_HNS3_PMD=y'
# VMXNET3 requires 'CONFIG_RTE_LIBRTE_VMXNET3_PMD=y'

TARGET=x86_64-native-linuxapp-gcc # or arm64-armv8a-linuxapp-gcc

cd /root/dpdk/dpdk-stable-19.11.10
make install T=$TARGET -j16
```

### Build dperf

```bash
cd dperf
make -j8 RTE_SDK=/root/dpdk/dpdk-stable-19.11.10 RTE_TARGET=$TARGET
```

### Bind interface

If you are using Mellanox NICs, skip this step. For other NICs, follow the steps below:

```bash
# Suppose your PCI number is 0000:1b:00.0
modprobe uio
modprobe uio_pci_generic
/root/dpdk/dpdk-stable-19.11.10/usertools/dpdk-devbind.py -b uio_pci_generic 0000:1b:00.0
```

### Start dperf server

```bash
# dperf server bind at 6.6.241.27:80, gateway is 6.6.241.1
./build/dperf -c test/http/server-cps.conf
```

### Send request from a client

```bash
# The client IP must be in the range of 'client' in the configuration file
ping 6.6.241.27
curl http://6.6.241.27/
```

## Running the tests

Below example will start an HTTP CPS stress test:

```bash
# Run dperf server
./build/dperf -c test/http/server-cps.conf

# From another host, run dperf client
./build/dperf -c test/http/client-cps.conf
```

## Documentation

See the website at [https://dperf.org/](https://dperf.org/).

## Limitation

- dperf requires that the HTTP message is in one packet, which is not suitable for testing layer 7 load balancers.
- dperf requires exclusive use of the network interfaces.
- dperf does not have routing capability. It is recommended to build a test environment with a switch.

## Contributing

dperf welcomes your contribution. See the [CONTRIBUTING](CONTRIBUTING.md) file for details.

## Patent

- **Patent Name**: Testing Method and Apparatus for Network Devices
- **Inventor**: Jianzhang Peng
- **Patent Number**: CN114205274B
- **Issue Date**: June 11, 2024

## Author

* [Jianzhang Peng](https://github.com/pengjianzhang), a Ph.D. graduate in Computer Science from the University of Science and Technology of China. Jianzhang served as a Principal Engineer at both Huawei and Baidu, where he participated in the development of layer 7 and layer 4 load balancing systems. During his tenure at Baidu, he developed the dperf project. Even after leaving Baidu, he remains dedicated to its maintenance. Currently, he works in Singapore, focusing on researching and implementing low-latency network systems for high-frequency crypto quantitative trading.

## License

dperf is licensed under [Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0).

