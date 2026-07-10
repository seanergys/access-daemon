SRC_DIRS  = $(EAR_SRCDIR)/metrics/common
SRC_DIRS += $(EAR_SRCDIR)/common
SRC_DIRS += $(EAR_SRCDIR)/common/utils
SRC_DIRS += $(EAR_SRCDIR)/common/system
SRC_DIRS += $(EAR_SRCDIR)/common/hardware

vpath %.c $(SRC_DIRS)
vpath %.h $(SRC_DIRS)

CXX ?= /bin/g++
PKG_CONFIG ?= pkg-config

ifndef SEANERBUS_PREFIX
$(error SEANERBUS_PREFIX is not set. Build from the repository root or export SEANERBUS_PREFIX)
endif

OBJS  = \
    msr.o \
    file.o \
    args.o \
    keeper.o \
    environment_common.o \
    math_operations.o \
    monitor.o \
    overhead.o \
    plugin_manager.o \
    plugin_manager_internal.o \
    poll.o \
    string.o \
    strtable.o \
    time.o \
    bithack.o

all: libcommon.so libseanerbus_wrapper.so
	rm -f *.o

%.o: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -c $< -o $@

plugin_manager_internal.o: plugin_manager_internal.c plugin_manager_internal.h
	$(CC) $(CC_FLAGS) -fPIC -DSEPARATOR="','" -c $< -o $@

libcommon.so: $(OBJS)
	$(CC) $(CC_FLAGS) -shared -o $@ $(OBJS)

seanerbus_wrapper.o: seanerbus_wrapper.cpp seanerbus_wrapper.h
	$(CXX) $(CC_FLAGS) -DFMT_HEADER_ONLY=1 \
		-I$(SEANERBUS_PREFIX)/include \
		$(shell $(PKG_CONFIG) --cflags capnp 2>/dev/null) \
		-std=c++17 -fPIC -c $< -o $@

libseanerbus_wrapper.so: seanerbus_wrapper.o $(SEANERBUS_PREFIX)/lib/libseanerbus.so
	$(CXX) $(CC_FLAGS) -fPIC -shared -o $@ $^ -Wl,-rpath,\$(SEANERBUS_PREFIX)/lib \
		$(shell $(PKG_CONFIG) --libs capnp 2>/dev/null) -lpthread

clean:
	rm -f *.o *.so
