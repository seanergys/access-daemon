CC           ?= gcc
CFLAGS       ?= -g -Wall
PREFIX       ?= /tmp/access-daemon
EAR_SRCDIR   ?= ../../../external/ear/src
STUBS_SRCDIR ?= ../../../external/stubs

CPPFLAGS += -I$(EAR_SRCDIR) -I$(STUBS_SRCDIR)
BINS      = libsim-nvml.so libsim-msr.so

.PHONY: all install clean

all: $(BINS)

libsim-nvml.so: nvml.c $(EAR_SRCDIR)/metrics/common/nvml.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -fvisibility=hidden -shared -o $@ $< $(LDFLAGS)

libsim-msr.so: msr.c $(EAR_SRCDIR)/metrics/common/msr.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -fvisibility=hidden -shared -o $@ $< $(LDFLAGS) -pthread

install: all
	mkdir -p $(PREFIX)/lib/plugins/preloads
	cp -f $(BINS) $(PREFIX)/lib/plugins/preloads

clean:
	rm -f *.o *.a *.so *.so.1 $(BINS)
