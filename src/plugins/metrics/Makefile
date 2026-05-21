OBJS  = \
    cpu_freq_act_acpi.so \
    cpu_freq_met_intel.so \
    cpu_pow_met_msr.so

all: $(OBJS)

%.so: %.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ $^

cpu_freq_met_intel.so: cpu_freq_met_intel.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ $^

cpu_freq_act_acpi.so: cpu_freq_act_acpi.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o $@ $^

clean:
	rm -f *.o *.so