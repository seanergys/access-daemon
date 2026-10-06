# (Hardware) Access Daemon Prototype

The Access Daemon is a plugin-based program that runs as a Linux daemon. Each plugin represents a different metric, such as CPU frequency, temperature, FLOPS, and so on. The AD receives requests and sends responses through the [Data Plane](https://gitlab.seanergys.fz-juelich.de/software/seanerbus).

## Table of contents

- [How to compile](#how-to-compile)
- [How to test requests via keyboard plugin](#how-to-test-requests-via-keyboard-plugin)
- [How to test requests via Data Plane](#how-to-test-requests-via-data-plane)
- [Available plugins of metrics and actuators](#available-plugins-of-metrics-and-actuators)
- [String examples for the Keyboard plugin](#string-examples-for-the-keyboard-plugin)
- [Folder structure](#folder-structure)
- [Building new plugins](#building-new-plugins)

## How to compile

1) Download the dependencies: `git submodule update --init --recursive` (mainly the [EAR Mirror](https://gitlab.seanergys.fz-juelich.de/software/ear)).
2) (Optional) If you want to build the Data Plane plugin, compile and install [Seanerbus](https://gitlab.seanergys.fz-juelich.de/software/seanerbus) and its [C bindings](https://gitlab.seanergys.fz-juelich.de/software/seanerbus-cpp). Then add the `SEANERBUS_PREFIX` path to the AD root Makefile.
3) Run `make`.

## How to test requests via keyboard plugin

1) Just `sudo ./run.sh` if you want to run the Access Daemon with the keyboard plugin to receive fake requests.
2) Then type a string that simulates a request received from the Data Plane:

```c
Enter request: met freq cpu all get
Metric Cpu_Freq (KHz): 1098000 1116000 1134000 1080000 1098000 1116000 1098000 1080000 
```
Keep in mind that the plugins in `./run.sh` may not be compatible with your system. You can replace them with alternatives or remove them. However, at least one compatible plugin is required to test the Access Daemon.
3) You can close the process typing `exit`, `quit` and `q`.

## How to test requests via Data Plane

1) Run `./run-data-plane.sh`.
2) Launch both the Seanerbus Broker and Seanerbus Naming Service in another terminal: `RUST_LOG=info cargo run` and `RUST_LOG=info cargo run --bin naming_server`.
3) Run `./src/tests/metrics_by_the_sea`. You will see how all registered metrics are translated into addresses/UUIDs. After that all metrics are requested and printed.

## Available plugins of metrics and actuators

| Name                          | Metric/Actuator   | Device type | API         | Status 
|-------------------------------|-------------------|-------------|----------------------|-------
| met.cpu_freq.msr.so           | Freq, AccFreq*    | Cpu         | APERF/MPERF   | <span style="color:green">**OK**</span>
| met.cpu_pow.msr.so            | Energy, Power*    | Cpu, Socket | RAPL                 | <span style="color:green">**OK**</span>
| met.gpu_freq.nvml.so          | Freq, AccFreq*    | Gpu         | NVML                 | <span style="color:red">**IN DEVELOPMENT**</span>
| met.gpu_pow.nvml.so           | Power, Energy*    | Gpu         | NVML                 | <span style="color:red">**IN DEVELOPMENT**</span>
| act.cpu_freq.driver_pstate.so | Freq              | Cpu         | Pstate driver        | <span style="color:green">**OK**</span>
| act.cpu_freq.driver_acpi.so   | Freq              | Cpu         | ACPI driver          | <span style="color:orange">**OK**</span>
| act.cpu_freq.hsmp.so          | Freq              | Cpu         | HSMP                 | <span style="color:orange">**OK**</span>
| act.cpu_pow.msr.so            | Power             | Cpu, Socket | RAPL                 | <span style="color:green">**OK**</span>
| act.cpu_pow.hsmp.so           | Power             | Socket      | HSMP                 | <span style="color:orange">**UPDATE PENDING**</span>

**[*]**: Available if `metrics_accumulator.so` plugin is loaded.

## String examples for the Keyboard plugin

Metrics examples:
```
all all    all    all  info         # Gets information of all available metrics and actuators.
met all    all    all  get          # Gets all metrics of all devices.
met freq   all    all  get          # Gets the frequency of all devices.
met freq   cpu    all  get          # Gets the frequency of all cpus.
met freq   cpu    0    get          # Gets the frequency of a specific cpu (0 in this case).
met power  all    all  get          # Gets the power consumption of all devices.
met energy all    all  get          # Gets the energy consumed of all devices.
met power  socket all  get          # Gets the power consumption of a specific socket (0 in this case).
... and so on
```
Actuators examples:
```
act all    all    all  get          # Gets all limits set in all actuators registers.
act freq   all    all  get          # Gets all frequency limits set in all devices. 
act freq   cpu    all  get          # Gets all frequency limits set in all cpus.
act freq   cpu    all  set <n args> # Sets new frequency limits in all cpus.
act freq   cpu    all  set <1 arg>  # Sets new frequency limit in all cpus (same for all cpus) 
act freq   cpu    0    set <1 arg>  # Sets new frequency limit in a specific cpu (0 itc) 
act power  all    all  get          # Gets all power limits set in all devices.
act power  cpu    all  get          # Gets all power limits set in all cpus.
act power  socket all  get          # Gets all power limits set in all sockets.
act power  cpu    all  set <n args> # Sets new power limits in all cpus. 
act power  cpu    0    set <1 arg>  # Sets new power limit in all cpus (same for all cpus)
... and so on
```

## Folder structure

1) In `src/common`, some EAR helper functions are compiled into `libcommon.so`, including the framework that calls specific plugin functions.
2) `src/plugins/general` contains plugins that do not produce metrics, such as `request_handler.so`, intended to be the Data Plane request parser, and `metrics_calles.so`, the bridge between communications and metrics.
3) `src/plugins/metrics` contains the plugins that return metrics.
4) `src/plugins/knobs_actuators` contains the plugins that set and get actuator limits.
5) `external` where some dependencies like EAR are downloaded.

