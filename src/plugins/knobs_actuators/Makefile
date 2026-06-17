OBJS  = \
    cpu_freq.driver_acpi.so \
    cpu_freq.driver_pstate.so \
    cpu_freq.driver_governor.so \
    cpu_freq.hsmp.so \
    cpu_pow.msr.so \
    cpu_pow.hsmp.so

all: $(OBJS)

%.so: %.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o act.$@ $^

clean:
	rm -f *.o *.so