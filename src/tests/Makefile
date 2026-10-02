SEA_CFLAGS   = -I$(SEANERBUS_PREFIX)/include
SEA_LDFLAGS  = -L$(SEANERBUS_PREFIX)/lib/ -lseanerbus_c -lseanerbus
EASY_CFLAGS  =
EASY_LDFLAGS = $(SEA_LDFLAGS) -L$(SRCDIR)/libraries -laccess_easybus

ifdef SEANERBUS_PREFIX
BINS += metrics_seanerbus \
        metrics_easybus
endif

all: $(BINS)

%: %.c
	$(CC) $(CC_FLAGS) -o $@ $^

metrics_seanerbus: metrics_seanerbus.c
	$(CC) $(CC_FLAGS) -I../common/ $(SEA_CFLAGS) -o $@ $^ $(SEA_LDFLAGS)

metrics_easybus: metrics_easybus.c
	$(CC) $(CC_FLAGS) -I../common/ -o $@ $^ $(EASY_LDFLAGS) $(SRCDIR)/common/libcommon.a -lm

install: ;

clean:
	rm -f $(BINS) *.o *.so