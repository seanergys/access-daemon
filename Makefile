CC       = /bin/gcc
CC_FLAGS = -I$(PWD)/src -I$(EAR_SRC) -g
EAR_SRC  = /home/jordi/Documents/git/ear_private/src
SBUS     = /usr/local/lib/libsbus.a

all:
	@ $(MAKE) -C 'src' all || exit

clean:
	@ $(MAKE) -C 'src' clean || exit

include ./Makefile.exports