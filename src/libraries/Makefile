# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: Apache-2.0

ifdef SEABUS_PREFIX
BINS = access_easybus.so \
       access_easybus.pc
endif

all: all.sub-folders $(BINS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

# RPATH covers the bindings' dependencies and both installed EasyBus paths.
access_easybus.so: access_easybus.c
	$(CC) $(CPPFLAGS) $(SEABUS_CPPFLAGS) $(CFLAGS) -I../common/ \
	    -fPIC --shared -o lib$@ $< $(LDFLAGS) $(SEABUS_LDFLAGS) ../common/libcommon.a \
	    -Wl,--disable-new-dtags,-rpath,'$$ORIGIN:$$ORIGIN/accessd'

access_easybus.pc: access_easybus.pc.in Makefile
	sed -e 's|@PREFIX@|$(abspath $(PREFIX))|g' \
	    -e 's|@SEABUS_LDFLAGS@|$(SEABUS_LDFLAGS)|g' $< > $@

install: install.sub-folders $(BINS)
ifdef SEABUS_PREFIX
	cp -f access_easybus.h $(PREFIX)/include/accessd
	cp -f libaccess_easybus.so $(PREFIX)/lib/accessd
	cp -f access_easybus.pc $(PREFIX)/lib/pkgconfig
	ln -sfn accessd/libaccess_easybus.so "$(PREFIX)/lib/libaccess_easybus.so"
	ln -sfn "$(abspath $(SEABUS_PREFIX))/lib/libseanerbus_c.so" "$(PREFIX)/lib/accessd/libseanerbus_c.so"
endif

clean: clean.sub-folders
	rm -f $(BINS) *.o *.so

%.sub-folders:
	@ $(MAKE) -C 'preloads' $* || exit
