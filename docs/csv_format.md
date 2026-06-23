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

`testbench_torque_sweep --arm --out torque_sweep.csv` writes one sweep CSV:

```text
command_torque_Nm
motor_torque_mean_Nm
dyn200_torque_mean_Nm
motor_torque_stddev_Nm
dyn200_torque_stddev_Nm
motor_torque_min_Nm
motor_torque_max_Nm
dyn200_torque_min_Nm
dyn200_torque_max_Nm
motor_samples
dyn200_samples
state_errors_before
state_errors_after
state_errors_delta
```

Plot it with:

```bash
.venv/bin/python tools/plot_torque_sweep.py torque_sweep.csv --out torque_sweep.png
```
