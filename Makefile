
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

LDLIBS += -lrte_pmd_bond

include $(RTE_SDK)/mk/rte.extapp.mk

#dpdk 20.11
else

PKGCONF = pkg-config

ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

CFLAGS := -O3 -g $(CFLAGS) -I./src

ifdef DPERF_DEBUG
CFLAGS += -DDPERF_DEBUG
endif

CFLAGS += -DHTTP_PARSE
CFLAGS += -Wno-address-of-packed-member -DALLOW_EXPERIMENTAL_API
CFLAGS += $(shell $(PKGCONF) --cflags libdpdk)

#fix lower version pkg-config
LDFLAGS0 = $(shell $(PKGCONF) --static --libs libdpdk) -lpthread -lrte_net_bond -lrte_bus_pci -lrte_bus_vdev
LDFLAGS1 = $(shell echo $(LDFLAGS0) | sed 's/-Wl,--whole-archive -Wl,--no-whole-archive -Wl,--export-dynamic/-Wl,--whole-archive/')
LDFLAGS2 = $(shell echo $(LDFLAGS1) | sed 's/.a -latomic/.a -Wl,--no-whole-archive -Wl,--export-dynamic -latomic/')
LDFLAGS += $(LDFLAGS2)

build/$(APP): $(SRCS-y)
	mkdir -p build
	gcc $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS)

clean:
	rm -rf build/

endif
