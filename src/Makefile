BINS = accessd

all: all.sub-folders accessd

accessd: access_daemon.c
	$(CC) $(CFLAGS) -I$(EAR_SRCDIR) -o $@ $^ -L$(SRCDIR)/common -lcommon -lpthread -ldl -lm

install: install.sub-folders
	cp $(BINS) $(PREFIX)/sbin

clean: clean.sub-folders

%.sub-folders:
	@ $(MAKE) -C 'common' $* || exit
	@ $(MAKE) -C 'libraries' $* || exit
	@ $(MAKE) -C 'plugins' $* || exit
	@ $(MAKE) -C 'tests' $* || exit
