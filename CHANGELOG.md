# Changelog

## v1.7.0 - 2024-06-06
### Added

- send http post requests

- dpdk pdump

    thanks ZhangPengfei

- fast_close: use RST to close connections

- support DPDK-23.11 and DPDK-24

- enable simd512

- disable ack

- configure retransmit timeout

### Changed

- config DPDK log level

- clear_screen: dperf can clear screen output and make the screen cleaner

- static linking

    thanks ZhangPengfei

### Fixed

- dpdk cleanup

    thanks ZhangPengfei

- CFLAGS override

    thanks ZhangPengfei

- missspell

    thanks ZhangPengfei

- don't clear sequences in time-wait state

## v1.6.0 - 2023-10-10
### Added

- dperf server returns large payload

- you can use the default l3 or l3l4 rss algorithm

### Changed

- minimun local port range(lport_range) can be set to 1

- optimization: remove useless sockets outside lport_range

- move docs to dperf.org

- adjust jumbo MTU

### Fixed

- fix: negative number of udp connections

- print more information when startup fails
    - not enough ports available
    - error message of server ip numbers

- spelling errors and expression issues

    thanks digger-yu, CChuancey, zrmin

- fix: find port id

- fix: create new connection after "duration" time

    thanks zrgtester

- fix: http_host or http_path can only be configured when http is enabled

- fix: dperf make error on dpdk-18.11

    thanks amberlipp

## v1.5.0 - 2023-03-10
### Added

- udp elephant flow

    thanks sheva7777
- client_hop

    thanks hgkiller
- supported Mellanox MT27500 [ConnectX-3]

    thanks SdotChen
- article: Using dperf to test the performance of DPVS(zh)

    thanks sheva7777
- article: How to install dperf on ubuntu(zh)

    thanks digger-yu
- article: Using dperf to test 100Gbps bandwidth(zh)

    thanks CHRIS123540
- article: DPVS v1.9.2 Performance Tests by dperf

    thanks ywc689
- article: dperf FAQ(zh)

### Changed

- print error numbers with red color

### Fixed

- flood with rss l3l4

    thanks sheva7777
- use unlikeyly to predict condition

    thanks panzhengyu
- address conflict in configuration file

    thanks CHRIS123540
- docs/configuration: update payload_size

## v1.4.0 - 2022-12-14
### Added

- support dpdk-22.11

    thanks ykzj

- support LoongArch

    thanks choumin

- payload random

    thanks uname-v

### Fixed

- crash at tcp reply reset

- vlan check

    thanks owenstake
- crash by kni_broadcast()

    thanks sheva7777

## v1.3.0 - 2022-09-01
### Added

- 10us keepalive request interval

    thanks jiawen94
- vlan

    thanks hgkiller
- config client local port range for google cloud

    thanks MichaelZhangCN, vsv1020, wanggaoli
- article: using dperf to test network bandwidth

    thanks thunderZH963(Hua Zhang)

### Fixed

- in 'rss auto' mode, the number of UDP concurrent connections is incorrectly counted
- mq_rx_none: don't set RTE_ETH_MQ_RX_RSS
- compile warning

    thanks digger-yu
- double semicolons

    thanks yangwenrui

## v1.2.0 - 2022-06-01
### Added

- http client
- change_dip: before the packet is sent, a huge IP pool is used to change the dest IP

### Changed

- rss support l3/l3l4/auto
- optimize statistics

### Fixed

- FIX: kni only use interface IP
- FIX: large rx/tx descriptor number
- FIX: print more message for bad gateway error
- FIX: tcp closing state
- FIX: separate tcp and http

## v1.1.0 - 2022-04-13
### Added

- vxlan

    thanks epsilon-rhk
- kni

    thanks Ppapzs
- bond

    thanks reinight@gmail.com, Ppapzs
- tos

    thanks Ppapzs
- packet_size

    thanks Ppapzs
- jumbo

    thanks Ppapzs
- wait stage
- rss
- quiet: turn off output statistics
- rtt

### Changed

- tcp/udp flood and dose not require FDIR
- support DPDK-20.11, DPDK-22.03
- server close idle sockets
- tcp rst on or off
- allow server ip num to be less than cpu number
- simplified keepalive configuration
- update readme
- related articles

    thanks metonymical

### Fixed

- FIX: client ip range check error
- FIX: failed to start using virtio

    thanks celia240
- FIX: divide by zero
- FIX: tcp stack bugs

    thanks sheva7777
- FIX: segmentation fault when there are few huge pages

    thanks yunfei_james@126.com
- FIX: byte order conversion bug

    thanks crezov

- FIX: docs

    thanks leohotfn

## v1.0.0 - 2022-02-22
### Added

- Slow start time configurable
- Configure files
- Design files

### Changed

- Change readme

### Fixed

- Fix ip csum (Issue #28)
- Fix compile warnning (Issue #20)
    
    thanks 1225951453
- Fix segfault at vmxnet3 (Issue #16)
    
    thanks Danshanshu
- Fix separate the log files of server and client
    
    thanks RaisonNiu 
