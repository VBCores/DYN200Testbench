# CSV Format

`testbench_monitor --mode csv` writes two buffered CSV files:

- `motor.csv` for VBDRIVE telemetry.
- `dyn200_brake.csv` for DYN-200 state/status rows.

Motor columns:

```text
host_time_ns
iface
source_node_id
subject_id
transfer_id
timestamp_us
position_rad
velocity_rad_s
torque_Nm
current_A
voltage_V
temperature_C
mcu_temperature_C
stator_temperature_C
has_fault
```

DYN-200 columns:

```text
host_time_ns
iface
source
source_node_id
subject_id
transfer_id
sample_counter
timestamp_us
angular_velocity_rad_s
torque_Nm
power_W
raw_speed
raw_torque
raw_power
status_flags
actual_acq_rate_hz
publication_rate_hz
crc_error_count
timeout_count
uart_error_count
notes
```

Fields that do not apply to a row are empty.
