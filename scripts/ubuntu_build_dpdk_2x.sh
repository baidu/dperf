#!/bin/bash

DPDK_VERSION=22.11.1
if [ $# -eq 1 ]; then
    DPDK_VERSION=$1
fi

HOME_DIR=/root
DPDK_XZ=dpdk-${DPDK_VERSION}.tar.xz
DPDK_TAR=dpdk-${DPDK_VERSION}.tar
DPDK_DIR=dpdk-stable-${DPDK_VERSION}

function install_re2c()
{
    cd $HOME_DIR
    wget https://github.com/skvadrik/re2c/releases/download/3.0/re2c-3.0.tar.xz
    tar -xvf re2c-3.0.tar.xz
    cd re2c-3.0
    ./configure
    make
    make install
}

function install_meson
{
    pip3 install meson ninja
}

function install_kernel_dev()
{
    VERSION=`uname -r`
    apt install kernel-devel-$VERSION -y
}

function install_dpdk()
{
    cd $HOME_DIR
    #wget http://fast.dpdk.org/rel/$DPDK_XZ
    tar -xvf $DPDK_XZ
    cd $DPDK_DIR

    # -Dbuildtype=debug
    OPT_LIBS=""
    expr match "${DPDK_VERSION}" "22.11." >/dev/null
    if [ $? -eq 0 ]; then
        OPT_LIBS="-Ddisable_libs=\"\""
    fi
    meson build --prefix=$HOME_DIR/$DPDK_DIR/mydpdk -Dbuildtype=debug -Dexamples=ALL -Denable_kmods=true $OPT_LIBS

    ninja -C build install
    /bin/sh /root/dpdk-stable-22.11.1/config/../buildtools/symlink-drivers-solibs.sh lib/x86_64-linux-gnu dpdk/pmds-21.0
    ldconfig
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
    export PKG_CONFIG_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/pkgconfig
    export LD_LIBRARY_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/
    make
}


pip3 install launchpadlib pyelftools
pip3 install --upgrade pip distlib setuptools
pip3 install meson ninja
apt install build-essential git pkg-config  libelf-dev


#install_kernel_dev
install_meson
#install_re2c
install_dpdk
install_igb_uio
install_dperf
