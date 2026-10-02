SEA_CFLAGS  = -I$(SEANERBUS_PREFIX)/include
SEA_LDFLAGS = -L$(SEANERBUS_PREFIX)/lib/ -lseanerbus_c

SRC_DIRS  =  $(EAR_SRCDIR)/common/hardware
SRC_DIRS += :$(EAR_SRCDIR)/data_center_monitor/plugins/tests
SRC_DIRS += :$(EAR_SRCDIR)/management/cpufreq

vpath %.c $(SRC_DIRS)
vpath %.h $(SRC_DIRS)

BINS  = \
    keyboard.so \
    metrics_accumulator.so \
    metrics_caller.so \
    request_handler.so \
    request_handler_text.so \
    topology_ext.so

ifdef SEANERBUS_PREFIX
BINS += seanerbus.so
endif

TOPO_OBJS = \
    cpuid.o \
    topology.o \
    topology_asm.o \
    topology_tdp.o \
    cpufreq_base.o

all: $(BINS)
	rm -f *.o

%.o: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -c $<

%.so: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ $^

seanerbus.so: seanerbus.c
	$(CC) $(CC_FLAGS) $(SEA_CFLAGS) -fPIC -shared -o $@ $^ -luuid $(SEA_LDFLAGS)

topology_ext.so: $(TOPO_OBJS)
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ topology_ext.c $(TOPO_OBJS)

install:
	mkdir -p $(PREFIX)/lib/plugins
	cp -f $(BINS) $(PREFIX)/lib/plugins

clean:
	rm -f *.o *.so
