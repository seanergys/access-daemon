# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: Apache-2.0

ifdef SEABUS_PREFIX
BINS += metrics_seanerbus \
        metrics_easybus
endif

all: $(BINS)

metrics_seanerbus: metrics_seanerbus.c
	$(CC) $(CPPFLAGS) $(SEABUS_CPPFLAGS) $(CFLAGS) -I../common/ -o $@ $< \
	    $(LDFLAGS) $(SEABUS_LDFLAGS) -lseanerbus

metrics_easybus: metrics_easybus.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -I../common/ -o $@ $< $(LDFLAGS) \
	    -L$(SRCDIR)/libraries -laccess_easybus $(SRCDIR)/common/libcommon.a -lm \
	    $(SEABUS_LDFLAGS)

install: ;

clean:
	rm -f $(BINS) *.o *.so