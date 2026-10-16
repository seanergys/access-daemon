SRC_DIRS  = $(EAR_SRCDIR)/metrics/common
SRC_DIRS += $(EAR_SRCDIR)/common
SRC_DIRS += $(EAR_SRCDIR)/common/utils
SRC_DIRS += $(STUBS_SRCDIR)/common/system
SRC_DIRS += $(EAR_SRCDIR)/common/system
SRC_DIRS += $(EAR_SRCDIR)/common/hardware

vpath %.c $(SRC_DIRS)
vpath %.h $(SRC_DIRS)

OBJS  = \
    args.o \
    bithack.o \
    cpuid.o \
    environment_common.o \
    file.o \
    folder.o \
    hsmp.o \
    hsmp_esmi.o \
    hsmp_driver.o \
    hsmp_pci.o \
    keeper.o \
    math_operations.o \
    monitor.o \
    msr.o \
    mrs.o \
    nvml.o \
    overhead.o \
    plugin_manager.o \
    plugin_manager_internal.o \
    pci.o \
    poll.o \
    random.o \
    string.o \
    string_enhanced.o \
    string_helpers.o \
    strtable.o \
    symplug.o \
    serial_buffer.o \
    system_file.o \
    time.o \
    topology.o \
    topology_asm.o \
    topology_tdp.o \
    user.o

all: libcommon.so libcommon.a
	rm -f *.o

%.o: %.c %.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $<

nvml.o: nvml.c nvml.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DEAR_INSTALL_PATH="" -fPIC -c $<

plugin_manager_internal.o: plugin_manager_internal.c plugin_manager_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -DSEPARATOR="','" -c $<

libcommon.a: $(OBJS)
	$(AR) rvs $@ $(OBJS)

libcommon.so: $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(LDFLAGS)

install:
	cp -f metrics_types.h $(PREFIX)/include
	cp -f libcommon.so $(PREFIX)/lib

clean:
	rm -f *.o *.so *.a
