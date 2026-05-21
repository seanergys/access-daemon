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

- `cpu_freq` is the name of the metric we want to get. `all` **will** be accepted to simulate a request for a set of metrics that Telemetry Collector would publish. 
- `met` is from metric, `act` is actuator.
- The command `get` works for metrics, and `get/set` for actuators.
- `all` is the device, in this case applied to all devices. But a single integer is valid too.

### Folder structure

1) In `src/common` some EAR helper functions are compiled in `libcommon.so`. Including the framework that calls specific functions of the plugins.
2) `src/plugins/general` contains the plugins that does not produce metrics, such as `query_handler.so`, intended to be the Data Plane communicator, and `metrics_central.so`, the bridge between communications and metrics.
3) `src/plugins/metrics` contains the plugins that produce metrics.

# Short-term improvements

1) The `query_handler.so` plugin has to be linked to Data Plane libraries, and configure the comms properly. 
2) The framework has to be capable to load additional plugins, because the plugin list comes from the configuration system, and that configuration is gotten from the Data Plane or other networking library.
3) A discovery function can be provided in the plugins. This could help to speed up the testing in different environments.
4) 