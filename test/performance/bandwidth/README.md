# Using dperf to test network bandwidth
## Introduction

DPDK(https://github.com/DPDK/dpdk) is a set of libraries and drivers for fast packet processing.
It supports many processor architectures and both FreeBSD and Linux.

dperf(https://github.com/baidu/dperf) is a DPDK based 100Gbps network performance and load testing software.

*Maybe dpdk-ans/f-stack/flexTOE are all open source high performant network frameworks based on DPDK and can be used to bw test, but they are not used by us because they do not support ARM or the supported dpdk version is too old.*

**Practice has proved that dperf can support the latest dpdk version, and support cx5/cx6 NIC, and can be compiled and run successfully on ARM.**

## Compile DPDK

1. install rdma-core driver and MLNX OFED

    (1) git clone https://github.com/linux-rdma/rdma-core

    (2) cd rdma-core/

    (3) bash build.sh

    (4) export RDMA_CORE_BUILD_DIR=/root/rdma-core/build

    (5) export C_INCLUDE_PATH=$RDMA_CORE_BUILD_DIR/include

    (6) export LIBRARY_PATH=$RDMA_CORE_BUILD_DIR/lib

    (7) export LD_LIBRARY_PATH=$RDMA_CORE_BUILD_DIR/lib

    (8) ./mlnxofedinstall  --upstream-libs --dpdk --with-mft --with-mstflint --force(refer to https://docs.nvidia.com/networking/display/MLNXOFEDv531001/Downloading+Mellanox+OFED)


2. apt install doca-tools
    
    note: This step is necessary, otherwise the dpdk version after v20(v19.11 not require it) will occur "rxp_compile.h not found".). 
    You can run "rxpbench" to check whether the installation is successful. If there are dependency errors in apt installation, we recommend using aptitude.


3. git clone https://github.com/DPDK/dpdk(version:22.07-rc2)(v22.03,v21.11,v20.11 are all ok).

4. meason build

5. cd build

6. ninja -j [THREADS NUM]
("export LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/:$LD_LIBRARY_PATH"/"export LD_LIBRARY_PATH=/usr/local/lib/aarch64-linux-gnu/:$LD_LIBRARY_PATH"/"export LD_LIBRARY_PATH=/root/rdma-core/build/lib/:$LD_LIBRARY_PATH" may be useful)

7. ninja install

8. config hugepage: "echo 4096 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages"

    Only the configuration of 2M page is shown here, and the rest is for reference http://doc.dpdk.org/spp/setup/getting_started.html.

9. run helloworld

    (1) cd /root/dpdk/examples/helloworld

    (2) make

    (3) ./build/helloworld("Probe PCI driver" and "hello from core" will be shown.)

## Compile dperf

1. git clone https://github.com/baidu/dperf/

2. cd dperf/

3. export PKG_CONFIG_PATH=/usr/local/lib/aarch64-linux-gnu/pkgconfig

4. make

5. run "./build/dperf" to check whether the installation is successful

## Configuration for bw test

1. Specific configuration information reference https://github.com/baidu/dperf/blob/main/docs/configuration.md

2. System setup

    (1) Client and server are connected through 100Gb bluefield-2 integrated connectx-6.

    (2) "nic_setip.sh" will help you to setup a series of ip addresses quickly.(bash nic_setip.sh [START IP] [the NUMBERS of IP ADDRESSES] [INTERFACE])

    (3) The NIC in client side is configured with an IP address within the range of 192.168.31.50-192.168.31.57.

    (4) The NIC in server side is configured with an IP address within the range of 192.168.31.200-192.168.31.207.

3. The configuration file for running dperf with 8 CPUs is placed "dperf-config".
    
    (1) Modify the number of CPU cores used by dperf by modifying the "CPU" and "server" configuration items.

    (2) Increasing "cc" gradually may help you improve bandwidth.

    (3) "dperf-config/client-template-bw.conf" and "dperf-config/server-template-bw.conf" provide templates for testing bandwidth.

## Authors
Hua Zhang hua.zhang.2106108@gmail.com 

