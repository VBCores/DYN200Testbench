# DYN-200 Protocol

Confirmed from the DYN-200 firmware sources and the vendored DSDL definitions under `dsdl/project_types/voltbro/dynamometer`.

Subjects:

```text
7509 uavcan.node.Heartbeat.1.0
5100 voltbro.dynamometer.state.1.0
5101 voltbro.dynamometer.status.1.0
5102 voltbro.dynamometer.command.1.0
5103 uavcan.primitive.scalar.Real32.1.0
```

State `5100` fields: `sample_counter`, `timestamp_us`, angular velocity rad/s, torque Nm, power W, raw speed/torque/power, and `status_flags`.

Status flag bits:

```text
0 sample_valid
1 scaling_valid
2 repeated_sample
3 modbus_timeout_recent
4 modbus_crc_error_recent
5 sensor_config_uncertain
6 brake_stub_active
```

Status `5101` fields include sample counter, CRC/timeout/frame sync/UART/Cyphal counters, actual acquisition rate, publication rate, runtime DYN-200 address, runtime baudrate, and last status byte.

Command `5102` values:

```text
1 START_ACQUISITION
2 STOP_ACQUISITION
3 SET_ACQUISITION_RATE
4 SET_PUBLICATION_RATE
5 ZERO_REQUEST
6 READ_STATUS
7 SET_MODBUS_ADDRESS_RAM_ONLY
8 SET_BAUD_RAM_ONLY
```

`SET_MODBUS_ADDRESS_RAM_ONLY` and `SET_BAUD_RAM_ONLY` change runtime settings only; they are not documented as persistent configuration writes. Valid RAM-only baudrates: `9600`, `14400`, `19200`, `38400`.

Brake command `5103` payload:

```text
saturated float32 value
```

Brake behavior in current firmware:

- finite `value` in `[0.0, 1.0]` writes `round(value * 4095)` to DAC1 channel 1 on PA4.
- `value = 0.0` commands zero brake output.
- out-of-range, NaN/Inf, or structurally invalid commands are rejected and do not change DAC output.
