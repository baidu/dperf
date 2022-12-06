
APP=dperf
SRCS-y := src/main.c src/socket.c src/config.c src/client.c src/mbuf_cache.c src/udp.c  \
          src/port.c src/mbuf.c src/arp.c src/icmp.c src/tcp.c src/tick.c src/http.c    \
          src/net_stats.c src/flow.c src/work_space.c src/cpuload.c src/config_keyword.c\
          src/socket_timer.c src/ip.c src/eth.c src/server.c src/dpdk.c src/ctl.c       \
          src/icmp6.c src/neigh.c src/vxlan.c src/csum.c src/kni.c src/bond.c src/lldp.c\
          src/rss.c src/ip_list.c src/http_parse.c src/trace.c

#dpdk 17.11, 18.11, 19.11
ifdef RTE_SDK

RTE_TARGET ?= x86_64-native-linuxapp-gcc
include $(RTE_SDK)/mk/rte.vars.mk
CFLAGS += -O3 -g -I./src
CFLAGS += -DHTTP_PARSE
CFLAGS += $(WERROR_FLAGS) -Wno-address-of-packed-member

ifdef DPERF_DEBUG
CFLAGS += -DDPERF_DEBUG
endif

include $(RTE_SDK)/mk/rte.extapp.mk

#dpdk 20.11
else

PKGCONF = pkg-config

ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

CFLAGS += -O3 -g -I./src
CFLAGS += -DHTTP_PARSE
CFLAGS += -Wno-address-of-packed-member
CFLAGS += $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS += $(shell $(PKGCONF) --libs libdpdk) -lpthread -lrte_net_bond -lrte_bus_pci -lrte_bus_vdev

build/$(APP): $(SRCS-y)
	mkdir -p build
	gcc $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS)

clean:
	rm -rf build/

endif
