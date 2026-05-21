VPATH  =  $(EAR_SRC)/common/hardware
VPATH += :$(EAR_SRC)/data_center_monitor/plugins/tests
VPATH += :$(EAR_SRC)/management/cpufreq

OBJS  = \
    keyboard.so \
    metrics_central.so \
    query_handler.so \
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

topology_ext.so: topology_ext.o $(TOPO_OBJS)
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ $^

clean:
	rm -f *.o *.so