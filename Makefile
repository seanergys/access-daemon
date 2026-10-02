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
	mkdir -p $(PREFIX) $(PREFIX)/sbin $(PREFIX)/include $(PREFIX)/lib
	@ $(MAKE) -C 'src' install || exit

clean:
	@ $(MAKE) -C 'src' clean || exit

include ./Makefile.exports
