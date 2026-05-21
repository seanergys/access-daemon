CC       = /bin/gcc
CC_FLAGS = -I$(PWD)/src -I$(EAR_SRC) -g
EAR_SRC  = /home/jordi/Documents/git/ear_private/src

all:
	@ $(MAKE) -C 'src' all || exit

clean:
	@ $(MAKE) -C 'src' clean || exit

include ./Makefile.exports