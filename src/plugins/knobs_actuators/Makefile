OBJS  = \
    cpu_freq_acpi.so \
    cpu_pow_msr.so

all: $(OBJS)

%.so: %.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o act_$@ $^

clean:
	rm -f *.o *.so