# Voltbro Testbench PC Cyphal Client

Standalone C++17 client library and CLI examples for PC-side access to:

- VBDRIVE-like motor telemetry over SocketCAN/Cyphal/CAN FD.
- DYN-200 dynamometer telemetry and commands.
- DYN-200 brake DAC commands.

The `VBCores/ethernet-can` bridge is external to this project. This client opens Linux SocketCAN CAN FD interfaces such as:

```text
vcan1.0 -> VBDRIVE motor, DYN-200 telemetry, brake DAC commands
```

Interface names are runtime options. The default testbench topology is one shared Cyphal/CAN bus on `vcan1.0`.

## Dependencies

- Ubuntu 24.04 or newer
- CMake 3.22+
- C++17 compiler
- Linux SocketCAN headers
- Optional: `can-utils` for `candump`
- `VBCores/libcxxcanard`. By default CMake downloads
  `https://github.com/VBCores/libcxxcanard.git` with FetchContent at pinned commit
  `07d2c2cd0e7719dc1321b1f91303e9f577e467b6`.
- Nunavut `nnvg` for regenerating DSDL headers. The repository also contains generated C headers in `generated/c`
  and generated C++ `libcxxcanard` traits in `generated/cpp`.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Options:

```text
VTC_BUILD_EXAMPLES=ON/OFF
VTC_GENERATE_DSDL=ON/OFF
NNVG_EXECUTABLE=/path/to/nnvg
VTC_LIBCXXCANARD_SOURCE_DIR=/path/to/libcxxcanard  # optional local checkout override
VTC_LIBCXXCANARD_GIT_TAG=07d2c2cd0e7719dc1321b1f91303e9f577e467b6
```

`VTC_LIBCXXCANARD_GIT_TAG` fixes the clean-machine FetchContent fallback to the tested `libcxxcanard` baseline.

## CMake Consumption

As a subdirectory:

```cmake
add_subdirectory(path/to/pc_cyphal_client)
target_link_libraries(your_app PRIVATE voltbro_testbench_client)
```

After installation:

```cmake
find_package(voltbro_testbench_client CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE voltbro::voltbro_testbench_client)
```

Install command:

```bash
cmake --install build --prefix ./install
```

Minimal single-bus usage:

```cpp
#include <voltbro_testbench_client/all.hpp>

int main() {
    auto bus = voltbro::testbench::makeCyphalInterface("vcan1.0", 100);
    voltbro::testbench::VbdriveClient motor(bus);
    voltbro::testbench::Dyn200Client dyn200(bus);
    voltbro::testbench::BrakeClient brake(bus);

    motor.onState([](const voltbro::testbench::VbdriveState& state) {
        (void) state;
    });
    dyn200.onState([](const voltbro::testbench::Dyn200State& state) {
        (void) state;
    });
    dyn200.onStatus([](const voltbro::testbench::Dyn200Status& status) {
        (void) status;
    });

    while (true) {
        bus->loop();
    }
}
```

## DSDL

Required DSDL definitions are stored under `dsdl/`. Normal builds use the committed `generated/c` and `generated/cpp` headers, so Nunavut is not required unless you regenerate DSDL. The first default CMake configure may need internet access to download the pinned `libcxxcanard` revision; use `VTC_LIBCXXCANARD_SOURCE_DIR` for offline builds with an existing checkout. The client uses `libcxxcanard` typed publication/subscriptions and generated Nunavut C + C++ traits:

```bash
tools/generate_dsdl.sh
```

If `nnvg` is installed in the local workspace venv, the script auto-detects `../.venv/bin/nnvg`. The script merges `dsdl/voltbro_types/voltbro` and `dsdl/project_types/voltbro` into a temporary namespace before invoking Nunavut, because PyDSDL does not allow two separate root namespaces with the same name. Set `LIBCXXCANARD_DIR=/path/to/libcxxcanard` if the traits templates are not available under `build/_deps/libcxxcanard-src`.

CMake generation is explicit:

```bash
cmake -S . -B build -DVTC_GENERATE_DSDL=ON -DNNVG_EXECUTABLE=/absolute/path/to/nnvg
cmake --build build -j
```

## Verify SocketCAN

```bash
ip link show
ip -details link show vcan1.0
candump vcan1.0
```

## Examples

```bash
./build/testbench_discover --iface vcan1.0 --duration-ms 3000
./build/testbench_monitor --iface vcan1.0 --mode summary --duration 10
./build/testbench_monitor --iface vcan1.0 --mode csv --duration 30 --motor-csv motor.csv --dyn-csv dyn200_brake.csv
./build/testbench_command --iface vcan1.0 motor --node-id 11 enable
./build/testbench_command --iface vcan1.0 motor --node-id 11 velocity 0.5
./build/testbench_command --iface vcan1.0 motor --node-id 11 stop
./build/testbench_command --iface vcan1.0 dyn200 pub-rate 50
./build/testbench_command --iface vcan1.0 dyn200 read-status
./build/testbench_command --iface vcan1.0 brake set 0.25
./build/testbench_command --iface vcan1.0 brake volts 2.5
./build/testbench_command --iface vcan1.0 brake off
./build/testbench_motor_sequence --iface vcan1.0 --node-id 11 --stage-ms 1500
```

Supported DYN-200 commands:

```text
dyn200 start
dyn200 stop
dyn200 acq-rate N
dyn200 pub-rate N
dyn200 zero
dyn200 read-status
dyn200 modbus-address N
dyn200 modbus-baud N
```

`dyn200 read-status` sends the DYN-200 `READ_STATUS` command on subject `5102`; the CLI then waits for bus traffic and prints the decoded summary if a status publication is observed.
`dyn200 modbus-address` and `dyn200 modbus-baud` are RAM-only runtime changes in the DYN-200 firmware; they are not documented as persistent configuration writes.

## Safety Notes

`testbench_command dyn200 zero` sends an explicit sensor zero request. No code zeroes the DYN-200 automatically during connect, monitor, or logging.

Brake commands publish `uavcan.primitive.scalar.Real32.1.0` on subject `5103`.
The single `value` field is normalized `0.0..1.0`; firmware maps it to DAC1 channel 1 on PA4, externally amplified to the brake input's `0-10 V` command range. `brake volts V` is a host-side convenience that maps `0.0..10.0 V` to normalized output. `brake off`, `brake estop`, and `brake disable` publish `value = 0.0`.

`testbench_motor_sequence` enables the motor, sends velocity/torque/position setpoints, and changes stand state. Run it only when the stand is physically safe.

`testbench_command` defaults to local Cyphal node ID `101` to avoid sharing the same source node as the Yakut examples, which commonly use node ID `100`. Override with `--local-node-id N` if needed.

## Cyphal Stack

The normal runtime path is built around `VBCores/libcxxcanard`:

- `CyphalInterface::create_heap<LinuxCAN, SystemAllocator>()` opens SocketCAN CAN FD interfaces.
- Incoming telemetry uses typed `AbstractSubscription<T>` handlers.
- Commands use `send_msg()`.
- VBDRIVE `state.is_on` uses `send_request()` and a typed `uavcan.register.Access.1.0` response subscription.

## Known Limitations

- `LinuxCAN` currently exits the process on SocketCAN open/bind errors, matching upstream `libcxxcanard` behavior.
- VBDrive telemetry subject `3811` and command subjects `2107 + node_id`, `3407 + node_id` were confirmed from `VBCores/VBDrive` source.
