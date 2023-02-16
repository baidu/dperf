

# 1. 项目简介

| 项目地址 | https://github.com/baidu/dperf                                                |
|------|-------------------------------------------------------------------------------|
| 实现原理 | https://github.com/baidu/dperf/blob/main/docs/design-CN.md                    |
| 参考链接 | [How to set up baidu dperf](https://metonymical.hatenablog.com/entry/2022/02/11/234927)                                                   |
|      | [dperf 快速上手](https://zhuanlan.zhihu.com/p/451340043)                                                           |
|      | [releases notes](https://github.com/baidu/dperf/releases)                                                             |
|      | [dperf FAQ](https://zhuanlan.zhihu.com/p/561093951)                                                                     |
|      | [作者的知乎文章列表](https://www.zhihu.com/people/artnowben/posts?page=1)                                                                     |
|      | [DPVS v1.9.2 Performance Tests](https://github.com/iqiyi/dpvs/blob/master/test/release/v1.9.2/performance.md)                                                 |
|      | https://doc.dpdk.org/guides/linux_gsg/sys_reqs.html#compilation-of-the-dpdk   |
|      | https://doc.dpdk.org/guides-22.11/linux_gsg/nic_perf_intel_platform.html      |
|      | http://doc.dpdk.org/guides/nics/overview.html                                 |
|      | https://en.wikipedia.org/wiki/Data-rate_units                                 |
|      | [RFC 3511](https://www.ietf.org/rfc/rfc3511.txt)                                                                      |
|      | [RFC 6349 - TCP 并发性能基准测试方法](https://www.rfc-editor.org/rfc/rfc6349)                                                     |
|      | [RFC 2544 - 以太网性能基准测试方法](https://www.rfc-editor.org/rfc/rfc2544)                                                        |
|      | [RFC 5180 - 多路复用和负载平衡性能基准测试方法](https://www.rfc-editor.org/rfc/rfc5180)                                                  |
| 作者邮箱 | pengjianzhang@gmail.com                                                       |

本文从新手小白的角度，记录了dperf从开始到放弃的过程

## 名词介绍
| Label        | Meaning                 | Explanation                                                                                                                                                                                                                                                                                                                              |
|--------------|-------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| CPS          | connections per second  | The number of L4 connections established per second.                                                                                                                                                                                                                                                                                     |
| TPS          | transactions per second | The number of L5-L7 transactions completed per second.                                                                                                                                                                                                                                                                                   |
| RPS          | requests per second     | The number of L5-L7 requests satisfied per second, often interchangeable with TPS.                                                                                                                                                                                                                                                       |
| RPC          | requests per connection | The number of L5-L7 requests completed for a single L4 connection. Example: HTTP/1.1 with 100 transactions per TCP connection would be 100-RPC.                                                                                                                                                                                          |
| bps or bit/s | bits per second         | The network bandwidth in bits per second. Some people use GB/s, Gb/s and Gbps interchangeably, but the unit is decidedly important. 1 GB/s (gigabyte per second) is 8 Gbps (gigabits per second) See: data-rate units. Things get a bit pedantic with GiB/s vs GB/s, but 8:1 bytes:bits is correct when working from the same base unit. |
| fps          | frames per second       | A measure of L2 frames per second, often used for core switching or firewalls. The distinction is that not all network frames contain valid data packets or datagrams.                                                                                                                                                                   |
| pps          | packets per second      | A measure of the L3/L4 packets per second, typically for TCP and UDP traffic.                                                                                                                                                                                                                                                            |
| CC           | concurrent connections  | The number of concurrently established L4 connections. Example: if you have 100 CPS, and each connection has a lifetime of 100 s, then the resultant CC is 10,000 (connection rate * connection lifetime).                                                                                                                               |

| Label     | Meaning                                       | Methodology                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
|-----------|-----------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| CPS       | connections per second                        | Determine the maximum number of fully functional (establishment, single small transaction, graceful termination) connections that can be established per second. Each transaction is a small object such that it will not result in a full size network frame.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| TPS / RPS | transactions per second / requests per second | Determine the maximum number of complete transactions that can be completed over a small number of long-lived connections. Each transaction requests a small object such that it will not result in a full size network frame.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| bps       | bits per second                               | Determine the maximum sustained throughput for a workload using long-lived connections and large (128 kB or higher) object sizes. This is often expressed for L4 and L7, and the numbers are typically different.Note that ADC industry metrics use a "single count" method for measuring throughput, such that traffic between the client and server is counted, but not the traffic between the client and ADC or between the ADC and the server.                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| fps       | frames per second                             | Determine the maximum rate at which network frames can be inspected, routed, or mitigated without unintended drops. This is particularly useful when comparing network defense performance, such as TCP SYN flood mitigation.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| CC        | concurrent connections                        | Determine the maximum number of concurrent connections that can be established without forcefully terminating any of the connections. This is typically tested by establishing millions of connections over a long period of time, and leaving them in an established state. The test is valid only if the connections can successfully complete one transaction before a graceful termination. Stability is achieved when the new connection rate matches the connection termination rate due to old connections being satisfied.Example: 10,000 CPS with a 300 second wait time and 1-RPC for a 128 byte object will result in a moving plateau of 3.0M CC. The test should run for a minimum of 300 seconds after the initial connections have been established. These tests take a long time, as there are three phases - establishment (300 seconds), wait and transaction (300 seconds), graceful shutdown (300 seconds). |


# 2. 环境说明
| 项目         | 备注               |
|------------|------------------|
| 操作系统       | Ubuntu 18.04     |
| 内存         | 总内存>=3GB，设置大页2GB |
| 网口         | 2个，dperf绑定2个网口   |
| CPU核数（线程数） | >=2              |
| 数据面网卡PCI号  |                  |

```
root@server:~# cat /proc/cpuinfo | grep 'model name' |uniq
model name      : Intel(R) Core(TM) i7-6700 CPU @ 3.40GHz
root@server:~# cat /proc/cpuinfo | grep 'cpu cores' |uniq
cpu cores       : 4
root@server:~#
root@server:~# cat /proc/meminfo | grep MemTotal
MemTotal:       16286624 kB
root@server:~#
```
# 3. dperf能干什么
测新建与并发数
dperf是一种基于DPDK的100Gbps网络性能和负载测试软件。
dperf是一款基于DPDK的高性能HTTP负载测试工具
特别适用于ThroughPut（吞吐量）、
CPS（Connection per seconds）、
CC（Concurrent Connection）负载测试。

# 4. 系统设置前提
Ubuntu 18.04 最小安装，设置网卡名，允许root用户登录ssh 桌面
内核版本 >= 4.4
glibc >= 2.7
这个ubuntu 1804已经满足要求了,下面几项根据需要修改
## 4.1 修改网卡名
```
修改ubuntu 网卡名为eth0 
1.修改grub文件
vim /etc/default/grub
查找
GRUB_CMDLINE_LINUX=""
修改为
GRUB_CMDLINE_LINUX="net.ifnames=0 biosdevname=0"
2.重新生成grub引导配置文件
grub-mkconfig -o /boot/grub/grub.cfg
```
## 4.2 允许root用户ssh登录
```
先为root账户设置密码
sudo passwd root
ubuntu@ubuntu-virtual-machine:~$ sudo passwd root
输入新的 UNIX 密码：
重新输入新的 UNIX 密码：
passwd：已成功更新密码
ubuntu@ubuntu-virtual-machine:~$ su root
密码：
root@ubuntu-virtual-machine:/home/ubuntu#

sudo vim /etc/ssh/sshd_config
按i进入编辑状态，并在前加“#”注释掉“PermitRootLogin without-password”
然后加入PermitRootLogin yes
sudo service ssh restart
```
## 4.3 ubuntu 修改hostname
```
vi /etc/hostname
vi /etc/host
```
## 4.4 允许root用户登录桌面
```
4.4.1
vim /usr/share/lightdm/lightdm.conf.d/50-ubuntu.conf
添加一句
greeter-show-manual-login=true
4.4.2
vim /etc/pam.d/gdm-autologin 
注释该行
#auth   required        pam_succeed_if.so user != root quiet_success
4.4.3
vim /etc/pam.d/gdm-password
注释该行
#auth   required        pam_succeed_if.so user != root quiet_success
4.4.4
 vim /root/.profile
 ```
 ```
 #~/.profile: executed by Bourne-compatible login shells.
if [ “$BASH” ]; then
if [ -f ~/.bashrc ]; then
. ~/.bashrc
fi
fi
tty -s && mesg n || true
#mesg n || true
重启系统 
```
## 4.5 NUMA
物理网卡和与 DPDK 接口相连虚拟机都有对应的 NUMA node,
目前dperf只支持在不同的NUMA node上用不同的网卡，
如果一台物理机只有一个NUMA node, 但有俩网卡这种情况dperf可能是不支持的
（这里准确的讲作者表示dperf是没有NUMA的限制，但未经测试）
```
root@server:~# numactl --hardware
available: 1 nodes (0)
node 0 cpus: 0 1 2 3 4 5 6 7
node 0 size: 15904 MB
node 0 free: 333 MB
node distances:
node   0
  0:  10
root@server:~#
root@server:~# numactl -H
available: 1 nodes (0)
node 0 cpus: 0 1 2 3 4 5 6 7
node 0 size: 15904 MB
node 0 free: 10275 MB
node distances:
node   0
  0:  10
root@server:~#
root@server:~# ls /sys/devices/system/node/
has_cpu  has_memory  has_normal_memory  node0  online  possible  power  uevent
root@server:~#
node0下保存着相关的信息
root@server:~# ls /sys/devices/system/node/node0



root@server:~# dmidecode -t memory | grep Locator
        Locator: DIMM1
        Bank Locator: Not Specified
        Locator: DIMM2
        Bank Locator: Not Specified
        Locator: DIMM3
        Bank Locator: Not Specified
        Locator: DIMM4
        Bank Locator: Not Specified
root@server:~# dmidecode -t memory | grep Speed
        Speed: 2133 MT/s
        Configured Clock Speed: 2133 MT/s
        Speed: 2133 MT/s
        Configured Clock Speed: 2133 MT/s
        Speed: Unknown
        Configured Clock Speed: Unknown
        Speed: Unknown
        Configured Clock Speed: Unknown
root@server:~# lspci -s 05:00.0 -vv | grep LnkSta
pcilib: sysfs_read_vpd: read failed: Input/output error
                LnkSta: Speed 5GT/s, Width x4, TrErr- Train- SlotClk+ DLActive- BWMgmt- ABWMgmt-
                LnkSta2: Current De-emphasis Level: -6dB, EqualizationComplete-, EqualizationPhase1-
root@server:~# 
```
# 5. 设置大页内存
dperf运行之前需要配置大页
大页内存相关知识点较多，另起文档跟踪Linux 标准大页和透明大页 

官方设置参考文档： http://dpdk-guide.gitlab.io/dpdk-guide/setup/hugepages.html

```
root@dperf:~# free -g
              总计         已用        空闲      共享    缓冲/缓存    可用
内存：           3           0           0        0         2         2
交换：           1           0           1
root@dperf:~#
通过"free -g"命令查看系统有多少大页，从上面可以看到系统有3G内存。
推荐设置的大页数为系统内存的一半，如果dperf报告内存不够，再增加大页即可。

注意大多数 x86_64 系统支持各种大小的大页面。
通过运行以下命令进行检查系统是否支持1GB大页面
root@server:~# if grep pdpe1gb /proc/cpuinfo >/dev/null 2>&1; then echo "1GB supported."; fi
1GB supported.
root@server:~#

作者表示：如果你的系统支持1G，最好配置为1G大页
```

## 5.1 2M 大页面设置
```
DPDK 应用程序通常需要大页面支持，出于加速目的，这应该足以让它运行
Fedora 和 RHEL 默认为 2M 大页面：
sysctl -w vm.nr_hugepages=204800

对于现实世界的应用程序，您希望使此类分配永久化。您可以通过将其添加到目录/etc/sysctl.conf或目录中的单独文件来执行此操作/etc/sysctl.d/：
echo 'vm.nr_hugepages=2048' > /etc/sysctl.d/hugepages.conf

如果您尝试运行 Pktgen 的主机具有 NUMA，即节点 0 和节点 1，
则必须按照DPDK 入门指南中所述在两个 NUMA 节点上配置大页面，即
echo 2048 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages 
echo 2048 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
```

## 5.2 1G 大页面设置
```
#vi /boot/grub/grub.cfg
#default_hugepagesz = 1 GB  hugepagesz = 1 G  hugepages = 4
#linux16 /vmlinuz-xxx ...   transparent_hugepage=never default_hugepagesz=1G hugepagesz=1G hugepages=4
#然后执行 重新生成 grub.cfg
#sudo grub-mkconfig -o /boot/grub/grub.cfg 

//要将其永久添加到内核命令行，
//请将其附加到 /etc/default/grub 中的 GRUB_CMDLINE_LINUX，
vi /etc/default/grub
// 添加如下内容
GRUB_CMDLINE_LINUX="net.ifnames=0 biosdevname=0 default_hugepagesz=1GB hugepagesz=1G hugepages=4"
上面例子：设置每个大页1G，设置4个大页。
// 更新 grub 配置
sudo update-grub
// 设置后重启生效 
reboot
#grub-mkconfig是liunx通用的用来配置grub.cfg 的命令，
#update-grub是ubuntu特有的用来配置grub.cfg 的命令。
#root@server:~/dperf# cat /usr/sbin/update-grub
##!/bin/sh
#set -e
#exec grub-mkconfig -o /boot/grub/grub.cfg "$@"
#root@server:~/dperf#
#update-grub实际上也是调用的grub-mkconfig 
#一般用update-grub就可以了 
```
## 5.3 vmware 虚拟机环境
```
vmware虚拟机环境
vmware虚拟机环境在这是大页时，需要额外增加一个参数"nopku"。
linux16 /vmlinuz-3.10.0-957.el7.x86_64 root=/dev/mapper/centos-root ro crashkernel=auto rd.lvm.lv=centos/root rd.lvm.lv=centos/swap rhgb quiet LANG=en_US.UTF-8 nopku transparent_hugepage=never default_hugepagesz=1G hugepagesz=1G hugepages=2

nopku transparent_hugepage=never default_hugepagesz=1G hugepagesz=1G hugepages=2

default_hugepagesz：在内核中定义了开机启动时分配的大页面的默认大小。
hugepagesz： 在内核中定义了开机启动时分配的大页面的大小。
             可选值为 2MB 和 1GB 。默认是 2MB 。
hugepages ：在内核中定义了开机启动时就分配的永久大页面的数量。默认为 0，即不分配。
   只有当系统有足够的连续可用页时，分配才会成功。由该参数保留的页不能用于其他用途。
```


## 5.4 查看配置的大页
```
root@server:~/dperf# cat /proc/meminfo | grep Huge
AnonHugePages:         0 kB
ShmemHugePages:        0 kB
HugePages_Total:     512
HugePages_Free:       80
HugePages_Rsvd:       61
HugePages_Surp:        0
Hugepagesize:       2048 kB
root@server:~/dperf#

如果你的系统正确配置了大页面支持，
你应该能够执行以下命令并看到非零值，对应于你的大页面配置
root@server:~# grep HugePages_ /proc/meminfo
HugePages_Total:     512
HugePages_Free:      509
HugePages_Rsvd:       61
HugePages_Surp:        0
root@server:~#
```
## 5.5 大页相关名次解释
| HugePages_Total                    | 是大页面池的大小                                                                                                                                                                                           |
|------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| HugePages_Free                     | 是池中尚未分配的大页面数。                                                                                                                                                                                      |
| HugePages_Rsvd                     | 是“reserved”的缩写，是已承诺从池中分配但尚未分配的大页面的数量<br>预留大页面保证应用程序能够在故障时从大页面池中分配一个大页面。                                                                                                                                                             |
| HugePages_Surp                     |  是“surplus”的缩写，是池中大于/proc/sys/vm/nr_hugepages. 剩余巨页的最大数量由 控制 /proc/sys/vm/nr_overcommit_hugepages。注意：当启用释放与每个hugetlb page关联的未使用的vmemmap pages特性时，当系统处于内存压力时，剩余huge pages的数量可能会暂时大于最大剩余huge pages数量。                                                                                                                                                                   |
| Hugepagesize                       | is the default hugepage size (in kB).                                                                                                                                                              |
| Hugetlb                            | 是所有大小的大页面消耗的内存总量（以 kB 为单位）。如果使用不同大小的大页，这个数字会超过HugePages_Total * Hugepagesize。要获得更详细的信息，请参阅 /sys/kernel/mm/hugepages（如下所述）。                                                                         |
|  cat /proc/filesystems | grep huge | 显示内核中配置的“hugetlbfs”类型的文件系统。                                                                                                                                                                        |
|                                    | 指示内核大页面池中“持久”大页面的当前数量。                                                                                                                                                                             |
|  cat /proc/sys/vm/nr_hugepages     | 当任务释放时，“持久”大页面将返回到大页面池。具有 root 权限的用户可以通过增加或减少 的值来动态分配更多或释放一些持久性大页面nr_hugepages。                                                                                                                    |

# 6. 升级系统python

##6.1 设置软件更新选项
软件和更新下，勾选重要的安全更新和推荐更新

##6.2 安装python3.9 

```
安装python3.9
在系统上安装一些必需的软件包
apt install build-essential software-properties-common libnuma-dev

为系统配置Deadsnakes PPA
sudo add-apt-repository ppa:deadsnakes/ppa  回车确认
sudo apt-get install python3.9
sudo apt-get install -y python3.9-distutils 
安装python组件
sudo apt-get install python3.9-dev python3.9-venv python3.9-gdbm python3-pip python3-setuptools

设置默认使用python3.9
$which python3.9
/usr/bin/python3.9
$sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.6 1
$sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.9 2
$sudo update-alternatives --config python3

升级pip/setuptools
pip3 install launchpadlib pyelftools
pip3 install --upgrade pip distlib setuptools 

## 强制重新安装pip
##python3 -m pip install --upgrade --force-reinstall pip
#root@dperf:~# cd /usr/lib/python3/dist-packages/
#root@dperf:/usr/lib/python3/dist-packages# sudo ln -s apt_pkg.cpython-36m-x86_64-linux-gnu.so apt_pkg.so
#root@dperf:/usr/lib/python3/dist-packages# 

```


```
WARNING: Running pip as the 'root' user can result in broken permissions and conflicting behaviour with the system package manager. It is recommended to use a virtual environment instead: https://pip.pypa.io/warnings/venv
python的这个WARNING:可以照此方法解决
python3 -m venv tutorial-env
1.正常
不显示回显
2.不正常
 出错原因：无法创建虚拟环境，因为ensurpip不可用，需要安装python3-venv包
apt install python3.10-venv 
source tutorial-env/bin/activate
python -m pip install novas
pip install --upgrade pip

从pip22.1开始，您现在可以使用参数选择退出警告：
pip install --root-user-action=ignore
```

# 7. DPDK下载与安装编译
下载地址：https://core.dpdk.org/download/

官方手册：https://doc.dpdk.org/guides-22.11/

先决条件 ：https://doc.dpdk.org/guides/linux_gsg/sys_reqs.html#bios-setting-prerequisite-on-x86

编者digger注：编译dperf之前需要先编译安装DPDK，推荐使用7.4安装脚本安装DPDK及dperf

```
dpdk 安装先决条件
内核版本 >= 4.4
glibc >= 2.7

root@dperf:~# ldd --version
ldd (Ubuntu GLIBC 2.27-3ubuntu1) 2.27
Copyright (C) 2018 自由软件基金会。
这是一个自由软件；请见源代码的授权条款。本软件不含任何没有担保；甚至不保证适销性
或者适合某些特殊目的。
由 Roland McGrath 和 Ulrich Drepper 编写。
root@dperf:~# uname -a
Linux dperf 4.15.0-20-generic #21-Ubuntu SMP Tue Apr 24 06:16:15 UTC 2018 x86_64 x86_64 x86_64 GNU/Linux
root@dperf:~#
```

##7.1 下载dpdk
```
root@dperf:~# wget https://fast.dpdk.org/rel/dpdk-22.11.1.tar.xz
root@dperf:~# apt install build-essential git pkg-config  libelf-dev 
root@dperf:~# tar -xvf dpdk-22.11.1.tar.xz
root@dperf:~# cd dpdk-stable-22.11.1/

## dpdk 22.03 需要pyelftools
## pip3 install pyelftools --upgrade
##安装meson ninja
root@dperf:~# pip3 install meson ninja
#pip3会将meson软件安装到/home/user/.local/bin
#而通过apt install 安装是使用/usr/bin/meson 且版本比较低，不推荐使用apt install 安装
#所以需要通过修改path路径使得pip安装的meson优先于系统meson被搜索到
#export PATH=~/.local/bin:$PATH
```

## 7.2 编译
```
// 使用选项 -Dexamples 指定编译所有样例程序，在dpdk源码根目录执行
meson -Dbuildtype=debug -Dexamples=ALL -Denable_kmods=true ./aa ##必须指定目标路径
meson build --prefix=/root/dpdk-stable-22.11.1/mydpdk -Denable_kmods=true -Ddisable_libs=""
meson build --prefix=/root/dpdk-stable-22.11.1/mydpdk -Dbuildtype=debug -Dexamples=ALL -Denable_kmods=true -Ddisable_libs=""
log

Build targets in project: 931

DPDK 22.11.1

  User defined options
    prefix      : /root/dpdk-stable-22.11.1/mydpdk
    disable_libs:
    enable_kmods: true


在构建之后的目标文件中ninja install
root@dperf:~/dpdk-stable-22.11.1# cd aa
root@dperf:~/dpdk-stable-22.11.1/aa# ninja -C build install

/bin/sh /root/dpdk-stable-22.11.1/config/../buildtools/symlink-drivers-solibs.sh lib/x86_64-linux-gnu dpdk/pmds-21.0
ldconfig
```

## 7.3 验证dpdk安装

```
root@dperf:~# /root/dpdk-stable-22.11.1/build/examples/dpdk-helloworld -l 0-1 -n 2
EAL: Detected 2 lcore(s)
EAL: Detected 1 NUMA nodes
EAL: Detected static linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: No available hugepages reported in hugepages-1048576kB
EAL: Probing VFIO support...
EAL: No legacy callbacks, legacy socket not created
hello from core 1
hello from core 0
root@dperf:~#
```


## 7.4 快速安装脚本
使用dperf项目的 scripts/build_dpdk_2x.sh脚本可以一键安装DPDK-22.11以及DPDK-20.11、DPDK-21.11，但原脚本未适配ubuntu，下面的可以在ubuntu 18.04上验证通过
脚本包括dpdk下载编译及dperf下载编译等
```
#!/bin/bash
# root@dperf:~#  lsb_release -a
# No LSB modules are available.
# Distributor ID: Ubuntu
# Description:    Ubuntu 18.04.6 LTS
# Release:        18.04
# Codename:       bionic
# root@dperf:~# python3 -V
# Python 3.9.15
# root@dperf:~# 

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
    #######################################
    ##新增判断如果当前目录有dpdk安装包就不下载
     if [ ! -f "$DPDK_XZ" ]; then
     wget http://fast.dpdk.org/rel/$DPDK_XZ
     fi
    #######################################
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
    export PKG_CONFIG_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/pkgconfig/
    make
    echo "/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/" >> /etc/ld.so.conf
    ldconfig
    #export LD_LIBRARY_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/
}


pip3 install launchpadlib pyelftools
pip3 install --upgrade pip distlib setuptools
pip3 install meson ninja
apt install build-essential git pkg-config  libelf-dev -y

#install_kernel_dev
install_meson
#install_re2c
install_dpdk
install_igb_uio
install_dperf
```

# 8. dperf下载与安装编译
2022 年 12 月 14 日，dperf 发布了新版本 v1.4.0 新增支持 DPDK-22.11

```
#apt install git pkg-config  libelf-dev
git clone https://github.com/baidu/dperf.git

cd dperf
make
export PKG_CONFIG_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/pkgconfig/
export LD_LIBRARY_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/

#export PKG_CONFIG_PATH=/root/dpdk/dpdk-stable-22.11.1/aa/lib/x86_64-linux-gnu/pkgconfig
#export LD_LIBRARY_PATH=/root/dpdk/dpdk-stable-20.11.2/mydpdk/lib64/
#RTE_SDK=/root/dpdk-stable-22.11.1 RTE_TARGET=$TARGET
#TARGET=x86_64-native-linuxapp-gcc
#make -j8 
./build/dperf -h #查看帮助信息
root@dperf:~/dperf# ./build/dperf -h                                            -h --help
-v --version
-t --test       Test configure file and exit
-c --conf file  Run with conf file
-m --manual     Show manual
root@dperf:~/dperf# ./build/dperf -v
1.4.0
root@dperf:~/dperf#
#ln -s /usr/bin/python3 /usr/bin/python
```

```
root@dperf:~/dperf# history
pip3 install launchpadlib pyelftools
pip3 install --upgrade pip distlib setuptools
pip3 install meson ninja
apt install build-essential git pkg-config  libelf-dev
tar -xvf dpdk-22.11.1.tar.xz
cd dpdk-stable-22.11.1/
meson build --prefix=/root/dpdk-stable-22.11.1/mydpdk -Denable_kmods=true -Ddisable_libs=""
ninja -C build install
cd mydpdk/lib/x86_64-linux-gnu/pkgconfig/
cd
git clone https://github.com/baidu/dperf.git
cd dperf
##LD_LIBRARY_PATH是临时的，写入ld.so.conf是永久的
##echo "/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/" >> /etc/ld.so.conf
##ldconfig 后生效
export PKG_CONFIG_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/pkgconfig/
make
export LD_LIBRARY_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/
./build/dperf -h ##查看dperf帮助信息
./build/dperf -v ##查看dperf版本信息
```

## 8.1 如何验证dperf成功启动
配置dperf用server模式运行，此时dperf不会主动发报文，我们可以
1. ping dperf的接口地址（'port'配置项的地址），判断网络的连通性
2. ping dperf的监听地址（'server'配置项的地址），判断网络的连通性
3. curl dperf的监听地址+监听端口
注意：dperf服务器只会响应'client'中的地址发出的TCP/UDP请求，在使用curl做测试时，需要把客户端的IP地址加入配置文件，dperf作为服务器运行时，可以配置多条'client'


# 9. DPDK绑定网卡
https://git.dpdk.org/dpdk-kmods/tree/windows/netuio/README.rst

##9.1 为什么要绑定网卡？
DPDK让用户态程序可以直接控制网卡收发报文，以达到极高的网络处理性能。这意味着，我们要把网卡从操作系统摘除，绑定用户态驱动。结果是，这些网卡从操作系统'消失'，ssh不能使用这个网卡。
管理面网卡与数据面网卡
运行DPDK程序的网卡称为数据面网卡，用于ssh登录等管理用途的网卡称为管理面网卡。使用dperf的服务器上必须要预留一张网卡为管理面网卡。

https://doc.dpdk.org/guides/linux_gsg/linux_drivers.html

## 9.2 查看dpdk是否支持该网卡
```
#查询网卡的devid号
lspci -nn | grep Ethernet
02:01.0 Ethernet controller [0200]: Intel Corporation 82545EM Gigabit Ethernet Controller (Copper) [8086:100f] (rev 01)
02:06.0 Ethernet controller [0200]: Intel Corporation 82545EM Gigabit Ethernet Controller (Copper) [8086:100f] (rev 01)

#在dpdk代码中搜索此devid号，
grep --include=*.h -rn -e '100f'

dpdk-stable-20.11.1/drivers/net/bnx2x/ecore_reg.h:1274:#define NIG_REG_BRB0_OUT_EN                                         0x100f8
dpdk-stable-20.11.1/drivers/net/bnx2x/ecore_reg.h:2112:#define NIG_REG_XCM0_OUT_EN                                         0x100f0
dpdk-stable-20.11.1/drivers/net/bnx2x/ecore_reg.h:2114:#define NIG_REG_XCM1_OUT_EN                                         0x100f4
dpdk-stable-20.11.1/drivers/net/bnx2x/ecore_hsi.h:2669:        #define SHMEM_AFEX_VERSION_MASK                  0x100f
dpdk-stable-20.11.1/drivers/common/sfc_efx/base/efx_regs_mcdi.h:349:#define        MC_CMD_ERR_NO_MAC_ADDR 0x100f
是Intel支持的一种虚拟网卡，可以绑定的
```
## 9.3 DPDP 支持网卡列表  
https://core.dpdk.org/supported/nics/

| AMD           | - axgbe (AMD EPYC™ Embedded 3000 family)                                                                                                                                                                                                                                                                                                                                              |
|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| AMD PENSANDO  | - ionic (DSC-25, DSC-100, DSC-200)                                                                                                                                                                                                                                                                                                                                                    |
| AMAZON        | - ena (Elastic Network Adapter)                                                                                                                                                                                                                                                                                                                                                       |
| AQUANTIA      | - atlantic (AQC100, AQC107, AQC108, AQC109)                                                                                                                                                                                                                                                                                                                                           |
| ATOMIC RULES  | - ark (Arkville Packet Conduit FX0/FX1)                                                                                                                                                                                                                                                                                                                                               |
| BROADCOM      | - bnxt (NetXtreme-C, NetXtreme-E, NetXtreme-S, StrataGX)                                                                                                                                                                                                                                                                                                                              |
| CHELSIO       | - cxgbe (Terminator 5, Terminator 6)                                                                                                                                                                                                                                                                                                                                                  |
| CISCO         | - enic (UCS Virtual Interface Card)                                                                                                                                                                                                                                                                                                                                                   |
| Google        | - gve (Google Virtual Ethernet)                                                                                                                                                                                                                                                                                                                                                       |
| HISILICON     | - hns3 (Kunpeng 920, Kunpeng 920s, Kunpeng 930)                                                                                                                                                                                                                                                                                                                                       |
| Huawei        | - hinic (IN200)                                                                                                                                                                                                                                                                                                                                                                       |
| Intel         | - e1000 (82540, 82545, 82546)<br>- e1000e (82571, 82572, 82573, 82574, 82583, ICH8, ICH9, ICH10, PCH, PCH2, I217, I218, I219)<br>- igb (82573, 82576, 82580, I210, I211, I350, I354, DH89xx)<br> -igc (I225, I226)<br>- ixgbe (82598, 82599, X520, X540, X550)<br> - i40e (X710, XL710, X722, XXV710)<br> - ice (E810, E822, E823)<br>- idpf (IPU E2100)<br>- fm10k (FM10420)<br>- ipn3ke (PAC N3000)<br>- ifc (IFC)|
| MARVELL       | - bnx2x (QLogic 578xx)<br>- liquidio (LiquidIO II CN23XX)<br>- mvneta (Marvell NETA)<br>- mvpp2 (Marvell Packet Processor v2)<br>- octeontx (CN83XX, CN82XX, CN81XX, CN80XX)<br>- octeontx2 (Marvell OCTEON TX2 family SoCs)<br>- thunderx (CN88XX)<br>- cnxk (CN9K, CN10K)<br>- qede (QLogic FastLinQ QL4xxxx)                                                                                              |
| NVIDIA        | - mlx4 (ConnectX-3, ConnectX-3 Pro)<br>- mlx5 (ConnectX-4, ConnectX-4 Lx, ConnectX-5, ConnectX-6, ConnectX-6 Dx, ConnectX-6 Lx, ConnectX-7, Bluefield, Bluefield-2)<br>- NVIDIA于2020年收购了Mellanox Technologies。DPDK文档和代码可能仍然包含Mellanox商标（如BlueField和ConnectX）的实例或引用，这些商标现在是NVIDIA商标。                                                                                                           |
| NETRONOME     | - nfp (NFP-4xxx, NFP-6xxx)                                                                                                                                                                                                                                                                                                                                                            |
| SOLARFLARE    | - sfc_efx (SFN7xxx, SFN8xxx, XtremeScale X2, Alveo SN1000 SmartNICs)                                                                                                                                                                                                                                                                                                                  |
| WANGXUN       | - ngbe (WX1860, SF200, SF400)<br> - txgbe (RP1000, RP2000, SP1000, WX1820)                                                                                                                                                                                                                                                                                                                |
| SOFTWARE NICS | - af_packet (Linux AF_PACKET socket)<br>- af_xdp (Linux AF_XDP socket)<br>- tap/tun (kernel L2/L3)<br>- pcap (file or kernel driver)<br>- ring (memory)<br>- memif (memory) |
                                                                                                                                                                                                                         |
## 9.4 查看网卡编号
```
## lshw -businfo -c network
 /root/dpdk-stable-22.11.1/usertools/dpdk-devbind.py -s
  lspci -v -s 02:00.0
```
## 9.5 下载安装网卡igb_uio驱动 
 7.4 快速安装脚本中已经包含

## 9.6 用igb_uio绑定网卡
```
ifconfig eth0 down
dpdk-devbind.py -b igb_uio 0000:02:05.0 
```
## 9.7 查看绑定结果 
```
root@dperf:~/dpdk-stable-22.11.1/usertools# ./dpdk-devbind.py -b igb_uio 0000:02:05.0
root@dperf:~/dpdk-stable-22.11.1/usertools# ./dpdk-devbind.py -s
Network devices using DPDK-compatible driver
============================================
0000:02:05.0 '82545EM Gigabit Ethernet Controller (Copper) 100f' drv=igb_uio unused=e1000
Network devices using kernel driver
===================================
0000:02:01.0 '82545EM Gigabit Ethernet Controller (Copper) 100f' if=eth0 drv=e1000 unused=igb_uio *Active*
No 'Baseband' devices detected
==============================
No 'Crypto' devices detected
============================
No 'DMA' devices detected
=========================
No 'Eventdev' devices detected
==============================
No 'Mempool' devices detected
=============================
No 'Compress' devices detected
==============================
No 'Misc (rawdev)' devices detected
===================================
No 'Regex' devices detected
===========================
root@dperf:~/dpdk-stable-22.11.1/usertools#
```

## 9.8 其他网卡本例不涉及


# 10 重启之后注意事项
```
//export LD_LIBRARY_PATH=/root/dpdk-stable-22.11.1/mydpdk/lib/x86_64-linux-gnu/
//sysctl -w vm.nr_hugepages=512
modprobe uio
insmod /root/dpdk-kmods/linux/igb_uio/igb_uio.ko
// insmod /lib/modules/4.15.0-202-generic/updates/drivers/net/ethernet/int                                                                                                                                                             el/ixgbe/ixgbe.ko
ifconfig eth3 down
ifconfig eth4 down
./dpdk-stable-22.11.1/usertools/dpdk-devbind.py -b igb_uio 0000:05:00.0
./dpdk-stable-22.11.1/usertools/dpdk-devbind.py -b igb_uio 0000:05:00.1
./dpdk-stable-22.11.1/usertools/dpdk-devbind.py -b igb_uio 0000:02:05.0
./dperf/build/dperf -c ./dperf/digger/client-throughput.conf
./dperf/build/dperf -c ./dperf/digger/server.conf
```

# 11 KNI 部分【本例不涉及】
```
官方文档：
https://doc.dpdk.org/guides-20.11/nics/kni.html
https://doc.dpdk.org/guides/nics/kni.html
```
# 12 实战部分--配置相关
## 12.1 名次解释
性能测试项主要来源于RFC3511中的定义


|名词                             |  解释                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
|---------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| TCP每秒新建（CPS）                | CPS测试的目的是确定最大的TCP连接建立速度。在RFC3511文档中对TCP每秒新建的测试方法进行了定义，同时也明确了测试过程中得参数设置。但在实际评测或客户验证时，对于CPS的配置却有一些出入。这里需要我们注意的事项请参见下列说明。<br>1. TCP每秒新建的参数配置，应根据客户或评测机构的实际情况进行配置。有的客户在测试CPS时，只关注TCP层的建立速度，使TCP连接建立成功即可，此关注的实际是TCP连接三次握手的过程（SYN，SYN ACK ，ACK），而且连接处于OPEN状态。而有的客户关注的是TCP的完整链接状态：包括三次握手建立、持续和关闭；而且在关闭过程中，又可以选择发送TCP FIN或者RST报文的方式。因此，在测试CPS前应与客户沟通测试的参数设置。<br>2. 在RFC3511文档中，描述了在测试CPS时，采用HTTP1.1的协议，并且响应Get请求。<br>而在有些评测机构中，在测试CPS时确实会采用HTTP1.0甚至TCP或其它基于TCP的协议进行测试。无论是否采用HTTP协议，对于CPS的测试过程基本没有影响，只是多了一个GET请求和应答。但传输的文件大小将会占用被测设备很大的系统资源，从而导致测试结果偏小。在RFC3511中，没有明确应答数据的大小，所以这里我们可以设置为1byte甚至更小。<br>3. 在测试过程中，应采用反复搜索的方式，以确定被测设备可以接受的TCP连接的最大请求速率。测试周期建议在10分钟以上。<br>4. 如上文所述，基于使用协议的不同，CPS可以分为L4（传输层）CPS和L7（应用层）CPS。而且大部分的厂商和客户都基本认同这两种CPS的叫法。 |
| HTTP的每秒处理事务数（TPS）        | TPS是指最大HTTP传输速率，考察被测设备的应用层处理能力。在RFC3511文档中要求协议使用HTTP1.0或者HTTP1.1。整个过程包括TCP三次握手以后执行HTTP事务请求（GET或者POST），并且得到答应，然后关闭TCP连接。需要注意的事项请参见下列说明。HTTP1.0和HTTP1.1协议的区别在HTTP1.0 协议中，每对请求/应答都使用一个新的连接。每次请求都需要客户端与服务器建立一个TCP连接，服务器收到请求并且返回应答后，会立即断开TCP连接。而HTTP1.1协议是可以保持这种连接状态，是可持久的。在一个TCP连接上可以传输多个HTTP请求/应答。当传输完成后，必须采用三次握手或四次握手的方法关闭链接（FIN，ACK，FIN ACK，ACK ） 。请求/应答的文件大小在RFC3511文档中没有明确说明文件的大小及类型。在测试TPS前一定要向客户或评测机构了解要求的请求文件的大小及类型。有些机构中采用1 字节或者100字节的文件，而有些机构则采用1K字节或者4K字节，文件类型也多种多样。所以，应多与客户或者评测机构进行实际的沟通。建议采用文件大小为1K字节、文件类型为文本文件；当采用HTTP1.0 协议时，建议Get 1次；当采用HTTP1.1协议时，建议Get 10次或者更多。通过1可以看到，在测试TPS时，采用HTTP1.0协议或采用HTTP1.1协议测试同一款被测设备，会得到两种不同的测试结果因为HTTP协议本身的原因，使得在传输过程中得到的事务数会有所不同。理论上讲，HTTP1.1 协议的传输事务数要高于HTTP1.0协议。但这并不能反映HTTP1.1 的每秒连接数要高于HTTP1.0的每秒连接数。同时需要注意到，测试HTTP TPS时如果采用HTTP1.0，其实和TCP的L7 CPS的性能一样。从测试方法和测试过程中看，两者基本没有区别。所以，在测试HTTP TPS时，一定要了解客户的关注重点，而且要与客户沟通采用哪个版本的HTTP协议。|
| HTTP有效吞吐量（HTTP Good Throughput） | 在RFC3511文档中没有明确描述HTTP有效吞吐量的测试方法，但通过TPS的测试方法，我们可以根据要求来编写HTTP有效吞吐量的方法。需要注意的事项请参见下列说明。1. 在TPS节，我们知道了HTTP1.0和HTTP1.1之间的区别在测试HTTP有效吞吐量时，应减少TCP的建立和关闭连接的消耗和延迟。所以在测试HTTP有效吞吐量时，建议采用HTTP1.1协议。2. 在测试HTTP有效吞吐量时，建议采用请求/应答的模式而且在HTTP1.1协议中，可以执行8个或8个以上的请求命令（GET）。请求的文件大小建议为64K字节的文本文件。当然，在实际测试过程中请求文件的大小及类型，应与客户或评测机构进行沟通。当请求命令越多，请求的文件越大，那么HTTP的吞吐量也就越大。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| 最大TCP并发连接数                      | 在RFC3511文档中描述的最大TCP并发连接数测试，是采用一种反复搜索机制进行，在每次反复过程中以低于被测设备所能承受的连接速率发送不同数量的并发连接，直至得出被测设备的最大TCP并发连接数。需要注意的事项请参见下列说明。在测试最大TCP并发连接数所采用的协议有些客户或厂商为了追求更高的数值，在测试此项时采用TCP层协议。而一些评测机构或客户要求在测试最大TCP并发连接数时采用HTTP协议，并且要求有请求/答应机制。在测试最大TCP并发连接数时使用应用层协议(HTTP)，请求/应答的传输文件大小会直接影响测试结果如果应用层流量很大，被测设备会使用很大的系统资源去处理包转发或包检查等，从而导致一些请求无法被处理，引起测试结果偏小；反之测试结果会偏大。这里建议当使用HTTP协议进行测试时，请求的文件不应过大，应在1Byte左右。在测试最大TCP并发连接数时，无论采用TCP协议还是采用应用层协议，都应考虑实际测试需要从1中我们知道，采用TCP协议进行测试对被测设备的压力要小，并且每条连接中的负载也比较小，被测设备在转发或处理时比较容易，从而很容易达到测试的数目要求。但这并不是说采用TCP协议就是没有意义的，它从另一角度反映了被测设备能维持TCP连接的最大数量。而如2中采用HTTP协议进行测试，会加大被测设备的压力，使得占用被测设备的资源去处理包转发或包检查。而且请求文件大小的不同，会引起测试结果发生偏差。在实际测试过程中，我们应与客户或评测机构进行沟通，从而测试出被测设备的真实能力，是否满足其要求。 |
| RT（Response-time）响应时间           | 执行一个请求从开始到最后收到响应数据所花费的总体时间，即从客户端发起请求到收到服务器响应结果的时间。该请求可以是任何东西，从内存获取，磁盘IO，复杂的数据库查询或加载完整的网页。暂时忽略传输时间，响应时间是处理时间和等待时间的总和。处理时间是完成请求要求的工作所需的时间，等待时间是请求在被处理之前必须在队列中等待的时间。响应时间是一个系统最重要的指标之一，它的数值大小直接反应了系统的快慢。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| TPS：是Transactions Per Second的缩写 | 也就是事务数/秒。它是软件测试结果的测量单位。一个事务是指一个客户端向服务器发送请求然后服务器做出响应的过程。客户端在发送请求时开始计时，收到服务器响应后结束计时，以此来计算使用的时间和完成的事务个数。QPS vs TPS：QPS基本类似于TPS，但是不同的是，对于一个页面的一次访问，形成一个TPS；但一次页面请求，可能产生多次对服务器的请求，服务器对这些请求，就可计入“QPS”之中。如，访问一个页面会请求服务器2次，一次访问，产生一个“T”，产生2个“Q”。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |

## 12.2 配置关键参数
https://github.com/baidu/dperf/blob/main/docs/configuration.md

## 12.3 配置参数中的必须项
| parameter | syntax                                       | default: | required | mode           |
|-----------|----------------------------------------------|----------|----------|----------------|
|  cps      | cps Number                                   | -        | yes      | client         |
| client    | client IPAddrStart IPAddrNumber              | -        | yes      | client, server |
|  cpu      |  cpu n0 n1 n2-n3...                          | -        | yes      | client, server |
|  duration | duration Time                                | 100s     | yes      | client, server |
|  listen   | listen Port Number                           | 80 1     | yes      |                |
|  mode     | client | server                              | -        | yes      |                |
|  port     | port PCI|BOND IPAddress Gateway [GatewayMAC] | -        | yes      | client, server |
|  server   | server IPAddrStart IPAddrNumber              | -        | yes      | client, server |


## 12.4 给网关配置mac地址
```
#port         pci             addr      gateway    [mac]
port       0000:01:00.0    6.6.245.3 6.6.245.1  b4:a9:fc:ab:7a:85
```

## 12.5 FDIR 
网卡支持FDIR
通常物理网卡支持FDIR，使用FDIR特性，可以发挥dperf的最佳性能。CPU数量需要与server的IP数量一致。
```
cpu          0 1 2 3 4 5 6 7
server       192.168.31.50   8
```

## 12.6 L3L4RSS
通常虚拟环境里不能设置FDIR，但是可以使用RSS，根据网卡特性使用L3或L3L4哈希策略，要求server IP的数量与CPU个数一致，client需要配置足够的IP，其IP在会在CPU间平分

```
cpu    0 1 2 3
rss    auto
client 192.168.1.100 10 
server 192.168.1.200 4
```
## 同一台机器用两个网卡，同时运行dperf客户端、dperf服务器
[配置参考]（https://github.com/baidu/dperf/tree/main/test/multi-instance）
- 需要准备2个网卡，每个dperf使用一个
- 需要配置socket_mem配置项，给两个dperf程序分配不同的大页及CPU
- 编者在该模式并未实际测试成功，期待大家补充本部分
```
http://doc.dpdk.org/guides/linux_gsg/linux_eal_parameters.html#linux-specific-eal-parameters
使用旧版 DPDK 内存分配模式。
--socket-mem <amounts of memory per socket>
为每个套接字预分配指定数量的内存。该参数是以逗号分隔的值列表。例如：
--socket-mem 1024,2048
这将在 socket 0 上分配 1 GB 的内存，在 socket 1 上分配 2048 MB 的内存。
```

```
##client-cps-1.conf
mode        client
cpu         0
socket_mem  40960,0
duration    5m
cps         2.4m
#port       pci             addr         gateway        [mac]
port        0000:5e:00.1    6.6.234.3    6.6.234.1
client      6.6.234.3   100
server      6.6.235.3   1
listen      80          5

tx_burst    64
launch_num  20

##server-cps-1.conf
mode            server
cpu             12
socket_mem      0,40960
duration        4.5m
#port           pci             addr         gateway        [mac]
port            0000:af:00.1    6.6.235.3    6.6.235.1
client          6.6.234.3       100
server          6.6.235.3       1
listen          80              5
tx_burst        64
launch_num      8
```


# 13  实战部分-测试 
## 13.1  Throughput 
//把payload_size调大，把并发调小就可以了
测试出现了skErr，你把tx burst调小试试
测试带宽，可以把协议改为udp
推荐使用pakcet_size 1500，不要使用payload_size
测试带宽时，可以多配置几个CPU，增加队列数

https://zhuanlan.zhihu.com/p/451341132


```
mode                        client
tx_burst                    1024
launch_num                  3
cpu                         0 1 2 3 4 5 6 7
payload_size                1400
duration                    120s
cps                         500
cc                          30000
keepalive                   1ms
port        0000:03:00.1    192.168.31.200   192.168.31.50 08:c0:eb:d1:f7:8f
client      192.168.31.200      8
server      192.168.31.50     8
listen 80 1
................................................................
mode                        server
tx_burst                    1024
cpu                         0 1 2 3 4 5 6 7
duration                    150s
payload_size                1400
keepalive                   1s
port                        0000:03:00.1    192.168.31.50   192.168.31.200
client      192.168.31.200      8
server      192.168.31.50     8
listen 80 1
```

## 13.2  CPS 
```
mode            client
tx_burst        128
launch_num      10
cpu             0
payload_size    1
duration        2m
cps             2.1m
#port           pci             addr      gateway        [mac]
port            0000:01:00.0    6.6.245.3 6.6.245.1  b4:a9:fc:ab:7a:85
#               addr_start      num
client          6.6.245.3		100
#               addr_start      num
server          6.6.247.3       1
#               port_start      num
listen          80              5
................................................................
mode            server
tx_burst        128
cpu             48
duration        3m
payload_size    1
#numa2
port            0000:87:00.0    6.6.247.3    6.6.247.1  b4:a9:fc:ab:7a:85
#               addr_start      num
client          6.6.245.3       100
#               addr_start      num
server          6.6.247.3       1
#               port_start      num
listen          80              5  
```

## 13.3  CC 
```
mode                        client
tx_burst                    128
launch_num                  10
cpu                         0
payload_size                1
duration                    10m
cps                         0.5m
cc                          100m
keepalive                   60s
#port                       pci             addr      gateway    [mac]
port                        0000:01:00.0    6.6.245.3 6.6.245.1  b4:a9:fc:ab:7a:85
#                           addr_start      num
client                      6.6.245.3		100
#                           addr_start      num
server                      6.6.247.3       1
#                           port_start      num
listen                      80              32
................................................................
mode            server
tx_burst        128
cpu             48
duration        10m
payload_size    1
keepalive       60s
#numa2
port            0000:87:00.0    6.6.247.3    6.6.247.1  b4:a9:fc:ab:7a:85
#               addr_start      num
client          6.6.245.3       100
#               addr_start      num
server          6.6.247.3       1
#               port_start      num
listen          80              32

```
## 13.4  udp pps 
在 PPS 测试中，dperf 客户端保持固定cps的 3k 和keepalive2ms，并调整并发连接cc以产生不同的pps流量。
与 CPS/CC 测试相同，使用 1 字节的极小有效载荷。
我们使用 UDP 协议进行测试。
此外，tx_burst在 dperf 中，客户端设置为 1 以减少流量激增。
```
mode            client
cpu             8-15
slow_start      60
tx_burst        128
launch_num      1
payload_size    1
duration        90s
protocol        udp
cps             3k
cc              [refer to performance data]
keepalive       2ms
port            0000:04:00.0    192.168.0.30 192.168.7.254
client          192.168.3.0     50
server          192.168.5.1     8
listen          80              1
................................................................
mode            server
cpu             0-7
tx_burst        128
payload_size    1
duration        100d
protocol        udp
keepalive       10s
port            0000:04:00.1    192.168.1.30   192.168.7.254
client          192.168.0.28    1
client          192.168.1.28    1 
client          192.168.1.30    1 
client          192.168.3.0     200
server          192.168.6.100   8
listen          80              1
```
## 13.5  时延
什么是网络时延
报文在网络上传输需要时间，经过交换机、路由器、防火墙、负载均衡等网络设备处理需要时间；报文在两点之间的传输成为时延；报文从A发到B，B再回复到A，这段时间称为RTT。数据中心内部经过交换机的时延约为1-2us，网线互连的RTT为3-5us。
dperf测试网络时延的原理
dperf client发送连接的首包（SYN或UDP报文）时记下当前时间，在收到响应报文时再记下时间，两个时间相减就是一个rtt。dperf统计很多连接的首包RTT，给出每秒平均值。
计算时间的函数

```
mode            client
cpu             1
tx_burst        4
launch_num      4
payload_size    1
duration        10d
protocol        udp
cps             1m
#port           pci             addr        gateway
port            0000:5e:00.1    6.6.234.4   6.6.234.1
#               addr_start      num
client          6.6.234.4       100
#               addr_start      num
server          6.6.213.4       1
#               port_start      num
listen          80              5
................................................................
mode            server
cpu             1
tx_burst        4
payload_size    1
duration        10d
protocol        udp
#port           pci             addr        gateway
port         0000:03:00.1    6.6.213.4   6.6.213.1
#               addr_start      num
client          6.6.234.4       100
#               addr_start      num
server          6.6.213.4       1
#               port_start      num
listen          80              5
```
## 13.6  ipv6 

```
mode    client
#cpu 0 1
cpu 0
#payload_size 1440
#payload_size 1440
#           seconds
duration    60
cps         10000
#port   pci             addr                gateway        [mac]
port    0000:03:00.1    2001:6:6:241::27    2001:6:6:241::1
#       addr_start      num
client   2001:6:6:241::25     2
#client  6.6.241.200     2
#       addr_start      num
server  2001:6:6:241::33      1
#server  6.6.241.12      1
#           port_start  num
listen      80          1
................................................................
mode    server
cpu 0
#           seconds
duration    1000s
#port   pci             addr                gateway        [mac]
port    0000:03:00.1    2001:6:6:241::27    2001:6:6:241::1
#       addr_start          num
client  2001:6:6:241::10    50
#       addr_start          num
server  2001:6:6:241::27    1
#           port_start  num
listen      80          1
```
## 13.7 其他宣称支持，但本文暂不涉及的项(欢迎补充)
详见
https://github.com/baidu/dperf/tree/main/test

bond
kni
kni-ipv6
vxlan
vxlan-ipv6 

# 14. dperf 统计信息说明
https://github.com/baidu/dperf/blob/main/docs/statistics-CN.md


| 参数     | 说明                                              |
|--------|-------------------------------------------------|
| bitsRx | 每秒接收到的bit。 （发送+接受的=总带宽 throughput）              |
| bitsTx | 每秒发送的bit。                                       |
| skOpen | 每秒打开的socket个数, 就是每秒新建连接数(CPS)。                  |
| skCon  | 当前处于打开状态的socket个数，即并发连接数。Concurrent connections |



# 15 dperf 报错信息排查
```
EAL: Detected CPU lcores: 2
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'PA'
EAL: No free 2048 kB hugepages reported on node 0
EAL: FATAL: Cannot get hugepage information.
EAL: Cannot get hugepage information.
rte_eal_init fail
dpdk_eal_init fail
dpdk init fail
root@dperf:~/dperf# 

没有配置hugepage导致

................................................................
line6:error 
配置错误，请查看是否缺少12.2下参数中的必须项
................................................................
ctrl+c 后再次启动容易出现下面的错误
已经存在其他进程
kill后在启动即可
root@dperf:~/dperf# ./build/dperf -c ./digger/server.conf
EAL: Detected CPU lcores: 2
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Cannot create lock on '/var/run/dpdk/rte/config'. Is another primary process running?
EAL: FATAL: Cannot init config
EAL: Cannot init config
rte_eal_init fail
dpdk_eal_init fail
dpdk init fail
root@dperf:~/dperf#
................................................................

Error: insufficient sockets. worker=0 sockets=65535 cps's cc=2000000
套接字不足
................................................................
bad queue_num 2 max rx 1 max tx 1

................................................................
Error: 'rss' is required if cpu num is not equal to server ip num

................................................................
Error: insufficient sockets. worker=0 sockets=65535 cc=100000
地址配少了，达不到这个目标，要多配一些client ip，我一般client 配100个ip


................................................................
root@server:~# ./dperf/build/dperf -c ./dperf/digger/client-throughput.conf
EAL: Detected CPU lcores: 8
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/2503/mp_socket
EAL: Selected IOVA mode 'PA'
EAL: Probe PCI driver: net_ixgbe (8086:10fb) device: 0000:05:00.1 (socket -1)
TELEMETRY: No legacy callbacks, legacy socket not created
socket allocation failed, size 0.15GB num 1966050
work space init error
................................................................

error: Segmentation fault 
驱动不对，换成igb_uio驱动
................................................................

```

## 其他可能遇到的错误,欢迎补充
```
keepalive request interval must be a multiple of 10us
microseconds can only be used if the interval is less than 1 millisecond
duplicate vlan
duplicate rss
unknown rss type \'%s\'\n", argv[1]);
rss type \'%s\' dose not support \'mq_rx_none\'\n", argv[1]);
unknown rss config \'%s\'\n", argv[2]);
duplicate quiet
'vxlan' requires cpu num to be equal to server ip num
cpu num less than server ip num at 'client' mode;
'rss' is required if cpu num is not equal to server ip num
Cannot enable vlan and vxlan at the same time
Cannot enable vlan and bond at the same time
client and server address conflict
local ip conflict with client address
gateway ip conflict with server address
local ip conflict with server address
gateway ip conflict with client address
wait in server config
slow_start in server config\n");
no targets
insufficient sockets. worker=%d sockets=%u cc=%lu\n", i, socket_num, cc);
insufficient sockets. worker=%d sockets=%u cps's cc=%lu\n", i, socket_num, cps_cc);
both payload_size and packet_size are set
big packet_size %d\n", cfg->packet_size);
big payload_size %d\n", cfg->payload_size);
bad mss %d\n", cfg->mss)
rss is not supported for vxlan.
\'rss auto|l3\' conflicts with \'flood\'
rss \'auto\' requires one server address.
'cc' requires 'keepalive'
\'change_dip\' only support client mode
\'change_dip\' only support flood mode
\'change_dip\' not support vxlan
bad ip address family of \'change_dip\'
number of \'change_dip\' is less than cpu number
The HTTP host/path cannot be set with packet_size or payload_size.
the HTTP host/path cannot be set in server mode.
The HTTP host/path cannot be set in udp protocol.
```
