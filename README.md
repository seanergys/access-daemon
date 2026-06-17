# (Hardware) Access Daemon Prototype

Although Access Daemon is not part of the demonstrator, I wanted a  prototype because I thought the best way to find design issues is by doing.

This will allow us to discuss the potential problems of the code, logic and structure, and therefore leave some of the work done for whoever has to develop the final version.

### How to compile

1) Download the EAR code in the [~~Seanergys mirror~~](https://gitlab.seanergys.fz-juelich.de/software/ear). **Problem!** The mirror is not updating the last EAR commits where I placed additional helper functions, I suggest you to use the [official EAR repository](https://gitlab.bsc.es/ear_team/ear_private) until is solved.
2) You don't have to compile EAR, but by now you must run `autoreconf -i && ./configure` to set some definitions.
3) Back in the folder of the Access Daemon Prototype, add the `EAR_SRC` path in the root Makefile.
4) Run `make`.

### How to run

1) Just `./run.sh`.
2) Then type a string that simulates a query received from the Data Plane:

```c
Enter query: met cpu_freq get all
cpu_freq values:
1206000 1224000 1188000 1242000 1260000 1206000 1206000 1152000
Enter query: act cpu_pow set all socket 40
```

Available querys:

| Domain | Metric   | Command       | Device     | Sub-device                         | Description
|--------|----------|---------------|------------|------------------------------------|------------
| met    | all      | get           |            |                                    | Returns all metrics of all devices.
| met    | cpu_freq | get           | all / #int |                                    | Returns the frequency (KHz) of all or specific cpu.
| met    | cpu_pow  | get           | all / #int |                                    | Returns the power consumption (W) of all or specific cpu.
| act    | cpu_freq | get/set/reset | all / #int |                                    | Sets the frequency limit (KHz) of all or specific cpu.
| act    | cpu_pow  | get/set/reset | all / #int | socket/core/uncore/dram (optional) | Sets the power limit (W) of all or specific sub-devices (socket is default)

### Folder structure

1) In `src/common` some EAR helper functions are compiled in `libcommon.so`. Including the framework that calls specific functions of the plugins.
2) `src/plugins/general` contains the plugins that does not produce metrics, such as `query_handler.so`, intended to be the Data Plane communicator, and `metrics_central.so`, the bridge between communications and metrics.
3) `src/plugins/metrics` contains the plugins that produce metrics.

# How to build new plugins

The Access Daemon Prototype uses a simple plugin framework from [EAR](https://gitlab.seanergys.fz-juelich.de/software/ear). The framework dynamically loads shared objects (`.so`) and invokes specific functions exported by those plugins.

One of the most important entry points is `plug_message_receive(suffix)`. Although it looks like a function, it is actually a macro that defines a symbol expected by the framework. When this symbol is implemented inside a plugin, it can be invoked automatically by the `metrics_central.so` plugin whenever a matching query is received.

For example, suppose you want to implement a plugin that reads the effective CPU frequency on x86 processors using MSRs. In query terminology, the metric request would be `cpu_freq met get`. You could implement this functionality in a plugin called `met.cpu_freq.msr.so`.

Inside that plugin, you would define:
```c
plug_message_receive(_cpu_freq_met_get)
```

When `metrics_central.so` receives a query requesting CPU frequency metrics, it internally triggers:
```c
plug_message_send("met_cpu_freq_get")
```

The framework then resolves and calls the corresponding `plug_message_receive(_met_cpu_freq_get)` implementation exported by the loaded plugin, which returns the requested metric.

To better understand this mechanism, it is recommended to inspect the implementations in:

* [`cpu_freq.dummy.c`](src/plugins/metrics/cpu_freq.dummy.c)
* [`cpu_freq.msr.c`](src/plugins/metrics/cpu_freq.msr.c)
* [`framework.h`](./src/common/framework.h)

The framework also allows multiple plugins to implement the same symbol for different architectures. For example, several plugins may define `plug_message_receive(_met_cpu_freq_get)`, each targeting a different CPU family or data source. You can select which plugins are loaded by editing [`run.sh`](./run.sh).

Another important mechanism is `plug_action_init(_suffix)`, commonly seen as `plug_action_init(_topology_ext)`. This macro defines initialization callbacks executed by the framework during plugin startup.

If a suffix is provided, the initialization function is executed *after* the plugin associated with that suffix has been initialized.

For example, [cpu_freq.msr.c](src/plugins/metrics/cpu_freq.msr.c) requires topology information before it can operate correctly. Therefore, it defines:

```c
plug_action_init(_topology_ext)
```

This guarantees that the function is executed only after `topology_ext.so` has completed its initialization. The framework also passes a pointer to the topology structure so the plugin can access the system topology information.

Finally, the `plug_identity()` macro is used to declare the plugin name and its dependencies on other plugins.

For a more complete description of the framework — officially called the **Plugin Manager**, you can visit [the header](https://gitlab.bsc.es/ear_team/ear_private/-/blob/development/src/common/system/plugin_manager.h) or the [EAR Wiki](https://gitlab.bsc.es/ear_team/ear_private/-/wikis/EDCMON).