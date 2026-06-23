# Completion Report

Build commands used:

```bash
cmake -S . -B build_fetch_default -DCMAKE_BUILD_TYPE=Release -DVTC_GENERATE_DSDL=OFF
cmake --build build_fetch_default -j

./build_fetch_default/testbench_command
./build_fetch_default/testbench_discover --iface vcan1.0 --duration-ms 100
./build_fetch_default/testbench_monitor --iface vcan1.0 --mode summary --duration 1
```

Detected/used compiler: GNU C++ 13.3.0.

Public APIs are header-friendly, but the runtime depends on compiled `VBCores/libcxxcanard` and its Linux `LinuxCAN` provider.

Cyphal stack used: `VBCores/libcxxcanard`.

Dependency source used by the default build:

```text
build_fetch_default/_deps/libcxxcanard-src
origin: https://github.com/VBCores/libcxxcanard
HEAD: 07d2c2cd0e7719dc1321b1f91303e9f577e467b6
```

The project pins `libcxxcanard` to the upstream revision that adds per-provider allocator ownership and callback-style subscriptions. Until upstream adds its own missing standard include, this project includes `<cstdlib>` before importing `libcxxcanard` headers.

DSDL generation method: DSDL definitions are vendored under `dsdl/`; `tools/generate_dsdl.sh` runs `nnvg` to generate both Nunavut C headers in `generated/c` and `libcxxcanard` C++ traits in `generated/cpp`.

Runtime architecture:

- SocketCAN is opened by `CyphalInterface::create_heap<LinuxCAN, SystemAllocator>()`.
- The default testbench topology is one shared SocketCAN/Cyphal interface, `vcan1.0`.
- Telemetry handlers inherit from `AbstractSubscription<T>`.
- Message commands use `send_msg()`.
- VBDRIVE `state.is_on` uses `send_request()` with `uavcan.register.Access.1.0` and a typed response subscription.
- `TestbenchClient` shares one `CyphalInterface` when motor and DYN interface names are equal.
- Supported examples are `testbench_discover`, `testbench_monitor`, `testbench_command`, `testbench_motor_sequence`, `testbench_simple_csv`, `testbench_direction_arrays`, and `testbench_torque_sweep`.
- Project-local Cyphal/CAN transport, CAN-ID construction/parsing, tail-byte handling, transfer-ID management, CAN-FD DLC padding, and command serializers are not active architecture; normal RX/TX is delegated to `VBCores/libcxxcanard`.

VBDRIVE subject IDs:

```text
3811 voltbro.foc.state_simple.1.0 telemetry
2107 + node_id voltbro.foc.command.1.0 direct FOC command port
3407 + node_id voltbro.foc.specific_control.1.0 high-level setpoint command port
384 uavcan.register.Access.1.0 service for state.is_on
```

DYN-200 subjects:

```text
7509 uavcan.node.Heartbeat.1.0
5100 voltbro.dynamometer.state.1.0
5101 voltbro.dynamometer.status.1.0
5102 voltbro.dynamometer.command.1.0
5103 uavcan.primitive.scalar.Real32.1.0
```

Known limitations:

- `LinuxCAN` currently exits the process on SocketCAN open/bind errors, matching upstream `libcxxcanard` behavior.
- Multiple independent `CyphalInterface` providers in one process are supported by the pinned `libcxxcanard` revision through per-provider allocator ownership via `CanardInstance::user_reference`.
- Live checks on `vcan1.0` confirmed VBDRIVE node `11`, DYN-200 node `42`, VBDRIVE telemetry, and DYN-200 state publications with the updated `libcxxcanard`.
