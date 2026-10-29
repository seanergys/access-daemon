ifdef SEABUS_PREFIX
BINS = access_easybus.so
endif

all: all.sub-folders $(BINS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

access_easybus.so: access_easybus.c
	$(CC) -Wall $(CPPFLAGS) $(SEABUS_CPPFLAGS) $(CFLAGS) -I../common/ \
	    -fPIC --shared -o lib$@ $< $(LDFLAGS) $(SEABUS_LDFLAGS) ../common/libcommon.a

install: install.sub-folders
	cp -f access_easybus.h $(PREFIX)/include
ifdef SEABUS_PREFIX
	cp -f libaccess_easybus.so $(PREFIX)/lib
endif

clean: clean.sub-folders
	rm -f $(BINS) *.o *.so

%.sub-folders:
	@ $(MAKE) -C 'preloads' $* || exit
