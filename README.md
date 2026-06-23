# Voltbro Testbench PC Cyphal Client

Standalone C++17 client library and CLI examples for PC-side access to:

- VBDRIVE-like motor telemetry over SocketCAN/Cyphal/CAN FD.
- DYN-200 dynamometer telemetry and commands.
- DYN-200 brake DAC commands.

The testbench works through `VBCores/ethernet-can`:

```text
https://github.com/VBCores/ethernet-can
```

Install, configure, and start `ethernet-can` first. It creates the Linux SocketCAN CAN FD interfaces that this client opens, such as:

```text
vcan1.0 -> VBDRIVE motor, DYN-200 telemetry, brake DAC commands
```

Interface names are runtime options. The default testbench topology is one shared Cyphal/CAN bus on `vcan1.0`.

For a single-document walkthrough covering installation, architecture, message loops, commands, parsing, and storage, see `docs/developer_guide.md`.

## Dependencies

- Ubuntu 24.04 or newer
- CMake 3.22+
- C++17 compiler
- Linux SocketCAN headers
- Optional: `can-utils` for `candump`
- A configured and running `VBCores/ethernet-can` bridge that creates `vcan1.0`.
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
./build/testbench_command --iface vcan1.0 motor --node-id 11 foc --torque 0 --angle 0 --velocity 10 --angle-kp 0 --velocity-kp 6 --i-kp 3 --i-ki 1300
./build/testbench_command --iface vcan1.0 dyn200 pub-rate 50
./build/testbench_command --iface vcan1.0 dyn200 read-status
./build/testbench_command --iface vcan1.0 brake set 0.25
./build/testbench_command --iface vcan1.0 brake volts 2.5
./build/testbench_command --iface vcan1.0 brake off
./build/testbench_motor_sequence --iface vcan1.0 --node-id 11 --stage-ms 1500
./build/testbench_simple_csv
./build/testbench_direction_arrays
./build/testbench_torque_sweep --arm --min 0 --max 40 --step 1 --out torque_sweep.csv
.venv/bin/python tools/plot_torque_sweep.py torque_sweep.csv --out torque_sweep.png
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

`dyn200 read-status` sends the DYN-200 `READ_STATUS` command on subject `5102`; the CLI then waits for bus traffic and prints one decoded status line if a status publication is observed.
`dyn200 modbus-address` and `dyn200 modbus-baud` are RAM-only runtime changes in the DYN-200 firmware; they are not documented as persistent configuration writes.

## Safety Notes

`testbench_command dyn200 zero` sends an explicit sensor zero request. No code zeroes the DYN-200 automatically during connect, monitor, or logging.

Brake commands publish `uavcan.primitive.scalar.Real32.1.0` on subject `5103`.
The single `value` field is normalized `0.0..1.0`; firmware maps it to DAC1 channel 1 on PA4, externally amplified to the brake input's `0-10 V` command range. `brake volts V` is a host-side convenience that maps `0.0..10.0 V` to normalized output. `brake off`, `brake estop`, and `brake disable` publish `value = 0.0`.

`testbench_motor_sequence` enables the motor, sends velocity/torque/position setpoints, and changes stand state. Run it only when the stand is physically safe.

`testbench_simple_csv` is a no-CLI tutorial example with Russian comments in the source. It opens `vcan1.0`, creates the motor and DYN-200 clients, enables the motor, sends one FOC command, records `simple_motor.csv` and `simple_dyn200.csv` for a fixed number of seconds, then stops and disables the motor.

`testbench_direction_arrays` is a no-CLI tutorial example with Russian comments. It enables the motor, sends a positive velocity FOC command for one second, stores position/velocity/torque samples in arrays while printing periodic samples to `cout`, then repeats the same flow with negative velocity.

`testbench_torque_sweep` enables the motor only when `--arm` is passed, sweeps FOC torque commands, records averaged motor and DYN-200 torque response to CSV, stops the motor, and disables it unless `--leave-enabled` is used. `tools/plot_torque_sweep.py` plots the sweep CSV to PNG with Python + Matplotlib; use the workspace `.venv` if Matplotlib is installed there.

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
