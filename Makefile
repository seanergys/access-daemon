CC               = /bin/gcc
CC_FLAGS         = -I$(PWD)/src -I$(EAR_SRCDIR) -g
ROOT_SRCDIR      = $(shell pwd)
EAR_SRCDIR       = /home/jordi/Documents/git/ear_private/src
SEANERBUS_PREFIX = /home/jordi/Documents/opt/seanerbus

all:
	@ $(MAKE) -C 'src' all || exit

clean:
	@ $(MAKE) -C 'src' clean || exit

include ./Makefile.exports
