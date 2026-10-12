BINS = accessd

all: all.sub-folders accessd

accessd: access_daemon.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(EAR_SRCDIR) -o $@ $< \
	    $(LDFLAGS) -L$(SRCDIR)/common -lcommon -lpthread -ldl -lm

install: install.sub-folders
	cp $(BINS) $(PREFIX)/sbin

clean: clean.sub-folders
	rm -f $(BINS)

%.sub-folders:
	@ $(MAKE) -C 'common' $* || exit
	@ $(MAKE) -C 'libraries' $* || exit
	@ $(MAKE) -C 'plugins' $* || exit
	@ $(MAKE) -C 'tests' $* || exit
