SRC_DIRS  =  $(EAR_SRCDIR)/common/hardware
SRC_DIRS += :$(EAR_SRCDIR)/data_center_monitor/plugins/tests
SRC_DIRS += :$(EAR_SRCDIR)/management/cpufreq

vpath %.c $(SRC_DIRS)
vpath %.h $(SRC_DIRS)

OBJS  = \
    keyboard.so \
    metrics_accumulator.so \
    metrics_caller.so \
    request_handler.so \
    request_handler_text.so \
    seanerbus.so \
    topology_ext.so

TOPO_OBJS = \
    cpuid.o \
    topology.o \
    topology_asm.o \
    topology_tdp.o \
    cpufreq_base.o

RQST_OBJS = \
    request_handler.o

all: $(OBJS)
	rm -f *.o

%.o: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -c $<

%.so: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ $^

seanerbus.so: seanerbus.c
	$(CC) $(CC_FLAGS) -fPIC -shared -I$(SEANERBUS_PREFIX)/include -o $@ $^ \
	-Wl,--disable-new-dtags,-rpath,\$(SEANERBUS_PREFIX)/lib $(SEANERBUS_PREFIX)/lib/libseanerbus_c.so

topology_ext.so: $(TOPO_OBJS)
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ topology_ext.c $(TOPO_OBJS)

clean:
	rm -f *.o *.so