## Building new plugins

The Access Daemon Prototype uses a simple plugin framework from [EAR](https://gitlab.seanergys.fz-juelich.de/software/ear). The framework dynamically loads shared objects (`.so`) and invokes specific functions exported by those plugins.

One of the most important entry points is `plug_message_receive(suffix)`. Although it looks like a function, it is actually a macro that defines a symbol expected by the framework. When this symbol is implemented inside a plugin, it can be invoked automatically by the `metrics_caller.so` plugin whenever a matching request is received.

For example, suppose you want to implement a plugin that reads the effective CPU frequency on x86 processors using MSRs. In request terminology, the metric request would be `met freq cpu all get`. You could implement this functionality in a plugin called `met.cpu_freq.msr.so`.

Inside that plugin, you would define:
```c
plug_message_receive(_met_freq_cpu_get)
```

When `metrics_caller.so` receives a query requesting CPU frequency metrics, it internally triggers:
```c
plug_message_send("met_freq_cpu_get", ...)
```

The framework then resolves and calls the corresponding `plug_message_receive(_met_freq_cpu_get)` implementation exported by the loaded plugin, which returns the requested metric.

To better understand this mechanism, it is recommended to inspect the implementations of:

* [`metrics/cpu_freq.msr.c`](src/plugins/metrics/cpu_freq.msr.c)
* [`metrics/cpu_pow.msr.c`](src/plugins/metrics/cpu_pow.msr.c)
* [`common/framework.h`](./src/common/framework.h)

Several plugins may define the same callbacks like `plug_message_receive(_met_freq_cpu_get)`, each targeting a different CPU family or data source. You can select which plugins are loaded by editing [`run.sh`](./run.sh).

Another important mechanism is `plug_init(_suffix)`, commonly seen as `plug_init(_topology_ext)`. This macro defines initialization callbacks executed by the framework during plugin startup.

If a suffix is provided, the initialization function is executed *after* the plugin associated with that suffix has been initialized.

For example, [cpu_freq.msr.c](src/plugins/metrics/cpu_freq.msr.c) requires topology information before it can operate correctly. Therefore, it defines:

```c
plug_action_init(_topology_ext)
```

This guarantees that the function is executed only after `topology_ext.so` has completed its initialization. The framework also passes a pointer to the topology structure so the plugin can access the system topology information.

Finally, the `plug_identity()` macro is used to declare the plugin name and its dependencies on other plugins.

For a more complete description of the framework — officially called the **Plugin Manager**, you can visit [the header](https://gitlab.bsc.es/ear_team/ear_private/-/blob/development/src/common/system/plugin_manager.h) or the [EAR Wiki](https://gitlab.bsc.es/ear_team/ear_private/-/wikis/EDCMON).
