BINS  = \
    cpu_freq.fake.so \
    cpu_freq.msr.so \
    cpu_pow.msr.so \
    gpu.nvml.so

all: $(BINS)

%.so: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o met.$@ $< $(LDFLAGS)

install:
	cp -f *.so $(PREFIX)/lib/accessd/plugins/metrics

clean:
	rm -f *.o *.so