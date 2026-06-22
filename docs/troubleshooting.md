# Troubleshooting

Check interfaces:

```bash
ip link show vcan1.0
ip -details -statistics link show vcan1.0
candump vcan1.0
```

Common problems:

- Interface does not exist: configure/start `ethernet-can` first.
- Permission error opening SocketCAN: run with appropriate capabilities or group permissions.
- No traffic from one device: verify bridge wiring/configuration and `candump vcan1.0`.
- DYN-200 visible but no motor mapping: check VBDRIVE traffic on `vcan1.0`; only subject `3811` telemetry is decoded.
- DSDL generation missing: install Nunavut `nnvg`, or use the committed `generated/c` and `generated/cpp` headers.
- libcxxcanard download fails: allow CMake FetchContent network access, or set `VTC_LIBCXXCANARD_SOURCE_DIR=/path/to/libcxxcanard` to use an existing checkout.
- CSV file cannot be written: check output directory permissions.
