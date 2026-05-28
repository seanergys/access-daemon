# (Hardware) Access Daemon Prototype

Although Access Daemon is not part of the demonstrator, I wanted a  prototype because I thought the best way to find design issues is by doing.

This will allow us to discuss the potential problems of the code, logic and structure, and therefore leave some of the work done for whoever has to develop the final version.

### How to compile

1) Download the EAR code in the [Seanergys mirror](https://gitlab.seanergys.fz-juelich.de/software/ear).
2) You don't have to compile EAR, but by now you must run `autoreconf -i && ./configure` to set some definitions.
3) Back in the folder of the Access Daemon Prototype, add the `EAR_SRC` path in the root Makefile.
4) Run `make`.

### How to run

1) Just `./run.sh`.
2) Then type a string that simulates a query received from the Data Plane:

```c
Enter query: cpu_freq met get all
cpu_freq values:
1206000 1224000 1188000 1242000 1260000 1206000 1206000 1152000
Enter query:
```

Available querys:

| Metric   | Domain  | Command       | Device     | Description
|----------|---------|---------------|------------|------------
| all      | met     | get           |            | Returns all metrics for all devices.
| cpu_freq | met     | get           | all / #int | Returns the frequency of all or specific cpu.
| cpu_pow  | met     | get           | all / #int | Returns the power consumption of all or specific cpu.
| cpu_freq | met     | get/set/reset | all / #int | Returns the frequency of all or specific cpu.


### Folder structure

1) In `src/common` some EAR helper functions are compiled in `libcommon.so`. Including the framework that calls specific functions of the plugins.
2) `src/plugins/general` contains the plugins that does not produce metrics, such as `query_handler.so`, intended to be the Data Plane communicator, and `metrics_central.so`, the bridge between communications and metrics.
3) `src/plugins/metrics` contains the plugins that produce metrics.

# How to build new plugins

The Access Daemon Prototype is using a simple [framework](.src/common/framework.h) from [EAR](https://gitlab.seanergys.fz-juelich.de/software/ear) that calls specific functions of shared objects. Above all there is the function called `plug_message_receive(suffix)`. This function which is in fact a macro, once is defined in the `.so` will be called by the plugin `metrics_central.so` when receiving a concrete query.

In example, if you want to add a plugin that reads the effective CPU frequency, or `cpu_freq_met_get` in the queries terminology, and you want to do it for x86 CPUs through MSR, you can build a plugin called `cpu_freq_met_msr.so`. By defining the function `plug_message_receive(_cpu_freq_met_get)`, when the `metrics_central.so` plugin receives a query that asks for CPU frequencies, it will call `plug_message_send("cpu_freq_met_get")` which will also trigger the `plug_message_receive(_cpu_freq_met_get)` function that returns the metric.

I suggest you to inspect the code of [cpu_freq_met_dummy.c](./src/plugins/metrics/cpu_freq_met_dummy.c) and [cpu_freq_met_msr.c](./src/plugins/metrics/cpu_freq_met_msr.c) classes.

You can build different plugins that defines the `plug_message_receive(_cpu_freq_met_get)` symbol, but for different architectures. You can edit the [./run.sh](./run.sh) to add or remove the plugins you want to test.

Finally, the `plug_action_init()`, or `plug_action_init(_topology_ext)` as can be seen regularly, are important and work in similar way. The `plug_action_init(suffix)` macro is just an initialization function called by the framework. If you add a suffix to this function, it will be called after the plugin with the same name as the suffix. 

In example, the [cpu_freq_met_msr.c](./src/plugins/metrics/cpu_freq_met_msr.c) plugin requires the topology to be initialized. Then it defines the `plug_action_init(_topology_ext)` function that will be called after the `topology_ext.so` plugin initialization, and will come with pointer to the  topology structure.

As the final note, the `plug_identity()` macro returns the name of the plugin and its dependencies with other plugins.

If you want the full details about the `Plugin Manager` framework, which is its official name, you can visit the [its header](https://gitlab.bsc.es/ear_team/ear_private/-/blob/development/src/common/system/plugin_manager.h) or the [EAR Wiki](https://gitlab.bsc.es/ear_team/ear_private/-/wikis/EDCMON).