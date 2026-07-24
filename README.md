# (Hardware) Access Daemon Prototype

The Access Daemon is a plugin-based program that runs as a Linux daemon. Each plugin represents a different metric, such as CPU frequency, temperature, FLOPS, and so on. The AD receives requests and sends responses through the [Data Plane](https://gitlab.seanergys.fz-juelich.de/software/seanerbus).

## Table of contents

- [How to compile](#how-to-compile)
  - [How to compile and test Seanerbus](#how-to-compile-and-test-seanerbus)
- [How to test requests via keyboard plugin](#how-to-test-requests-via-keyboard-plugin)
- [How to test requests via Data Plane](#how-to-test-requests-via-data-plane)
- [Available string requests](#available-string-requests)
- [Folder structure](#folder-structure)
- [Building new plugins](#building-new-plugins)

## How to compile

1) Download the EAR code from the [EAR Seanergys mirror](https://gitlab.seanergys.fz-juelich.de/software/ear).
2) Enter the EAR directory.
3) You don't have to compile EAR, but for now you must run `autoreconf -i && ./configure` to set some definitions.
4) (Optional) If you want to build the Data Plane plugin, compile and install [Seanerbus](https://gitlab.seanergys.fz-juelich.de/software/seanerbus).
5) Back in the Access Daemon Prototype directory, add the `EAR_SRCDIR` (and `SEANERBUS_PREFIX`) path to the root Makefile.
6) Run `make`.

### How to compile and test Seanerbus

The Seanerbus repository does not currently include this information in its README.md, so we provide a short guide here.

```c
git submodule update --init --recursive # Download dependencies such as fmt
sudo apt install capnproto libcapnp-dev # Download and install Cap'n Proto

cd seanerbus
git clean -xdf && git reset --hard
cmake -S cpp_examples -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/opt/seanerbus
cmake --build build -j2 
cmake --install build
```
```c
# If cpp_examples is not enough
cmake -S bindings/cpp -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/opt/seanerbus
```
```c
cd server
RUST_LOG=info cargo run
```
```c
cd cpp_examples/build
./response_test a366335b-52f0-4658-a42b-7e73e83ec91f
```
```c
cd cpp_examples/build
./request_test a366335b-52f0-4658-a42b-7e73e83ec91f
```

## How to test requests via keyboard plugin

1) Just `sudo ./run.sh` if you want to run the Access Daemon with the keyboard plugin to receive fake requests.
2) Then type a string that simulates a request received from the Data Plane:

```c
Enter request: met freq cpu all get
cpu_freq values:
1206000 1224000 1188000 1242000 1260000 1206000 1206000 1152000
Enter request: act pow cpu all set socket 40
```
Keep in mind that the plugins in `./run.sh` may not be compatible with your system. You can replace them with alternatives or remove them. However, at least one compatible plugin is required to test the Access Daemon.

## How to test requests via Data Plane

1) Run `./run-data-plane.sh`
2) Load the Seanerbus broker in another terminal: `RUST_LOG=info cargo run`.
3) Send a `HelloWorldReqV1` containing a string request, as in the keyboard plugin test. For example: `met freq cpu all get`. You can find an example in `cpp_examples/request_test` in the Seanerbus source folder. It will look like this:

```c
auto res = seanerbus::msg::HelloWorldReqV1("met freq cpu all get").to_capnp();
```

Make sure the UUIDs of both the Access Daemon and the request test are the same.

## Available string requests:

| Domain | Metric   | Device type | Device number | Command       | Args                        | Description
|--------|----------|-------------|---------------|---------------|-----------------------------|------------
| met    | all[^1]  | all         | all           | get           | -                           | Gets all metrics of all sensor devices.
| met    | freq     | cpu         | all / #int    | get           | -                           | Gets the frequency (KHz) of all or a specific CPU sensor.
| met    | pow      | cpu         | all / #int    | get           | -                           | Gets the power consumption (W) of all or a specific CPU sensor (pp0).[^2]
| met    | pow      | socket      | all / #int    | get           | -                           | Gets the power consumption (W) of all or a specific socket sensor (pkg).
| act    | all      | all         | all           | get           | -                           | Gets the values set in actuators of all devices
| act    | freq     | cpu         | all / #int    | get/set/reset | N uint64_t values (set)[^3] | Gets/sets/resets the frequency limit (KHz) of all or a specific CPU actuator.
| act    | pow      | cpu         | all / #int    | get/set/reset | N uint64_t values (set)     | Gets/sets/resets the power limit (W) of all or a specific CPU (pp0) actuator.
| act    | pow      | socket      | all / #int    | get/set/reset | N uint64_t values (set)     | Gets/sets/resets the power limit (W) of all or a specific socket (pkg) actuator.

