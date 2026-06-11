PROGRAM := c_request_responder

CC ?= gcc
CXX ?= g++
PKG_CONFIG ?= pkg-config

CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags seanerbus 2>/dev/null)
LDLIBS += $(shell $(PKG_CONFIG) --libs seanerbus 2>/dev/null)

OBJS := receiver.o seanerbus_c.o

.PHONY: all clean check

all: check $(PROGRAM)

check:
	@$(PKG_CONFIG) --exists seanerbus || { \
		echo "ERROR: pkg-config no encuentra 'seanerbus'." >&2; \
		echo "Prueba: export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:\$$PKG_CONFIG_PATH" >&2; \
		exit 1; \
	}

$(PROGRAM): $(OBJS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

receiver.o: receiver.c seanerbus_c.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

seanerbus_c.o: seanerbus_c.cpp seanerbus_c.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(PROGRAM) $(OBJS)
