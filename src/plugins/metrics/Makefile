BINS  = \
    cpu_freq.fake.so \
    cpu_freq.msr.so \
    cpu_pow.msr.so

all: $(BINS)

%.so: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o met.$@ $< $(LDFLAGS)

install:
	mkdir -p $(PREFIX)/lib/plugins/metrics
	cp -f *.so $(PREFIX)/lib/plugins/metrics

clean:
	rm -f *.o *.so