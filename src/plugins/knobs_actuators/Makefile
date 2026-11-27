# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: Apache-2.0

BINS  = \
    cpu_freq.hsmp.so \
    cpu_freq.driver_pstate.so \
    cpu_freq.driver_acpi.so \
    cpu_freq.driver_governor.so \
    cpu_pow.msr.so \
    cpu_pow.hsmp.so

all: $(BINS)

%.so: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o act.$@ $< $(LDFLAGS)

install:
	cp -f *.so $(PREFIX)/lib/accessd/plugins/actuators

clean:
	rm -f *.o *.so