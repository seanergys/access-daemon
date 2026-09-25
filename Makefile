CC               = /bin/gcc
CC_FLAGS         = -I$(PWD)/src -I$(EAR_SRCDIR) -I$(STUBS_SRCDIR) -g
EAR_SRCDIR       = $(PWD)/external/ear/src
STUBS_SRCDIR     = $(PWD)/external/stubs
SEANERBUS_PREFIX =

all:
	@ $(MAKE) -C 'src' all || exit

clean:
	@ $(MAKE) -C 'src' clean || exit

include ./Makefile.exports
