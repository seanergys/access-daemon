OBJS  = \
    cpu_freq.msr.so \
    cpu_pow.msr.so

all: $(OBJS)

%.so: %.c
	$(CC) $(CC_FLAGS) -fPIC -shared -o met.$@ $^

clean:
	rm -f *.o *.so