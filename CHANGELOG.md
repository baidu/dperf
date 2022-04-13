# Changelog

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
