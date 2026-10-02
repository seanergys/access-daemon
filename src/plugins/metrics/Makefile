BINS  = \
    cpu_freq.msr.so \
    cpu_pow.msr.so

all: $(BINS)

%.so: %.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o met.$@ $^

install:
	mkdir -p $(PREFIX)/lib/plugins/metrics
	cp -f *.so $(PREFIX)/lib/plugins/metrics

clean:
	rm -f *.o *.so