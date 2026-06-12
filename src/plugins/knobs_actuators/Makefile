OBJS  = \
    cpu_freq_driver_acpi.so \
    cpu_freq_driver_pstate.so \
    cpu_freq_driver_governor.so \
    cpu_freq_hsmp.so \
    cpu_pow_msr.so \
    cpu_pow_hsmp.so

all: $(OBJS)

%.so: %.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o act_$@ $^

clean:
	rm -f *.o *.so