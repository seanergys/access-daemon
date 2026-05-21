VPATH  = :$(EAR_SRC)/metrics/common
VPATH +=  $(EAR_SRC)/common
VPATH += :$(EAR_SRC)/common/utils
VPATH += :$(EAR_SRC)/common/system
VPATH += :$(EAR_SRC)/common/hardware

OBJS  = \
    msr.o \
    file.o \
    args.o \
    environment_common.o \
    math_operations.o \
    monitor.o \
    overhead.o \
    plugin_manager.o \
    poll.o \
    string.o \
    strtable.o \
    time.o \
    bithack.o

all: $(OBJS) libcommon.so
	rm -f *.o

%.o: %.c %.h
	$(CC) $(CC_FLAGS) -fPIC -c $<

libcommon.so:
	$(CC) $(CC_FLAGS) -shared -o $@ *.o

clean:
	rm -f *.o *.so