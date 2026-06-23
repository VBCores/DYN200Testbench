# ethernet-can Setup

`VBCores/ethernet-can` is configured separately:

```text
https://github.com/VBCores/ethernet-can
```

Install, configure, and start it before running this client. This project does not implement the `ethernet-can` UDP data-plane protocol; it uses the SocketCAN interfaces that the bridge creates.

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
