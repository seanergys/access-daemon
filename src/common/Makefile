SRC_DIRS  = $(EAR_SRCDIR)/metrics/common
SRC_DIRS += $(EAR_SRCDIR)/common
SRC_DIRS += $(EAR_SRCDIR)/common/utils
SRC_DIRS += $(EAR_SRCDIR)/common/system
SRC_DIRS += $(EAR_SRCDIR)/common/hardware

vpath %.c $(SRC_DIRS)
vpath %.h $(SRC_DIRS)

OBJS  = \
    msr.o \
    file.o \
    args.o \
    environment_common.o \
    hsmp.o \
    hsmp_esmi.o \
    hsmp_driver.o \
    hsmp_pci.o \
    keeper.o \
    math_operations.o \
    monitor.o \
    overhead.o \
    plugin_manager.o \
    plugin_manager_internal.o \
    poll.o \
    string.o \
    string_enhanced.o \
    string_helpers.o \
    strtable.o \
    time.o \
    bithack.o

all: libcommon.so libcommon.a
	rm -f *.o

%.o: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -c $< -o $@

plugin_manager_internal.o: plugin_manager_internal.c plugin_manager_internal.h
	$(CC) $(CC_FLAGS) -fPIC -DSEPARATOR="','" -c $< -o $@

libcommon.a: $(OBJS)
	$(AR) rvs $@ $(OBJS)

libcommon.so: $(OBJS)
	$(CC) $(CC_FLAGS) -shared -o $@ $(OBJS)

install:
	cp -f metrics_types.h $(PREFIX)/include
	cp -f libcommon.so $(PREFIX)/lib

clean:
	rm -f *.o *.so