[^1]: The `all` keyword can be used more broadly than this table shows. For example, it can be used to get all RAPL domains when requesting the `pow` metric or actuator.

[^2]: Support for the RAPL `dram` and `pp1` domains has also been developed, but is not exposed until needed.

[^3]: You can send one value for `all` devices. Then all the devices within an actuator domain will be set to the same value.

More examples of string/text requests:
```c
met all  all all get
met freq cpu all get
met freq cpu 2 get # Frequency of the cpu #2
met pow all all get # Power consumption of all RAPL domains
met pow socket 1 get # Power consumption of socket (pkg) #1
act pow socket all get # Power limit of all sockets (pkgs)
act pow socket all set 40 50 # New power limits for all sockets
act pow socket 0 set 40 # New power limit for socket #0
```

## Folder structure

1) In `src/common`, some EAR helper functions are compiled into `libcommon.so`, including the framework that calls specific plugin functions.
2) `src/plugins/general` contains plugins that do not produce metrics, such as `query_handler.so`, intended to be the Data Plane communicator, and `metrics_central.so`, the bridge between communications and metrics.
3) `src/plugins/metrics` contains the plugins that return metrics.
4) `src/plugins/knobs_actuators` contains the plugins that set and get actuator limits.

## Building new plugins

The Access Daemon Prototype uses a simple plugin framework from [EAR](https://gitlab.seanergys.fz-juelich.de/software/ear). The framework dynamically loads shared objects (`.so`) and invokes specific functions exported by those plugins.

One of the most important entry points is `plug_message_receive(suffix)`. Although it looks like a function, it is actually a macro that defines a symbol expected by the framework. When this symbol is implemented inside a plugin, it can be invoked automatically by the `metrics_central.so` plugin whenever a matching request is received.

For example, suppose you want to implement a plugin that reads the effective CPU frequency on x86 processors using MSRs. In request terminology, the metric request would be `met freq cpu get`. You could implement this functionality in a plugin called `met.cpu_freq.msr.so`.

Inside that plugin, you would define:
```c
plug_message_receive(_met_freq_cpu_get)
```

When `metrics_central.so` receives a query requesting CPU frequency metrics, it internally triggers:
```c
plug_message_send("met_freq_cpu_get")
```

The framework then resolves and calls the corresponding `plug_message_receive(_met_freq_cpu_get)` implementation exported by the loaded plugin, which returns the requested metric.

To better understand this mechanism, it is recommended to inspect the implementations of:

* [`cpu_freq.dummy.c`](src/plugins/metrics/cpu_freq.dummy.c)
* [`cpu_freq.msr.c`](src/plugins/metrics/cpu_freq.msr.c)
* [`framework.h`](./src/common/framework.h)

The framework also allows multiple plugins to implement the same symbol for different architectures. For example, several plugins may define `plug_message_receive(_met_freq_cpu_get)`, each targeting a different CPU family or data source. You can select which plugins are loaded by editing [`run.sh`](./run.sh).

Another important mechanism is `plug_action_init(_suffix)`, commonly seen as `plug_action_init(_topology_ext)`. This macro defines initialization callbacks executed by the framework during plugin startup.

If a suffix is provided, the initialization function is executed *after* the plugin associated with that suffix has been initialized.

For example, [cpu_freq.msr.c](src/plugins/metrics/cpu_freq.msr.c) requires topology information before it can operate correctly. Therefore, it defines:

```c
plug_action_init(_topology_ext)
```

This guarantees that the function is executed only after `topology_ext.so` has completed its initialization. The framework also passes a pointer to the topology structure so the plugin can access the system topology information.

Finally, the `plug_identity()` macro is used to declare the plugin name and its dependencies on other plugins.

For a more complete description of the framework — officially called the **Plugin Manager**, you can visit [the header](https://gitlab.bsc.es/ear_team/ear_private/-/blob/development/src/common/system/plugin_manager.h) or the [EAR Wiki](https://gitlab.bsc.es/ear_team/ear_private/-/wikis/EDCMON).
