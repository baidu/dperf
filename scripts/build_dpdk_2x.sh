#!/bin/sh

DPDK_VERSION=22.11.1
if [ $# -eq 1 ]; then
    DPDK_VERSION=$1
fi

HOME_DIR=/root/dpdk/
DPDK_XZ=dpdk-${DPDK_VERSION}.tar.xz
DPDK_TAR=dpdk-${DPDK_VERSION}.tar
DPDK_DIR=dpdk-stable-${DPDK_VERSION}

function install_re2c()
{
    cd $HOME_DIR
    wget https://github.com/skvadrik/re2c/releases/download/1.0.3/re2c-1.0.3.tar.gz
    tar -zxvf re2c-1.0.3.tar.gz
    cd re2c-1.0.3/
    ./configure
    make
    make install
}

function install_ninja()
{
    cd $HOME_DIR
    wget https://github.com/ninja-build/ninja/archive/refs/tags/v1.11.0.tar.gz
    tar -zxvf v1.11.0.tar.gz
    cd ninja-1.11.0/
    ./configure.py --bootstrap
    cp ninja /usr/bin/
}

function install_meson
{
    pip3 install meson
}

function install_kernel_dev()
{
    VERSION=`uname -r`
    yum install kernel-devel-$VERSION -y
}

function install_dpdk()
{
    cd $HOME_DIR
    wget http://fast.dpdk.org/rel/$DPDK_XZ
    xz -d $DPDK_XZ
    tar -xf $DPDK_TAR
    cd $DPDK_DIR

    # -Dbuildtype=debug
    OPT_LIBS=""
    expr match "${DPDK_VERSION}" "22.11." >/dev/null
    if [ $? -eq 0 ]; then
        OPT_LIBS="-Ddisable_libs=\"\""
    fi
    meson build --prefix=$HOME_DIR/$DPDK_DIR/mydpdk -Denable_kmods=true $OPT_LIBS

    ninja -C build install
}

function install_igb_uio()
{
    cd $HOME_DIR
    git clone http://dpdk.org/git/dpdk-kmods
    cd dpdk-kmods/linux/igb_uio/
    make
}

function install_dperf()
{
    cd $HOME_DIR
    git clone https://github.com/baidu/dperf.git
    cd dperf
    export PKG_CONFIG_PATH=$HOME_DIR/$DPDK_DIR/mydpdk/lib64/pkgconfig
    make
}

mkdir -p $HOME_DIR

yum install gcc gcc-c++ make numactl-devel git
pip3 install pyelftools

install_kernel_dev
install_meson
install_re2c
install_ninja
install_dpdk
install_igb_uio
#install_dperf
