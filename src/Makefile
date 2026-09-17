all: all.sub-folders accessd

accessd: access_daemon.c
	$(CC) $(CFLAGS) -I$(EAR_SRCDIR) -o $@ $^ -L./common -lcommon -lpthread -ldl -lm

clean: clean.sub-folders

%.sub-folders:
	@ $(MAKE) -C 'common' $* || exit
	@ $(MAKE) -C 'plugins' $* || exit
	@ $(MAKE) -C 'tests' $* || exit
