# VBDRIVE Protocol

Local investigation found `voltbro/foc` DSDL in the DYN-200 firmware checkout. GitHub source `VBCores/VBDrive` was checked on 2026-06-09 and confirmed the ports and command handlers in `App/app.cpp`.

Implemented telemetry:

```text
Subject ID: 3811
Type: voltbro.foc.state_simple.1.0
Expected rate: about 1 kHz
```

Parsed fields:

```text
timestamp: uavcan.time.SynchronizedTimestamp.1.0
angle: rad
velocity: rad/s
torque: Nm
current: A
bus_voltage: V
mcu_temp: published in K; client exposes deg C
stator_temp: published in K; client exposes deg C
has_fault: bit
```

Command ports:

```text
2107 + node_id voltbro.foc.command.1.0
3407 + node_id voltbro.foc.specific_control.1.0
```

The CLI high-level `velocity` and `position` commands use
`voltbro.foc.command.1.0` on `2107 + node_id`. This serializes to a 28-byte
Cyphal payload; with the single-frame Cyphal tail byte and CAN FD DLC rounding,
the transmitted frame is 32 bytes. This matches the VBDRIVE firmware behavior
confirmed on the bench.

`voltbro.foc.specific_control.1.0` fields:

```text
uint8 set_point_type
float32 set_point_value
```

Setpoint types:

```text
0 VELOCITY -> motor->set_velocity_point()
1 TORQUE   -> motor->set_torque_point()
2 POSITION -> motor->set_angle_point()
3 VOLTAGE  -> motor->set_voltage_point()
4 UNIVERSAL currently not handled by firmware
```

`voltbro.foc.command.1.0` fields:

```text
torque Nm
angle rad
velocity rad/s
angle_kp
velocity_kp
I_kp
I_ki
```

The firmware applies this through `motor->set_foc_point()` and `motor->set_current_regulator_params()`.

Runtime registers confirmed in VBDrive:

```text
state.is_on
state.errors
limit.current
limit.torque
limit.speed
limit.min_angle
limit.max_angle
angle.offset
```

`state.is_on` is controlled through `uavcan.register.Access` service ID `384`. The client implements a short single-frame Access request for this boolean register through `VbdriveClient::enableMotor()` and `disableMotor()`. A normal startup sequence is:

```bash
./build/testbench_command --iface vcan1.0 motor --node-id 11 enable
./build/testbench_command --iface vcan1.0 motor --node-id 11 velocity 0.5
```
