# dperf Build Tips

## Build dperf With DPDK-17/18/19
### Build DPDK
    #Suppose we use dpdk-19.11.10.

    TARGET=x86_64-native-linuxapp-gcc or arm64-armv8a-linuxapp-gcc
    cd /root/dpdk/dpdk-stable-19.11.10
    make install T=$TARGET -j16

### Build dperf
    cd dperf
    make -j8 RTE_SDK=/root/dpdk/dpdk-stable-19.11.10 RTE_TARGET=$TARGET

### Run dperf
    ./build/dperf -h

## Build dperf With DPDK-20
### Build DPDK
    #Suppose we use dpdk-20.11.2.
    #Build dpdk-20.11.2
    ./scripts/build_dpdk_2x.sh 20.11.2

    cd /root/dpdk/dpdk-stable-20.11.2
    meson build --prefix=/root/dpdk/dpdk-stable-20.11.2/mydpdk -Denable_kmods=true
    ninja -C build install

### Build dperf
    export PKG_CONFIG_PATH=/root/dpdk/dpdk-stable-20.11.2/mydpdk/lib64/pkgconfig/
    cd dperf
    make

### Run dperf
    export LD_LIBRARY_PATH=/root/dpdk/dpdk-stable-20.11.2/mydpdk/lib64/
    ./build/dperf -h

## Build dperf With DPDK-22.11
### Build DPDK
    #Suppose we use dpdk-22.11.1.
    #Build dpdk-22.11.1
    ./scripts/build_dpdk_2x.sh 22.11.1

    cd /root/dpdk/dpdk-stable-22.11.1
    meson build --prefix=/root/dpdk/dpdk-stable-22.11.1/mydpdk -Denable_kmods=true -Ddisable_libs=""
    ninja -C build install

### Build dperf
    export PKG_CONFIG_PATH=/root/dpdk/dpdk-stable-22.11.1/mydpdk/lib64/pkgconfig/
    cd dperf
    make

### Run dperf
    export LD_LIBRARY_PATH=/root/dpdk/dpdk-stable-22.11.1/mydpdk/lib64/
    ./build/dperf -h

## Mellanox Interface Driver
- CentOS/RHEL 7.5, DPDK-17.11: MLNX_OFED_LINUX-4.3-3.0.2.1-rhel7.5-x86_64.iso
- CentOS/RHEL 7.9 or Kernel >= 4.14: rdma-core-stable-v22
