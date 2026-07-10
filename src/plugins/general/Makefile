VPATH  =  $(EAR_SRCDIR)/common/hardware
VPATH += :$(EAR_SRCDIR)/data_center_monitor/plugins/tests
VPATH += :$(EAR_SRCDIR)/management/cpufreq

COMMON_DIR = ../../common
DATA_PLANE_RPATH = -Wl,-rpath,'$$ORIGIN/../../common'

OBJS  = \
    data_plane.so \
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

data_plane.o: data_plane.c data_plane.h $(COMMON_DIR)/seanerbus_wrapper.h
	$(CC) $(CC_FLAGS) -fPIC -c $< -o $@

data_plane.so: data_plane.o $(COMMON_DIR)/libseanerbus_wrapper.so
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ data_plane.o $(DATA_PLANE_RPATH) -L$(COMMON_DIR) -lseanerbus_wrapper

topology_ext.so: topology_ext.o $(TOPO_OBJS)
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ $^

clean:
	rm -f *.o *.so
