BINS  = \
    cpu_freq.hsmp.so \
    cpu_freq.driver_pstate.so \
    cpu_freq.driver_acpi.so \
    cpu_freq.driver_governor.so \
    cpu_pow.msr.so \
    cpu_pow.hsmp.so

all: $(BINS)

%.so: %.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o act.$@ $^

install:
	mkdir -p $(PREFIX)/lib/plugins/actuators
	cp -f *.so $(PREFIX)/lib/plugins/actuators

clean:
	rm -f *.o *.so