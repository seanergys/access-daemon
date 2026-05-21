# Important question!

Is the Access Daemon aware of the concept of metrics?

There are two possible design approaches:
- **Metric-aware daemon**. If the daemon understands metrics, it must map each metric to the underlying hardware. This implies knowledge of the node architecture, required registers, and bit-level layouts needed to compute each metric. In this case, the daemon would go beyond being a minimal privileged interface and would instead become significantly more complex, requiring a larger and more sophisticated set of plugins.
- **Metric-agnostic daemon**. If the daemon has no knowledge of metrics, it would only handle low-level operations, such as reading from or writing to specific register addresses (e.g., MSR, PCI). In this model, higher-level metric computation would be delegated to external components.

If the Access Daemon knows nothing about a metric, it will never receive this request `{"action": "set_power_cap", "device": "cpu0", "value": 100}`, it will receive something like this `{"action": "write", "type": "msr", "address": 0x619, "value": 0x0000ff00}`. But then it also won't know which actuators should be more protected than others against multiple writings from different components.


# REQ-UC3-CMI-3
SHALL handle telemetry collection overhead such that monitoring does not measurably impact HPC application performance.

| CP Subrequirement | Feature Epics       | Feature Description Details
|-------------------|---------------------|----------------------------
| CP1               | Timeout             | Hardware calls must be controlled by a timeout. In case the functions don't have a timeout argument, a thread have to be used to avoid losing the control of the execution. The timeout should be provided by configuration system.
|                   | Batch of operations | In a single request, multiple reads/writes to the different devices of the same type must be supported.
|                   | Batch of metrics    | In case the Daemon is metric-aware, returning a collection of related metrics may be considered.

| Subrequirement ID | Feature Epic        | Test ID          | Test Description
|-------------------|---------------------|------------------|----------------
| UC3-CMI-3-CP1     | Timeout             | UC3-CMI-3-CP1-T1 | Check if the timeout is provided by the configuration system.
|                   | Batch of operations | UC3-CMI-3-CP1-T2 | Send requests and check if the response includes the readings or the hardware devices have changed in accordance.
|                   | Batch of metrics    | UC3-CMI-3-CP1-T3 | Send requests and check if the response includes the collection of metrics.

# REQ-UC3-CMI-4
SHALL provide access control for telemetry data based on user roles and authentication.

| CP Subrequirement | Feature Epics               | Feature Description Details
|-------------------|-----------------------------|----------------------------
| CP1               | Authentication              | If there is an interface to send request to the Daemon, out from Data Plane, the daemon shall authenticate the sender.
|                   | Message completeness        | Each request must include a User, Component or Role to be authorized.
|                   | List of authorized entities | A list of authorized Users, Components and Roles must be provided by the configuration system to be able to accept or decline a request.

| Subrequirement ID | Feature Epic                | Test ID          | Test Description
|-------------------|-----------------------------|------------------|----------------
| UC3-CMI-4-CP1     | Authentication              | UC3-CMI-4-CP1-T1 | Validate if the system rejects invalid requests.
|                   | Message completeness        | UC3-CMI-4-CP1-T2 | Validate if the system rejects incomplete requests.
|                   | List of authorized entities | UC3-CMI-4-CP1-T3 | Validate if the system rejects unauthorized requests.

# REQ-UC8-CMI-2
SHALL enforce per-component and per-node power caps recommended by the scheduler.

| CP Subrequirement | Feature Epics       | Feature Description Details
|-------------------|---------------------|----------------------------
| CP1               | Essential plugins   | Energy control plugins, such as those for MSR, HSMP, IPMI and NVML/ROCM/L0, should be considered essential and distributed with the daemon.
|                   | System health check | Each plugin must implement a validation function to verify that the system meets the required conditions for its correct operation.

| Subrequirement ID | Feature Epic                | Test ID          | Test Description
|-------------------|-----------------------------|------------------|----------------
| UC8-CMI-2-CP1     | System health check          | UC8-CMI-2-CP1-T2 | Check if all the loaded plugins includes the health check function.