SEANERBUS_PREFIX = /home/jordi/Documents/opt/seanerbus

all:
	gcc -std=c11 -I $(SEANERBUS_PREFIX)/include sea_client.c -o sea_client -Wl,--disable-new-dtags,-rpath,\$(SEANERBUS_PREFIX)/lib $(SEANERBUS_PREFIX)/lib/libseanerbus_c.so
	gcc -std=c11 -I $(SEANERBUS_PREFIX)/include sea_server.c -o sea_server -Wl,--disable-new-dtags,-rpath,\$(SEANERBUS_PREFIX)/lib $(SEANERBUS_PREFIX)/lib/libseanerbus_c.so