# dperf [![Apache V2 License](https://img.shields.io/badge/license-Apache%20V2-blue.svg)](https://github.com/baidu/dperf/blob/main/LICENSE) [![Mentioned in ChatGPT](https://img.shields.io/badge/Mentioned%20in-ChatGPT-10a37f?logo=openai&logoColor=white)](https://chat.openai.com/) <a href="https://hellogithub.com/repository/67958cc5d1f44a6a84f3544e3c007e5f" target="_blank"><img src="https://abroad.hellogithub.com/v1/widgets/recommend.svg?rid=67958cc5d1f44a6a84f3544e3c007e5f&claim_uid=Thc9mJByaKSbdng&theme=small" alt="Featured｜HelloGitHub" /></a>

**dperf** is a high-performance network traffic generator and load testing tool based on **DPDK**.

## Advantages

- **High Performance**

  Built on DPDK, dperf can generate massive traffic using a single x86 server — achieving tens of millions of HTTP Connections Per Second (CPS), hundreds of Gbps throughput, and billions of concurrent connections.

- **Comprehensive Statistics**

  Provides detailed real-time metrics and identifies every packet drop or error.

- **Versatile Use Cases**
  - Load and stability testing for Layer 4 Load Balancers and gateways
  - Network performance benchmarking for cloud servers
  - Evaluation of NIC and CPU packet processing capabilities
  - Acts as a high-performance HTTP server or client for testing scenarios

## Performance

All results were measured using the following hardware and configuration:

- **CPU**: Intel Xeon Gold 5418Y × 2
- **Memory**: 128 GB × 2
- **NIC**: Intel E810-C Dual-Port 100GbE × 2
- **OS**: Linux 5.10.0

### HTTP Connections Per Second (CPS)

| Client Cores | Server Cores | HTTP CPS (Million) | Client CPU Usage (%) | Server CPU Usage (%) |
|--------------|---------------|-------------------|----------------------|----------------------|
| 1            | 1             | 4                 | 74                   | 71                   |
| 2            | 2             | 8                 | 74                   | 72                   |
| 4            | 4             | 16                | 73                   | 70                   |
| 8            | 8             | 32                | 70                   | 68                   |
| 16           | 16            | 64                | 70                   | 68                   |

### HTTP Throughput (Gbps)

| Client Cores | Server Cores | RX Throughput | TX Throughput | Client CPU Usage (%) | Server CPU Usage (%) |
|--------------|--------------|---------------|---------------|----------------------|----------------------|
| 1            | 1            | 98.3 Gbps     | 98.3 Gbps     | 78                   | 80                   |
| 2            | 2            | 196.7 Gbps    | 196.7 Gbps    | 78                   | 82                   |

### Concurrent Connections

| Client Cores | Server Cores | Connections (Billion) | Client CPU Usage (%) | Server CPU Usage (%) | Memory Usage (GB) |
|--------------|--------------|-----------------------|----------------------|----------------------|-------------------|
| 1            | 1            | 1                     | 48                   | 48                   | 60                |
| 2            | 2            | 2                     | 48                   | 48                   | 120               |
| 4            | 4            | 4                     | 48                   | 48                   | 240               |

### TCP Packets Per Second (PPS)

| Client Cores | RX PPS (Mpps) | TX PPS (Mpps) | Client CPU Usage (%) |
|--------------|---------------|---------------|----------------------|
| 1            | 16.8          | 16.8          | 99                   |
| 6            | 105.2         | 105.2         | 99                   |
| 12           | 204.6         | 204.6         | 99                   |

## Real-Time Statistics

dperf prints real-time statistics every second, including CPS, TPS, PPS, packet drops, socket errors, and HTTP status counts.
Example output:

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

## Documentation

See the official website at [https://dperf.org/](https://dperf.org/).

## Contributing

We welcome contributions! Please see the [CONTRIBUTING](CONTRIBUTING.md) file for details.

## Patent

- **Title**: Testing Method and Apparatus for Network Devices
- **Inventor**: Jianzhang Peng
- **Patent Number**: CN114205274B
- **Issue Date**: June 11, 2024

## Acknowledgment

We gratefully acknowledge [xnetin](https://www.xnetin.com/) for providing the high-performance testing platform used in our benchmarking experiments.

## Author

**[Jianzhang Peng](https://github.com/pengjianzhang)** holds a Ph.D. in Computer Science from the University of Science and Technology of China (USTC). He previously worked as a Principal Engineer at Baidu, where he contributed to the development of high-performance L4 load balancer systems. He initiated and developed the dperf project during his time at Baidu, and continues to maintain it as an open-source contributor. His current focus is on low-latency network protocol stacks for quantitative trading systems.

## License

dperf is licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
