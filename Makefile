CC               = /bin/gcc
CC_FLAGS         = -I$(PWD)/src -I$(EAR_SRCDIR) -I$(STUBS_SRCDIR) -g
SRCDIR           = $(PWD)/src
PREFIX           = /tmp/access-daemon
EAR_SRCDIR       = $(PWD)/external/ear/src
STUBS_SRCDIR     = $(PWD)/external/stubs
SEANERBUS_PREFIX = /home/jordi/Documents/opt/seanerbus

all:
	@ $(MAKE) -C 'src' all || exit

install:
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/etc
	mkdir -p $(PREFIX)/include
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/sbin
	@ $(MAKE) -C 'etc' install || exit
	@ $(MAKE) -C 'src' install || exit

clean:
	@ $(MAKE) -C 'src' clean || exit

include ./Makefile.exports
