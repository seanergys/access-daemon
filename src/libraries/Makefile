SEA_CFLAGS  = -I$(SEANERBUS_PREFIX)/include
SEA_LDFLAGS = -L$(SEANERBUS_PREFIX)/lib/ -lseanerbus_c

ifdef SEANERBUS_PREFIX
BINS = access_easybus.so
endif

all: $(BINS)

%.o: %.c
	$(CC) $(CC_FLAGS) -o $@ $^

access_easybus.so: access_easybus.c
	$(CC) -Wall $(CC_FLAGS) -I../common/ $(SEA_CFLAGS) -fPIC --shared -o lib$@ $^ $(SEA_LDFLAGS)

install:
	cp -f access_easybus.h $(PREFIX)/include
ifdef SEANERBUS_PREFIX
	cp -f libaccess_easybus.so $(PREFIX)/lib
endif

clean:
	rm -f $(BINS) *.o *.so
