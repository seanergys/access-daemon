SEA_CFLAGS  = -I $(SEANERBUS_PREFIX)/include
SEA_LDFLAGS = -Wl,--disable-new-dtags,-rpath,\$(SEANERBUS_PREFIX)/lib $(SEANERBUS_PREFIX)/lib/libseanerbus_c.so

BINS  = \
    metrics_by_the_sea

all: $(BINS)

%: %.c
	$(CC) $(CC_FLAGS) -o $@ $^

metrics_by_the_sea: metrics_by_the_sea.c
	$(CC) $(CC_FLAGS) $(SEA_CFLAGS) -o $@ $^ $(SEA_LDFLAGS)

clean:
	rm -f $(BINS) *.o *.so