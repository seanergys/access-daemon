OBJS  = \
    cpu_freq_msr.so \
    cpu_pow_msr.so

all: $(OBJS)

%.so: %.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o met_$@ $^

clean:
	rm -f *.o *.so