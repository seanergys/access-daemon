all: sub-folders accessd

sub-folders:
	@ $(MAKE) -C 'common' all || exit
	@ $(MAKE) -C 'plugins' all || exit

accessd: access_daemon.c
	$(CC) $(CFLAGS) -I$(EAR_SRC) -o $@ $^ -L./common -lcommon -lpthread -ldl -lm

clean:
	@ $(MAKE) -C 'common' clean || exit
	@ $(MAKE) -C 'plugins' clean || exit