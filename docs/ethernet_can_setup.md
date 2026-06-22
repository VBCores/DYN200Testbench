# ethernet-can Setup

`VBCores/ethernet-can` is configured separately. This client does not implement its UDP data-plane protocol; it uses the SocketCAN interfaces that the bridge creates.

Current expected setup:

```text
vcan1.0 -> VBDRIVE motor + DYN-200 + brake DAC commands
```

Checks:

```bash
ip link show vcan1.0
ip -details link show vcan1.0
candump vcan1.0
```
