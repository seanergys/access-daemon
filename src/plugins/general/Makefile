SEA_CFLAGS  = -I$(SEANERBUS_PREFIX)/include
SEA_LDFLAGS = -L$(SEANERBUS_PREFIX)/lib/ -lseanerbus_c

# EAR's config_install.h stub is empty. Match ear_private's local API ABI.
EARD_CFLAGS ?= -DMAX_CPUS_SUPPORTED=512 -DMAX_SOCKETS_SUPPORTED=4 -DMAX_GPUS_SUPPORTED=8 -DUSE_GPUS=1

SRC_DIRS  =  $(EAR_SRCDIR)/common/hardware
SRC_DIRS += :$(EAR_SRCDIR)/data_center_monitor/plugins/tests
SRC_DIRS += :$(EAR_SRCDIR)/management/cpufreq

vpath %.c $(SRC_DIRS)
vpath %.h $(SRC_DIRS)

BINS  = \
    eard_impostor.so \
    keyboard.so \
    metrics_accumulator.so \
    metrics_caller.so \
    request_handler.so \
    request_handler_text.so \
    topology_ext.so

TOPO_OBJS = \
    cpufreq_base.o

ifdef SEANERBUS_PREFIX
BINS += seanerbus.so
endif

all: $(BINS)
	rm -f *.o

%.o: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -c $<

%.so: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ $^

eard_impostor.so: eard_impostor.c eard_impostor.h
	$(CC) $(CC_FLAGS) $(EARD_CFLAGS) -fPIC -shared -o $@ $^ -lpthread

seanerbus.so: seanerbus.c
	$(CC) $(CC_FLAGS) $(SEA_CFLAGS) -fPIC -shared -o $@ $^ -luuid $(SEA_LDFLAGS)

topology_ext.so: $(TOPO_OBJS)
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ topology_ext.c $(TOPO_OBJS)

install:
	mkdir -p $(PREFIX)/lib/plugins
	cp -f $(BINS) $(PREFIX)/lib/plugins

clean:
	rm -f *.o *.so
