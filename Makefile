CC               = /bin/gcc
CPPFLAGS         = -I$(PWD)/src -I$(EAR_SRCDIR) -I$(STUBS_SRCDIR)
CFLAGS           = -g -Wall
CFLAGS          += $(EXTRA_CFLAGS)
LDFLAGS          = $(EXTRA_LDFLAGS)
SRCDIR           = $(PWD)/src
PREFIX          ?= /tmp/access-daemon
EAR_SRCDIR       = $(PWD)/external/ear/src
STUBS_SRCDIR     = $(PWD)/external/stubs
SEABUS_PREFIX    = /home/jordi/Documents/opt/seanerbus
SEABUS_CPPFLAGS  = -I$(SEABUS_PREFIX)/include
SEABUS_LDFLAGS   = -L$(SEABUS_PREFIX)/lib/ -lseanerbus_c -lseanerbus -luuid
NVML_PREFIX      =
NVML_CPPFLAGS    = -I$(NVML_PREFIX)/include
NVML_LDFLAGS     = -L$(NVML_PREFIX)/lib/ -lnmvl

all:
	@ $(MAKE) -C 'src' all || exit

install:
	@ $(MAKE) -C 'etc' install || exit
	@ $(MAKE) -C 'src' install || exit

clean:
	@ $(MAKE) -C 'src' clean || exit

include ./Makefile.exports
