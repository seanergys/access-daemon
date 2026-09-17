data_plane.so: data_plane.cpp
	$(CXX) $(CC_FLAGS) -I$(SEANERBUS_PREFIX)/include \
		-std=c++17 -Wno-write-strings -fPIC -shared -o $@ $< \
		-Wl,-rpath,\$(SEANERBUS_PREFIX)/lib $(SEANERBUS_PREFIX)/lib/libseanerbus.so